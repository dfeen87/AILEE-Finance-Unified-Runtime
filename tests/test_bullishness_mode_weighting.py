import pytest

from core.finance_kernel.volume_advisory import IntradayVolumeAdvisory
from core.finance_kernel.chart_intelligence import ChartIntelligenceOperator
from ailee_finance.domains.finance.sell_governance import compute_sell_ceiling

def test_bullishness_mode_weighting_multipliers():
    # Test multipliers for STANDARD, CONSERVATIVE, HYPER, CONTRARIAN
    modes = {
        "STANDARD": {"vol_mult": 1.0, "depth_mult": 1.0},
        "CONSERVATIVE": {"vol_mult": 1.08, "depth_mult": 1.10},
        "HYPER": {"vol_mult": 1.25, "depth_mult": 1.30},
        "CONTRARIAN": {"vol_mult": 1.30, "depth_mult": 1.35}
    }

    base_vol = 10.0
    base_depth = 100.0

    # Conservative
    c_vol = base_vol * modes["CONSERVATIVE"]["vol_mult"]
    c_depth = base_depth * modes["CONSERVATIVE"]["depth_mult"]
    assert abs(c_vol - 10.8) < 1e-6
    assert abs(c_depth - 110.0) < 1e-6

    # Hyper
    h_vol = base_vol * modes["HYPER"]["vol_mult"]
    h_depth = base_depth * modes["HYPER"]["depth_mult"]
    assert h_vol == 12.5
    assert h_depth == 130.0

    # Contrarian
    k_vol = base_vol * modes["CONTRARIAN"]["vol_mult"]
    k_depth = base_depth * modes["CONTRARIAN"]["depth_mult"]
    assert k_vol == 13.0
    assert k_depth == 135.0

def test_contrarian_backend_analytics_modulation():
    op = IntradayVolumeAdvisory()
    inp = {
        "current_volume": 3000.0,
        "avg_volume": 1000.0,
        "price_change": -0.015,
        "vwap_deviation": -0.010,
        "symbol": "SPY",
        "trust_score": 0.85,
        "manipulation_score": 0.0,
        "hft_bias_config": {
            "enabled": True,
            "bullishness_mode": "CONTRARIAN",
            "trust_threshold_bullish": 0.70,
            "manipulation_threshold": 0.30,
            "contrarian_oversold_weight_mult": 1.25,
            "contrarian_oversold_threshold": 0.65,
            "contrarian_hf_impulse_scale": 1.25,
            "contrarian_sell_ceiling_factor": 0.85
        }
    }
    validated = op.validate(inp)
    processed = op.preprocess(validated)
    res = op.execute(processed)

    assert res["oversold_state"] is True
    assert res["contrarian_buy_signal"] is True
    assert res["recommended_weight"] > 0.0

def test_chart_intelligence_contrarian_buy_zone():
    op = ChartIntelligenceOperator()
    inp = {
        "last_price": 100.0,
        "ewma_volatility": 25.0,
        "baseline_volatility": 10.0,
        "bid_size": 10.0,
        "ask_size": 10.0,
        "baseline_depth": 50.0,
        "rolling_correlation": 0.1,
        "expected_correlation": 0.8,
        "volume_anomaly_ratio": 3.0,
        "trust_score": 0.85,
        "manipulation_score": 0.1,
        "layer_locks_engaged": False,
        "hft_bias_config": {
            "enabled": True,
            "bullishness_mode": "CONTRARIAN"
        }
    }
    validated = op.validate(inp)
    processed = op.preprocess(validated)
    res = op.execute(processed)

    assert "pattern_diagnostics" in res
    assert res["pattern_diagnostics"]["pattern_group"] == "StressGroup"
    assert res["pattern_diagnostics"]["contrarian_buy_zone"] is True

def test_contrarian_sell_ceiling_factor():
    allowed_standard = compute_sell_ceiling(1, 100.0, bullish_active=True, bullish_sell_ceiling_factor=0.80)
    allowed_contrarian = compute_sell_ceiling(1, 100.0, bullish_active=True, bullish_sell_ceiling_factor=0.85)

    assert allowed_standard == 48.0 # 100 * 0.6 * 0.8
    assert allowed_contrarian == 51.0 # 100 * 0.6 * 0.85
