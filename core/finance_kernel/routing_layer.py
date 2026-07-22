# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Layer 9 — Deterministic Liquidity Routing (Finance Runtime Kernel)."""

import math
from core.finance_kernel.arbitration_layer import AssetId

# ============================================================================
# VERSIONING & REGISTRY TAGS
# ============================================================================

LIQUIDITY_ROUTING_V1 = "LIQUIDITY_ROUTING_V1"
LIQUIDITY_CAPS_V1 = "LIQUIDITY_CAPS_V1"
ROUTING_TABLE_V1 = "ROUTING_TABLE_V1"
SHOCK_BOUNDS_V1 = "SHOCK_BOUNDS_V1"

# ============================================================================
# STRESS LEVEL ENUM
# ============================================================================

class StressLevel:
    NORMAL = 0
    ELEVATED = 1
    CRISIS = 2

# ============================================================================
# STRUCTS / DATA CLASSES
# ============================================================================

class LiquidityCap:
    def __init__(self, asset_id: int, max_outflow_ratio: float, max_inflow_ratio: float, stress_level: int):
        self.asset_id = asset_id
        self.max_outflow_ratio = float(max_outflow_ratio)
        self.max_inflow_ratio = float(max_inflow_ratio)
        self.stress_level = int(stress_level)

class RoutingRule:
    def __init__(self, source: int, primary_target: int, fallback_target: int, stress_level: int, preferred_flow_ratio: float):
        self.source = source
        self.primary_target = primary_target
        self.fallback_target = fallback_target
        self.stress_level = int(stress_level)
        self.preferred_flow_ratio = float(preferred_flow_ratio)

class ShockBounds:
    def __init__(self, max_portfolio_liquidity_shift_per_step: float, max_asset_liquidity_shift_per_step: float):
        self.max_portfolio_liquidity_shift_per_step = float(max_portfolio_liquidity_shift_per_step)
        self.max_asset_liquidity_shift_per_step = float(max_asset_liquidity_shift_per_step)

class CrossAssetDecision:
    def __init__(self, asset_id: int, target_allocation_ratio: float, flags: int = 0):
        self.asset_id = asset_id
        self.target_allocation_ratio = float(target_allocation_ratio)
        self.flags = int(flags)

class StressProfile:
    def __init__(self, stress_level: int, volatility_index: float = 0.0, drawdown_index: float = 0.0, correlation_index: float = 0.0):
        self.stress_level = int(stress_level)
        self.volatility_index = float(volatility_index)
        self.drawdown_index = float(drawdown_index)
        self.correlation_index = float(correlation_index)

class LiquidityState:
    def __init__(self, asset_id: int, current_liquidity_value: float, current_allocation_ratio: float, flags: int = 0):
        self.asset_id = asset_id
        self.current_liquidity_value = float(current_liquidity_value)
        self.current_allocation_ratio = float(current_allocation_ratio)
        self.flags = int(flags)

class LiquidityFlow:
    def __init__(self, source: int, target: int, amount: float):
        self.source = source
        self.target = target
        self.amount = float(amount)

class RoutingTraceStep:
    def __init__(self, source: int, target: int, stress_level: int, proposed_flow: float, actual_flow: float, log: str):
        self.source = source
        self.target = target
        self.stress_level = stress_level
        self.proposed_flow = proposed_flow
        self.actual_flow = actual_flow
        self.log = log

class RoutingResultSummary:
    def __init__(self, total_shift_value: float, flow_count: int):
        self.total_shift_value = float(total_shift_value)
        self.flow_count = int(flow_count)

class DetailedRoutingResult:
    def __init__(self):
        self.summary = RoutingResultSummary(0.0, 0)
        self.flows = []
        self.flow_count = 0
        self.trace_steps = []

# ============================================================================
# CORE ROUTING FUNCTION
# ============================================================================

