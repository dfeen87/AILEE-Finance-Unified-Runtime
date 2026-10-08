#!/usr/bin/env python3
"""Exercise the actual REST example through startup, HTTP, shutdown and rebind."""

import argparse
import json
import math
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def request(port, path, payload=None):
    data = None if payload is None else json.dumps(payload).encode()
    req = urllib.request.Request(
        f"http://127.0.0.1:{port}{path}", data=data,
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=2) as response:
        require(response.status == 200, f"{path} returned {response.status}")
        return json.loads(response.read())


def exercise(binary, port, directory, iteration):
    log_path = directory / f"server-{iteration}.log"
    with log_path.open("w") as log:
        process = subprocess.Popen(
            [str(binary), str(port), "127.0.0.1"], cwd=directory,
            stdout=log, stderr=subprocess.STDOUT,
        )
        try:
            deadline = time.monotonic() + 10
            while True:
                require(process.poll() is None, "REST process exited during startup")
                try:
                    health = request(port, "/health")
                    break
                except (urllib.error.URLError, TimeoutError, ConnectionError):
                    require(time.monotonic() < deadline, "REST startup timed out")
                    time.sleep(0.02)
            require(health.get("status") == "healthy", "health contract changed")
            decision = request(port, "/api/decision", [
                {"value": 0.01, "confidence": 0.95, "model_id": 0},
                {"value": 0.011, "confidence": 0.95, "model_id": 1},
            ])
            require(decision.get("status") == "valid", "valid decision contract changed")
            require(all(math.isfinite(decision[key]) for key in ("final_value", "confidence")),
                    "decision must contain finite numeric output")
            process.terminate()
            try:
                code = process.wait(timeout=12)
            except subprocess.TimeoutExpired as exc:
                raise RuntimeError("SIGTERM did not complete REST shutdown within 12 seconds") from exc
            require(code == 0, f"REST shutdown returned {code}")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=2)
    require("Server stopped. Goodbye!" in log_path.read_text(),
            "REST example did not complete its cleanup path")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server-binary", type=Path,
                        default=Path(__file__).resolve().parents[1] / "rest_api_server")
    args = parser.parse_args()
    binary = args.server_binary.resolve()
    require(binary.is_file() and os.access(binary, os.X_OK),
            f"Required REST executable is missing or not executable: {binary}")
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    with tempfile.TemporaryDirectory(prefix="aille-rest-lifecycle-") as temporary:
        directory = Path(temporary)
        try:
            for iteration in range(2):
                exercise(binary, port, directory, iteration)
            # A bind collision must finish startup and cleanup without waiting
            # forever or shutting down the existing listener.
            with socket.socket() as occupied:
                occupied.bind(("127.0.0.1", 0))
                occupied.listen()
                collision_port = occupied.getsockname()[1]
                result = subprocess.run(
                    [str(binary), str(collision_port), "127.0.0.1"], cwd=directory,
                    capture_output=True, text=True, timeout=12,
                )
                require(result.returncode == 0, "bind-collision cleanup failed")
                require("Failed to start server" in result.stdout + result.stderr,
                        "occupied port was not rejected")
        except Exception:
            for log_path in sorted(directory.glob("server-*.log")):
                print(log_path.read_text()[-4000:])
            raise
    print("REST lifecycle passed: HTTP decision, SIGTERM shutdown, rebind, and bind collision")


if __name__ == "__main__":
    main()
