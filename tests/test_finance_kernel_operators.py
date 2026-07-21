# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Unit tests for the Finance Runtime Kernel Operators."""

import pytest
from core.finance_kernel.kernel_context import FinanceKernelContext
from core.finance_kernel.kernel_config import FinanceKernelConfig
from core.finance_kernel.kernel_registry import (
    RiskOperator,
    GovernorOperator,
    TransactionOperator,
    LedgerOperator,
    ModelSignal,
    AILLEEngine
)

def test_risk_operator():
    ctx = FinanceKernelContext("ledger-1", "session-1")
    cfg = FinanceKernelConfig()
    op = RiskOperator(ctx, cfg)

    # Valid and low-confidence signals
    input_data = {
        "signals": [
            {"value": 0.05, "confidence": 0.90, "model_id": 1},
            {"value": -0.02, "confidence": 0.30, "model_id": 2}, # falls within grace threshold (0.25 - 0.35)
            {"value": 0.01, "confidence": 0.10, "model_id": 3}  # rejected
        ]
    }

    res = op.execute(input_data)
    signals = res["signals"]

    # We should have exactly 2 valid/grace signals
    assert len(signals) == 2
    assert signals[0].model_id == 1
    assert signals[0].confidence == 0.90
    assert signals[1].model_id == 2
    assert signals[1].confidence == 0.24  # 0.30 * 0.8


def test_governor_operator():
    ctx = FinanceKernelContext("ledger-1", "session-1")
    cfg = FinanceKernelConfig()
    op = GovernorOperator(ctx, cfg)

    # Signals reaching consensus
    input_data = {
        "signals": [
            {"value": 0.05, "confidence": 0.90, "model_id": 1},
            {"value": 0.04, "confidence": 0.85, "model_id": 2}
        ]
    }

    res = op.execute(input_data)
    assert res["consensus"] is not None
    assert res["consensus"]["consensus_value"] == 0.045
    assert res["consensus"]["models_agreed"] == 2


def test_transaction_operator():
    ctx = FinanceKernelContext("ledger-1", "session-1")
    cfg = FinanceKernelConfig()
    op = TransactionOperator(ctx, cfg)

    input_data = {
        "signals": [
            {"value": 0.05, "confidence": 0.90, "model_id": 1},
            {"value": 0.04, "confidence": 0.85, "model_id": 2}
        ]
    }

    res = op.execute(input_data)
    assert "decision" in res
    assert "engine" in res

    decision = res["decision"]
    assert decision.status == "DECISION_VALID"
    assert decision.models_agreed == 2
    assert decision.fallback_used is False


def test_ledger_operator():
    ctx = FinanceKernelContext("ledger-1", "session-1")
    cfg = FinanceKernelConfig()
    op = LedgerOperator(ctx, cfg)

    decision_data = {
        "final_value": 0.76159,
        "status": "DECISION_VALID",
        "confidence": 0.875
    }

    input_data = {
        "decision": decision_data,
        "context": ctx.to_dict()
    }

    res = op.execute(input_data)
    assert res["status"] == "recorded"
    assert "hash" in res
    assert len(res["hash"]) == 16  # hex representation of 64-bit int

    record = res["record"]
    assert record["ledger_id"] == "ledger-1"
    assert record["session_id"] == "session-1"
    assert record["final_value"] == 0.76159
    assert record["status"] == "DECISION_VALID"
    assert record["hash"] == res["hash"]
