# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Unit tests for Layer 13 — Deterministic Stress‑Regime Override."""

import pytest
from core.finance_kernel.arbitration_layer import AssetId
from core.finance_kernel.portfolio_constraints import AssetAllocation
from core.finance_kernel.stress_override import (
    StressMode,
    StressOverrideRules,
    SafeBaseline,
    StressTraceStep,
    StressPortfolioState,
    SafeBaselineContainer,
    StressTraceSteps,
    apply_stress_regime_override
)

def test_stress_override_normal_flow():
    # 1. Setup baseline
    baselines = SafeBaselineContainer([
        SafeBaseline(AssetId.CASH, 0.70, flags=1),
        SafeBaseline(AssetId.BTC, 0.20),
        SafeBaseline(AssetId.ETH, 0.10)
    ])

    # 2. Setup previous allocations S_t
    prev_allocs = [
        AssetAllocation(AssetId.CASH, 0.55, risk_score=0.0),
        AssetAllocation(AssetId.BTC, 0.30, risk_score=60.0),
        AssetAllocation(AssetId.ETH, 0.15, risk_score=80.0)
    ]

    # 3. Setup rules
    rules = StressOverrideRules(
        volatility_threshold=0.50,
        drawdown_threshold=0.10,
        correlation_threshold=0.80,
        residual_threshold=0.20,
        crash_dampening_factor=0.50,
        fallback_lambda=0.70,
        mode=StressMode.NORMAL
    )

    # 4. NORMAL Flow (no override)
    curr_allocs = [
        AssetAllocation(AssetId.CASH, 0.43, risk_score=0.0),
        AssetAllocation(AssetId.BTC, 0.45, risk_score=60.0),
        AssetAllocation(AssetId.ETH, 0.12, risk_score=80.0)
    ]

    state = StressPortfolioState(
        portfolio_risk=0.0,
        stress_index=0.0,
        volatility_index=0.10,
        drawdown_index=0.02,
        correlation_index=0.30,
        temporal_residual_sum=0.05,
        stress_level=StressMode.NORMAL
    )

    trace = StressTraceSteps()

    apply_stress_regime_override(rules, state, prev_allocs, curr_allocs, baselines, trace, False)

    # Allocations remain unchanged
    assert curr_allocs[0].allocation == pytest.approx(0.43)
    assert curr_allocs[1].allocation == pytest.approx(0.45)
    assert curr_allocs[2].allocation == pytest.approx(0.12)

    # No flags in trace
    assert trace.count == 3
    assert trace.steps[0].flags == 0
    assert trace.steps[1].flags == 0
    assert trace.steps[2].flags == 0


def test_stress_override_exposure_freeze():
    baselines = SafeBaselineContainer([
        SafeBaseline(AssetId.CASH, 0.70, flags=1),
        SafeBaseline(AssetId.BTC, 0.20),
        SafeBaseline(AssetId.ETH, 0.10)
    ])

    prev_allocs = [
        AssetAllocation(AssetId.CASH, 0.55, risk_score=0.0),
        AssetAllocation(AssetId.BTC, 0.30, risk_score=60.0),
        AssetAllocation(AssetId.ETH, 0.15, risk_score=80.0)
    ]

    rules = StressOverrideRules(
        volatility_threshold=0.50,
        drawdown_threshold=0.10,
        correlation_threshold=0.80,
        residual_threshold=0.20,
        crash_dampening_factor=0.50,
        fallback_lambda=0.70,
        mode=StressMode.NORMAL
    )

    curr_allocs = [
        AssetAllocation(AssetId.CASH, 0.43, risk_score=0.0),
        AssetAllocation(AssetId.BTC, 0.45, risk_score=60.0), # Increase (prev 0.30) -> Frozen to 0.30, then dampened to 0.15
        AssetAllocation(AssetId.ETH, 0.12, risk_score=80.0)  # Decrease (prev 0.15) -> NOT frozen, dampened to 0.06
    ]

    state = StressPortfolioState(
        portfolio_risk=0.0,
        stress_index=0.0,
        volatility_index=0.40, # Volatility (0.40) > 0.5 * 0.50 -> triggers freeze & STRESS mode
        drawdown_index=0.02,
        correlation_index=0.30,
        temporal_residual_sum=0.05,
        stress_level=StressMode.NORMAL
    )

    trace = StressTraceSteps()

    apply_stress_regime_override(rules, state, prev_allocs, curr_allocs, baselines, trace, False)

    # CASH: unchanged
    assert curr_allocs[0].allocation == pytest.approx(0.43)
    # BTC: frozen (0.30) * dampened (0.50) = 0.15
    assert curr_allocs[1].allocation == pytest.approx(0.15)
    # ETH: dampened (0.12) * 0.50 = 0.06
    assert curr_allocs[2].allocation == pytest.approx(0.06)

    assert trace.count == 3
    # BTC flag has bit 1 (frozen) and bit 0 (dampened) set
    assert (trace.steps[1].flags & (1 << 1)) != 0
    assert (trace.steps[1].flags & (1 << 0)) != 0
    # ETH flag has bit 0 (dampened) but not bit 1 (frozen)
    assert (trace.steps[2].flags & (1 << 0)) != 0
    assert (trace.steps[2].flags & (1 << 1)) == 0


