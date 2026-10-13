"""Endpoint/mode/consent regressions; remote origins are construction-only."""
import os
from pathlib import Path
import subprocess

import pytest

from test_alpaca_transport import ACTIONS, FAILURE, SUCCESS, adapter, certificates

ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture(scope="module")
def authorization_driver(tmp_path_factory):
    binary = tmp_path_factory.mktemp("alpaca-authorization") / "driver"
    subprocess.run([
        "g++", "-std=c++17", "-O2", "-DNDEBUG", "-pthread",
        str(ROOT / "tests/alpaca_authorization_driver.cpp"),
        str(ROOT / "ailee_plugins/plugins/execution/alpaca/AlpacaExecution.cpp"),
        "-lssl", "-lcrypto", "-o", str(binary),
    ], check=True, capture_output=True, text=True, timeout=120)
    return binary


def run(driver, url, action="inspect", mode="paper", state="none", path="direct", trust=None, cwd=None):
    env = dict(os.environ)
    if trust:
        env["SSL_CERT_FILE"] = str(trust)
    result = subprocess.run([str(driver), path, url, action, mode, state],
                            env=env, cwd=cwd, check=True, capture_output=True, text=True, timeout=15)
    return next(line[7:] for line in result.stdout.splitlines() if line.startswith("RESULT="))


@pytest.mark.parametrize("url,mode,state,expected", [
    ("https://paper-api.alpaca.markets", "paper", "none", "1:0"),
    ("https://api.alpaca.markets", "live", "consent", "1:1"),
    ("https://api.alpaca.markets", "paper", "consent", "0:0"),
    ("https://api.alpaca.markets", "paper", "none", "0:0"),
    ("https://paper-api.alpaca.markets", "live", "consent", "0:1"),
    ("https://paper-api.alpaca.markets", "live", "none", "0:1"),
    ("https://api.alpaca.markets", "live", "none", "0:1"),
    ("https://paper-api.alpaca.markets", "paper", "consent", "1:0"),
    ("https://paper-api.alpaca.markets:443/", "paper", "none", "1:0"),
])
def test_official_destination_binding(authorization_driver, url, mode, state, expected):
    assert run(authorization_driver, url, mode=mode, state=state) == expected


@pytest.mark.parametrize("url", [
    "", "api.alpaca.markets", "http://api.alpaca.markets", "http://paper-api.alpaca.markets",
    "ftp://paper-api.alpaca.markets", "https://paper.evil.invalid", "https://api.alpaca.markets.evil.invalid",
    "https://paper-api.alpaca.markets@evil.invalid", "https://user@paper-api.alpaca.markets",
    "https://paper-api.alpaca.markets?paper", "https://paper-api.alpaca.markets#paper",
    "https://paper-api.alpaca.markets/v2", "https://paper-api.alpaca.markets:444",
    "https://paper-api.alpaca.markets:", "https://paper-api.alpaca.markets:443junk",
    "https://paper-api.alpaca.markets:0", "https://localhost:65536", "https://localhost:-1",
    "https://localhost.evil.invalid", "https://127.0.0.1.evil.invalid", "https://127.1",
    "https://paper-api.alpaca.markets\\@evil.invalid", " https://paper-api.alpaca.markets",
    "https://paper-api.alpaca.markets\n", "https://paper-api.alpaca.markets.",
])
def test_ambiguous_destination_disabled(authorization_driver, url):
    assert run(authorization_driver, url) == "0:0"


@pytest.mark.parametrize("path", ["env", "registry"])
@pytest.mark.parametrize("url,expected", [
    ("https://paper-api.alpaca.markets", "1:0"),
    ("https://api.alpaca.markets", "0:0"),
    ("https://paper.evil.invalid", "0:0"),
    ("https://evil.invalid", "0:0"),
    ("", "1:0"),
])
def test_environment_cannot_authorize_live(authorization_driver, path, url, expected):
    assert run(authorization_driver, url, path=path) == expected


@pytest.mark.parametrize("action", ACTIONS)
def test_missing_live_consent_no_network(authorization_driver, certificates, action):
    connections = []
    with adapter(connections=connections) as (url, requests):
        assert run(authorization_driver, url, action, mode="live", trust=certificates["valid"][0]) == FAILURE[action]
        assert requests == []
        assert connections == []


@pytest.mark.parametrize("action", ACTIONS)
def test_authorized_live_local_tls(authorization_driver, certificates, action):
    with adapter(certificates["valid"]) as (url, requests):
        assert run(authorization_driver, url, action, "live", "consent", trust=certificates["valid"][0]) == SUCCESS[action]
        assert len(requests) == 1


@pytest.mark.parametrize("action", ACTIONS)
@pytest.mark.parametrize("suffix", ["/ignored", "?paper", "#paper", ":junk"])
def test_invalid_local_origin_no_network(authorization_driver, action, suffix):
    connections = []
    with adapter(connections=connections) as (url, requests):
        assert run(authorization_driver, url + suffix, action) == FAILURE[action]
        assert requests == []
        assert connections == []


