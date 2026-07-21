# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Integration and Orchestration tests for the Finance Runtime Kernel."""

import pytest
import math
from core.finance_kernel import (
    create_finance_kernel,
    run_finance_operator,
    run_transaction_operator,
    run_risk_operator,
    run_governor_operator,
    run_ledger_operator,
    FinanceRuntimeKernel,
    OperatorResult,
    PipelineResult,
    KernelContextError,
    KernelConfigurationError
)
from core.finance_kernel.finance_kernel import make_json_compatible

def test_kernel_initialization():
    kernel = create_finance_kernel()
    assert isinstance(kernel, FinanceRuntimeKernel)
    assert kernel.context.ledger_id == "default-ledger"
    assert kernel.config.operator_timeout == 30.0


def test_kernel_execute_single_operator():
    signals = [
        {"value": 0.05, "confidence": 0.90, "model_id": 1},
        {"value": -0.01, "confidence": 0.05, "model_id": 2}
    ]

    # Test single run via factory API
    res = run_finance_operator(
        name="risk_operator",
        input_data={"signals": signals},
        context_overrides={"ledger_id": "kernel-test-ledger"},
        config_overrides={"json_compat_mode": True}
    )

    # Output is a dict since json_compat_mode is True
    assert isinstance(res, dict)
    assert res["status"] == "SUCCESS"
    assert res["operator_name"] == "risk_operator"
    assert res["duration"] > 0.0

    data = res["data"]
    # only signal with id=1 remains since id=2 fails confidence
    assert len(data["signals"]) == 1
    assert data["signals"][0]["model_id"] == 1


def test_kernel_pipeline_sequencing():
    kernel = create_finance_kernel(
        context_overrides={"ledger_id": "pipeline-ledger"},
        config_overrides={"json_compat_mode": False}
    )

    # Let's run a pipeline: risk_operator -> transaction_operator
    signals = [
        {"value": 0.05, "confidence": 0.90, "model_id": 1},
        {"value": 0.04, "confidence": 0.85, "model_id": 2}
    ]

    input_data = {"signals": signals}
    pipeline_res = kernel.execute_pipeline(["risk_operator", "transaction_operator"], input_data)

    assert isinstance(pipeline_res, PipelineResult)
    assert len(pipeline_res.results) == 2

    # First operator output
    op1 = pipeline_res.results[0]
    assert isinstance(op1, OperatorResult)
    assert op1.operator_name == "risk_operator"
    assert len(op1.data["signals"]) == 2

    # Second operator output
    op2 = pipeline_res.results[1]
    assert op2.operator_name == "transaction_operator"

    decision = op2.data["decision"]
    assert decision.status == "DECISION_VALID"
    assert decision.models_agreed == 2


def test_strict_determinism_replay_consistency():
    # Identical runs with identical inputs must produce exact matching outputs.
    signals = [
        {"value": 0.03, "confidence": 0.80, "model_id": 1},
        {"value": 0.04, "confidence": 0.82, "model_id": 2},
        {"value": -0.05, "confidence": 0.91, "model_id": 3}
    ]

    input_data = {"signals": signals}

    kernel = create_finance_kernel(config_overrides={"json_compat_mode": True})

    res1 = kernel.execute_operator("transaction_operator", input_data)
    res2 = kernel.execute_operator("transaction_operator", input_data)

    assert res1["status"] == "SUCCESS"
    assert res2["status"] == "SUCCESS"

    d1 = res1["data"]["decision"]
    d2 = res2["data"]["decision"]

    assert d1["final_value"] == d2["final_value"]
    assert d1["status"] == d2["status"]
    assert d1["confidence"] == d2["confidence"]
    assert d1["models_agreed"] == d2["models_agreed"]


def test_compatibility_layer_functions():
    signals = [
        {"value": 0.05, "confidence": 0.90, "model_id": 1},
        {"value": 0.04, "confidence": 0.85, "model_id": 2}
    ]

    # 1. run_risk_operator
    res_risk = run_risk_operator(signals)
    assert res_risk.status == "SUCCESS"
    assert len(res_risk.data["signals"]) == 2

    # 2. run_governor_operator
    res_gov = run_governor_operator(signals)
    assert res_gov.status == "SUCCESS"
    assert res_gov.data["consensus"]["models_agreed"] == 2

    # 3. run_transaction_operator
    res_trans = run_transaction_operator(signals)
    assert res_trans.status == "SUCCESS"
    assert res_trans.data["decision"].status == "DECISION_VALID"

    # 4. run_ledger_operator
    res_ledger = run_ledger_operator(res_trans.data["decision"])
    assert res_ledger.status == "SUCCESS"
    assert res_ledger.data["status"] == "recorded"
