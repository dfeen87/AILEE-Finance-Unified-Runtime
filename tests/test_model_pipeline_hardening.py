"""Independent model quorum, numeric ingress and atomic history regressions."""
import math
import sys
from copy import deepcopy

import pytest

from simulations.aille_simulation import AILLEConfig, AILLEEngine, ModelSignal, check_consensus
from core.finance_kernel.kernel_registry import GovernorOperator, LedgerOperator, RiskOperator, TransactionOperator


@pytest.mark.parametrize("field", ["min_confidence_threshold", "grace_confidence_threshold",
    "sign_agreement_threshold", "fallback_position_scale", "fallback_alpha", "fallback_beta"])
@pytest.mark.parametrize("value", [math.nan, math.inf, -math.inf, True, "0.5"])
def test_model_config_rejects_malformed_numeric_policy(field, value):
    with pytest.raises(ValueError):
        AILLEConfig(**{field: value})


@pytest.mark.parametrize("field,value", [("fallback_window_size", 0), ("fallback_window_size", True),
    ("min_models_required", 0), ("min_models_required", 11), ("max_model_count", 0),
    ("enable_dynamic_fallback", "false"), ("fallback_position_scale", -1.0),
    ("fallback_position_scale", 1.1), ("fallback_alpha", -1.0), ("fallback_beta", -1.0)])
def test_model_config_enforces_domains_and_reachable_quorum(field, value):
    with pytest.raises(ValueError):
        AILLEConfig(**{field: value})


@pytest.mark.parametrize("field", ["value", "confidence"])
@pytest.mark.parametrize("value", [math.nan, math.inf, -math.inf, True, "0.9", 10**400])
def test_model_ingress_rejects_before_history_mutation(field, value):
    engine = AILLEEngine()
    engine.make_decision([ModelSignal(0.1, 0.9, 0), ModelSignal(0.1, 0.9, 1)])
    previous = deepcopy((engine.fallback_buffer, engine.confidence_buffer))
    signal = ModelSignal(0.1, 0.9, 2)
    setattr(signal, field, value)
    with pytest.raises(ValueError):
        engine.make_decision([signal, ModelSignal(0.1, 0.9, 3)])
    assert (engine.fallback_buffer, engine.confidence_buffer) == previous


@pytest.mark.parametrize("operator", [RiskOperator, GovernorOperator, TransactionOperator])
def test_operator_does_not_normalize_boolean_or_duplicate_identity(operator):
    for signals in ([{"value": True, "confidence": 0.9, "model_id": 1},
                     {"value": 0.1, "confidence": 0.9, "model_id": 2}],
                    [{"value": 0.1, "confidence": 0.9, "model_id": 1},
                     {"value": 0.1, "confidence": 0.9, "model_id": 1}]):
        with pytest.raises(ValueError):
            operator().execute({"signals": signals})


def test_direct_consensus_enforces_independent_trusted_quorum():
    with pytest.raises(ValueError):
        check_consensus([ModelSignal(0.1, 0.9, 1)] * 2, AILLEConfig())
    assert check_consensus([ModelSignal(0.1, 0.01, 1), ModelSignal(0.1, 0.01, 2)], AILLEConfig()) is None


def test_extreme_finite_consensus_keeps_real_mean_and_bounded_output():
    signals = [ModelSignal(sys.float_info.max, 0.9, 0), ModelSignal(sys.float_info.max, 0.9, 1)]
    assert check_consensus(signals, AILLEConfig()) == (sys.float_info.max, 2)
    decision = AILLEEngine().make_decision(signals)
    assert decision.final_value == 1.0
    assert math.isfinite(decision.confidence)


def test_mutable_config_is_revalidated_atomically():
    cfg = AILLEConfig()
    engine = AILLEEngine(cfg)
    signals = [ModelSignal(0.1, 0.9, 0), ModelSignal(0.1, 0.9, 1)]
    engine.make_decision(signals)
    previous = deepcopy((engine.fallback_buffer, engine.confidence_buffer))
    cfg.fallback_position_scale = math.inf
    with pytest.raises(ValueError):
        engine.make_decision(signals)
    assert (engine.fallback_buffer, engine.confidence_buffer) == previous