@pytest.mark.parametrize("url", ["https://api.alpaca.markets", "https://paper.evil.invalid", "garbage"])
def test_mock_ignores_unused_destination(authorization_driver, url):
    assert run(authorization_driver, url, mode="live", state="mock") == "1:1"


@pytest.mark.parametrize("state,expected", [("none", "0"), ("consent", "1"), ("mock", "1")])
def test_daemon_constructor_and_dry_run(authorization_driver, certificates, tmp_path, state, expected):
    with adapter(certificates["valid"]) as (url, requests):
        assert run(authorization_driver, url, mode="live", state=state, path="daemon",
                   trust=certificates["valid"][0], cwd=tmp_path) == expected
        if state == "consent":
            assert len(requests) == 3
            assert all(method == "GET" and path == "/v2/account" for method, path, _, _ in requests)
        else:
            assert requests == []


@pytest.mark.parametrize("action", ACTIONS)
@pytest.mark.parametrize("state", ["missing-key", "missing-secret", "disabled", "mock"])
def test_credentials_and_mock_isolation(authorization_driver, action, state):
    connections = []
    with adapter(connections=connections) as (url, requests):
        result = run(authorization_driver, url, action, state=state)
        expected = {"submit": "MOCK-ALPACA-1", "cancel": "1", "flatten": "1", "equity": "100000.000000"}
        assert result == (expected[action] if state == "mock" else FAILURE[action])
        assert requests == connections == []


@pytest.mark.parametrize("action", ACTIONS)
def test_lockout_preserves_risk_reduction(authorization_driver, action):
    with adapter() as (url, requests):
        assert run(authorization_driver, url, action, state="lockout") == (
            FAILURE[action] if action == "submit" else SUCCESS[action])
        assert len(requests) == (0 if action == "submit" else 1)


@pytest.mark.parametrize("path", ["direct", "env", "registry"])
def test_local_paper_compatible_through_all_factories(authorization_driver, path):
    with adapter() as (url, requests):
        assert run(authorization_driver, url, "submit", path=path) == SUCCESS["submit"]
        assert len(requests) == 1


@pytest.mark.parametrize("args,exit_code,network", [
    (["--mode=live", "--no-mock", "--enable-auto-execute"], 1, False),
    (["--mode=live", "--confirm-live", "--no-mock"], 0, True),
    (["--mode=paper", "--no-mock"], 0, True),
    (["--mode=paper", "--no-mock", "--enable-auto-execute"], 0, True),
    (["--mode=live", "--confirm-live", "--enable-auto-execute"], 0, False),
])
def test_actual_cli_authorization_and_dry_run(authorization_driver, tmp_path, args, exit_code, network):
    connections = []
    with adapter(connections=connections) as (url, requests):
        env = dict(os.environ, ALPACA_BASE_URL=url)
        result = subprocess.run([str(authorization_driver), "cli", *args], env=env,
                                cwd=tmp_path, capture_output=True, text=True, timeout=15)
        assert result.returncode == exit_code
        if network:
            assert connections and requests
            if "--enable-auto-execute" in args:
                assert any(method == "POST" for method, _, _, _ in requests)
            else:
                assert all(method == "GET" and path == "/v2/account" for method, path, _, _ in requests)
                assert "DRY_RUN_SIGNAL" in result.stdout
        else:
            assert requests == connections == []


def test_cli_environment_invalid_destination_fails_before_equity(authorization_driver, tmp_path):
    connections = []
    with adapter(connections=connections) as (url, requests):
        result = subprocess.run([str(authorization_driver), "cli", "--mode=paper", "--no-mock"],
                                env=dict(os.environ, ALPACA_BASE_URL=url + "/ignored"),
                                cwd=tmp_path, capture_output=True, text=True, timeout=15)
        assert result.returncode == 1
        assert requests == connections == []


@pytest.mark.parametrize("url", ["HTTP://LOCALHOST:8080/", "https://127.0.0.1:12345", "https://[::1]:12345"])
def test_explicit_loopback_origin_policy(authorization_driver, url):
    assert run(authorization_driver, url) == "1:0"


@pytest.mark.parametrize("action", ACTIONS)
@pytest.mark.parametrize("status", [302, 307])
def test_https_redirect_cannot_change_destination_or_downgrade(authorization_driver, certificates, action, status):
    connections = []
    with adapter(connections=connections) as (target, target_requests):
        with adapter(certificates["valid"], status=status, location=target) as (url, requests):
            assert run(authorization_driver, url, action, "live", "consent",
                       trust=certificates["valid"][0]) == FAILURE[action]
            assert len(requests) == 1
            assert target_requests == connections == []
