# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Unit tests for Python AnomalyDetectionOperator (Layer 16)."""

import pytest
from core.finance_kernel.anomaly_detection import AnomalyDetectionOperator, AnomalyState, AnomalyAdvisory
from core.finance_kernel.kernel_context import FinanceKernelContext
from core.finance_kernel.kernel_config import FinanceKernelConfig


def test_anomaly_operator_baseline_normal():
    operator = AnomalyDetectionOperator()
    input_data = {
        "symbol": "SPY",
        "pair_symbol": "QQQ",
        "last_price": 500.0,
        "volume": 1000.0,
        "bid_size": 500.0,
        "ask_size": 500.0,
        "ewma_volatility": 0.01,
        "baseline_volatility": 0.01,
        "baseline_depth": 1000.0,
        "rolling_correlation": 0.95,
        "expected_correlation": 0.95,
        "vol_debounce_count": 5,
        "liq_debounce_count": 5,
        "corr_debounce_count": 5,
    }
    validated = operator.validate(input_data)
    processed = operator.preprocess(validated)
    result = operator.execute(processed)

    assert result["advisory_active"] is False
    assert result["volatility_anomaly"] is False
    assert result["liquidity_anomaly"] is False
    assert result["correlation_break"] is False
    assert len(result["messages"]) == 0


def test_anomaly_operator_volatility_expansion():
    operator = AnomalyDetectionOperator()
    input_data = {
        "symbol": "SPY",
        "last_price": 500.0,
        "ewma_volatility": 0.035,
        "baseline_volatility": 0.01,
        "vol_debounce_count": 3,
        "vol_debounce_target": 3,
    }
    validated = operator.validate(input_data)
    processed = operator.preprocess(validated)
    result = operator.execute(processed)

    assert result["advisory_active"] is True
    assert result["volatility_anomaly"] is True
    assert "Caution: abnormal volatility detected in SPY" in result["messages"][0]


def test_anomaly_operator_liquidity_displacement():
    operator = AnomalyDetectionOperator()
    input_data = {
        "symbol": "QQQ",
        "last_price": 400.0,
        "bid_size": 100.0,
        "ask_size": 100.0,
        "baseline_depth": 1000.0, # 80% thinning
        "ewma_volatility": 0.01,
        "baseline_volatility": 0.01,
        "liq_debounce_count": 3,
        "liq_debounce_target": 3,
    }
    validated = operator.validate(input_data)
    processed = operator.preprocess(validated)
    result = operator.execute(processed)

    assert result["advisory_active"] is True
    assert result["liquidity_anomaly"] is True
    assert "Caution: liquidity displacement detected in QQQ" in result["messages"][0]


def test_anomaly_operator_correlation_break():
    operator = AnomalyDetectionOperator()
    input_data = {
        "symbol": "SPY",
        "pair_symbol": "QQQ",
        "last_price": 500.0,
        "ewma_volatility": 0.01,
        "baseline_volatility": 0.01,
        "rolling_correlation": 0.10,
        "expected_correlation": 0.90,
        "corr_debounce_count": 5,
        "corr_debounce_target": 5,
    }
    validated = operator.validate(input_data)
    processed = operator.preprocess(validated)
    result = operator.execute(processed)

    assert result["advisory_active"] is True
    assert result["correlation_break"] is True
    assert "Caution: correlation break observed between SPY and QQQ" in result["messages"][0]
