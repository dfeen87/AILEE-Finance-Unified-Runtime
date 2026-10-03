# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""HFT Bullish Bias Layer - Safety-Gated Configuration and Execution Helpers."""

import math
from typing import Any, Dict, Optional


def is_bullish_mode_allowed(
    trust_score: float,
    manipulation_score: float,
    drawdown_state: Any = False,
    hft_bias_config: Optional[Dict[str, Any]] = None
) -> bool:
    """
    Evaluates whether bullish mode is allowed based on safety rails:
    1. hft_bias.enabled must be True
    2. trust_score >= trust_threshold_bullish (default 0.70)
    3. manipulation_score <= manipulation_threshold (default 0.30)
    4. daily_drawdown_limit is NOT near breach or breached
    """
    if hft_bias_config is None:
        hft_bias_config = {
            "enabled": True,
            "bullishness_mode": "STANDARD",
            "trust_threshold_bullish": 0.70,
            "manipulation_threshold": 0.30,
        }

    enabled = hft_bias_config.get("enabled", True)
    if not isinstance(enabled, bool) or not enabled:
        return False

    try:
        trust = float(trust_score)
        manipulation = float(manipulation_score)
        trust_thresh = float(hft_bias_config.get("trust_threshold_bullish", 0.70))
        manip_thresh = float(hft_bias_config.get("manipulation_threshold", 0.30))
    except (TypeError, ValueError):
        return False

    if not all(math.isfinite(value) for value in (trust, manipulation, trust_thresh, manip_thresh)):
        return False

    if trust < trust_thresh:
        return False

    if manipulation > manip_thresh:
        return False

    if isinstance(drawdown_state, bool):
        if drawdown_state:  # True indicates near breach or breached
            return False
    elif isinstance(drawdown_state, (int, float)):
        # If float represents current drawdown percentage/ratio, e.g. 0.04 (4%)
        drawdown = float(drawdown_state)
        if not math.isfinite(drawdown) or drawdown < 0.0:
            return False
        if drawdown >= 0.04:  # Near breach threshold
            return False
    elif isinstance(drawdown_state, dict):
        safety_fields = ("near_breach", "breached", "locked_out")
        present_fields = [field for field in safety_fields if field in drawdown_state]
        if not present_fields:
            return False
        if any(not isinstance(drawdown_state[field], bool) for field in present_fields):
            return False
        if any(drawdown_state[field] for field in present_fields):
            return False
    else:
        return False

    return True
