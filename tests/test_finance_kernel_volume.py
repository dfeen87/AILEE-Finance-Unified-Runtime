# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Unit tests for the Intraday Volume Advisory operator."""

import json
import math
import sys
from decimal import Decimal, localcontext

import pytest
from core.finance_kernel import run_finance_operator
from core.finance_kernel.kernel_errors import KernelConfigurationError, OperatorExecutionError
from core.finance_kernel.kernel_context import FinanceKernelContext
from core.finance_kernel.kernel_config import FinanceKernelConfig
from core.finance_kernel.kernel_registry import create_default_registry
from core.finance_kernel.finance_kernel import FinanceRuntimeKernel
from core.finance_kernel.volume_advisory import IntradayVolumeAdvisory, VolumeState, calculate_hft_delta_v
from core.finance_kernel.volume_execution import VolumeExecutionOperator

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

def test_volume_operator_contrarian_oversold_index_etf():
    context = FinanceKernelContext(ledger_id="test-ledger", session_id="test-session")
    config = FinanceKernelConfig(enable_contrarian_oversold=True)
    registry = create_default_registry()
    kernel = FinanceRuntimeKernel(context, config, registry)

    # Condition A strong oversold for SPY
    input_data = {
        "symbol": "SPY",
        "current_volume": 3000000.0,
        "avg_volume": 1000000.0,
        "price_change": -0.015,
        "vwap_deviation": -0.010
    }

    result = kernel.execute_operator("volume_operator", input_data)
    assert result.status == "SUCCESS"
    data = result.data
    assert data["oversold_state"] is True
    assert data["contrarian_buy_signal"] is True
    assert data["oversold_score"] > 0.5

def test_volume_operator_contrarian_override():
    context = FinanceKernelContext(ledger_id="test-ledger", session_id="test-session")
    config = FinanceKernelConfig(enable_contrarian_oversold=False)
    registry = create_default_registry()
    kernel = FinanceRuntimeKernel(context, config, registry)

    input_data = {
        "symbol": "QQQ",
        "current_volume": 3000000.0,
        "avg_volume": 1000000.0,
        "price_change": -0.015,
        "vwap_deviation": -0.010,
        "contrarian_override": 1 # Force enable
    }

    result = kernel.execute_operator("volume_operator", input_data)
    assert result.status == "SUCCESS"
    data = result.data
    assert data["oversold_state"] is True
    assert data["contrarian_buy_signal"] is True

    # Test force disable
    input_data["contrarian_override"] = -1
    result2 = kernel.execute_operator("volume_operator", input_data)
    assert result2.data["oversold_state"] is True
    assert result2.data["contrarian_buy_signal"] is False

def test_volume_operator_contrarian_safety_precedence():
    context = FinanceKernelContext(
        ledger_id="test-ledger",
        session_id="test-session",
        metadata={"kill_switch": True}
    )
    config = FinanceKernelConfig(enable_contrarian_oversold=True)
    registry = create_default_registry()
    kernel = FinanceRuntimeKernel(context, config, registry)

    input_data = {
        "symbol": "SPY",
        "current_volume": 3000000.0,
        "avg_volume": 1000000.0,
        "price_change": -0.015,
        "vwap_deviation": -0.010
    }

    result = kernel.execute_operator("volume_operator", input_data)
    assert result.status == "SUCCESS"
    data = result.data
    assert data["recommended_weight"] == 0.0
    assert data["contrarian_buy_signal"] is False
    assert data["oversold_state"] is False

def test_volume_operator_hft_math_delta_v():
    ticks = [
        {"p_input": 0.05, "w": 0.2, "v": 1.5, "M": 1.0, "dt": 0.001},
        {"p_input": 0.10, "w": 0.1, "v": 2.0, "M": 0.0, "dt": 0.001} # Mass floor M >= 1e-6
    ]
    dv = calculate_hft_delta_v(isp=1.0, efficiency=0.95, alpha=0.1, v0=0.0, ticks=ticks)
    assert dv > 0.0

    zero_eff_dv = calculate_hft_delta_v(isp=1.0, efficiency=0.0, alpha=0.1, v0=0.0, ticks=ticks)
    assert zero_eff_dv == 0.0

    empty_dv = calculate_hft_delta_v(isp=1.0, efficiency=0.95, alpha=0.1, v0=0.0, ticks=[])
    assert empty_dv == 0.0

def test_volume_operator_hft_mode():
    context = FinanceKernelContext(ledger_id="test-ledger", session_id="test-session")
    config = FinanceKernelConfig()
    registry = create_default_registry()
    kernel = FinanceRuntimeKernel(context, config, registry)

    input_data = {
        "current_volume": 20000.0,
        "avg_volume": 10000.0,
        "price_change": 0.008,
        "vwap_deviation": 0.0,
        "enable_hft": True,
        "hft_p_input": 0.08,
        "hft_mass": 1.0
    }

    result = kernel.execute_operator("volume_operator", input_data)
    assert result.status == "SUCCESS"
    data = result.data
    assert data["hft_active"] is True
    assert data["hft_delta_v"] > 0.0


