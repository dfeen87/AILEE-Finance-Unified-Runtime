# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Pure deterministic Fibonacci & Golden Ratio math functions for AILEE Finance."""

from typing import NamedTuple


class RetracementLevels(NamedTuple):
    level_236: float
    level_382: float
    level_618: float
    level_786: float


class ExtensionLevels(NamedTuple):
    level_1272: float
    level_1618: float
    level_2618: float


def compute_retracements(high: float, low: float) -> RetracementLevels:
    rng = high - low
    return RetracementLevels(
        level_236=high - rng * 0.236,
        level_382=high - rng * 0.382,
        level_618=high - rng * 0.618,
        level_786=high - rng * 0.786
    )


def compute_extensions(high: float, low: float) -> ExtensionLevels:
    rng = high - low
    return ExtensionLevels(
        level_1272=high + rng * 1.272,
        level_1618=high + rng * 1.618,
        level_2618=high + rng * 2.618
    )


def golden_ratio() -> float:
    return 1.61803398875


def project_trend(base: float, delta: float) -> float:
    return base + delta * golden_ratio()
