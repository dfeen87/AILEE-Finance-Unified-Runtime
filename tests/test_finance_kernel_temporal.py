# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Unit tests for Layer 12 — Deterministic Temporal Consistency Guard."""

import pytest
from core.finance_kernel.arbitration_layer import AssetId
from core.finance_kernel.temporal_consistency import (
    TemporalState,
    TemporalResidual,
    TemporalTraceStep,
    TemporalPortfolioState,
    TemporalStates,
    TemporalResiduals,
    TemporalTraceSteps,
    TemporalAction,
    enforce_temporal_consistency
)

def test_temporal_consistency_normal_flow():
    # AssetId 1 starts with allocation 0.10. Moves smoothly to 0.12. (Drift 0.02 <= 0.05)
    prev_states = TemporalStates([
        TemporalState(AssetId.BTC, prev_allocation=0.10, prev_risk_score=60.0, prev_prev_allocation=0.08)
    ])
    curr_states = TemporalStates([
        TemporalState(AssetId.BTC, prev_allocation=0.12, prev_risk_score=60.0)
    ])

    prev_portfolio = TemporalPortfolioState()
    curr_portfolio = TemporalPortfolioState()
    residuals = TemporalResiduals()
    trace = TemporalTraceSteps()

    enforce_temporal_consistency(
        prev_states, curr_states, prev_portfolio, curr_portfolio, residuals, trace, max_drift_threshold=0.05
    )

    # 0.12 is preserved because delta is smooth
    assert curr_states.states[0].prev_allocation == pytest.approx(0.12)
    assert curr_states.states[0].prev_prev_allocation == pytest.approx(0.10)
    assert curr_states.states[0].flags == 0
    assert residuals.residuals[0].residual == pytest.approx(0.02)
    assert curr_portfolio.residual_sum == pytest.approx(0.02)
    assert curr_portfolio.portfolio_risk == pytest.approx(0.12 * 60.0)
    assert trace.steps[0].action_taken == TemporalAction.NO_CHANGE

def test_temporal_consistency_drift_clamp():
    # AssetId 1 moves from 0.10 to 0.20. (Drift 0.10 > max_drift 0.05)
    # Expected to clamp to 0.10 + 0.05 = 0.15
    prev_states = TemporalStates([
        TemporalState(AssetId.BTC, prev_allocation=0.10, prev_risk_score=60.0, prev_prev_allocation=0.08)
    ])
    curr_states = TemporalStates([
        TemporalState(AssetId.BTC, prev_allocation=0.20, prev_risk_score=60.0)
    ])

    prev_portfolio = TemporalPortfolioState()
    curr_portfolio = TemporalPortfolioState()
    residuals = TemporalResiduals()
    trace = TemporalTraceSteps()

    enforce_temporal_consistency(
        prev_states, curr_states, prev_portfolio, curr_portfolio, residuals, trace, max_drift_threshold=0.05
    )

    assert curr_states.states[0].prev_allocation == pytest.approx(0.15)
    assert curr_states.states[0].prev_prev_allocation == pytest.approx(0.10)
    assert curr_states.states[0].flags == 0
    assert residuals.residuals[0].residual == pytest.approx(0.05)
    assert curr_portfolio.residual_sum == pytest.approx(0.05)
    assert trace.steps[0].action_taken == TemporalAction.CLAMPED
    assert trace.steps[0].log == "Drift clamped"

def test_temporal_consistency_walkthrough():
    """
    Example walkthrough where the portfolio tries to oscillate between two states and Layer 12 stabilizes it.
    AssetId 1:
    - w_{t-1} = 0.20
    - w_{t} = 0.35
    - Proposed w_{t+1} = 0.15
    - Delta_t = +0.15
    - Delta_{t+1} = -0.20
    - Sign flips -> Oscillation detected!
    - Dampened target = 0.35 + 0.5 * (-0.20) = 0.25
    - Drift from w_{t} (0.35) is 0.10 > max_drift_threshold (0.05)
    - Clamped target = 0.35 - 0.05 = 0.30
    """
    prev_states = TemporalStates([
        TemporalState(AssetId.BTC, prev_allocation=0.35, prev_risk_score=60.0, prev_prev_allocation=0.20)
    ])
    curr_states = TemporalStates([
        TemporalState(AssetId.BTC, prev_allocation=0.15, prev_risk_score=60.0)
    ])

    prev_portfolio = TemporalPortfolioState()
    curr_portfolio = TemporalPortfolioState()
    residuals = TemporalResiduals()
    trace = TemporalTraceSteps()

    enforce_temporal_consistency(
        prev_states, curr_states, prev_portfolio, curr_portfolio, residuals, trace, max_drift_threshold=0.05
    )

    # Verify resolved allocation is 0.30
    assert curr_states.states[0].prev_allocation == pytest.approx(0.30)
    assert curr_states.states[0].prev_prev_allocation == pytest.approx(0.35)
    assert (curr_states.states[0].flags & 1) != 0  # oscillation_detected bit set

    # Residual = |0.30 - 0.35| = 0.05
    assert residuals.residuals[0].residual == pytest.approx(0.05)
    assert curr_portfolio.residual_sum == pytest.approx(0.05)
    assert curr_portfolio.portfolio_risk == pytest.approx(0.30 * 60.0)

    assert trace.steps[0].action_taken == TemporalAction.OSCILLATED_AND_CLAMPED
    assert trace.steps[0].log == "Oscillated & clamped"
    assert trace.steps[0].before_value == pytest.approx(0.15)
    assert trace.steps[0].after_value == pytest.approx(0.30)
