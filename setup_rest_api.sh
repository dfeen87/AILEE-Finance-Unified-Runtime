#!/bin/bash
# Fetch an immutable, checksum-verified HTTP dependency.
set -euo pipefail
mkdir -p external
http_temp=$(mktemp external/httplib.XXXXXX)
trap 'rm -f "$http_temp"' EXIT
curl --fail --location --silent --show-error --proto '=https' --tlsv1.2 \
  https://raw.githubusercontent.com/yhirose/cpp-httplib/24e58ce5b25d55045af4cd6f463e83a8082fbe6b/httplib.h \
  -o "$http_temp"
python3 - "$http_temp" <<'PYTHON'
import hashlib
from pathlib import Path
import sys
expected = "805169049e6a6e9ba7729d7e01d14ced825029c88a277586602a6b38df4d5357"
if hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest() != expected:
    raise SystemExit("cpp-httplib checksum mismatch")
PYTHON
mv "$http_temp" external/httplib.h
printf 'Verified cpp-httplib v0.64.0 installed\n'
