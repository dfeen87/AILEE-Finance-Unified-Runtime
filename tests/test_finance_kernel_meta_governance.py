# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Unit tests for Layer 14 — Deterministic Meta‑Governance Lock."""

import pytest
from core.finance_kernel.meta_governance import (
    apply_meta_governance_lock,
    MetaGovernanceReason,
    MetaGovernanceState,
    MetaGovernanceTraceStep
)

class MockSummary:
    def __init__(self, **kwargs):
        for k, v in kwargs.items():
            setattr(self, k, v)

class MockObject:
    def __init__(self, **kwargs):
        for k, v in kwargs.items():
            setattr(self, k, v)


def test_meta_governance_lock_clean():
    decision = MockObject(summary=MockSummary(total_residual=0.02))
    constraints = MockObject(summary=MockSummary(remaining_violations=0, final_portfolio_risk=20.0, max_risk_budget=25.0))
    stress_state = MockObject(stress_level=0)
    stress_trace = MockObject(count=0)
    temporal_state = MockObject(residual_sum=0.04)

    state, trace = apply_meta_governance_lock(decision, constraints, stress_state, stress_trace, temporal_state)

    assert state.execution_ready == 1
    assert state.final_portfolio_risk == pytest.approx(20.0)
    assert state.final_residual_sum == pytest.approx(0.06)
    assert state.final_stress_level == 0

    assert len(trace) == 1
    assert trace[0].reason_code == 0
    assert trace[0].log == "Meta-governance lock verified & ready"


def test_meta_governance_lock_governor_conflict():
    decision = MockObject(summary=MockSummary(total_residual=0.08)) # > 0.05
    constraints = MockObject(summary=MockSummary(remaining_violations=0, final_portfolio_risk=20.0, max_risk_budget=25.0))
    stress_state = MockObject(stress_level=0)
    stress_trace = MockObject(count=0)
    temporal_state = MockObject(residual_sum=0.04)

    state, trace = apply_meta_governance_lock(decision, constraints, stress_state, stress_trace, temporal_state)

    assert state.execution_ready == 0
    assert len(trace) == 1
    assert trace[0].reason_code == MetaGovernanceReason.GOVERNOR_CONFLICT
    assert trace[0].log == "Governor conflict detected"


def test_meta_governance_lock_constraint_violation():
    decision = MockObject(summary=MockSummary(total_residual=0.02))
    constraints = MockObject(summary=MockSummary(remaining_violations=0, final_portfolio_risk=28.0, max_risk_budget=25.0)) # Risk exceeds budget!
    stress_state = MockObject(stress_level=0)
    stress_trace = MockObject(count=0)
    temporal_state = MockObject(residual_sum=0.04)

    state, trace = apply_meta_governance_lock(decision, constraints, stress_state, stress_trace, temporal_state)

    assert state.execution_ready == 0
    assert len(trace) == 1
    assert trace[0].reason_code == MetaGovernanceReason.CONSTRAINT_VIOLATION
    assert trace[0].log == "Constraint violation detected"


def test_meta_governance_lock_stress_override_missing():
    decision = MockObject(summary=MockSummary(total_residual=0.02))
    constraints = MockObject(summary=MockSummary(remaining_violations=0, final_portfolio_risk=20.0, max_risk_budget=25.0))
    stress_state = MockObject(stress_level=1) # STRESS
    stress_trace = MockObject(count=0) # count=0, so missing!
    temporal_state = MockObject(residual_sum=0.04)

    state, trace = apply_meta_governance_lock(decision, constraints, stress_state, stress_trace, temporal_state)

    assert state.execution_ready == 0
    assert len(trace) == 1
    assert trace[0].reason_code == MetaGovernanceReason.STRESS_OVERRIDE_MISSING
    assert trace[0].log == "Stress override missing"


def test_meta_governance_lock_temporal_inconsistent():
    decision = MockObject(summary=MockSummary(total_residual=0.02))
    constraints = MockObject(summary=MockSummary(remaining_violations=0, final_portfolio_risk=20.0, max_risk_budget=25.0))
    stress_state = MockObject(stress_level=0)
    stress_trace = MockObject(count=0)
    temporal_state = MockObject(residual_sum=0.15) # > 0.10!

    state, trace = apply_meta_governance_lock(decision, constraints, stress_state, stress_trace, temporal_state)

    assert state.execution_ready == 0
    assert len(trace) == 1
    assert trace[0].reason_code == MetaGovernanceReason.TEMPORAL_INCONSISTENT
    assert trace[0].log == "Temporal residual too high"
