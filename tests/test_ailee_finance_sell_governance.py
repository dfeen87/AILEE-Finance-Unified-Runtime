import os
import json
import math
import sys
from decimal import Decimal, localcontext
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
from core.finance_kernel.kernel_config import KernelConfigurationError


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


def _decimal_consensus_reference(prices, confidences):
    # Independently evaluate the documented formula without binary float
    # overflow/underflow in the mean or squared deviations.
    with localcontext() as context:
        context.prec = 1400
        decimal_prices = [Decimal.from_float(price) for price in prices]
        count = Decimal(len(prices))
        mean = sum(decimal_prices) / count
        variance = sum((price - mean) ** 2 for price in decimal_prices) / count
        price_score = max(Decimal(0), Decimal(1) - Decimal(5) * variance.sqrt() / mean)
        confidence = sum(Decimal.from_float(value) for value in confidences) / count
        return float(max(Decimal(0), min(Decimal(1), price_score * confidence)))


@pytest.mark.parametrize("prices, confidences", [
    ([1e308, 1e308], [0.95, 0.95]),
    ([sys.float_info.max, sys.float_info.max], [0.95, 0.95]),
    ([1e200, 1e200 + 1e190], [0.95, 0.95]),
    ([1e-308, 2e-308], [0.95, 0.95]),
    ([5e-324, 1e-323], [0.95, 0.95]),
    ([1e-200, 1.01e-200], [0.95, 0.95]),
    ([100.0, 200.0], [sys.float_info.max, sys.float_info.max]),
    ([100.0, 100.0], [-0.5, 1.5]),
    ([100.0, 100.0, 100.0], [sys.float_info.max, -sys.float_info.max, 1e-308]),
    ([100.0, 100.0], [5e-324, 5e-324]),
    ([100.0, 100.0, 100.0], [sys.float_info.max] * 3),
], ids=["overflowing-mean", "max-finite", "overflowing-square", "underflowing-square",
        "subnormal-prices", "near-small-prices", "overflowing-confidence-mean", "signed-confidence",
        "confidence-cancellation", "subnormal-confidence", "weighted-confidence-boundary"])
def test_consensus_preserves_formula_for_extreme_finite_inputs(prices, confidences):
    feeds = [{"price": price, "confidence": confidence} for price, confidence in zip(prices, confidences)]
    assert consensus_validation(feeds) == pytest.approx(_decimal_consensus_reference(prices, confidences),
                                                     abs=0.0, rel=1e-12)


@pytest.mark.parametrize("confidence", [math.nan, math.inf, -math.inf])
def test_consensus_does_not_promote_non_finite_confidence(confidence):
    assert consensus_validation([{"price": 100.0, "confidence": confidence},
                                 {"price": 100.0, "confidence": confidence}]) == 0.0


def test_consensus_keeps_ignored_non_positive_prices_and_zero_confidence():
    assert consensus_validation([-1.0, -0.0, 0.0, {"price": 100.0, "confidence": 0.0}]) == 0.0
    assert consensus_validation([-1.0, -0.0, 0.0, 100.0, 100.0]) == 1.0


@pytest.mark.parametrize("price", [1e308, sys.float_info.max, 5e-324])
def test_sell_pipeline_keeps_agreeing_extreme_price_feeds(price, tmp_path):
    audit_path = tmp_path / "sell.jsonl"
    domain = AileeFinanceDomain(log_path=str(audit_path))
    decision = AileeFinanceTrustPipeline(domain=domain).process_sell({
        "position_size": 1000.0, "trust_score": 0.9, "market": {"liquidity": 1.0},
        "feeds": [{"price": price, "confidence": 0.95}, {"price": price, "confidence": 0.95}],
    })
    assert decision.level == 0
    assert decision.consensus_score == pytest.approx(0.95)
    assert decision.allowed_sell_amount == 800.0
    assert json.loads(audit_path.read_text())["consensus_score"] == 0.95


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

    # Bullish mode active: allowed sell amount reduced by bullish_sell_ceiling_factor (0.80)
    decision = domain.evaluate_sell(signals)
    assert isinstance(decision, SellGovernanceDecision)
    assert decision.level == 0
    assert decision.bullish_mode_active is True
    assert decision.allowed_sell_amount == 800.0
    assert decision.trust_score == 0.90
    assert decision.manipulation_score == 0.0
    assert decision.consensus_score > 0.80

    # Disabled bias: restores neutral sell ceiling
    neutral_signals = dict(signals, hft_bias_config={"enabled": False})
    neutral_decision = domain.evaluate_sell(neutral_signals)
    assert neutral_decision.bullish_mode_active is False
    assert neutral_decision.allowed_sell_amount == 1000.0

    # Verify audit log entry
    assert log_file.exists()
    lines = log_file.read_text().strip().split("\n")
    assert len(lines) == 2
    log_entry = json.loads(lines[0])
    assert log_entry["level"] == 0
    assert log_entry["allowed_sell_amount"] == 800.0
    assert log_entry["bullish_mode_active"] is True