def route_liquidity(
    decisions: list,
    stress: StressProfile,
    states: list,
    caps: list,
    table: list,
    bounds: ShockBounds
) -> DetailedRoutingResult:
    """Pure functional deterministic liquidity routing matching the C++ equivalent."""
    result = DetailedRoutingResult()

    if not states:
        return result

    # Determine Portfolio total value
    total_portfolio_value = sum(s.current_liquidity_value for s in states)
    if total_portfolio_value <= 0.0:
        return result

    # Setup trace step helper
    def add_trace_step(src: int, tgt: int, stress_lvl: int, proposed: float, actual: float, msg: str):
        result.trace_steps.append(RoutingTraceStep(src, tgt, stress_lvl, proposed, actual, msg))

    proposed_flows = []
    flow_sources = []
    flow_targets = []

    target_inflow_accum = {}

    # Step 1: Movable liquidity & Rule evaluation per state
    for state in states:
        src_id = state.asset_id

        # Find target allocation from decisions
        target_alloc = state.current_allocation_ratio
        for dec in decisions:
            if dec.asset_id == src_id:
                target_alloc = dec.target_allocation_ratio
                break

        surplus_ratio = max(0.0, state.current_allocation_ratio - target_alloc)
        surplus_value = surplus_ratio * total_portfolio_value

        # Find liquidity cap for this source asset at this stress level
        max_outflow_ratio = 1.0
        for cap in caps:
            if cap.asset_id == src_id and cap.stress_level == stress.stress_level:
                max_outflow_ratio = cap.max_outflow_ratio
                break

        cap_outflow = state.current_liquidity_value * max_outflow_ratio
        movable = min(surplus_value, cap_outflow)

        if movable <= 0.0:
            continue

        # Find RoutingRule for this source asset + stress level
        matched_rule = None
        for rule in table:
            if rule.source == src_id and rule.stress_level == stress.stress_level:
                matched_rule = rule
                break

        if not matched_rule:
            add_trace_step(src_id, src_id, stress.stress_level, movable, 0.0, "No routing rule found")
            continue

        desired_flow = movable * matched_rule.preferred_flow_ratio
        if desired_flow <= 0.0:
            continue

        primary = matched_rule.primary_target
        fallback = matched_rule.fallback_target

        def is_blocked(target: int, flow_amt: float) -> bool:
            # Find target state
            tgt_state = None
            for s in states:
                if s.asset_id == target:
                    tgt_state = s
                    break

            if not tgt_state:
                return True

            # Check flags
            if tgt_state.flags & 1:
                return True

            # Find target caps for inflow limit
            max_inflow_ratio = 1.0
            for cap in caps:
                if cap.asset_id == target and cap.stress_level == stress.stress_level:
                    max_inflow_ratio = cap.max_inflow_ratio
                    break

            max_inflow_val = tgt_state.current_liquidity_value * max_inflow_ratio
            current_inflow_accum = target_inflow_accum.get(target, 0.0)

            if current_inflow_accum + flow_amt > max_inflow_val:
                return True

            return False

        final_target = src_id
        trace_log = "Both targets blocked"

        if not is_blocked(primary, desired_flow):
            final_target = primary
            trace_log = "Routed to primary target"
        elif not is_blocked(fallback, desired_flow):
            final_target = fallback
            trace_log = "Primary blocked, fallback used"

        if final_target != src_id:
            target_inflow_accum[final_target] = target_inflow_accum.get(final_target, 0.0) + desired_flow
            proposed_flows.append(desired_flow)
            flow_sources.append(src_id)
            flow_targets.append(final_target)
            add_trace_step(src_id, final_target, stress.stress_level, desired_flow, desired_flow, trace_log)
        else:
            add_trace_step(src_id, src_id, stress.stress_level, desired_flow, 0.0, trace_log)

    # Step 4: Shock bounds clamping
    asset_flows = {}
    for i in range(len(proposed_flows)):
        src = flow_sources[i]
        tgt = flow_targets[i]
        asset_flows[src] = asset_flows.get(src, 0.0) - proposed_flows[i]
        asset_flows[tgt] = asset_flows.get(tgt, 0.0) + proposed_flows[i]

    asset_scaling_factors = {}
    for state in states:
        max_allowed_shift = bounds.max_asset_liquidity_shift_per_step * state.current_liquidity_value
        net_f = asset_flows.get(state.asset_id, 0.0)
        abs_net_f = abs(net_f)

        if abs_net_f > max_allowed_shift and abs_net_f > 0.0:
            asset_scaling_factors[state.asset_id] = max_allowed_shift / abs_net_f
        else:
            asset_scaling_factors[state.asset_id] = 1.0

    # Scale down proposed flows by bottlenecks
    for i in range(len(proposed_flows)):
        src = flow_sources[i]
        tgt = flow_targets[i]
        min_scale = min(asset_scaling_factors.get(src, 1.0), asset_scaling_factors.get(tgt, 1.0))
        if min_scale < 1.0:
            proposed_flows[i] *= min_scale

    # Portfolio-level scale
    total_shift = sum(proposed_flows)
    max_allowed_portfolio_shift = bounds.max_portfolio_liquidity_shift_per_step * total_portfolio_value
    portfolio_scale = 1.0
    if total_shift > max_allowed_portfolio_shift and total_shift > 0.0:
        portfolio_scale = max_allowed_portfolio_shift / total_shift

    total_final_shift = 0.0
    for i in range(len(proposed_flows)):
        final_amt = proposed_flows[i] * portfolio_scale
        if final_amt > 0.0:
            result.flows.append(LiquidityFlow(flow_sources[i], flow_targets[i], final_amt))
            total_final_shift += final_amt

    result.flow_count = len(result.flows)
    result.summary.flow_count = result.flow_count
    result.summary.total_shift_value = total_final_shift

    # Update trace steps actual flows
    for step in result.trace_steps:
        if step.source != step.target and step.actual_flow > 0.0:
            final_amount = 0.0
            for flow in result.flows:
                if flow.source == step.source and flow.target == step.target:
                    final_amount = flow.amount
                    break
            step.actual_flow = final_amount
            if final_amount < step.proposed_flow:
                step.log = "Flow clamped by shock bounds"

    return result
