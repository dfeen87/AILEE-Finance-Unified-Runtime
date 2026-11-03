"""Non-authoritative workflow evidence derived from delta-v results."""

from copy import deepcopy
from dataclasses import asdict, dataclass
import re
import math
from typing import Any, Dict, Mapping, Optional

from .models import DeltaVResult, FORMULA_VERSION, INTEGRATION_METHOD, MAX_SAMPLES
from .validation import MathValidationError, finite_number

EVIDENCE_CONTEXT_KEY = "ailee_delta_v"


@dataclass(frozen=True)
class DeltaVEvidence:
    delta_v: float
    formula_version: str
    integration_method: str
    sample_count: int
    start_time: float
    end_time: float
    source_digest: str


def validate_delta_v_evidence(evidence):
    if not isinstance(evidence, Mapping):
        raise MathValidationError("mathematical evidence must be a mapping")
    expected = {"delta_v", "formula_version", "integration_method", "sample_count", "start_time", "end_time", "source_digest"}
    if set(evidence) != expected:
        raise MathValidationError("invalid mathematical evidence schema")
    for name in ("delta_v", "start_time", "end_time"):
        finite_number(evidence[name], name)
    if evidence["end_time"] <= evidence["start_time"]:
        raise MathValidationError("evidence time interval must increase")
    if type(evidence["sample_count"]) is not int or not 2 <= evidence["sample_count"] <= MAX_SAMPLES:
        raise MathValidationError("invalid evidence sample count")
    if evidence["formula_version"] != FORMULA_VERSION or evidence["integration_method"] != INTEGRATION_METHOD:
        raise MathValidationError("unknown mathematical evidence contract")
    if not isinstance(evidence["source_digest"], str) or re.fullmatch("[0-9a-f]{64}", evidence["source_digest"]) is None:
        raise MathValidationError("invalid mathematical source digest")
    return dict(evidence)


def build_delta_v_evidence(result: DeltaVResult) -> DeltaVEvidence:
    if not isinstance(result, DeltaVResult):
        raise MathValidationError("result must be a DeltaVResult instance")
    integral = finite_number(result.integral_value, "integral_value")
    prefactor = finite_number(result.prefactor, "prefactor")
    delta_v = finite_number(result.delta_v, "delta_v")
    if prefactor < 0 or not math.isfinite(prefactor * integral) or delta_v != prefactor * integral:
        raise MathValidationError("inconsistent mathematical result")
    evidence = DeltaVEvidence(
        delta_v=result.delta_v,
        formula_version=result.formula_version,
        integration_method=result.integration_method,
        sample_count=result.sample_count,
        start_time=result.start_time,
        end_time=result.end_time,
        source_digest=result.source_digest,
    )
    validate_delta_v_evidence(asdict(evidence))
    return evidence


def attach_delta_v_evidence(
    context: Optional[Mapping[str, Any]], result: DeltaVResult
) -> Dict[str, Any]:
    """Return a deep snapshot with compact evidence; never mutate ``context``."""
    if context is not None and not isinstance(context, Mapping):
        raise MathValidationError("context must be a mapping or None")
    snapshot = deepcopy(dict(context or {}))
    snapshot[EVIDENCE_CONTEXT_KEY] = asdict(build_delta_v_evidence(result))
    return snapshot