@pytest.mark.parametrize("field", ["trust_score", "telemetry_trust", "hardware_integrity", "model_confidence"])
@pytest.mark.parametrize("evidence", [math.nan, math.inf, -math.inf], ids=["nan", "inf", "negative-inf"])
@pytest.mark.parametrize("threshold", [0.7, 0.0])
def test_sell_pipeline_does_not_launder_non_finite_trust(field, evidence, threshold, tmp_path):
    audit_path = tmp_path / "sell.jsonl"
    pipeline = AileeFinanceTrustPipeline(domain=AileeFinanceDomain(log_path=str(audit_path)))
    signals = {
        "position_size": 1000.0,
        "market": {"liquidity": 1.0},
        "feeds": [{"price": 100.0, "confidence": 0.95}, {"price": 100.1, "confidence": 0.95}],
        "hft_bias_config": {"trust_threshold_bullish": threshold},
        field: evidence,
    }
    decision = pipeline.process_sell(signals)
    assert decision.trust_score == 0.0
    assert decision.bullish_mode_active is False
    assert decision.level == 3
    assert decision.allowed_sell_amount == 100.0
    logged = json.loads(audit_path.read_text())
    assert logged["trust_score"] == 0.0
    assert logged["bullish_mode_active"] is False


@pytest.mark.parametrize("field", ["bid_liquidity_drop", "spread_widening"])
@pytest.mark.parametrize("evidence", [math.nan, math.inf, -math.inf, "bad", None, 10 ** 400],
                         ids=["nan", "inf", "negative-inf", "malformed", "missing-value", "oversized-int"])
@pytest.mark.parametrize("threshold", [0.3, 1.0])
def test_sell_pipeline_does_not_launder_invalid_manipulation(field, evidence, threshold, tmp_path):
    audit_path = tmp_path / "sell.jsonl"
    pipeline = AileeFinanceTrustPipeline(domain=AileeFinanceDomain(log_path=str(audit_path)))
    signals = {
        "position_size": 1000.0,
        "trust_score": 0.9,
        "market": {"liquidity": 1.0, field: evidence},
        "hft_bias_config": {"manipulation_threshold": threshold},
        "feeds": [{"price": 100.0, "confidence": 0.95}, {"price": 100.1, "confidence": 0.95}],
    }
    decision = pipeline.process_sell(signals)
    assert decision.manipulation_score == 1.0
    assert decision.bullish_mode_active is False
    assert decision.level == 3
    assert math.isfinite(decision.allowed_sell_amount)
    logged = json.loads(audit_path.read_text())
    assert logged["manipulation_score"] == 1.0
    assert logged["bullish_mode_active"] is False


def test_sell_pipeline_keeps_valid_maximum_manipulation_with_maximum_threshold(tmp_path):
    domain = AileeFinanceDomain(log_path=str(tmp_path / "sell.jsonl"))
    decision = AileeFinanceTrustPipeline(domain=domain).process_sell({
        "position_size": 1000.0, "trust_score": 0.9,
        "hft_bias_config": {"manipulation_threshold": 1.0},
        "market": {"liquidity": 1.0, "spoofed_bids": True, "bid_liquidity_drop": 1.0,
                   "mev_detected": True, "spread_widening": 1.0},
    })
    assert decision.manipulation_score == 1.0
    assert decision.bullish_mode_active is True
    assert decision.level == 3
    assert decision.allowed_sell_amount == 50.0


@pytest.mark.parametrize("trust, expected", [(-1.0, 0.0), (-0.0, 0.0), (0.9, 0.9), (2.0, 1.0),
                                          (-1e308, 0.0), (1e308, 1.0), ("0.9", 0.9)])
