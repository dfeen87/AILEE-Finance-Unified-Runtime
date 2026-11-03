"""Adversarial numeric evidence must never become a larger SELL permission."""

import json
import math
import sys
from unittest.mock import Mock

import pytest

from ailee_finance.core_min import AileeFinanceTrustPipeline
from ailee_finance.domains.finance.ailee_finance_domain import AileeFinanceDomain, SellGovernanceDecision
from ailee_finance.domains.finance.sell_governance import (
    compute_sell_ceiling, consensus_validation, grace_layer_sell_adjustment,
    validate_sell_intent,
)
from core.finance_kernel.unified_runtime import UnifiedRuntimeOperator


BAD = [math.nan, math.inf, -math.inf, True, None, "bad", "0.1", 10**400]


def sell_signals(**overrides):
    return dict({"position_size": 1000.0, "trust_score": 0.9, "volatility": 0.1,
                 "market": {"liquidity": 1.0}, "feeds": [100.0, 100.0]}, **overrides)


@pytest.mark.parametrize("value", BAD + [-100.0])
@pytest.mark.parametrize("fallback", [False, True])
def test_invalid_position_is_zero_in_domain_and_exception_fallback(value, fallback, tmp_path):
    domain = Mock() if fallback else AileeFinanceDomain(log_path=str(tmp_path / "sell.jsonl"))
    if fallback:
        domain.evaluate_sell.side_effect = RuntimeError("domain unavailable")
    decision = AileeFinanceTrustPipeline(domain).process_sell(sell_signals(position_size=value))
    assert decision.level == 3
    assert decision.allowed_sell_amount == 0.0
    assert not decision.bullish_mode_active
    json.dumps(decision.to_dict(), allow_nan=False)


@pytest.mark.parametrize("field", ["volatility", "liquidity"])
@pytest.mark.parametrize("value", BAD + [-1.0])
def test_invalid_market_context_cannot_authorize_full_sell(field, value, tmp_path):
    signals = sell_signals()
    if field == "liquidity":
        signals["market"][field] = value
    else:
        signals[field] = value
    assert validate_sell_intent(signals)["intent_valid"] is False
    decision = AileeFinanceTrustPipeline(AileeFinanceDomain(log_path=str(tmp_path / "sell.jsonl"))).process_sell(signals)
    assert decision.level == 3
    assert 0.0 <= decision.allowed_sell_amount <= 100.0
    assert not decision.bullish_mode_active
    json.dumps(decision.to_dict(), allow_nan=False)


@pytest.mark.parametrize("value", BAD)
def test_direct_sell_ceiling_rejects_invalid_position(value):
    assert compute_sell_ceiling(0, value) == 0.0


@pytest.mark.parametrize("factor", [math.nan, math.inf, -math.inf, -1.0, 1.01, True])
def test_direct_sell_ceiling_cannot_expand_hard_position_limit(factor):
    assert compute_sell_ceiling(0, 1000.0, True, factor) == 0.0


@pytest.mark.parametrize("value", BAD + [-1.0])
def test_grace_does_not_turn_invalid_volatility_into_normal_market(value):
    assert grace_layer_sell_adjustment(value, 1000.0) == 0.0


@pytest.mark.parametrize("price", [math.nan, math.inf, -math.inf, True, 10**400])
def test_malformed_price_is_not_dropped_from_otherwise_agreeing_feeds(price):
    assert consensus_validation([100.0, 100.0, {"price": price}]) == 0.0


@pytest.mark.parametrize("value", BAD)
def test_bad_feed_confidence_is_rejected_without_throwing(value):
    assert consensus_validation([{"price": 100.0, "confidence": value}, 100.0]) == 0.0


def test_extreme_finite_position_remains_bounded_and_serializable(tmp_path):
    domain = AileeFinanceDomain(log_path=str(tmp_path / "sell.jsonl"))
    decision = domain.evaluate_sell(sell_signals(position_size=sys.float_info.max))
    assert decision.level == 0
    assert decision.allowed_sell_amount == sys.float_info.max * 0.8
    json.dumps(decision.to_dict(), allow_nan=False)
    assert json.loads((tmp_path / "sell.jsonl").read_text())["allowed_sell_amount"] == decision.allowed_sell_amount


def test_invalid_evaluation_preserves_request_and_valid_recovery(tmp_path):
    domain = AileeFinanceDomain(log_path=str(tmp_path / "sell.jsonl"))
    pipeline = AileeFinanceTrustPipeline(domain)
    request = sell_signals(volatility=math.inf)
    baseline = json.dumps(request)
    assert pipeline.process_sell(request).level == 3
    assert json.dumps(request) == baseline
    assert pipeline.process_sell(sell_signals()).allowed_sell_amount == 800.0


@pytest.mark.parametrize("amount", [math.nan, math.inf, -math.inf, -1.0, 1001.0])
def test_pipeline_rechecks_mutable_domain_output_before_return(amount):
    decision = SellGovernanceDecision(0, 800.0, 0.9, 0.0, 0.9, "custom domain")
    decision.allowed_sell_amount = amount
    domain = Mock()
    domain.evaluate_sell.return_value = decision
    result = AileeFinanceTrustPipeline(domain).process_sell(sell_signals())
    assert result.level == 3
    assert result.allowed_sell_amount == 100.0
    json.dumps(result.to_dict(), allow_nan=False)


@pytest.mark.parametrize("field", ["anomaly_severity", "msgam_clamp_limit", "msgam_stabilization_factor", "max_allowed_risk"])
@pytest.mark.parametrize("value", BAD)
@pytest.mark.parametrize("direct", [False, True])
def test_unified_rejects_malformed_numeric_evidence_before_permission(field, value, direct):
    op = UnifiedRuntimeOperator()
    request = op.preprocess({"anomaly_active": True, "msgam_risk_elevated": True})
    request[field] = value
    with pytest.raises(ValueError, match=field):
        op.execute(request) if direct else op.preprocess(request)


@pytest.mark.parametrize("field,value", [
    ("msgam_clamp_limit", -0.1), ("msgam_clamp_limit", 1.1),
    ("msgam_stabilization_factor", -0.1), ("msgam_stabilization_factor", 1.1),
    ("max_allowed_risk", -0.1), ("max_allowed_risk", 1.1),
    ("anomaly_severity", -1.0), ("anomaly_severity", 100.1), ("stress_level", 3),
    ("cycle_sequence_id", -1), ("cycle_sequence_id", 1.5), ("cycle_sequence_id", True),
    ("meta_execution_ready", "false"), ("enforce_strict_lock", "false"),
])
def test_unified_rejects_out_of_domain_policy_and_untyped_authority(field, value):
    with pytest.raises(ValueError, match=field):
        UnifiedRuntimeOperator().preprocess({field: value})


def test_unified_preserves_boundaries_optional_policy_and_valid_recovery():
    op = UnifiedRuntimeOperator()
    result = op.execute(op.preprocess({"msgam_risk_elevated": True,
        "msgam_clamp_limit": 0.0, "msgam_stabilization_factor": 1.0, "max_allowed_risk": 0.0}))
    assert result["recommended_execution_scale"] == 0.0
    assert result["execution_permitted"] is False
    optional = op.execute(op.preprocess({"meta_execution_ready": False, "enforce_strict_lock": False}))
    assert optional["execution_permitted"] is False
    assert op.execute(op.preprocess({}))["execution_permitted"] is True
