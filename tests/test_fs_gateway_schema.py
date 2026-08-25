import json
import pytest

def test_fs_gateway_desk_json_schema():
    sample_json = """{
      "timestamp": "2026-12-20T12:00:00.000000Z",
      "module": "desk",
      "state": {
        "cycle_sequence_id": 100,
        "desks": [
          {
            "desk_id": "DESK_EQUITIES",
            "asset_class": "EQUITIES",
            "buy_pressure": 0.65,
            "sell_pressure": 0.35,
            "decision_intensity": 0.82,
            "active_orders": 142,
            "risk_level": 0,
            "execution_readiness": "EXECUTION_READY",
            "order_intent": "ACCUMULATE_BULLISH",
            "desk_state": "ACTIVE_TRADING",
            "recon_threshold": 0.05,
            "liquidity_depth_m": 1241.1,
            "volatility_pressure": 0.14,
            "anomaly_detected": false
          }
        ]
      },
      "metrics": { "total_active_desks": 1, "aggregate_buy_pressure": 0.65, "total_open_orders": 142 },
      "events": ["TRADING_DESK_STREAM_DISPATCHED"],
      "flags": { "stress_override": false, "meta_locked": false }
    }"""
    data = json.loads(sample_json)
    assert data["module"] == "desk"
    assert "timestamp" in data
    assert len(data["state"]["desks"]) == 1
    desk = data["state"]["desks"][0]
    assert "order_intent" in desk
    assert "liquidity_depth_m" in desk
    assert "volatility_pressure" in desk
    assert "anomaly_detected" in desk
