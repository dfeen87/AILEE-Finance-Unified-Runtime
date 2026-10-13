"""Exercise the actual C++ adapter against local HTTP/TLS servers only."""

from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
from pathlib import Path
import ssl
import subprocess
import threading

import pytest


ROOT = Path(__file__).resolve().parents[1]
ACTIONS = ("submit", "cancel", "flatten", "equity")
SUCCESS = {"submit": "local-order", "cancel": "1", "flatten": "1", "equity": "75000.000000"}
FAILURE = {"submit": "", "cancel": "0", "flatten": "0", "equity": "-1.000000"}


@pytest.fixture(scope="module")
def driver(tmp_path_factory):
    binary = tmp_path_factory.mktemp("alpaca-driver") / "driver"
    subprocess.run([
        "g++", "-std=c++17", "-O2", "-DNDEBUG", "-pthread",
        str(ROOT / "tests/alpaca_transport_driver.cpp"),
        str(ROOT / "ailee_plugins/plugins/execution/alpaca/AlpacaExecution.cpp"),
        "-lssl", "-lcrypto", "-o", str(binary),
    ], check=True, capture_output=True, text=True, timeout=120)
    return binary


@pytest.fixture(scope="module")
def certificates(tmp_path_factory):
    directory = tmp_path_factory.mktemp("alpaca-certs")
    result = {}
    for name, hostname in (("valid", "localhost"), ("wrong_host", "wrong.invalid")):
        cert, key = directory / f"{name}.pem", directory / f"{name}.key"
        subprocess.run([
            "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
            "-keyout", str(key), "-out", str(cert), "-days", "1",
            "-subj", f"/CN={hostname}", "-addext", f"subjectAltName=DNS:{hostname}",
        ], check=True, capture_output=True, text=True, timeout=30)
        result[name] = (cert, key)
    return result


@contextmanager
def adapter(certificate=None, status=200):
    requests = []

    class Handler(BaseHTTPRequestHandler):
        def respond(self):
            size = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(size).decode()
            requests.append((self.command, self.path, dict(self.headers), body))
            payload = b'{"equity":"75000"}' if self.path == "/v2/account" else b'{"id":"local-order"}'
            self.send_response(status)
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)

        do_GET = do_POST = do_DELETE = respond

        def log_message(self, *args):
            pass

    server = HTTPServer(("127.0.0.1", 0), Handler)
    if certificate:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(*map(str, certificate))
        server.socket = context.wrap_socket(server.socket, server_side=True)
    worker = threading.Thread(target=server.serve_forever, kwargs={"poll_interval": 0.01}, daemon=True)
    worker.start()
    try:
        scheme = "https" if certificate else "http"
        yield f"{scheme}://localhost:{server.server_port}", requests
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=5)
        assert not worker.is_alive()


def invoke(driver, url, action, mode="network", trust=None):
    env = dict(os.environ)
    if trust:
        env["SSL_CERT_FILE"] = str(trust)
    completed = subprocess.run(
        [str(driver), url, action, mode], env=env, check=True,
        capture_output=True, text=True, timeout=15,
    )
    return next(line.removeprefix("RESULT=") for line in completed.stdout.splitlines() if line.startswith("RESULT="))


@pytest.mark.parametrize("action", ACTIONS)
def test_tls_trusted_adapter_acknowledgment(driver, certificates, action):
    with adapter(certificates["valid"]) as (url, requests):
        assert invoke(driver, url, action, trust=certificates["valid"][0]) == SUCCESS[action]
        assert len(requests) == 1
        method, path, headers, body = requests[0]
        assert headers["APCA-API-KEY-ID"] == "synthetic-pass-c-key"
        assert headers["APCA-API-SECRET-KEY"] == "synthetic-pass-c-secret"
        assert (method, path) == {
            "submit": ("POST", "/v2/orders"), "cancel": ("DELETE", "/v2/orders/local-order"),
            "flatten": ("DELETE", "/v2/positions/SPY"), "equity": ("GET", "/v2/account"),
        }[action]
        if action == "submit":
            assert json.loads(body) == {"symbol": "SPY", "qty": 2, "side": "buy", "type": "market", "time_in_force": "day"}


@pytest.mark.parametrize("action", ACTIONS)
@pytest.mark.parametrize("failure", ("untrusted", "wrong_host"))
def test_tls_rejection_sends_no_authenticated_request(driver, certificates, action, failure):
    server_cert = certificates["valid" if failure == "untrusted" else "wrong_host"]
    trust = certificates["wrong_host"][0]
    with adapter(server_cert) as (url, requests):
        assert invoke(driver, url, action, trust=trust) == FAILURE[action]
        assert requests == []


@pytest.mark.parametrize("action", ACTIONS)
def test_explicit_local_http_remains_compatible(driver, action):
    with adapter() as (url, requests):
        assert invoke(driver, url, action) == SUCCESS[action]
        assert len(requests) == 1


@pytest.mark.parametrize("action", ACTIONS)
def test_https_does_not_fall_back_to_plain_http(driver, action):
    with adapter() as (url, requests):
        assert invoke(driver, url.replace("http://", "https://"), action) == FAILURE[action]
        assert requests == []


@pytest.mark.parametrize("action", ACTIONS)
def test_tls_adapter_rejection_is_not_success(driver, certificates, action):
    with adapter(certificates["valid"], status=401) as (url, requests):
        assert invoke(driver, url, action, trust=certificates["valid"][0]) == FAILURE[action]
        assert len(requests) == 1


@pytest.mark.parametrize("action", ACTIONS)
@pytest.mark.parametrize("mode", ("mock", "disabled"))
def test_mock_and_missing_credentials_do_not_contact_adapter(driver, certificates, action, mode):
    with adapter(certificates["valid"]) as (url, requests):
        result = invoke(driver, url, action, mode, trust=certificates["valid"][0])
        if mode == "disabled":
            assert result == FAILURE[action]
        elif action == "submit":
            assert result == "MOCK-ALPACA-1"
        elif action == "equity":
            assert result == "100000.000000"
        else:
            assert result == "1"
        assert requests == []


def test_lockout_does_not_submit_to_adapter(driver, certificates):
    with adapter(certificates["valid"]) as (url, requests):
        assert invoke(driver, url, "submit", "lockout", certificates["valid"][0]) == ""
        assert requests == []
