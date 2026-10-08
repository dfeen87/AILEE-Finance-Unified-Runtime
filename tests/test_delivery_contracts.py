"""Exercise distribution artifacts outside the source checkout."""

import os
from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess
import sys
import zipfile

import pytest


ROOT = Path(__file__).resolve().parents[1]


def test_wheel_contains_runtime_and_works_without_dev_dependencies(tmp_path):
    source = tmp_path / "source"
    source.mkdir()
    for name in ("setup.py", "LICENSE"):
        shutil.copy2(ROOT / name, source / name)
    for name in ("ailee_finance", "core", "src"):
        shutil.copytree(ROOT / name, source / name, ignore=shutil.ignore_patterns("__pycache__"))
    # Automatic discovery must not turn a neighboring development directory into SDK output.
    (source / "tests").mkdir()
    (source / "tests" / "__init__.py").write_text("")
    subprocess.run(
        [sys.executable, "setup.py", "bdist_wheel", "--dist-dir", str(tmp_path / "dist")],
        cwd=source, check=True, capture_output=True, text=True,
    )
    wheel = next((tmp_path / "dist").glob("*.whl"))
    installed = tmp_path / "installed"
    with zipfile.ZipFile(wheel) as archive:
        names = archive.namelist()
        assert "ailee_finance/core_min.py" in names
        assert "core/finance_kernel/kernel_config.py" in names
        assert not any(name.startswith(("tests/", "src/", "agents.py", "market_sim.py")) for name in names)
        metadata = archive.read(next(name for name in names if name.endswith(".dist-info/METADATA"))).decode()
        assert "Requires-Dist: pytest" not in metadata
        archive.extractall(installed)
    # -I -S excludes the checkout, PYTHONPATH, user site, and installed test dependencies.
    script = """
import sys
import json
sys.path.insert(0, sys.argv[1])
import ailee_finance
from ailee_finance.core_min import AileeFinanceTrustPipeline
from core.finance_kernel import FinanceKernelConfig
from core.finance_kernel.kernel_errors import KernelConfigurationError
if ailee_finance.__version__ != '24.0.0':
    raise RuntimeError('incorrect wheel version')
FinanceKernelConfig()
pipeline = AileeFinanceTrustPipeline()
signals = {
    'position_size': 1000.0, 'trust_score': 0.9, 'market': {},
    'feeds': [{'price': 100.0, 'confidence': 0.95},
              {'price': 100.0, 'confidence': 0.95}],
}
decision = pipeline.process_sell(signals)
if decision.level != 0 or not decision.bullish_mode_active or decision.allowed_sell_amount != 800.0:
    raise RuntimeError('installed wheel cannot execute a valid SELL advisory')
signals['trust_score'] = float('nan')
protected = pipeline.process_sell(signals)
if protected.level != 3 or protected.bullish_mode_active:
    raise RuntimeError('installed wheel promotes invalid trust evidence')
json.dumps(protected.to_dict(), allow_nan=False)
try:
    FinanceKernelConfig(operator_timeout=float('nan'))
except KernelConfigurationError:
    pass
else:
    raise RuntimeError('installed wheel lost BEDROCK validation')
"""
    consumer_command = [sys.executable, "-I", "-S"]
    if sys.flags.optimize:
        consumer_command.append("-O")
    subprocess.run(consumer_command + ["-c", script, str(installed)], cwd=tmp_path, check=True)


def test_docker_context_excludes_nested_secrets_and_keeps_build_inputs(tmp_path):
    docker = shutil.which("docker")
    if docker is None:
        pytest.skip("Docker context integration requires Docker and a running daemon")
    source = tmp_path / "context"
    source.mkdir()
    if (ROOT / ".dockerignore").exists():
        shutil.copy2(ROOT / ".dockerignore", source / ".dockerignore")
    excluded = (".env", "nested/.env.production", "nested/private.key", "nested/credentials.json", "nested/.git/config")
    retained = ("Makefile", "aille.hpp", "external/httplib.h", "config/sim_config.yaml", "core/finance_kernel/kernel_config.py", "nested/.env.example")
    for name in excluded + retained:
        path = source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("synthetic build-context probe\n")
    output = tmp_path / "export"
    env = dict(os.environ, DOCKER_CONFIG=str(tmp_path / "docker-config"))
    subprocess.run(
        [docker, "buildx", "build", "--output", f"type=local,dest={output}", "-f", "-", str(source)],
        input="FROM scratch\nCOPY . /\n", env=env, check=True, capture_output=True, text=True,
    )
    for name in excluded:
        assert not (output / name).exists(), f"secret-bearing path entered Docker context: {name}"
    for name in retained:
        assert (output / name).exists(), f"required build input was excluded: {name}"


