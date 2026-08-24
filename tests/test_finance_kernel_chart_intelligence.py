# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Unit tests for Layer 17 Real-Time Chart Intelligence Subsystem."""

import pytest
import math
from core.finance_kernel.chart_intelligence import (
    ChartIntelligenceOperator,
    ChartIndicatorType,
    ChartConditionState,
    PatternHint,
    BaselineState,
    ChartConditionPayload,
    PatternConditionPayload
)


def test_chart_intelligence_operator_basic():
    op = ChartIntelligenceOperator()
    input_data = {
        "symbol": "SPY",
        "pair_symbol": "QQQ",
        "last_price": 500.0,
        "ewma_volatility": 0.015,
        "baseline_volatility": 0.010,
        "bid_size": 100.0,
        "ask_size": 100.0,
        "baseline_depth": 200.0,
        "rolling_correlation": 0.95,
        "expected_correlation": 0.95,
        "volume_anomaly_ratio": 1.0,
        "timestamp_ns": 123456789
    }
    validated = op.validate(input_data)
    preprocessed = op.preprocess(validated)
    result = op.execute(preprocessed)

    assert result["symbol"] == "SPY"
    assert len(result["payloads"]) == 4
    assert result["volatility_regime"] == "VolatilityExpansionBands:Neutral"
    assert result["liquidity_regime"] == "LiquidityDisplacementZones:Neutral"
    assert result["correlation_regime"] == "CorrelationDivergenceIndex:Neutral"


def test_nan_inf_inputs_handling():
    op = ChartIntelligenceOperator()
    input_data = {
        "symbol": "SPY",
        "last_price": float("nan"),
        "ewma_volatility": float("inf"),
        "baseline_volatility": float("nan"),
        "bid_size": float("inf"),
        "ask_size": 0.0,
        "baseline_depth": float("nan"),
        "rolling_correlation": float("nan"),
        "expected_correlation": float("inf"),
        "timestamp_ns": 100
    }
    preprocessed = op.preprocess(input_data)
    result = op.execute(preprocessed)

    assert result["symbol"] == "SPY"
    assert not math.isnan(result["payloads"][0]["score"])
    assert not math.isinf(result["payloads"][0]["score"])


def test_zero_baselines_handling():
    op = ChartIntelligenceOperator()
    input_data = {
        "symbol": "SPY",
        "last_price": 100.0,
        "ewma_volatility": 0.05,
        "baseline_volatility": 0.0,
        "baseline_depth": 0.0,
        "vol_30d": 0.0,
        "liq_30d": 0.0
    }
    preprocessed = op.preprocess(input_data)
    result = op.execute(preprocessed)

    assert result["payloads"][0]["score"] >= 0.0
    assert result["payloads"][1]["score"] >= 0.0


def test_extreme_volatility_compression():
    op = ChartIntelligenceOperator()
    input_data = {
        "symbol": "SPY",
        "last_price": 100.0,
        "ewma_volatility": 0.001,
        "baseline_volatility": 0.010,
        "vol_5m": 0.001,
        "vol_1h": 0.001,
        "vol_30d": 0.010
    }
    preprocessed = op.preprocess(input_data)
    result = op.execute(preprocessed)

    vol_payload = result["payloads"][0]
    assert vol_payload["state"] == "Compression"
    assert vol_payload["score"] < 50.0


def test_liquidity_vacuum_conditions():
    op = ChartIntelligenceOperator()
    input_data = {
        "symbol": "SPY",
        "last_price": 100.0,
        "ewma_volatility": 0.01,
        "baseline_volatility": 0.01,
        "bid_size": 5.0,
        "ask_size": 5.0,
        "baseline_depth": 100.0,
        "volume_anomaly_ratio": 3.0
    }
    preprocessed = op.preprocess(input_data)
    result = op.execute(preprocessed)

    liq_payload = result["payloads"][1]
    assert liq_payload["state"] == "Displaced"
    assert liq_payload["metrics"][0] == 0.90 # 90% depth thinning


def test_correlation_collapse_scenarios():
    op = ChartIntelligenceOperator()
    input_data = {
        "symbol": "SPY",
        "last_price": 100.0,
        "ewma_volatility": 0.01,
        "baseline_volatility": 0.01,
        "rolling_correlation": -0.50,
        "expected_correlation": 0.85
    }
    preprocessed = op.preprocess(input_data)
    result = op.execute(preprocessed)

    corr_payload = result["payloads"][2]
    assert corr_payload["state"] == "Broken"
    assert result["correlation_regime"] == "CorrelationDivergenceIndex:Broken"


def test_pattern_diagnostic_cup_and_handle():
    op = ChartIntelligenceOperator()
    input_data = {
        "symbol": "SPY",
        "last_price": 100.0,
        "ewma_volatility": 0.010,
        "baseline_volatility": 0.010,
        "bid_size": 90.0,
        "ask_size": 90.0,
        "baseline_depth": 100.0,
        "rolling_correlation": 0.90,
        "expected_correlation": 0.90,
        "vol_5m": 0.010,
        "vol_1h": 0.010,
        "vol_30d": 0.010
    }
    preprocessed = op.preprocess(input_data)
    result = op.execute(preprocessed)

    diag = result["pattern_diagnostics"]
    assert diag["pattern_hint"] in ["CupHandleLike", "PennantLike", "FlagLike", "None"]
    assert "scores" in diag
