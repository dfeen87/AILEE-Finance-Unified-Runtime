# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Layer 10 — Multi‑Governor Reconciliation Engine (Finance Runtime Kernel)."""

# ============================================================================
# VERSIONING & CONSTANTS
# ============================================================================

GOVERNOR_LADDER_V1 = "GOVERNOR_LADDER_V1"
OVERRIDE_MATRIX_V1 = "OVERRIDE_MATRIX_V1"
GLOBAL_ASSET_ID = 0xFFFF

# ============================================================================
# ENUMS
# ============================================================================

class GovernorType:
    COMPLIANCE = 0
    RISK       = 1
    LIQUIDITY  = 2
    RETURN     = 3
    STRATEGY   = 4

class ReconciliationFlags:
    NONE       = 0
    HARD_BLOCK = 1
    RISK_LIMIT = 2
    VOL_CLAMP  = 4

# ============================================================================
# DATA CLASSES / STRUCTS
# ============================================================================

class GovernorProposal:
    def __init__(self, asset_id: int, governor_type: int, proposed_value: float, risk_score: float = 0.0, flags: int = 0):
        self.asset_id = int(asset_id)
        self.governor_type = int(governor_type)
        self.proposed_value = float(proposed_value)
        self.risk_score = float(risk_score)
        self.flags = int(flags)

class GovernorDecision:
    def __init__(self, asset_id: int = 0, flags_applied: int = 0, resolved_type: int = 4, final_value: float = 0.0):
        self.asset_id = int(asset_id)
        self.flags_applied = int(flags_applied)
        self.resolved_type = int(resolved_type)
        self.final_value = float(final_value)

class ReconciliationTraceStep:
    def __init__(self, asset_id: int, governor_type: int, action_taken: int, proposed_value: float, interim_value: float, log: str):
        self.asset_id = int(asset_id)
        self.governor_type = int(governor_type)
        self.action_taken = int(action_taken)
        self.proposed_value = float(proposed_value)
        self.interim_value = float(interim_value)
        self.log = str(log)

class ReconciliationResidual:
    def __init__(self, asset_id: int = 0, residual_value: float = 0.0):
        self.asset_id = int(asset_id)
        self.residual_value = float(residual_value)

class ReconciledResultSummary:
    def __init__(self, total_residual: float = 0.0, trace_count: int = 0):
        self.total_residual = float(total_residual)
        self.trace_count = int(trace_count)

class ReconciledResult:
    def __init__(self):
        self.decision = GovernorDecision()
        self.residual = ReconciliationResidual()
        self.summary = ReconciledResultSummary()
        self.trace_steps = []

# ============================================================================
# CORE RECONCILIATION FUNCTION
# ============================================================================

def get_governor_weight(gov_type: int) -> float:
    weights = {
        GovernorType.COMPLIANCE: 1.0,
        GovernorType.RISK:       0.8,
        GovernorType.LIQUIDITY:  0.6,
        GovernorType.RETURN:     0.5,
        GovernorType.STRATEGY:   0.5
    }
    return weights.get(gov_type, 0.0)