@pytest.mark.parametrize("filename, pattern", (
    ("setup.py", r'version="([0-9.]+)"'),
    ("CITATION.cff", r'^version: ([0-9.]+)$'),
    ("docker-compose.yml", r'image: ailee-finance-unified-runtime:([0-9.]+)'),
    ("ailee_runtime/fs_gateway/fs_gateway.hpp", r'FS_GATEWAY_VERSION = "([0-9.]+)"'),
    ("ailee_plugins/plugins/dashboard/index.html", r'class="v-tag">V([0-9.]+)</span>'),
    ("diagnostics.py", r'VERSION_HEADER = "AILLEE Diagnostics v([0-9.]+)"'),
    ("update.py", r'^VERSION = "([0-9.]+)"'),
))
def test_active_delivery_versions_match_runtime(filename, pattern):
    from ailee_finance import __version__

    match = re.search(pattern, (ROOT / filename).read_text(), re.MULTILINE)
    assert match is not None, f"missing version metadata: {filename}"
    assert match.group(1) == __version__, f"inconsistent delivery version: {filename}"


def test_release_rebuilds_stale_binaries_and_hashes_packaged_artifacts(tmp_path):
    source = tmp_path / "source"
    source.mkdir()
    archive = tmp_path / "source.tar"
    subprocess.run(["git", "archive", "--output", str(archive), "HEAD"], cwd=ROOT, check=True)
    subprocess.run(["tar", "-xf", str(archive), "-C", str(source)], check=True)
    shutil.copy2(ROOT / "Makefile", source / "Makefile")
    binaries = ("demo", "rest_api_server", "websocket_server", "dashboard_server", "benchmark", "test_suite", "bin/ailee_fs_gateway")
    for name in binaries:
        path = source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("#!/bin/sh\n# STALE ARTIFACT\nexit 0\n")
        path.chmod(0o755)
        os.utime(path, (1, 1))
    compiler = tmp_path / "compiler"
    compiler.write_text(f"""#!{sys.executable}
import pathlib, sys
if '--version' in sys.argv:
    print('Pass B controlled compiler')
else:
    output = pathlib.Path(sys.argv[sys.argv.index('-o') + 1])
    output.write_text('#!/bin/sh\\n# CURRENT ARTIFACT\\nexit 0\\n')
    output.chmod(0o755)
""")
    compiler.chmod(0o755)
    subprocess.run(["make", "release", f"CXX={compiler}"], cwd=source, check=True, capture_output=True, text=True)
    manifest = json.loads((source / "release/build_manifest.json").read_text())
    for name in binaries:
        packaged = source / "release" / Path(name).name
        assert "CURRENT ARTIFACT" in packaged.read_text(), f"release reused stale binary: {name}"
        assert manifest["binaries"][name] == hashlib.sha256(packaged.read_bytes()).hexdigest()


def test_docker_build_identity_tracks_artifact_changes(tmp_path):
    instructions = (ROOT / "Dockerfile").read_text().replace("\\\n", " ").splitlines()
    command = next(line[4:] for line in instructions if line.startswith("RUN ") and "BUILD_HASH" in line)
    command = command.replace("/src/BUILD_HASH", "BUILD_HASH")
    for name in ("Makefile", "aille.hpp", "demo", "test_suite"):
        (tmp_path / name).write_text("original artifact\n")
    subprocess.run(["sh", "-ec", command], cwd=tmp_path, check=True)
    original = (tmp_path / "BUILD_HASH").read_text()
    subprocess.run(["sh", "-ec", command], cwd=tmp_path, check=True)
    assert (tmp_path / "BUILD_HASH").read_text() == original
    (tmp_path / "test_suite").write_text("changed regression artifact\n")
    subprocess.run(["sh", "-ec", command], cwd=tmp_path, check=True)
    assert (tmp_path / "BUILD_HASH").read_text() != original
