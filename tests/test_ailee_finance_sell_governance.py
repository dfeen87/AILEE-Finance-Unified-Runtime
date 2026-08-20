import os
import json
import pytest
from unittest.mock import MagicMock
from ailee_finance.domains.finance.sell_governance import (
    validate_sell_intent,
    compute_sell_ceiling,
    detect_sell_manipulation,
    grace_layer_sell_adjustment,
    consensus_validation,
)
from ailee_finance.domains.finance.ailee_finance_domain import (
    AileeFinanceDomain,
    SellGovernanceDecision,
)
from ailee_finance.core_min import AileeFinanceTrustPipeline


def test_validate_sell_intent():
    # Legitimate signals
    valid_signals = {
        "position_size": 100.0,
        "volatility": 0.15,
        "market": {"liquidity": 1.0},
        "intent_flag": True
    }
    res = validate_sell_intent(valid_signals)
    assert res["intent_valid"] is True

    # Invalid intent flag
    invalid_flag = dict(valid_signals, intent_flag=False, intent_reason="User canceled order")
    res_flag = validate_sell_intent(invalid_flag)
    assert res_flag["intent_valid"] is False
    assert "User canceled order" in res_flag["reason"]

    # Non-positive position size
    zero_pos = dict(valid_signals, position_size=0.0)
    res_pos = validate_sell_intent(zero_pos)
    assert res_pos["intent_valid"] is False

    # Collapsed liquidity
    collapsed = dict(valid_signals, market={"liquidity": 0.001})
    res_col = validate_sell_intent(collapsed)
    assert res_col["intent_valid"] is False


def test_compute_sell_ceiling():
    position_size = 1000.0

    # Level 0: 100%
    assert compute_sell_ceiling(0, position_size) == 1000.0

    # Level 1: 60%
    assert compute_sell_ceiling(1, position_size) == 600.0

    # Level 2: 30%
    assert compute_sell_ceiling(2, position_size) == 300.0

    # Level 3: 10%
    assert compute_sell_ceiling(3, position_size) == 100.0

    # Unknown level defaults to level 3 (10%)
    assert compute_sell_ceiling(99, position_size) == 100.0


def test_detect_sell_manipulation():
    clean_market = {
        "spoofed_bids": False,
        "bid_liquidity_drop": 0.0,
        "mev_detected": False,
        "spread_widening": 0.0
    }
    assert detect_sell_manipulation(clean_market) == 0.0

    manipulated_market = {
        "spoofed_bids": True,
        "bid_liquidity_drop": 0.5,
        "mev_detected": True,
        "spread_widening": 0.2
    }
    score = detect_sell_manipulation(manipulated_market)
    assert score > 0.70
    assert score <= 1.0


def test_grace_layer_sell_adjustment():
    sell_amount = 500.0

    # Low volatility: no change
    assert grace_layer_sell_adjustment(0.15, sell_amount) == 500.0

    # Moderate volatility: mild dampening
    mod_adjusted = grace_layer_sell_adjustment(0.35, sell_amount)
    assert mod_adjusted < 500.0
    assert mod_adjusted > 0.0

    # High volatility: strong dampening
    high_adjusted = grace_layer_sell_adjustment(0.80, sell_amount)
    assert high_adjusted < mod_adjusted


def test_consensus_validation():
    # Empty feeds
    assert consensus_validation([]) == 0.0

    # High agreement feeds
    consistent_feeds = [
        {"feed_id": "f1", "price": 100.0, "confidence": 0.95},
        {"feed_id": "f2", "price": 100.2, "confidence": 0.90},
        {"feed_id": "f3", "price": 99.8, "confidence": 0.92},
    ]
    high_score = consensus_validation(consistent_feeds)
    assert high_score > 0.85

    # Divergent feeds
    divergent_feeds = [
        {"feed_id": "f1", "price": 100.0, "confidence": 0.90},
        {"feed_id": "f2", "price": 150.0, "confidence": 0.90},
        {"feed_id": "f3", "price": 50.0, "confidence": 0.90},
    ]
    low_score = consensus_validation(divergent_feeds)
    assert low_score < high_score


def test_ailee_finance_domain_evaluation(tmp_path):
    log_file = tmp_path / "ailee_finance_sell_audit.log"
    domain = AileeFinanceDomain(log_path=str(log_file))

    signals = {
        "position_size": 1000.0,
        "trust_score": 0.90,
        "volatility": 0.10,
        "market": {
            "spoofed_bids": False,
            "bid_liquidity_drop": 0.0,
            "mev_detected": False,
            "spread_widening": 0.0,
            "liquidity": 1.0
        },
        "feeds": [
            {"feed_id": "f1", "price": 100.0, "confidence": 0.95},
            {"feed_id": "f2", "price": 100.1, "confidence": 0.95}
        ],
        "intent_flag": True
    }

    decision = domain.evaluate_sell(signals)
    assert isinstance(decision, SellGovernanceDecision)
    assert decision.level == 0
    assert decision.allowed_sell_amount == 1000.0
    assert decision.trust_score == 0.90
    assert decision.manipulation_score == 0.0
    assert decision.consensus_score > 0.80

    # Verify audit log entry
    assert log_file.exists()
    lines = log_file.read_text().strip().split("\n")
    assert len(lines) == 1
    log_entry = json.loads(lines[0])
    assert log_entry["level"] == 0
    assert log_entry["allowed_sell_amount"] == 1000.0


def test_ailee_finance_trust_pipeline_fallback(tmp_path):
    log_file = tmp_path / "ailee_finance_sell_audit.log"
    domain = AileeFinanceDomain(log_path=str(log_file))
    pipeline = AileeFinanceTrustPipeline(domain=domain)

    # Valid signals pipeline run
    signals = {
        "position_size": 200.0,
        "trust_score": 0.75,
        "volatility": 0.20,
        "market": {"spoofed_bids": False},
        "feeds": [{"price": 10.0}],
        "intent_flag": True
    }
    decision = pipeline.process_sell(signals)
    assert decision.level in (0, 1)

    # Exception simulation leading to fallback Level 3 protective mode
    mock_domain = MagicMock()
    mock_domain.evaluate_sell.side_effect = RuntimeError("Critical domain error")
    faulty_pipeline = AileeFinanceTrustPipeline(domain=mock_domain)

    faulty_signals = {"position_size": 500.0}
    fallback_decision = faulty_pipeline.process_sell(faulty_signals)
    assert fallback_decision.level == 3
    assert fallback_decision.allowed_sell_amount == 50.0  # 500.0 * 0.1
    assert fallback_decision.trust_score == 0.0
    assert fallback_decision.manipulation_score == 1.0
    assert fallback_decision.consensus_score == 0.0
    assert "Fallback triggered: Critical domain error" in fallback_decision.reason
