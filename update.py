#!/usr/bin/env python3
# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""AILLE framework update metadata preview.

Version 25.0.0 is staged on a pull request and has no published update artifact.
This utility reports status; it does not download or install updates.
"""

VERSION = "25.0.0"
FALLBACK_URL = None


def main():
    print(f"--- AILLE Framework Update Service v{VERSION} ---")
    print("Release status: unreleased candidate; no fallback payload is published.")
    print("No updates downloaded or applied.")


if __name__ == "__main__":
    main()
