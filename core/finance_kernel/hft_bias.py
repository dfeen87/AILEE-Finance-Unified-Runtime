# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""HFT Bullish Bias Layer - Safety-Gated Configuration and Execution Helpers."""

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
            "trust_threshold_bullish": 0.70,
            "manipulation_threshold": 0.30,
        }

    enabled = bool(hft_bias_config.get("enabled", True))
    if not enabled:
        return False

    trust_thresh = float(hft_bias_config.get("trust_threshold_bullish", 0.70))
    manip_thresh = float(hft_bias_config.get("manipulation_threshold", 0.30))

    if float(trust_score) < trust_thresh:
        return False

    if float(manipulation_score) > manip_thresh:
        return False

    if isinstance(drawdown_state, bool):
        if drawdown_state:  # True indicates near breach or breached
            return False
    elif isinstance(drawdown_state, (int, float)):
        # If float represents current drawdown percentage/ratio, e.g. 0.04 (4%)
        if float(drawdown_state) >= 0.04:  # Near breach threshold
            return False
    elif isinstance(drawdown_state, dict):
        if drawdown_state.get("near_breach", False) or drawdown_state.get("breached", False) or drawdown_state.get("locked_out", False):
            return False

    return True
