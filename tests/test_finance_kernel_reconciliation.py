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
