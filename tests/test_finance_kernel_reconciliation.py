# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Unit tests for Layer 10 — Multi‑Governor Reconciliation Engine (Finance Runtime Kernel)."""

import pytest
from core.finance_kernel.arbitration_layer import AssetId
from core.finance_kernel.governor_reconciliation import (
    GovernorType,
    ReconciliationFlags,
    GovernorProposal,
    reconcile_governors
)

def test_layer10_reconciliation_walkthrough():
    proposals = [
        # STRATEGY: Proposed Value = 100.0
        GovernorProposal(asset_id=AssetId.BTC, governor_type=GovernorType.STRATEGY, proposed_value=100.0),
        # RETURN: Proposed Value = 120.0
        GovernorProposal(asset_id=AssetId.BTC, governor_type=GovernorType.RETURN, proposed_value=120.0),
        # LIQUIDITY: Limit Proposed Value = 80.0
        GovernorProposal(asset_id=AssetId.BTC, governor_type=GovernorType.LIQUIDITY, proposed_value=80.0),
        # RISK: Proposed Value = 50.0, risk_score = 80.0
        GovernorProposal(asset_id=AssetId.BTC, governor_type=GovernorType.RISK, proposed_value=50.0, risk_score=80.0),
        # COMPLIANCE: Proposed Value = 0.0
        GovernorProposal(asset_id=AssetId.BTC, governor_type=GovernorType.COMPLIANCE, proposed_value=0.0)
    ]

    res = reconcile_governors(proposals, AssetId.BTC)

    assert res.decision.final_value == 50.0
    assert res.decision.resolved_type == GovernorType.RISK
    assert res.decision.flags_applied == (ReconciliationFlags.VOL_CLAMP | ReconciliationFlags.RISK_LIMIT)

    # Residual Calculation:
    # COMP: 1.0 * |0.0 - 50.0| = 50.0
    # RISK: 0.8 * |50.0 - 50.0| = 0.0
    # LIQ: 0.6 * |80.0 - 50.0| = 18.0
    # RET: 0.5 * |120.0 - 50.0| = 35.0
    # STRAT: 0.5 * |100.0 - 50.0| = 25.0
    # Sum = 128.0
    assert res.residual.residual_value == 128.0
    assert res.summary.total_residual == 128.0
    assert res.summary.trace_count > 0

def test_layer10_compliance_hard_block():
    proposals = [
        GovernorProposal(asset_id=AssetId.BTC, governor_type=GovernorType.STRATEGY, proposed_value=100.0),
        GovernorProposal(asset_id=AssetId.BTC, governor_type=GovernorType.COMPLIANCE, proposed_value=0.0, flags=ReconciliationFlags.HARD_BLOCK)
    ]

    res = reconcile_governors(proposals, AssetId.BTC)

    assert res.decision.final_value == 0.0
    assert res.decision.resolved_type == GovernorType.COMPLIANCE
    assert (res.decision.flags_applied & ReconciliationFlags.HARD_BLOCK) != 0


def test_layer10_reconciliation_hardening():
    # 1. Determinism
    proposals = [
        GovernorProposal(asset_id=AssetId.BTC, governor_type=GovernorType.STRATEGY, proposed_value=100.0)
    ]

    res1 = reconcile_governors(proposals, AssetId.BTC)
    res2 = reconcile_governors(proposals, AssetId.BTC)

    assert res1.decision.final_value == res2.decision.final_value
    assert res1.residual.residual_value == res2.residual.residual_value
    assert len(res1.trace_steps) == len(res2.trace_steps)

    # 2. Failure Mode (Empty proposal input list)
    res_empty = reconcile_governors([], AssetId.BTC)
    assert res_empty.decision.final_value == 0.0
    assert res_empty.decision.resolved_type == GovernorType.STRATEGY

    # 3. Risk Boundary Condition (Exactly 75.0 vs 75.1)
    boundary_proposals1 = [
        GovernorProposal(asset_id=AssetId.BTC, governor_type=GovernorType.STRATEGY, proposed_value=100.0),
        GovernorProposal(asset_id=AssetId.BTC, governor_type=GovernorType.RISK, proposed_value=50.0, risk_score=75.0) # not severe (>75.0 is severe)
    ]
    res_boundary1 = reconcile_governors(boundary_proposals1, AssetId.BTC)
    assert res_boundary1.decision.final_value == 100.0 # no severe clamp

    boundary_proposals2 = [
        GovernorProposal(asset_id=AssetId.BTC, governor_type=GovernorType.STRATEGY, proposed_value=100.0),
        GovernorProposal(asset_id=AssetId.BTC, governor_type=GovernorType.RISK, proposed_value=50.0, risk_score=75.1) # severe (>75.0 is severe)
    ]
    res_boundary2 = reconcile_governors(boundary_proposals2, AssetId.BTC)
    assert res_boundary2.decision.final_value == 50.0 # severe clamp applied!
