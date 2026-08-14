# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Unit tests for the Intraday Volume Advisory operator."""

import pytest
from core.finance_kernel.kernel_context import FinanceKernelContext
from core.finance_kernel.kernel_config import FinanceKernelConfig
from core.finance_kernel.kernel_registry import create_default_registry
from core.finance_kernel.finance_kernel import FinanceRuntimeKernel

def test_volume_operator_registration():
    registry = create_default_registry()
    assert "volume_operator" in registry.list_operators()
    assert registry.get_operator_role("volume_operator") == "governor"

def test_volume_operator_normal_flow():
    context = FinanceKernelContext(ledger_id="test-ledger", session_id="test-session")
    config = FinanceKernelConfig()
    registry = create_default_registry()
    kernel = FinanceRuntimeKernel(context, config, registry)

    input_data = {
        "current_volume": 1500000.0,
        "avg_volume": 1000000.0,
        "price_change": 0.005,
        "vwap_deviation": 0.002
    }

    result = kernel.execute_operator("volume_operator", input_data)
    assert result.status == "SUCCESS"
    data = result.data
    assert "recommended_weight" in data
    assert "risk_score" in data
    assert "risk_elevated" in data
    assert "growth_favorable" in data

    # Growth should be favorable under normal strong volume/price trend
    assert data["growth_favorable"] is True
    assert data["risk_elevated"] is False
    assert data["risk_score"] < 50.0

def test_volume_operator_risk_flow():
    context = FinanceKernelContext(ledger_id="test-ledger", session_id="test-session")
    config = FinanceKernelConfig()
    registry = create_default_registry()
    kernel = FinanceRuntimeKernel(context, config, registry)

    input_data = {
        "current_volume": 5000000.0,
        "avg_volume": 1000000.0,
        "price_change": -0.015,
        "vwap_deviation": -0.012
    }

    result = kernel.execute_operator("volume_operator", input_data)
    assert result.status == "SUCCESS"
    data = result.data
    assert data["risk_elevated"] is True
    assert data["growth_favorable"] is False
    assert data["recommended_weight"] < 0.5

def test_volume_operator_kill_switch():
    context = FinanceKernelContext(
        ledger_id="test-ledger",
        session_id="test-session",
        metadata={"kill_switch": True}
    )
    config = FinanceKernelConfig()
    registry = create_default_registry()
    kernel = FinanceRuntimeKernel(context, config, registry)

    input_data = {
        "current_volume": 1500000.0,
        "avg_volume": 1000000.0,
        "price_change": 0.005,
        "vwap_deviation": 0.002
    }

    result = kernel.execute_operator("volume_operator", input_data)
    assert result.status == "SUCCESS"
    data = result.data
    assert data["recommended_weight"] == 0.0
    assert data["risk_score"] == 100.0
    assert data["risk_elevated"] is True
    assert data["growth_favorable"] is False

def test_volume_operator_smoothing():
    context = FinanceKernelContext(ledger_id="test-ledger", session_id="test-session")
    config = FinanceKernelConfig()
    registry = create_default_registry()
    kernel = FinanceRuntimeKernel(context, config, registry)

    input_data = {
        "current_volume": 1000000.0,
        "avg_volume": 1000000.0,
        "price_change": 0.0,
        "vwap_deviation": 0.0,
        "prev_volume_anomaly_ratio": 5.0
    }

    result = kernel.execute_operator("volume_operator", input_data)
    assert result.status == "SUCCESS"
    data = result.data
    # Smoothed ratio = 0.2 * 1.0 + 0.8 * 5.0 = 4.2
    # vol_risk = 4.2 * 15 = 63.0
    assert data["risk_score"] >= 60.0

def test_volume_operator_market_stabilizer_coupling():
    context = FinanceKernelContext(ledger_id="test-ledger", session_id="test-session")
    config = FinanceKernelConfig()
    registry = create_default_registry()
    kernel = FinanceRuntimeKernel(context, config, registry)

    input_data = {
        "current_volume": 1500000.0,
        "avg_volume": 1000000.0,
        "price_change": 0.005,
        "vwap_deviation": 0.002,
        "stabilizer_factor": 0.5,
        "stabilizer_risk_elevated": True,
        "stabilizer_risk_score": 80.0
    }

    result = kernel.execute_operator("volume_operator", input_data)
    assert result.status == "SUCCESS"
    data = result.data
    assert data["recommended_weight"] <= 0.5
    assert data["risk_elevated"] is True

def test_volume_operator_temporal_step_clamping():
    context = FinanceKernelContext(ledger_id="test-ledger", session_id="test-session")
    config = FinanceKernelConfig()
    registry = create_default_registry()
    kernel = FinanceRuntimeKernel(context, config, registry)

    input_data = {
        "current_volume": 5000000.0,
        "avg_volume": 1000000.0,
        "price_change": -0.02,
        "vwap_deviation": -0.01,
        "prev_recommended_weight": 1.0
    }

    result = kernel.execute_operator("volume_operator", input_data)
    assert result.status == "SUCCESS"
    data = result.data
    # Should clamp step shift from 1.0 down to 1.0 - 0.15 = 0.85
    assert abs(data["recommended_weight"] - 0.85) < 1e-4
