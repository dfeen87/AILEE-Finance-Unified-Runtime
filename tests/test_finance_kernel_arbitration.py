# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Unit and integration tests for Layer 8 — Cross-Asset Deterministic Arbitration."""

import pytest
import math
from core.finance_kernel.arbitration_layer import (
    AssetId,
    AdvisoryFlags,
    Advisory,
    Ladder,
    ScalingRules,
    arbitrate
)

def test_layer8_arbitration_walkthrough_consistency():
    ladder = Ladder()
    rules = ScalingRules()

    advisories = [
        # 1. BTC (BRGAM)
        Advisory(
            asset_id=AssetId.BTC,
            risk_score=45.0,
            safety_level=0.85,
            liquidity_level=0.90,
            regulatory_level=0.50,
            return_score=0.70,
            confidence=0.80,
            raw_flags=AdvisoryFlags.NONE
        ),
        # 2. ETH (ERGAM)
        Advisory(
            asset_id=AssetId.ETH,
            risk_score=55.0,
            safety_level=0.50,
            liquidity_level=0.80,
            regulatory_level=0.25,
            return_score=0.60,
            confidence=0.75,
            raw_flags=AdvisoryFlags.NONE
        ),
        # 3. Gold (CRGAM-X)
        Advisory(
            asset_id=AssetId.GOLD,
            risk_score=15.0,
            safety_level=0.95,
            liquidity_level=0.60,
            regulatory_level=0.90,
            return_score=0.30,
            confidence=0.85,
            raw_flags=AdvisoryFlags.PREFERRED
        ),
        # 4. Equity High-Risk
        Advisory(
            asset_id=AssetId.EQUITY_HIGH_RISK,
            risk_score=90.0,
            safety_level=0.30,
            liquidity_level=0.10,
            regulatory_level=0.40,
            return_score=0.95,
            confidence=0.90,
            raw_flags=AdvisoryFlags.HARD_BLOCK
        )
    ]

    result = arbitrate(advisories, ladder, rules)

    assert result.decision_count == 4

    # Verification (matching C++ walkthrough values exactly):
    # Expected Weights at end of walkthrough:
    # BTC: 0.42075
    # ETH: 0.0288
    # GOLD: 0.3978
    # EQUITY: 0.0
    # Total Weight: 0.84735
    # Expected Allocations:
    # BTC: 0.42075 / 0.84735 = 0.496548
    # ETH: 0.02880 / 0.84735 = 0.033990
    # GOLD: 0.39780 / 0.84735 = 0.469463
    # EQUITY: 0.0

    assert math.isclose(result.decisions[3].recommended_allocation, 0.0, abs_tol=1e-6)
    assert math.isclose(result.decisions[0].recommended_allocation, 0.496548, rel_tol=1e-4)
    assert math.isclose(result.decisions[1].recommended_allocation, 0.033990, rel_tol=1e-4)
    assert math.isclose(result.decisions[2].recommended_allocation, 0.469463, rel_tol=1e-4)

    # Verify that total allocation sum is exactly 1.0
    total_alloc = sum(d.recommended_allocation for d in result.decisions)
    assert math.isclose(total_alloc, 1.0, abs_tol=1e-6)

    # Verify that we have trace steps
    assert len(result.trace.steps) > 0
    assert result.trace.step_count > 0


def test_layer8_empty_advisories_returns_empty_result():
    ladder = Ladder()
    rules = ScalingRules()
    result = arbitrate([], ladder, rules)
    assert result.decision_count == 0
    assert len(result.decisions) == 0
    assert result.trace.step_count == 0


def test_layer8_safety_failures_exclude_completely():
    ladder = Ladder()
    rules = ScalingRules()

    # Asset safety falls under safety hurdle (0.3)
    advisories = [
        Advisory(AssetId.BTC, 10.0, 0.25, 0.9, 0.9, 0.5, 0.8)
    ]
    result = arbitrate(advisories, ladder, rules)
    assert math.isclose(result.decisions[0].recommended_allocation, 0.0, abs_tol=1e-6)


def test_layer8_arbitration_hardening():
    # 1. Determinism
    ladder = Ladder()
    rules = ScalingRules()
    advisories = [
        Advisory(AssetId.BTC, 45.0, 0.85, 0.90, 0.50, 0.70, 0.80)
    ]

    result1 = arbitrate(advisories, ladder, rules)
    result2 = arbitrate(advisories, ladder, rules)

    assert result1.decision_count == result2.decision_count
    assert math.isclose(result1.decisions[0].recommended_allocation, result2.decisions[0].recommended_allocation, abs_tol=1e-9)
    assert len(result1.trace.steps) == len(result2.trace.steps)

    # 2. Boundary Condition & Failure Mode (All below safety hurdle)
    low_safety_advisories = [
        Advisory(AssetId.BTC, 45.0, 0.34, 0.90, 0.50, 0.70, 0.80) # 0.34 < 0.35 safety hurdle
    ]
    result_fail = arbitrate(low_safety_advisories, ladder, rules)
    assert math.isclose(result_fail.decisions[0].recommended_allocation, 0.0, abs_tol=1e-9)
    assert len(result_fail.trace.steps) > 0
    assert "Safety failure" in result_fail.trace.steps[0].log

    # Exactly 0.35 boundary (marginal cap)
    marginal_advisories = [
        Advisory(AssetId.BTC, 45.0, 0.35, 0.90, 0.50, 0.70, 0.80) # 0.35 boundary
    ]
    result_marginal = arbitrate(marginal_advisories, ladder, rules)
    assert any("Marginal safety cap" in step.log for step in result_marginal.trace.steps)
