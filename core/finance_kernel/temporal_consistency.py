# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Layer 12 — Deterministic Temporal Consistency Guard."""

# ============================================================================
# VERSIONING & REGISTRY TAGS
# ============================================================================

TEMPORAL_CONSISTENCY_V1 = "TEMPORAL_CONSISTENCY_V1"

# ============================================================================
# ENUMS
# ============================================================================

class TemporalAction:
    NO_CHANGE              = 0
    CLAMPED                = 1
    DAMPENED               = 2
    OSCILLATED_AND_CLAMPED  = 3

# ============================================================================
# CORE CLASSES / STRUCTS
# ============================================================================

class TemporalState:
    def __init__(self, asset_id: int, prev_allocation: float, prev_risk_score: float, prev_prev_allocation: float = 0.0, flags: int = 0):
        self.asset_id = int(asset_id)
        self.prev_allocation = float(prev_allocation)
        self.prev_risk_score = float(prev_risk_score)
        self.prev_prev_allocation = float(prev_prev_allocation)
        self.flags = int(flags)

class TemporalResidual:
    def __init__(self, asset_id: int, expected_allocation: float, actual_allocation: float, residual: float):
        self.asset_id = int(asset_id)
        self.expected_allocation = float(expected_allocation)
        self.actual_allocation = float(actual_allocation)
        self.residual = float(residual)

class TemporalTraceStep:
    def __init__(self, asset_id: int, action_taken: int, before_value: float, after_value: float, residual: float, log: str):
        self.asset_id = int(asset_id)
        self.action_taken = int(action_taken)
        self.before_value = float(before_value)
        self.after_value = float(after_value)
        self.residual = float(residual)
        self.log = str(log)

class TemporalPortfolioState:
    def __init__(self, portfolio_risk: float = 0.0, residual_sum: float = 0.0):
        self.portfolio_risk = float(portfolio_risk)
        self.residual_sum = float(residual_sum)

# ============================================================================
# CONTAINERS
# ============================================================================

class TemporalStates:
    def __init__(self, states: list = None):
        self.states = list(states) if states is not None else []

    @property
    def count(self) -> int:
        return len(self.states)

class TemporalResiduals:
    def __init__(self, residuals: list = None):
        self.residuals = list(residuals) if residuals is not None else []

    @property
    def count(self) -> int:
        return len(self.residuals)

class TemporalTraceSteps:
    def __init__(self, steps: list = None):
        self.steps = list(steps) if steps is not None else []

    @property
    def count(self) -> int:
        return len(self.steps)

# ============================================================================
# CORE ALGORITHM
# ============================================================================

def enforce_temporal_consistency(
    prev_states: TemporalStates,
    curr_states: TemporalStates,
    prev_portfolio: TemporalPortfolioState,
    curr_portfolio: TemporalPortfolioState,
    residuals: TemporalResiduals,
    trace: TemporalTraceSteps,
    max_drift_threshold: float = 0.05
) -> None:
    """Pure functional deterministic temporal consistency guard matching C++ exactly."""
    prev_list = prev_states.states if hasattr(prev_states, "states") else prev_states
    curr_list = curr_states.states if hasattr(curr_states, "states") else curr_states

    total_residual = 0.0
    current_portfolio_risk = 0.0

    residuals_list = []
    trace_list = []

    for curr_asset in curr_list:
        w_i_t = 0.0
        w_i_t_minus_1 = 0.0
        found_prev = False

        for prev_asset in prev_list:
            if prev_asset.asset_id == curr_asset.asset_id:
                w_i_t = prev_asset.prev_allocation
                w_i_t_minus_1 = prev_asset.prev_prev_allocation
                found_prev = True
                break

        original_proposed = curr_asset.prev_allocation
        current_val = original_proposed
        has_oscillation = False
        has_clamp = False

        if found_prev:
            delta_t = w_i_t - w_i_t_minus_1
            delta_t1 = current_val - w_i_t

            if (delta_t > 0.0001 and delta_t1 < -0.0001) or (delta_t < -0.0001 and delta_t1 > 0.0001):
                has_oscillation = True
                current_val = w_i_t + 0.5 * delta_t1
                curr_asset.flags |= 1
            else:
                curr_asset.flags &= ~1
        else:
            curr_asset.flags &= ~1

        drift = abs(current_val - w_i_t)
        if drift > max_drift_threshold:
            has_clamp = True
            if current_val > w_i_t:
                current_val = w_i_t + max_drift_threshold
            else:
                current_val = w_i_t - max_drift_threshold

        action = TemporalAction.NO_CHANGE
        log_msg = "No temporal change"

        if has_oscillation and has_clamp:
            action = TemporalAction.OSCILLATED_AND_CLAMPED
            log_msg = "Oscillated & clamped"
        elif has_oscillation:
            action = TemporalAction.DAMPENED
            log_msg = "Oscillation dampened"
        elif has_clamp:
            action = TemporalAction.CLAMPED
            log_msg = "Drift clamped"

        curr_asset.prev_allocation = current_val
        curr_asset.prev_prev_allocation = w_i_t

        final_residual = abs(current_val - w_i_t)
        total_residual += final_residual

        residuals_list.append(
            TemporalResidual(curr_asset.asset_id, w_i_t, current_val, final_residual)
        )

        trace_list.append(
            TemporalTraceStep(curr_asset.asset_id, action, original_proposed, current_val, final_residual, log_msg)
        )

        current_portfolio_risk += abs(current_val) * curr_asset.prev_risk_score

    curr_portfolio.residual_sum = total_residual
    curr_portfolio.portfolio_risk = current_portfolio_risk

    if hasattr(residuals, "residuals"):
        residuals.residuals = residuals_list
    else:
        residuals[:] = residuals_list

    if hasattr(trace, "steps"):
        trace.steps = trace_list
    else:
        trace[:] = trace_list