def reconcile_governors(proposals: list, asset_id: int) -> ReconciledResult:
    """Pure functional Multi-Governor Reconciliation matching C++ exactly."""
    result = ReconciledResult()
    result.decision.asset_id = asset_id
    result.residual.asset_id = asset_id

    if not proposals:
        result.decision.resolved_type = GovernorType.STRATEGY
        result.decision.final_value = 0.0
        return result

    # Find relevant proposals for this asset
    comp_prop = None
    risk_prop = None
    liq_prop = None
    ret_prop = None
    strat_prop = None

    for prop in proposals:
        if prop.asset_id != asset_id:
            continue
        g_type = prop.governor_type
        if g_type == GovernorType.COMPLIANCE:
            comp_prop = prop
        elif g_type == GovernorType.RISK:
            risk_prop = prop
        elif g_type == GovernorType.LIQUIDITY:
            liq_prop = prop
        elif g_type == GovernorType.RETURN:
            ret_prop = prop
        elif g_type == GovernorType.STRATEGY:
            strat_prop = prop

    current_val = 0.0
    has_active_base = False
    resolved_type = GovernorType.STRATEGY
    flags_applied = ReconciliationFlags.NONE

    def add_trace_step(gov_type: int, action: int, proposed: float, interim: float, msg: str):
        result.trace_steps.append(
            ReconciliationTraceStep(asset_id, gov_type, action, proposed, interim, msg)
        )

    # 1. STRATEGY (Base level proposal)
    if strat_prop:
        current_val = strat_prop.proposed_value
        has_active_base = True
        resolved_type = GovernorType.STRATEGY
        add_trace_step(GovernorType.STRATEGY, 0, strat_prop.proposed_value, current_val, "Base strategy proposal")

    # 2. RETURN
    if ret_prop:
        proposed = ret_prop.proposed_value
        if not has_active_base:
            current_val = proposed
            has_active_base = True
            resolved_type = GovernorType.RETURN
            add_trace_step(GovernorType.RETURN, 0, proposed, current_val, "Return primary proposal")
        else:
            current_val = proposed
            resolved_type = GovernorType.RETURN
            add_trace_step(GovernorType.RETURN, 0, proposed, current_val, "Return override/refinement")

    # 3. LIQUIDITY (Magnitude limit clamp)
    if liq_prop:
        proposed_limit = liq_prop.proposed_value
        if has_active_base:
            abs_val = abs(current_val)
            if abs_val > proposed_limit:
                sign = 1.0 if current_val >= 0.0 else -1.0
                current_val = sign * proposed_limit
                flags_applied |= ReconciliationFlags.VOL_CLAMP
                add_trace_step(GovernorType.LIQUIDITY, 2, proposed_limit, current_val, "Liquidity magnitude clamp")
            else:
                add_trace_step(GovernorType.LIQUIDITY, 0, proposed_limit, current_val, "Liquidity limit pass")
        else:
            current_val = proposed_limit
            has_active_base = True
            resolved_type = GovernorType.LIQUIDITY
            add_trace_step(GovernorType.LIQUIDITY, 0, proposed_limit, current_val, "Liquidity base proposal")

    # 4. RISK
    if risk_prop:
        proposed_risk_val = risk_prop.proposed_value
        if risk_prop.risk_score > 75.0:
            if has_active_base:
                if abs(current_val) > abs(proposed_risk_val):
                    current_val = proposed_risk_val
                    resolved_type = GovernorType.RISK
                    flags_applied |= ReconciliationFlags.RISK_LIMIT
                    add_trace_step(GovernorType.RISK, 1, proposed_risk_val, current_val, "Risk severe threshold clamp")
                else:
                    add_trace_step(GovernorType.RISK, 0, proposed_risk_val, current_val, "Risk limits within threshold")
            else:
                current_val = proposed_risk_val
                has_active_base = True
                resolved_type = GovernorType.RISK
                add_trace_step(GovernorType.RISK, 0, proposed_risk_val, current_val, "Risk primary proposal")
        else:
            if risk_prop.flags & ReconciliationFlags.RISK_LIMIT:
                if abs(current_val) > abs(proposed_risk_val):
                    current_val = proposed_risk_val
                    flags_applied |= ReconciliationFlags.RISK_LIMIT
                    add_trace_step(GovernorType.RISK, 2, proposed_risk_val, current_val, "Risk flag limit clamp")
                else:
                    add_trace_step(GovernorType.RISK, 0, proposed_risk_val, current_val, "Risk flags pass")
            else:
                add_trace_step(GovernorType.RISK, 0, proposed_risk_val, current_val, "Risk moderate status pass")

    # 5. COMPLIANCE
    if comp_prop:
        if comp_prop.flags & ReconciliationFlags.HARD_BLOCK:
            current_val = 0.0
            resolved_type = GovernorType.COMPLIANCE
            flags_applied |= ReconciliationFlags.HARD_BLOCK
            add_trace_step(GovernorType.COMPLIANCE, 3, comp_prop.proposed_value, current_val, "Compliance HARD_BLOCK triggered")
        else:
            add_trace_step(GovernorType.COMPLIANCE, 0, comp_prop.proposed_value, current_val, "Compliance pass")

    result.decision.final_value = current_val
    result.decision.resolved_type = resolved_type
    result.decision.flags_applied = flags_applied

    # Calculate Residual
    total_residual = 0.0
    for prop in proposals:
        if prop.asset_id != asset_id:
            continue
        w = get_governor_weight(prop.governor_type)
        diff = abs(prop.proposed_value - current_val)
        total_residual += w * diff

    result.residual.residual_value = total_residual
    result.summary.total_residual = total_residual
    result.summary.trace_count = len(result.trace_steps)

    return result
