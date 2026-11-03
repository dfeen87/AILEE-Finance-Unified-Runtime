"""Release metadata consistency checks for active distribution surfaces."""

import json
import re
from pathlib import Path

import ailee_finance


ROOT = Path(__file__).resolve().parents[1]
EXPECTED_VERSION = "25.0.0"


def test_active_release_metadata_is_consistent():
    assert ailee_finance.__version__ == EXPECTED_VERSION
    assert (ROOT / "release" / "VERSION").read_text().strip() == EXPECTED_VERSION
    registry = json.loads((ROOT / "update_source.json").read_text())
    assert registry["version"] == EXPECTED_VERSION
    # Unreleased source metadata must not advertise a non-existent artifact.
    assert registry["update_channel"] == "unreleased"
    assert registry["fallback_payload_url"] is None

    cmake = (ROOT / "CMakeLists.txt").read_text()
    assert f"project(AILLE VERSION {EXPECTED_VERSION}" in cmake

    header = (ROOT / "aille.hpp").read_text()
    assert f'AILLE_VERSION = "{EXPECTED_VERSION}"' in header
    assert re.search(r"AILLE_VERSION_MAJOR\s*=\s*25;", header)
    assert re.search(r"AILLE_VERSION_MINOR\s*=\s*0;", header)
    assert re.search(r"AILLE_VERSION_PATCH\s*=\s*0;", header)

def test_release_build_and_documentation_version():
    makefile = (ROOT / "Makefile").read_text()
    assert f'echo "{EXPECTED_VERSION}" > release/VERSION' in makefile
    assert f'test_v{EXPECTED_VERSION.split(".")[0]}:' in makefile
    readme = (ROOT / "README.md").read_text()
    assert f'badge/version-{EXPECTED_VERSION}-' in readme
    assert f'ailee-finance-unified-runtime:{EXPECTED_VERSION}' in readme
