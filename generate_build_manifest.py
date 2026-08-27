#!/usr/bin/env python3
import os
import sys
import hashlib
import subprocess
import json
import datetime

def get_sha256(filepath):
    if not os.path.isfile(filepath):
        return None
    h = hashlib.sha256()
    with open(filepath, 'rb') as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def generate_manifest():
    # Binaries to check
    binary_paths = [
        'demo',
        'rest_api_server',
        'websocket_server',
        'dashboard_server',
        'benchmark',
        'test_suite',
        'bin/ailee_fs_gateway'
    ]

    binaries = {}
    for b in binary_paths:
        if os.path.isfile(b):
            h = get_sha256(b)
            if h:
                binaries[b] = h

    sources = {}
    try:
        src_files = subprocess.check_output(
            ['git', 'ls-files', '*.cpp', '*.hpp', '*.py', '*.c', '*.h'],
            stderr=subprocess.DEVNULL
        ).decode().splitlines()
        for s in sorted(src_files):
            if os.path.isfile(s):
                h = get_sha256(s)
                if h:
                    sources[s] = h
    except Exception:
        pass

    commit = ''
    try:
        commit = subprocess.check_output(
            ['git', 'rev-parse', 'HEAD'],
            stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        commit = 'unknown'

    epoch = os.environ.get('SOURCE_DATE_EPOCH')
    if epoch:
        ts = datetime.datetime.fromtimestamp(int(epoch), tz=datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
    else:
        ts = datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')

    cxx = os.environ.get('CXX', 'g++')
    try:
        cxx_ver = subprocess.check_output([cxx, '--version'], stderr=subprocess.DEVNULL).decode().splitlines()[0]
    except Exception:
        cxx_ver = 'unknown'

    try:
        py_ver = subprocess.check_output(['python3', '--version'], stderr=subprocess.DEVNULL).decode().strip()
    except Exception:
        py_ver = sys.version.split()[0]

    try:
        ssl_ver = subprocess.check_output(['openssl', 'version'], stderr=subprocess.DEVNULL).decode().strip()
    except Exception:
        ssl_ver = 'unknown'

    manifest = {
        'binaries': binaries,
        'compiler_version': cxx_ver,
        'dependency_versions': {
            'gcc': cxx_ver,
            'python3': py_ver,
            'openssl': ssl_ver,
            'websocketpp': '0.8.2'
        },
        'git_commit': commit,
        'source_files': sources,
        'timestamp': ts
    }

    manifest_path = 'build_manifest.json'
    with open(manifest_path, 'w', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2, sort_keys=True)
        f.write('\n')

    print(f"✓ Generated deterministic build manifest -> {manifest_path}")

    if os.environ.get('SIGN_BUILD') == '1':
        sig_path = 'build_manifest.json.asc'
        try:
            subprocess.run(
                ['gpg', '--batch', '--yes', '--detach-sign', '--armor', manifest_path],
                check=True,
                stderr=subprocess.DEVNULL
            )
            print(f"✓ Created GPG detached signature -> {sig_path}")
        except Exception as e:
            print(f"✗ Failed to GPG sign build manifest: {e}", file=sys.stderr)
            sys.exit(1)

if __name__ == '__main__':
    generate_manifest()
