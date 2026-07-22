# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Layer 11 — Deterministic Portfolio-Wide Constraint Engine."""

from core.finance_kernel.arbitration_layer import AssetId

# ============================================================================
# VERSIONING & REGISTRY TAGS
# ============================================================================

PORTFOLIO_CONSTRAINTS_V1 = "PORTFOLIO_CONSTRAINTS_V1"
SECTOR_CAPS_V1           = "SECTOR_CAPS_V1"
CORRELATION_DAMPENING_V1 = "CORRELATION_DAMPENING_V1"
RISK_BUDGET_V1           = "RISK_BUDGET_V1"

# ============================================================================
# ENUMS
# ============================================================================

class SectorId:
    FIAT             = 0
    CRYPTO           = 1
    PRECIOUS_METALS  = 2
    ENERGY           = 3
    OTHER            = 4

class ConstraintStage:
    EXPOSURE_CLAMP        = 1
    SECTOR_CAP_ENFORCE    = 2
    CORRELATION_DAMPEN    = 3
    RISK_BUDGET_ENFORCE   = 4

class ConstraintAction:
    NO_CHANGE             = 0
    CLAMPED               = 1
    DAMPENED              = 2
    SCALED                = 3

# ============================================================================
# STRUCTS / DATA CLASSES
# ============================================================================

class ConstraintRule:
    def __init__(self, asset_id: int, max_long_exposure: float, max_short_exposure: float, is_active: bool = True):
        self.asset_id = int(asset_id)
        self.max_long_exposure = float(max_long_exposure)
        self.max_short_exposure = float(max_short_exposure)
        self.is_active = bool(is_active)

class SectorDefinition:
    def __init__(self, sector_id: int, max_sector_exposure: float, sector_name: str, is_active: bool = True):
        self.sector_id = int(sector_id)
        self.max_sector_exposure = float(max_sector_exposure)
        self.sector_name = str(sector_name)
        self.is_active = bool(is_active)

class CorrelationProfile:
    def __init__(self, asset_a: int, asset_b: int, correlation_score: float, is_active: bool = True):
        self.asset_a = int(asset_a)
        self.asset_b = int(asset_b)
        self.correlation_score = float(correlation_score)
        self.is_active = bool(is_active)

class RiskBudget:
    def __init__(self, max_portfolio_risk: float, is_active: bool = True):
        self.max_portfolio_risk = float(max_portfolio_risk)
        self.is_active = bool(is_active)

class AssetAllocation:
    def __init__(self, asset_id: int, allocation: float, risk_score: float):
        self.asset_id = int(asset_id)
        self.allocation = float(allocation)
        self.risk_score = float(risk_score)

class ConstraintTraceStep:
    def __init__(self, asset_id: int, stage: int, action_taken: int, before_value: float, after_value: float, log: str):
        self.asset_id = int(asset_id)
        self.stage = int(stage)
        self.action_taken = int(action_taken)
        self.before_value = float(before_value)
        self.after_value = float(after_value)
        self.log = str(log)

class ConstraintResultSummary:
    def __init__(self, initial_portfolio_risk: float = 0.0, final_portfolio_risk: float = 0.0, trace_count: int = 0, remaining_violations: int = 0, max_risk_budget: float = 0.0):
        self.initial_portfolio_risk = float(initial_portfolio_risk)
        self.final_portfolio_risk = float(final_portfolio_risk)
        self.trace_count = int(trace_count)
        self.remaining_violations = int(remaining_violations)
        self.max_risk_budget = float(max_risk_budget)

class PortfolioConstraintResult:
    def __init__(self):
        self.allocations = []
        self.summary = ConstraintResultSummary()
        self.trace_steps = []

# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

def map_asset_to_sector(asset_id: int) -> int:
    if asset_id == AssetId.CASH:
        return SectorId.FIAT
    elif asset_id in (AssetId.BTC, AssetId.ETH):
        return SectorId.CRYPTO
    elif asset_id == AssetId.GOLD:
        return SectorId.PRECIOUS_METALS
    elif asset_id == AssetId.OIL:
        return SectorId.ENERGY
    else:
        return SectorId.OTHER

# ============================================================================
# CORE ALGORITHM
# ============================================================================

