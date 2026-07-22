# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Layer 13 — Deterministic Stress‑Regime Override."""

from core.finance_kernel.arbitration_layer import AssetId
from core.finance_kernel.portfolio_constraints import AssetAllocation

# ============================================================================
# VERSIONING & REGISTRY TAGS
# ============================================================================

STRESS_REGIME_OVERRIDE_V1 = "STRESS_REGIME_OVERRIDE_V1"

# ============================================================================
# ENUMS
# ============================================================================

class StressMode:
    NORMAL = 0
    STRESS = 1
    CRISIS = 2

# ============================================================================
# CORE CLASSES / STRUCTS
# ============================================================================

class StressOverrideRules:
    def __init__(
        self,
        volatility_threshold: float,
        drawdown_threshold: float,
        correlation_threshold: float,
        residual_threshold: float,
        crash_dampening_factor: float,
        fallback_lambda: float,
        mode: int = StressMode.NORMAL
    ):
        self.volatility_threshold = float(volatility_threshold)
        self.drawdown_threshold = float(drawdown_threshold)
        self.correlation_threshold = float(correlation_threshold)
        self.residual_threshold = float(residual_threshold)
        self.crash_dampening_factor = float(crash_dampening_factor)
        self.fallback_lambda = float(fallback_lambda)
        self.mode = int(mode)


class SafeBaseline:
    def __init__(self, asset_id: int, baseline_allocation: float, flags: int = 0):
        self.asset_id = int(asset_id)
        self.baseline_allocation = float(baseline_allocation)
        self.flags = int(flags)


class StressTraceStep:
    def __init__(self, asset_id: int, flags: int, original_allocation: float, adjusted_allocation: float):
        self.asset_id = int(asset_id)
        self.flags = int(flags)
        self.original_allocation = float(original_allocation)
        self.adjusted_allocation = float(adjusted_allocation)


class StressPortfolioState:
    def __init__(
        self,
        portfolio_risk: float,
        stress_index: float,
        volatility_index: float,
        drawdown_index: float,
        correlation_index: float,
        temporal_residual_sum: float,
        stress_level: int = StressMode.NORMAL
    ):
        self.portfolio_risk = float(portfolio_risk)
        self.stress_index = float(stress_index)
        self.volatility_index = float(volatility_index)
        self.drawdown_index = float(drawdown_index)
        self.correlation_index = float(correlation_index)
        self.temporal_residual_sum = float(temporal_residual_sum)
        self.stress_level = int(stress_level)


# ============================================================================
# CONTAINERS
# ============================================================================

class SafeBaselineContainer:
    def __init__(self, baselines: list = None):
        self.baselines = list(baselines) if baselines is not None else []

    @property
    def count(self) -> int:
        return len(self.baselines)


class StressTraceSteps:
    def __init__(self, steps: list = None):
        self.steps = list(steps) if steps is not None else []

    @property
    def count(self) -> int:
        return len(self.steps)


# ============================================================================
# CORE ALGORITHM
# ============================================================================

def apply_stress_regime_override(
    rules: StressOverrideRules,
    state: StressPortfolioState,
    prev_allocations: list,
    allocations: list,
    baselines: SafeBaselineContainer,
    trace: StressTraceSteps,
    normal_safety_failed: bool
) -> None:
    """Pure functional deterministic stress‑regime override matching C++ exactly."""

    # 1. Stress Regime Evaluation
    effective_mode = rules.mode

    exceeds_crisis_thresholds = (
        state.volatility_index > rules.volatility_threshold or
        state.drawdown_index > rules.drawdown_threshold or
        state.correlation_index > rules.correlation_threshold or
        state.temporal_residual_sum > rules.residual_threshold
    )

    exceeds_stress_thresholds = (
        state.volatility_index > 0.5 * rules.volatility_threshold or
        state.drawdown_index > 0.5 * rules.drawdown_threshold or
        state.correlation_index > 0.5 * rules.correlation_threshold or
        state.temporal_residual_sum > 0.5 * rules.residual_threshold
    )

    if state.stress_level == StressMode.CRISIS or exceeds_crisis_thresholds:
        effective_mode = StressMode.CRISIS
    elif state.stress_level == StressMode.STRESS or exceeds_stress_thresholds:
        effective_mode = StressMode.STRESS

    # 2. Check if exposure freeze is active
    # We use rules.volatility_threshold and rules.drawdown_threshold as the freeze triggers (at 0.5x threshold)
    exposure_freeze_active = (
        state.volatility_index > 0.5 * rules.volatility_threshold or
        state.drawdown_index > 0.5 * rules.drawdown_threshold
    )

    trace_list = []

    # Handle lists or wrapper containers
    prev_list = prev_allocations.allocations if hasattr(prev_allocations, "allocations") else prev_allocations
    curr_list = allocations.allocations if hasattr(allocations, "allocations") else allocations
    baselines_list = baselines.baselines if hasattr(baselines, "baselines") else baselines

    for alloc in curr_list:
        original = alloc.allocation
        current_val = original
        step_flags = 0

        # Retrieve previous allocation for the asset
        prev_alloc_val = 0.0
        found_prev = False
        for prev_alloc in prev_list:
            if prev_alloc.asset_id == alloc.asset_id:
                prev_alloc_val = prev_alloc.allocation
                found_prev = True
                break

        # Define risk-bearing asset conditions
        #   - Non-CASH asset
        #   - risk_score > 40.0
        is_risk_bearing = (alloc.asset_id != AssetId.CASH and alloc.risk_score > 40.0)

        # Stage A: Exposure Freeze
        if exposure_freeze_active and is_risk_bearing and found_prev:
            if current_val > prev_alloc_val:
                current_val = prev_alloc_val
                step_flags |= (1 << 1)  # bit 1: frozen

        # Stage B: Crash Dampening
        if (effective_mode in (StressMode.STRESS, StressMode.CRISIS)) and is_risk_bearing:
            current_val = current_val * rules.crash_dampening_factor
            step_flags |= (1 << 0)  # bit 0: dampened

        # Stage C: Fallback Compression
        if normal_safety_failed or effective_mode == StressMode.CRISIS:
            # Find baseline allocation
            baseline_val = 0.0
            for baseline in baselines_list:
                if baseline.asset_id == alloc.asset_id:
                    baseline_val = baseline.baseline_allocation
                    break
            current_val = (1.0 - rules.fallback_lambda) * current_val + rules.fallback_lambda * baseline_val
            step_flags |= (1 << 2)  # bit 2: fallback_applied

        # Apply final adjusted allocation
        alloc.allocation = current_val

        # Record trace step
        trace_list.append(
            StressTraceStep(alloc.asset_id, step_flags, original, current_val)
        )

    if hasattr(trace, "steps"):
        trace.steps = trace_list
    else:
        trace[:] = trace_list
