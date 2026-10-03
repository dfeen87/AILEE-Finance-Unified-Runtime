"""Release metadata consistency checks for active distribution surfaces."""

import json
import re
from pathlib import Path

import ailee_finance


ROOT = Path(__file__).resolve().parents[1]
EXPECTED_VERSION = "24.0.0"


def test_active_release_metadata_is_consistent():
    assert ailee_finance.__version__ == EXPECTED_VERSION
    assert (ROOT / "release" / "VERSION").read_text().strip() == EXPECTED_VERSION
    assert json.loads((ROOT / "update_source.json").read_text())["version"] == EXPECTED_VERSION

    cmake = (ROOT / "CMakeLists.txt").read_text()
    assert f"project(AILLE VERSION {EXPECTED_VERSION}" in cmake

    header = (ROOT / "aille.hpp").read_text()
    assert f'AILLE_VERSION = "{EXPECTED_VERSION}"' in header
    assert re.search(r"AILLE_VERSION_MAJOR\s*=\s*24;", header)
    assert re.search(r"AILLE_VERSION_MINOR\s*=\s*0;", header)
    assert re.search(r"AILLE_VERSION_PATCH\s*=\s*0;", header)