def test_trust_normalization_preserves_finite_legacy_values(trust, expected, tmp_path):
    domain = AileeFinanceDomain(log_path=str(tmp_path / "sell.jsonl"))
    assert domain.compute_trust_score({"trust_score": trust}) == expected


@pytest.mark.parametrize("trust", [-1.0, -0.0, 0.0])
def test_sell_pipeline_keeps_finite_clamped_zero_with_zero_threshold(trust, tmp_path):
    domain = AileeFinanceDomain(log_path=str(tmp_path / "sell.jsonl"))
    decision = AileeFinanceTrustPipeline(domain=domain).process_sell({
        "position_size": 1000.0,
        "trust_score": trust,
        "hft_bias_config": {"trust_threshold_bullish": 0.0},
        "market": {"liquidity": 1.0},
    })
    assert decision.trust_score == 0.0
    assert decision.bullish_mode_active is True
    assert decision.level == 3
    assert decision.allowed_sell_amount == 100.0


def test_composite_trust_keeps_weighted_finite_negative_values(tmp_path):
    domain = AileeFinanceDomain(log_path=str(tmp_path / "sell.jsonl"))
    assert domain.compute_trust_score({"telemetry_trust": -0.5, "hardware_integrity": 1.0,
                                       "model_confidence": 1.0}) == pytest.approx(0.4)


@pytest.mark.parametrize("field", ["bid_liquidity_drop", "spread_widening"])
@pytest.mark.parametrize("evidence", [-1e308, -0.0, 0.0, "0.0"])
def test_manipulation_keeps_non_positive_finite_legacy_values(field, evidence):
    assert detect_sell_manipulation({field: evidence}) == 0.0


@pytest.mark.parametrize("config", [
    {"bullish_sell_ceiling_factor": 2.0},
    {"bullish_sell_ceiling_factor": math.inf},
    {"bullish_sell_ceiling_factor": math.nan},
    {"bullish_execution_scale": 2.0},
    {"enabled": "false"},
], ids=["ceiling-over-position", "infinite-ceiling", "nan-ceiling", "execution-out-of-domain", "untyped-enable"])
def test_sell_domain_enforces_hft_config_at_constructor_and_request(config, tmp_path):
    audit_path = tmp_path / "sell.jsonl"
    with pytest.raises(KernelConfigurationError):
        AileeFinanceDomain(log_path=str(audit_path), hft_bias_config=config)

    domain = AileeFinanceDomain(log_path=str(audit_path))
    baseline = dict(domain.hft_bias_config)
    signals = {"position_size": 1000.0, "trust_score": 0.9, "market": {"liquidity": 1.0},
               "feeds": [{"price": 100.0, "confidence": 0.95}, {"price": 100.1, "confidence": 0.95}]}
    with pytest.raises(KernelConfigurationError):
        domain.evaluate_sell(dict(signals, hft_bias_config=config))
    assert domain.hft_bias_config == baseline

    fallback = AileeFinanceTrustPipeline(domain=domain).process_sell(dict(signals, hft_bias_config=config))
    assert fallback.level == 3
    assert fallback.bullish_mode_active is False
    assert fallback.allowed_sell_amount == 100.0
    assert domain.hft_bias_config == baseline
    assert not audit_path.exists()

    recovered = domain.evaluate_sell(signals)
    assert recovered.level == 0
    assert recovered.allowed_sell_amount == 800.0
    assert recovered.bullish_mode_active is True


@pytest.mark.parametrize("config, expected", [(None, 800.0), ({}, 800.0), ({"enabled": False}, 1000.0),
                                              ({"bullish_sell_ceiling_factor": 0.1}, 100.0),
                                              ({"bullish_sell_ceiling_factor": 1.0}, 1000.0)])
def test_sell_pipeline_keeps_optional_partial_and_boundary_hft_configs(config, expected, tmp_path):
    domain = AileeFinanceDomain(log_path=str(tmp_path / "sell.jsonl"), hft_bias_config=config)
    decision = AileeFinanceTrustPipeline(domain=domain).process_sell({
        "position_size": 1000.0, "trust_score": 0.9, "hft_bias_config": config,
        "market": {"liquidity": 1.0},
        "feeds": [{"price": 100.0, "confidence": 0.95}, {"price": 100.1, "confidence": 0.95}],
    })
    assert decision.level == 0
    assert decision.allowed_sell_amount == expected
    assert decision.bullish_mode_active is (not (config and config.get("enabled") is False))


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