def test_volume_operator_rejects_unrepresentable_score_before_success():
    with pytest.raises(OperatorExecutionError, match="oversold_score.*representable"):
        run_finance_operator(
            "volume_operator",
            {"current_volume": 10.0, "avg_volume": 1.0,
             "price_change": -0.02, "vwap_deviation": -0.02},
            config_overrides={"contrarian_oversold_aggressiveness": sys.float_info.max},
        )


@pytest.mark.parametrize("aggressiveness", [0.0, -0.0])
def test_zero_aggressiveness_with_extreme_negative_prices_remains_json_compatible(aggressiveness):
    result = run_finance_operator(
        "volume_operator",
        {"current_volume": 10.0, "avg_volume": 1.0,
         "price_change": -sys.float_info.max, "vwap_deviation": -sys.float_info.max},
        config_overrides={"contrarian_oversold_aggressiveness": aggressiveness,
                          "json_compat_mode": True},
    )
    assert result["status"] == "SUCCESS"
    assert result["data"]["oversold_score"] == 0.0
    assert json.loads(json.dumps(result, allow_nan=False)) == result


@pytest.mark.parametrize("price_change,aggressiveness", [
    (-sys.float_info.max, 1e-308),
    (-sys.float_info.max / 32.0, 1.0),
])
def test_representable_score_survives_overflowing_normalization(price_change, aggressiveness):
    result = run_finance_operator(
        "volume_operator",
        {"current_volume": 0.0, "avg_volume": 1.0,
         "price_change": price_change, "vwap_deviation": 0.0},
        config_overrides={"contrarian_oversold_aggressiveness": aggressiveness,
                          "json_compat_mode": True},
    )
    # Independent high-precision reference: the final value fits despite the
    # unscaled price normalization overflowing binary64.
    with localcontext() as decimal_context:
        decimal_context.prec = 80
        expected = float(
            (Decimal.from_float(-price_change) - Decimal.from_float(0.007))
            / Decimal.from_float(0.015) * Decimal.from_float(0.4)
            * Decimal.from_float(aggressiveness)
        )
    assert result["status"] == "SUCCESS"
    assert math.isfinite(result["data"]["oversold_score"])
    assert result["data"]["oversold_score"] == pytest.approx(expected, rel=1e-15)
    assert json.loads(json.dumps(result, allow_nan=False)) == result


@pytest.mark.parametrize("hft_bias", [
    {"bullish_multiplier_price": float("inf")},
    {"bullish_execution_scale": 2.0},
    [],
])
def test_volume_hft_request_cannot_bypass_configuration_bounds(hft_bias):
    with pytest.raises(KernelConfigurationError):
        run_finance_operator(
            "volume_operator",
            {"current_volume": 2.0, "avg_volume": 1.0,
             "price_change": 0.008, "vwap_deviation": 0.0,
             "enable_hft": True, "hft_bias_config": hft_bias},
        )


def test_volume_hft_request_preserves_disabled_bias_and_mode_extensions():
    result = run_finance_operator(
        "volume_operator",
        {"current_volume": 1.0, "avg_volume": 1.0,
         "price_change": 0.0, "vwap_deviation": 0.0,
         "enable_hft": True, "hft_p_input": 0.08,
         "hft_bias_config": {"enabled": False, "bullishness_mode": "CONTRARIAN",
                             "contrarian_oversold_threshold": 0.0}},
        config_overrides={"json_compat_mode": True},
    )
    assert result["status"] == "SUCCESS"
    assert result["data"]["contrarian_buy_signal"] is True
    assert result["data"]["hft_delta_v"] == pytest.approx(
        calculate_hft_delta_v(1.0, 0.95, 0.1, 0.0,
                              [{"p_input": 0.08, "w": 0.15, "v": 1.0,
                                "M": 1.0, "dt": 0.001}])
    )
    assert json.loads(json.dumps(result, allow_nan=False)) == result


@pytest.mark.parametrize("hft_bias", [
    {"bullish_multiplier_price": float("inf")},
    {"bullish_execution_scale": 2.0},
    [],
])
def test_volume_execution_rejects_unvalidated_hft_configuration(hft_bias, tmp_path):
    with pytest.raises(KernelConfigurationError):
        VolumeExecutionOperator(hft_bias_config=hft_bias,
                                audit_log_file=str(tmp_path / "audit.log"))


def test_volume_execution_revalidates_changed_hft_config_before_mutation(tmp_path):
    operator = VolumeExecutionOperator(audit_log_file=str(tmp_path / "audit.log"))
    operator.hft_bias_config["bullish_multiplier_price"] = float("inf")
    operator.current_equity = operator.peak_equity + 1.0
    original = dict(operator.__dict__)
    with pytest.raises(KernelConfigurationError):
        operator.process_tick({"growth_favorable": True}, 10.0)
    assert operator.__dict__ == original
    assert not (tmp_path / "audit.log").exists()