def test_stress_override_crash_dampening():
    baselines = SafeBaselineContainer([
        SafeBaseline(AssetId.CASH, 0.70, flags=1),
        SafeBaseline(AssetId.BTC, 0.20),
        SafeBaseline(AssetId.ETH, 0.10)
    ])

    prev_allocs = [
        AssetAllocation(AssetId.CASH, 0.55, risk_score=0.0),
        AssetAllocation(AssetId.BTC, 0.30, risk_score=60.0),
        AssetAllocation(AssetId.ETH, 0.15, risk_score=80.0)
    ]

    rules = StressOverrideRules(
        volatility_threshold=0.50,
        drawdown_threshold=0.10,
        correlation_threshold=0.80,
        residual_threshold=0.20,
        crash_dampening_factor=0.50,
        fallback_lambda=0.70,
        mode=StressMode.NORMAL
    )

    curr_allocs = [
        AssetAllocation(AssetId.CASH, 0.43, risk_score=0.0),
        AssetAllocation(AssetId.BTC, 0.40, risk_score=60.0),
        AssetAllocation(AssetId.ETH, 0.12, risk_score=80.0)
    ]

    state = StressPortfolioState(
        portfolio_risk=0.0,
        stress_index=0.0,
        volatility_index=0.10,
        drawdown_index=0.02,
        correlation_index=0.30,
        temporal_residual_sum=0.05,
        stress_level=StressMode.STRESS # STRESS mode -> Crash Dampening
    )

    trace = StressTraceSteps()

    apply_stress_regime_override(rules, state, prev_allocs, curr_allocs, baselines, trace, False)

    # CASH: unchanged
    assert curr_allocs[0].allocation == pytest.approx(0.43)
    # BTC: dampened -> 0.40 * 0.50 = 0.20
    assert curr_allocs[1].allocation == pytest.approx(0.20)
    # ETH: dampened -> 0.12 * 0.50 = 0.06
    assert curr_allocs[2].allocation == pytest.approx(0.06)

    assert trace.count == 3
    assert (trace.steps[1].flags & (1 << 0)) != 0
    assert (trace.steps[2].flags & (1 << 0)) != 0


def test_stress_override_crisis_fallback():
    baselines = SafeBaselineContainer([
        SafeBaseline(AssetId.CASH, 0.70, flags=1),
        SafeBaseline(AssetId.BTC, 0.20),
        SafeBaseline(AssetId.ETH, 0.10)
    ])

    prev_allocs = [
        AssetAllocation(AssetId.CASH, 0.55, risk_score=0.0),
        AssetAllocation(AssetId.BTC, 0.30, risk_score=60.0),
        AssetAllocation(AssetId.ETH, 0.15, risk_score=80.0)
    ]

    rules = StressOverrideRules(
        volatility_threshold=0.50,
        drawdown_threshold=0.10,
        correlation_threshold=0.80,
        residual_threshold=0.20,
        crash_dampening_factor=0.50,
        fallback_lambda=0.70,
        mode=StressMode.NORMAL
    )

    curr_allocs = [
        AssetAllocation(AssetId.CASH, 0.43, risk_score=0.0),
        AssetAllocation(AssetId.BTC, 0.40, risk_score=60.0),
        AssetAllocation(AssetId.ETH, 0.12, risk_score=80.0)
    ]

    state = StressPortfolioState(
        portfolio_risk=0.0,
        stress_index=0.0,
        volatility_index=0.10,
        drawdown_index=0.02,
        correlation_index=0.30,
        temporal_residual_sum=0.05,
        stress_level=StressMode.CRISIS # triggers CRISIS & fallback compression
    )

    trace = StressTraceSteps()

    apply_stress_regime_override(rules, state, prev_allocs, curr_allocs, baselines, trace, False)

    # CRISIS:
    # CASH: (0.30 * 0.43) + (0.70 * 0.70) = 0.129 + 0.490 = 0.619
    # BTC: dampened -> 0.40 * 0.5 = 0.20, then fallback -> (0.30 * 0.20) + (0.70 * 0.20) = 0.06 + 0.14 = 0.20
    # ETH: dampened -> 0.12 * 0.5 = 0.06, then fallback -> (0.30 * 0.06) + (0.70 * 0.10) = 0.018 + 0.07 = 0.088

    assert curr_allocs[0].allocation == pytest.approx(0.619)
    assert curr_allocs[1].allocation == pytest.approx(0.20)
    assert curr_allocs[2].allocation == pytest.approx(0.088)

    assert trace.count == 3
    # CASH has fallback applied (bit 2) but not dampened
    assert (trace.steps[0].flags & (1 << 2)) != 0
    assert (trace.steps[0].flags & (1 << 0)) == 0

    # BTC has dampened and fallback
    assert (trace.steps[1].flags & (1 << 0)) != 0
    assert (trace.steps[1].flags & (1 << 2)) != 0
