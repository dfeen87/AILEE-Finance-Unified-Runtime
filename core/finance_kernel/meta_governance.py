# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Layer 14 — Deterministic Meta‑Governance Lock."""

from typing import Tuple, List

# ============================================================================
# VERSIONING & REGISTRY TAGS
# ============================================================================

META_GOVERNANCE_LOCK_V1 = "META_GOVERNANCE_LOCK_V1"

# ============================================================================
# CONSTANTS
# ============================================================================

META_GOVERNANCE_RESIDUAL_THRESHOLD = 0.05
TEMPORAL_RESIDUAL_CRITICAL_THRESHOLD = 0.10

# ============================================================================
# REASON CODES (Bitmask)
# ============================================================================

class MetaGovernanceReason:
    GOVERNOR_CONFLICT         = 1
    CONSTRAINT_VIOLATION      = 2
    STRESS_OVERRIDE_MISSING   = 4
    TEMPORAL_INCONSISTENT     = 8

# ============================================================================
# CORE CLASSES / STRUCTS
# ============================================================================

class MetaGovernanceState:
    def __init__(
        self,
        final_portfolio_risk: float = 0.0,
        final_residual_sum: float = 0.0,
        final_stress_level: int = 0,
        execution_ready: int = 0
    ):
        self.final_portfolio_risk = float(final_portfolio_risk)
        self.final_residual_sum = float(final_residual_sum)
        self.final_stress_level = int(final_stress_level)
        self.execution_ready = int(execution_ready)


class MetaGovernanceTraceStep:
    def __init__(self, reason_code: int, log: str):
        self.reason_code = int(reason_code)
        self.log = str(log)


# ============================================================================
# CORE ALGORITHM
# ============================================================================

def apply_meta_governance_lock(
    decision,
    constraints,
    stress_state,
    stress_trace,
    temporal_state
) -> Tuple[MetaGovernanceState, List[MetaGovernanceTraceStep]]:
    """Pure functional deterministic meta-governance lock matching C++ exactly."""
    trace_steps = []

    # 1. Resolve summaries/fields based on object types or dicts
    total_residual = 0.0
    if hasattr(decision, "summary"):
        total_residual = decision.summary.total_residual
    elif isinstance(decision, dict) and "summary" in decision:
        total_residual = decision["summary"].get("total_residual", 0.0)
    elif isinstance(decision, dict):
        total_residual = decision.get("total_residual", 0.0)

    # Constraints summary
    remaining_violations = 0
    final_portfolio_risk = 0.0
    max_risk_budget = 0.0
    if hasattr(constraints, "summary"):
        remaining_violations = getattr(constraints.summary, "remaining_violations", 0)
        final_portfolio_risk = constraints.summary.final_portfolio_risk
        max_risk_budget = getattr(constraints.summary, "max_risk_budget", 0.0)
    elif isinstance(constraints, dict) and "summary" in constraints:
        summary_dict = constraints["summary"]
        remaining_violations = summary_dict.get("remaining_violations", 0)
        final_portfolio_risk = summary_dict.get("final_portfolio_risk", 0.0)
        max_risk_budget = summary_dict.get("max_risk_budget", 0.0)

    # Stress state summary
    stress_level = 0
    if hasattr(stress_state, "stress_level"):
        stress_level = stress_state.stress_level
    elif isinstance(stress_state, dict):
        stress_level = stress_state.get("stress_level", 0)

    # Stress trace
    stress_trace_count = 0
    if hasattr(stress_trace, "count"):
        stress_trace_count = stress_trace.count
    elif hasattr(stress_trace, "steps"):
        stress_trace_count = len(stress_trace.steps)
    elif isinstance(stress_trace, (list, tuple)):
        stress_trace_count = len(stress_trace)

    # Temporal state
    residual_sum = 0.0
    if hasattr(temporal_state, "residual_sum"):
        residual_sum = temporal_state.residual_sum
    elif isinstance(temporal_state, dict):
        residual_sum = temporal_state.get("residual_sum", 0.0)

    # 2. Meta-checks
    governor_conflict = (total_residual > META_GOVERNANCE_RESIDUAL_THRESHOLD)
    constraint_violation = (remaining_violations > 0 or final_portfolio_risk > max_risk_budget)
    stress_override_missing = (stress_level >= 1 and stress_trace_count == 0)
    temporal_inconsistent = (residual_sum > TEMPORAL_RESIDUAL_CRITICAL_THRESHOLD)

    # 3. Log trace steps
    if governor_conflict:
        trace_steps.append(MetaGovernanceTraceStep(
            MetaGovernanceReason.GOVERNOR_CONFLICT,
            "Governor conflict detected"
        ))
    if constraint_violation:
        trace_steps.append(MetaGovernanceTraceStep(
            MetaGovernanceReason.CONSTRAINT_VIOLATION,
            "Constraint violation detected"
        ))
    if stress_override_missing:
        trace_steps.append(MetaGovernanceTraceStep(
            MetaGovernanceReason.STRESS_OVERRIDE_MISSING,
            "Stress override missing"
        ))
    if temporal_inconsistent:
        trace_steps.append(MetaGovernanceTraceStep(
            MetaGovernanceReason.TEMPORAL_INCONSISTENT,
            "Temporal residual too high"
        ))

    execution_ready = not (
        governor_conflict or
        constraint_violation or
        stress_override_missing or
        temporal_inconsistent
    )

    if execution_ready:
        trace_steps.append(MetaGovernanceTraceStep(0, "Meta-governance lock verified & ready"))

    final_state = MetaGovernanceState(
        final_portfolio_risk=final_portfolio_risk,
        final_residual_sum=total_residual + residual_sum,
        final_stress_level=stress_level,
        execution_ready=1 if execution_ready else 0
    )

    return final_state, trace_steps
