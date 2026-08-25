import pytest

def test_desk_stream_fields():
    desk = {
        "desk_id": "DESK_CRYPTO",
        "asset_class": "CRYPTO",
        "buy_pressure": 0.78,
        "sell_pressure": 0.22,
        "decision_intensity": 0.95,
        "active_orders": 215,
        "risk_level": 1,
        "execution_readiness": "EXECUTION_READY",
        "order_intent": "MOMENTUM_BREAKOUT",
        "desk_state": "HIGH_VOLATILITY",
        "recon_threshold": 0.05,
        "liquidity_depth_m": 192.1,
        "volatility_pressure": 0.56,
        "anomaly_detected": True
    }
    assert desk["buy_pressure"] + desk["sell_pressure"] == 1.0
    assert desk["recon_threshold"] == 0.05
    assert desk["order_intent"] == "MOMENTUM_BREAKOUT"
    assert desk["anomaly_detected"] is True
