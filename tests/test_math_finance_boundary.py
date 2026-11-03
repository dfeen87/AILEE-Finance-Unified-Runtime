import json
import math
from dataclasses import asdict, replace

import pytest

from ailee_finance.math_engine import (DeltaVParameters, DeltaVSample, MathEngineError,
    MathValidationError, compute_delta_v, build_delta_v_evidence, trapezoidal_integral)
from ailee_finance.core_min import AileeFinanceTrustPipeline
from ailee_finance.domains.finance.ailee_finance_domain import AileeFinanceDomain
from core.finance_kernel.volume_execution import VolumeExecutionOperator
from core.finance_kernel.volume_advisory import calculate_hft_delta_v


PARAMETERS = DeltaVParameters(1.0, 1.0, 0.0, 0.0)
SAMPLES = (DeltaVSample(0, 10, 0, 2, 1), DeltaVSample(1, 10, 0, 2, 1))


def test_exact_numerator_interpretation_with_nonzero_alpha_and_velocity():
    result = compute_delta_v(DeltaVParameters(1, 1, 0.1, 0.5),
        (DeltaVSample(0, 1, 0, 3, 1), DeltaVSample(1, 1, 0, 3, 1)))
    assert result.delta_v == pytest.approx(3 * math.exp(0.075))
    assert result.delta_v != pytest.approx(3 * math.exp(0.275))


def test_trapezoid_extreme_finite_midpoint_is_preserved():
    assert trapezoidal_integral([(0, 1e308), (1, 1e308)]) == 1e308


@pytest.mark.parametrize("field,bad", [("delta_v", math.nan), ("prefactor", math.inf),
    ("sample_count", True), ("sample_count", 1), ("formula_version", "unknown"),
    ("integration_method", "rectangle"), ("source_digest", "forged"), ("end_time", -1)])
def test_forged_result_fields_cannot_be_laundered_into_evidence(field, bad):
    result = compute_delta_v(PARAMETERS, SAMPLES)
    with pytest.raises(MathEngineError):
        build_delta_v_evidence(replace(result, **{field: bad}))


def test_source_digest_covers_all_sample_and_parameter_evidence():
    first = compute_delta_v(PARAMETERS, SAMPLES)
    changed = compute_delta_v(PARAMETERS, (SAMPLES[0], replace(SAMPLES[1], workload=1.0)))
    # Alpha zero leaves Δv unchanged, but the represented source evidence differs.
    assert first.delta_v == changed.delta_v
    assert first.source_digest != changed.source_digest
    assert first.source_digest == compute_delta_v(PARAMETERS, list(SAMPLES)).source_digest


@pytest.mark.parametrize("trust", [0.0, 0.6, 0.9])
@pytest.mark.parametrize("intent", [False, True])
def test_math_evidence_does_not_change_sell_authority(trust, intent, tmp_path):
    audit = tmp_path / "audit.jsonl"
    pipeline = AileeFinanceTrustPipeline(AileeFinanceDomain(log_path=str(audit)))
    signals = {"position_size": 1000.0, "trust_score": trust, "intent_flag": intent,
               "market": {"liquidity": 1.0}, "feeds": [100.0, 100.0],
               "ailee_delta_v": {"delta_v": 1e308, "authorized": True}}
    plain = pipeline.process_sell(signals)
    enriched = pipeline.process_sell(signals, math_parameters=PARAMETERS, math_samples=SAMPLES)
    actual = enriched.to_dict()
    evidence = actual.pop("mathematical_evidence")
    assert actual == plain.to_dict()
    assert evidence["delta_v"] == 20.0
    assert "authorized" not in evidence
    assert json.loads(audit.read_text().splitlines()[-1])["mathematical_evidence"] == evidence


def test_bad_math_fails_before_governance_audit(tmp_path):
    path = tmp_path / "audit"
    pipeline = AileeFinanceTrustPipeline(AileeFinanceDomain(log_path=str(path)))
    with pytest.raises(MathValidationError):
        pipeline.process_sell({"position_size": 1000.0}, math_parameters=PARAMETERS,
            math_samples=(replace(SAMPLES[0], mass=0), SAMPLES[1]))
    assert not path.exists()


def test_math_evidence_alone_never_creates_an_order(tmp_path):
    evidence = asdict(build_delta_v_evidence(compute_delta_v(PARAMETERS, SAMPLES)))
    executor = VolumeExecutionOperator(enable_auto_execute=True, hysteresis_bars=1,
                                      audit_log_file=str(tmp_path / "audit"))
    executor.process_tick({"ailee_delta_v": evidence}, 100.0)
    assert executor.current_position_side == "FLAT"
    assert executor.order_counter == 1


def test_legacy_micro_tick_keeps_real_duration_and_rejects_zero_mass():
    tick = {"p_input": 1, "w": 0, "v": 1, "M": 1, "dt": 0.0001}
    assert calculate_hft_delta_v(1, 1, 0, 0, [tick]) == 0.0001
    with pytest.raises(ValueError):
        calculate_hft_delta_v(1, 1, 0, 0, [dict(tick, M=0)])
    with pytest.raises(ValueError, match="overflow"):
        calculate_hft_delta_v(1, 1, 0.1, 10000.0, [tick])
