"""Synthetic loopback-only broker acknowledgment and durable replay boundaries."""
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
from pathlib import Path
import subprocess
import threading
import pytest
from cpp_toolchain import tls_platform_flags
ROOT = Path(__file__).resolve().parents[1]

@pytest.fixture(scope="module")
def integrity_driver(tmp_path_factory):
    binary=tmp_path_factory.mktemp("integrity-driver")/"driver"
    subprocess.run(["g++","-std=c++17","-O2","-DNDEBUG","-pthread",
        str(ROOT/"tests/alpaca_integrity_driver.cpp"),str(ROOT/"ailee_plugins/plugins/execution/alpaca/AlpacaExecution.cpp"),
        "-lssl","-lcrypto",*tls_platform_flags(),"-o",str(binary)],check=True,capture_output=True,text=True)
    return binary

@contextmanager
def broker(body=b'{"id": "acknowledged-order", "status": "accepted"}', status=200, account_body=None):
    requests=[]
    class Handler(BaseHTTPRequestHandler):
        def request(self):
            raw=self.rfile.read(int(self.headers.get("Content-Length",0)))
            requests.append((self.command,self.path,raw))
            payload=account_body if self.path=="/v2/account" and account_body is not None else body
            self.send_response(status); self.send_header("Content-Length",str(len(payload))); self.end_headers(); self.wfile.write(payload)
        do_POST=do_GET=do_DELETE=request
        def log_message(self,*args): pass
    server=HTTPServer(("127.0.0.1",0),Handler)
    worker=threading.Thread(target=server.serve_forever,daemon=True); worker.start()
    try: yield f"http://localhost:{server.server_port}",requests
    finally: server.shutdown(); server.server_close(); worker.join()

def invoke(driver,url,journal,mode="submit"):
    result=subprocess.run([str(driver),url,str(journal),mode],check=True,capture_output=True,text=True,timeout=15)
    return next(line[7:] for line in result.stdout.splitlines() if line.startswith("RESULT="))

@pytest.mark.parametrize("body",[b'{}',b'{"nested":{"id":"fake"}}',b'{"id":null}',b'{"id":""}',b'{"id":123}',
    b'{"id":"one","id":"two"}',b'{"id":"accepted"} trailing',b'{"id":"yes","status":"rejected"}',b'{"id":"yes"',b'[]'])
def test_ambiguous_ack_is_never_success_and_never_retried(integrity_driver,tmp_path,body):
    with broker(body) as (url,requests):
        journal=tmp_path/"orders"
        assert invoke(integrity_driver,url,journal)==""
        assert invoke(integrity_driver,url,journal)==""
        assert len(requests)==1

@pytest.mark.parametrize("status",[200,201])
def test_real_id_limit_and_client_identity_are_preserved(integrity_driver,tmp_path,status):
    with broker(status=status) as (url,requests):
        journal=tmp_path/"orders"
        assert invoke(integrity_driver,url,journal,"fractional")=="acknowledged-order"
        assert invoke(integrity_driver,url,journal,"fractional")=="acknowledged-order"
        assert len(requests)==1
        assert json.loads(requests[0][2])=={"symbol":"SPY","qty":2.5,"side":"buy","type":"limit","limit_price":100,
            "time_in_force":"day","client_order_id":"integrity-intent"}

@pytest.mark.parametrize("mode",["missing-journal","market","sell","nan-quantity","infinite-limit","too-large","bad-symbol","bad-ref","bad-config","cancel-path","flatten-path"])
def test_unsafe_requests_never_reach_network(integrity_driver,tmp_path,mode):
    with broker() as (url,requests):
        assert invoke(integrity_driver,url,tmp_path/"orders",mode) in ("","0")
        assert requests==[]

def test_same_identity_cannot_change_payload(integrity_driver,tmp_path):
    with broker() as (url,requests):
        assert invoke(integrity_driver,url,tmp_path/"orders","collision")=="acknowledged-order:"
        assert len(requests)==1

@pytest.mark.parametrize("body",[b'{"equity":"nan"}',b'{"equity":"Infinity"}',b'{"equity":"75000garbage"}',
    b'{"equity":-1}',b'{"equity":true}',b'{"nested":{"equity":"75000"}}',b'{"equity":"75000","equity":"1"}'])
def test_malformed_equity_never_becomes_healthy_account(integrity_driver,tmp_path,body):
    with broker(body) as (url,requests):
        assert invoke(integrity_driver,url,tmp_path/"orders","equity")=="-1"


def test_corrupt_journal_blocks_network(integrity_driver,tmp_path):
    journal=tmp_path/"orders"
    journal.write_text("AILEE-INTENT-V1\nP\tpartial")
    journal.chmod(0o600)
    with broker() as (url,requests):
        assert invoke(integrity_driver,url,journal)==""
        assert requests==[]

@pytest.mark.parametrize("body",[b'{"id":"accepted-unfilled","status":"accepted"}',b'{}'])
def test_daemon_never_turns_accepted_or_unknown_order_into_filled_position(integrity_driver,tmp_path,body):
    with broker(body,account_body=b'{"equity":"100000"}') as (url,requests):
        assert invoke(integrity_driver,url,tmp_path/"orders","daemon")=="1:2"
        assert sum(method=="POST" for method,_,_ in requests)==1

@pytest.mark.parametrize("mode",["route-fallback","route-zero","route-nan","route-negative"])
def test_generic_routing_never_converts_invalid_decision_into_order(integrity_driver,tmp_path,mode):
    with broker() as (url,requests):
        assert invoke(integrity_driver,url,tmp_path/"orders",mode)==""
        assert requests==[]


def test_durable_store_has_one_exclusive_writer(integrity_driver,tmp_path):
    with broker() as (url,requests):
        assert invoke(integrity_driver,url,tmp_path/"orders","locked-store")=="0"
        assert requests==[]


def test_accepted_symbol_is_not_executed_again_under_new_identity(integrity_driver,tmp_path):
    with broker() as (url,requests):
        journal=tmp_path/"orders"
        assert invoke(integrity_driver,url,journal)=="acknowledged-order"
        assert invoke(integrity_driver,url,journal,"new-ref")==""
        assert len(requests)==1


def test_symlinked_journal_is_rejected(integrity_driver,tmp_path):
    target=tmp_path/"target"; target.write_text(""); target.chmod(0o600)
    journal=tmp_path/"orders"; journal.symlink_to(target)
    with broker() as (url,requests):
        assert invoke(integrity_driver,url,journal)==""
        assert requests==[]


def test_safety_fault_attempts_cancel_of_known_pending_order(integrity_driver,tmp_path):
    with broker(account_body=b'{"equity":"100000"}') as (url,requests):
        assert invoke(integrity_driver,url,tmp_path/"orders","daemon-kill")=="1:2"
        assert sum(method=="POST" for method,_,_ in requests)==1
        assert any(method=="DELETE" and path=="/v2/orders/acknowledged-order" for method,path,_ in requests)
