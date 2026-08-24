# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Unit tests for Python Finance Kernel Unified Runtime Operator (Layer 19)."""

import pytest
from core.finance_kernel.unified_runtime import (
    UnifiedRuntimeOperator,
    UNIFIED_STATUS_NOMINAL,
    UNIFIED_STATUS_DEGRADED,
    UNIFIED_STATUS_STRESS_OVERRIDE,
    UNIFIED_STATUS_META_LOCKED
)


def test_unified_runtime_nominal_cycle():
    op = UnifiedRuntimeOperator()
    input_data = {
        "cycle_sequence_id": 0,
        "stream_degraded": False,
        "trigger_stress_escalation": False,
        "anomaly_active": False,
        "msgam_risk_elevated": False,
        "stress_level": 0,
        "meta_execution_ready": True
    }
    pre = op.preprocess(input_data)
    res = op.execute(pre)

    assert res["cycle_sequence_id"] == 1
    assert res["system_status"] == UNIFIED_STATUS_NOMINAL
    assert res["execution_permitted"] is True
    assert not res["hft_freeze_active"]
    assert res["system_confidence"] == 1.0
    assert res["recommended_execution_scale"] == 1.0


def test_unified_runtime_stream_and_anomaly_degradation():
    op = UnifiedRuntimeOperator()
    input_data = {
        "cycle_sequence_id": 1,
        "stream_degraded": True,
        "anomaly_active": True,
        "anomaly_severity": 0.5,
        "stress_level": 0,
        "meta_execution_ready": True
    }
    pre = op.preprocess(input_data)
    res = op.execute(pre)

    assert res["system_status"] == UNIFIED_STATUS_DEGRADED
    assert res["hft_freeze_active"] is True
    assert res["system_confidence"] < 1.0
    assert res["recommended_execution_scale"] < 1.0


def test_unified_runtime_stress_override_escalation():
    op = UnifiedRuntimeOperator()
    input_data = {
        "cycle_sequence_id": 2,
        "trigger_stress_escalation": True,
        "auto_escalate_faults": True,
        "stress_level": 0,
        "meta_execution_ready": True
    }
    pre = op.preprocess(input_data)
    res = op.execute(pre)

    assert res["system_status"] == UNIFIED_STATUS_STRESS_OVERRIDE
    assert res["fault_escalated"] == 1
    assert res["recommended_execution_scale"] == 0.0
    assert res["hft_freeze_active"] is True


def test_unified_runtime_meta_governance_lock():
    op = UnifiedRuntimeOperator()
    input_data = {
        "cycle_sequence_id": 3,
        "meta_execution_ready": False,
        "enforce_strict_lock": True
    }
    pre = op.preprocess(input_data)
    res = op.execute(pre)

    assert res["system_status"] == UNIFIED_STATUS_META_LOCKED
    assert res["execution_permitted"] is False
    assert res["recommended_execution_scale"] == 0.0