def apply_portfolio_constraints(
    proposed_allocs: list,
    rules: list,
    sectors: list,
    correlations: list,
    budget: RiskBudget
) -> PortfolioConstraintResult:
    """Pure functional deterministic portfolio-wide constraints matching C++ exactly."""
    result = PortfolioConstraintResult()
    result.allocations = [
        AssetAllocation(a.asset_id, a.allocation, a.risk_score) for a in proposed_allocs
    ]

    def add_trace_step(asset_id: int, stage: int, action: int, before: float, after: float, msg: str):
        result.trace_steps.append(
            ConstraintTraceStep(asset_id, stage, action, before, after, msg)
        )

    # Initial risk calculation
    initial_risk = sum(abs(a.allocation) * a.risk_score for a in result.allocations)
    result.summary.initial_portfolio_risk = initial_risk

    # Stage 1: Max-Exposure Clamping (Per Asset, Per Direction)
    for alloc in result.allocations:
        orig_val = alloc.allocation
        rule = None
        for r in rules:
            if r.asset_id == alloc.asset_id and r.is_active:
                rule = r
                break

        if rule:
            if alloc.allocation >= 0.0:
                if alloc.allocation > rule.max_long_exposure:
                    alloc.allocation = rule.max_long_exposure
                    add_trace_step(alloc.asset_id, ConstraintStage.EXPOSURE_CLAMP, ConstraintAction.CLAMPED, orig_val, alloc.allocation, "Max long exposure clamp")
                else:
                    add_trace_step(alloc.asset_id, ConstraintStage.EXPOSURE_CLAMP, ConstraintAction.NO_CHANGE, orig_val, alloc.allocation, "Within exposure limit")
            else:
                abs_alloc = abs(alloc.allocation)
                if abs_alloc > rule.max_short_exposure:
                    alloc.allocation = -rule.max_short_exposure
                    add_trace_step(alloc.asset_id, ConstraintStage.EXPOSURE_CLAMP, ConstraintAction.CLAMPED, orig_val, alloc.allocation, "Max short exposure clamp")
                else:
                    add_trace_step(alloc.asset_id, ConstraintStage.EXPOSURE_CLAMP, ConstraintAction.NO_CHANGE, orig_val, alloc.allocation, "Within short exposure limit")
        else:
            add_trace_step(alloc.asset_id, ConstraintStage.EXPOSURE_CLAMP, ConstraintAction.NO_CHANGE, orig_val, alloc.allocation, "No exposure rule")

    # Stage 2: Sector Caps Enforcement
    for sec_def in sectors:
        if not sec_def.is_active:
            continue

        current_sector = sec_def.sector_id
        sector_alloc_sum = sum(
            abs(a.allocation) for a in result.allocations if map_asset_to_sector(a.asset_id) == current_sector
        )

        if sector_alloc_sum > sec_def.max_sector_exposure and sector_alloc_sum > 0.0:
            scale = sec_def.max_sector_exposure / sector_alloc_sum
            msg = f"Sector {sec_def.sector_name} cap breach"
            for alloc in result.allocations:
                if map_asset_to_sector(alloc.asset_id) == current_sector:
                    before = alloc.allocation
                    alloc.allocation *= scale
                    add_trace_step(alloc.asset_id, ConstraintStage.SECTOR_CAP_ENFORCE, ConstraintAction.CLAMPED, before, alloc.allocation, msg)
        else:
            for alloc in result.allocations:
                if map_asset_to_sector(alloc.asset_id) == current_sector:
                    add_trace_step(alloc.asset_id, ConstraintStage.SECTOR_CAP_ENFORCE, ConstraintAction.NO_CHANGE, alloc.allocation, alloc.allocation, "Sector cap pass")

    # Stage 3: Pairwise Correlation Dampening
    CORR_CLUSTER_THRESHOLD = 0.70
    CORR_CLUSTER_MAX_ALLOCATION = 0.30

    cluster_labels = {i: i for i in range(len(result.allocations))}

    for profile in correlations:
        if not profile.is_active or profile.correlation_score <= CORR_CLUSTER_THRESHOLD:
            continue

        idx_a, idx_b = -1, -1
        for i, alloc in enumerate(result.allocations):
            if alloc.asset_id == profile.asset_a:
                idx_a = i
            if alloc.asset_id == profile.asset_b:
                idx_b = i

        if idx_a != -1 and idx_b != -1:
            old_label = cluster_labels[idx_b]
            new_label = cluster_labels[idx_a]
            if old_label != new_label:
                for k in cluster_labels:
                    if cluster_labels[k] == old_label:
                        cluster_labels[k] = new_label

    cluster_sums = {}
    cluster_sizes = {}
    for i, alloc in enumerate(result.allocations):
        label = cluster_labels[i]
        cluster_sums[label] = cluster_sums.get(label, 0.0) + abs(alloc.allocation)
        cluster_sizes[label] = cluster_sizes.get(label, 0) + 1

    for i, alloc in enumerate(result.allocations):
        label = cluster_labels[i]
        if cluster_sizes[label] > 1 and cluster_sums[label] > CORR_CLUSTER_MAX_ALLOCATION:
            scale = CORR_CLUSTER_MAX_ALLOCATION / cluster_sums[label]
            before = alloc.allocation
            alloc.allocation *= scale
            add_trace_step(alloc.asset_id, ConstraintStage.CORRELATION_DAMPEN, ConstraintAction.DAMPENED, before, alloc.allocation, "Correlation cluster dampened")
        else:
            add_trace_step(alloc.asset_id, ConstraintStage.CORRELATION_DAMPEN, ConstraintAction.NO_CHANGE, alloc.allocation, alloc.allocation, "Correlation cluster pass")

    # Stage 4: Risk-Budget Enforcement
    current_risk = sum(abs(a.allocation) * a.risk_score for a in result.allocations)

    if budget.is_active and current_risk > budget.max_portfolio_risk and current_risk > 0.0:
        scale = budget.max_portfolio_risk / current_risk
        for alloc in result.allocations:
            if alloc.asset_id != AssetId.CASH:
                before = alloc.allocation
                alloc.allocation *= scale
                add_trace_step(alloc.asset_id, ConstraintStage.RISK_BUDGET_ENFORCE, ConstraintAction.SCALED, before, alloc.allocation, "Risk budget scaling applied")
            else:
                add_trace_step(alloc.asset_id, ConstraintStage.RISK_BUDGET_ENFORCE, ConstraintAction.NO_CHANGE, alloc.allocation, alloc.allocation, "Cash excluded from scaling")
    else:
        for alloc in result.allocations:
            add_trace_step(alloc.asset_id, ConstraintStage.RISK_BUDGET_ENFORCE, ConstraintAction.NO_CHANGE, alloc.allocation, alloc.allocation, "Risk budget pass")

    # Calculate final portfolio risk
    final_risk = sum(abs(a.allocation) * a.risk_score for a in result.allocations)
    result.summary.final_portfolio_risk = final_risk
    result.summary.trace_count = len(result.trace_steps)
    result.summary.max_risk_budget = float(budget.max_portfolio_risk) if budget.is_active else 999999.0
    result.summary.remaining_violations = 1 if (budget.is_active and final_risk > budget.max_portfolio_risk) else 0

    return result
