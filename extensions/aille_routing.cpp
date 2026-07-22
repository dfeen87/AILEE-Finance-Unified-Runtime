#include "aille_routing.hpp"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace AILLE {

DetailedRoutingResult route_liquidity(
    const CrossAssetDecisions& decisions,
    const StressProfile& stress,
    const LiquidityStateSet& states,
    const LiquidityCaps& caps,
    const RoutingTable& table,
    const ShockBounds& bounds
) {
    DetailedRoutingResult result{};

    if (states.count == 0) {
        return result;
    }

    // Determine Portfolio total value
    double total_portfolio_value = 0.0;
    for (size_t i = 0; i < states.count; ++i) {
        total_portfolio_value += states.states[i].current_liquidity_value;
    }

    if (total_portfolio_value <= 0.0) {
        return result;
    }

    // Setup trace step helper
    auto add_trace_step = [&](AssetId src, AssetId tgt, uint8_t stress_lvl, double proposed, double actual, const char* msg) {
        if (result.trace.step_count < RoutingTrace::MAX_STEPS) {
            auto& step = result.trace.steps[result.trace.step_count++];
            step.source = src;
            step.target = tgt;
            step.stress_level = stress_lvl;
            step.reserved_flags = 0;
            step.proposed_flow = proposed;
            step.actual_flow = actual;
            std::strncpy(step.log, msg, sizeof(step.log) - 1);
            step.log[sizeof(step.log) - 1] = '\0';
        }
    };

    // Keep track of flows before clamping. We can have up to DetailedRoutingResult::MAX_FLOWS
    // We will do deterministic flows matching source assets
    double proposed_flows[DetailedRoutingResult::MAX_FLOWS];
    AssetId flow_sources[DetailedRoutingResult::MAX_FLOWS];
    AssetId flow_targets[DetailedRoutingResult::MAX_FLOWS];
    size_t temp_flow_count = 0;

    // We also track how much total inflow is heading to each asset ID so we can evaluate target blockage
    // Simple fixed array lookup for target inflow accumulators
    struct AssetInflowAccumulator {
        AssetId asset_id;
        double current_inflow;
    };
    AssetInflowAccumulator target_inflows[LiquidityStateSet::MAX_ASSETS];
    size_t target_inflow_count = 0;

    auto get_or_create_inflow_accum = [&](AssetId id) -> double& {
        for (size_t k = 0; k < target_inflow_count; ++k) {
            if (target_inflows[k].asset_id == id) {
                return target_inflows[k].current_inflow;
            }
        }
        if (target_inflow_count < LiquidityStateSet::MAX_ASSETS) {
            target_inflows[target_inflow_count].asset_id = id;
            target_inflows[target_inflow_count].current_inflow = 0.0;
            return target_inflows[target_inflow_count++].current_inflow;
        }
        // Fallback reference, shouldn't be hit with bounded assets
        static double dummy = 0.0;
        return dummy;
    };

    // Step 1: Movable liquidity & Rule evaluation per state
    for (size_t i = 0; i < states.count; ++i) {
        const auto& state = states.states[i];
        AssetId src_id = state.asset_id;

        // Find target allocation from CrossAssetDecisions
        double target_alloc = state.current_allocation_ratio; // default if not found
        for (size_t j = 0; j < decisions.count; ++j) {
            if (decisions.decisions[j].asset_id == src_id) {
                target_alloc = decisions.decisions[j].target_allocation_ratio;
                break;
            }
        }

        double surplus_ratio = state.current_allocation_ratio - target_alloc;
        if (surplus_ratio < 0.0) {
            surplus_ratio = 0.0;
        }
        double surplus_value = surplus_ratio * total_portfolio_value;

        // Find liquidity cap for this source asset at this stress level
        double max_outflow_ratio = 1.0; // default if not found
        for (size_t j = 0; j < caps.count; ++j) {
            if (caps.caps[j].asset_id == src_id && caps.caps[j].stress_level == stress.stress_level) {
                max_outflow_ratio = caps.caps[j].max_outflow_ratio;
                break;
            }
        }

        double cap_outflow = state.current_liquidity_value * max_outflow_ratio;
        double movable = std::min(surplus_value, cap_outflow);

        if (movable <= 0.0) {
            continue;
        }

        // Find RoutingRule for this source asset + stress level
        const RoutingRule* matched_rule = nullptr;
        for (size_t j = 0; j < table.count; ++j) {
            if (table.rules[j].source == src_id && table.rules[j].stress_level == stress.stress_level) {
                matched_rule = &table.rules[j];
                break;
            }
        }

        if (!matched_rule) {
            // No rule found, flow remains at source
            add_trace_step(src_id, src_id, stress.stress_level, movable, 0.0, "No routing rule found");
            continue;
        }

        double desired_flow = movable * matched_rule->preferred_flow_ratio;
        if (desired_flow <= 0.0) {
            continue;
        }

        // Evaluate Blockage
        AssetId primary = matched_rule->primary_target;
        AssetId fallback = matched_rule->fallback_target;

        auto is_blocked = [&](AssetId target, double flow_amt) -> bool {
            // Find target state
            const LiquidityState* tgt_state = nullptr;
            for (size_t k = 0; k < states.count; ++k) {
                if (states.states[k].asset_id == target) {
                    tgt_state = &states.states[k];
                    break;
                }
            }

            if (!tgt_state) {
                return true; // Target doesn't exist
            }

            // Check flags (e.g. frozen/restricted/disallowed)
            // Bit 0: frozen/restricted
            if (tgt_state->flags & 1) {
                return true;
            }

            // Find target caps for inflow limit
            double max_inflow_ratio = 1.0; // default if not found
            for (size_t k = 0; k < caps.count; ++k) {
                if (caps.caps[k].asset_id == target && caps.caps[k].stress_level == stress.stress_level) {
                    max_inflow_ratio = caps.caps[k].max_inflow_ratio;
                    break;
                }
            }

            double max_inflow_val = tgt_state->current_liquidity_value * max_inflow_ratio;
            double& current_inflow_accum = get_or_create_inflow_accum(target);

            if (current_inflow_accum + flow_amt > max_inflow_val) {
                return true;
            }

            return false;
        };

        AssetId final_target = src_id;
        const char* trace_log = "Both targets blocked";

        if (!is_blocked(primary, desired_flow)) {
            final_target = primary;
            trace_log = "Routed to primary target";
        } else if (!is_blocked(fallback, desired_flow)) {
            final_target = fallback;
            trace_log = "Primary blocked, fallback used";
        }

        if (final_target != src_id) {
            // Log inflow to target
            get_or_create_inflow_accum(final_target) += desired_flow;

            if (temp_flow_count < DetailedRoutingResult::MAX_FLOWS) {
                proposed_flows[temp_flow_count] = desired_flow;
                flow_sources[temp_flow_count] = src_id;
                flow_targets[temp_flow_count] = final_target;
                temp_flow_count++;
            }
            add_trace_step(src_id, final_target, stress.stress_level, desired_flow, desired_flow, trace_log);
        } else {
            add_trace_step(src_id, src_id, stress.stress_level, desired_flow, 0.0, trace_log);
        }
    }

    // Step 4: Shock bounds clamping
    // We compute asset-level absolute flows per asset to apply asset shock bounds first.
    // Let's first accumulate absolute flow per asset (source outflow + target inflow).
    // Let's set up a net flow per asset ID.
    struct AssetFlowTracker {
        AssetId asset_id;
        double net_flow;
    };
    AssetFlowTracker asset_flows[LiquidityStateSet::MAX_ASSETS];
    size_t asset_flow_count = 0;

    auto get_or_create_flow_tracker = [&](AssetId id) -> double& {
        for (size_t k = 0; k < asset_flow_count; ++k) {
            if (asset_flows[k].asset_id == id) {
                return asset_flows[k].net_flow;
            }
        }
        if (asset_flow_count < LiquidityStateSet::MAX_ASSETS) {
            asset_flows[asset_flow_count].asset_id = id;
            asset_flows[asset_flow_count].net_flow = 0.0;
            return asset_flows[asset_flow_count++].net_flow;
        }
        static double dummy = 0.0;
        return dummy;
    };

    // Calculate initial net flow per asset from the proposed flows
    for (size_t i = 0; i < temp_flow_count; ++i) {
        get_or_create_flow_tracker(flow_sources[i]) -= proposed_flows[i];
        get_or_create_flow_tracker(flow_targets[i]) += proposed_flows[i];
    }

    // Now, apply the asset shock bounds constraint
    // max_asset_liquidity_shift_per_step represents maximum fraction of asset liquidity
    // For each asset, the absolute value of the net flow cannot exceed:
    // max_asset_liquidity_shift_per_step * asset_liquidity_value * total_portfolio_value? No,
    // "fraction of asset liquidity", i.e. max_asset_liquidity_shift_per_step * states[i].current_liquidity_value
    double asset_scaling_factors[LiquidityStateSet::MAX_ASSETS];
    for (size_t i = 0; i < states.count; ++i) {
        const auto& state = states.states[i];
        double max_allowed_shift = bounds.max_asset_liquidity_shift_per_step * state.current_liquidity_value;

        // Find net flow for this asset
        double net_f = 0.0;
        for (size_t k = 0; k < asset_flow_count; ++k) {
            if (asset_flows[k].asset_id == state.asset_id) {
                net_f = asset_flows[k].net_flow;
                break;
            }
        }

        double abs_net_f = std::abs(net_f);
        if (abs_net_f > max_allowed_shift && abs_net_f > 0.0) {
            asset_scaling_factors[i] = max_allowed_shift / abs_net_f;
        } else {
            asset_scaling_factors[i] = 1.0;
        }
    }

    // Clamp flows by the lower of source or target asset-level scaling factor to be absolutely safe,
    // or just apply the scale factor of the bottlenecked asset?
    // Let's scale down proposed flows. If any of the source or target asset scaling is < 1.0,
    // we should scale that flow to ensure the asset shift bound is strictly met.
    for (size_t i = 0; i < temp_flow_count; ++i) {
        AssetId src = flow_sources[i];
        AssetId tgt = flow_targets[i];
        double min_scale = 1.0;

        for (size_t k = 0; k < states.count; ++k) {
            if (states.states[k].asset_id == src || states.states[k].asset_id == tgt) {
                if (asset_scaling_factors[k] < min_scale) {
                    min_scale = asset_scaling_factors[k];
                }
            }
        }

        if (min_scale < 1.0) {
            proposed_flows[i] *= min_scale;
        }
    }

    // Now recalculate total portfolio shift: sum of absolute net flows or sum of proposed flows?
    // "max total absolute sum of all flows" / total portfolio shift.
    // Let's compute total absolute sum of all flows.
    double total_shift = 0.0;
    for (size_t i = 0; i < temp_flow_count; ++i) {
        total_shift += proposed_flows[i];
    }

    double max_allowed_portfolio_shift = bounds.max_portfolio_liquidity_shift_per_step * total_portfolio_value;
    double portfolio_scale = 1.0;
    if (total_shift > max_allowed_portfolio_shift && total_shift > 0.0) {
        portfolio_scale = max_allowed_portfolio_shift / total_shift;
    }

    // Apply the portfolio-level scale to all flows
    double total_final_shift = 0.0;
    for (size_t i = 0; i < temp_flow_count; ++i) {
        double final_amt = proposed_flows[i] * portfolio_scale;
        if (final_amt > 0.0) {
            if (result.flow_count < DetailedRoutingResult::MAX_FLOWS) {
                auto& flow = result.flows[result.flow_count++];
                flow.source = flow_sources[i];
                flow.target = flow_targets[i];
                flow.amount = final_amt;
                total_final_shift += final_amt;
            }
        }
    }

    result.summary.total_shift_value = total_final_shift;
    result.summary.flow_count = result.flow_count;

    // Update the trace step logs with actual clamped actual flows to show clamping happened
    for (size_t i = 0; i < result.trace.step_count; ++i) {
        auto& step = result.trace.steps[i];
        if (step.source != step.target && step.actual_flow > 0.0) {
            // Find corresponding final flow amount
            double final_amount = 0.0;
            for (size_t j = 0; j < result.flow_count; ++j) {
                if (result.flows[j].source == step.source && result.flows[j].target == step.target) {
                    final_amount = result.flows[j].amount;
                    break;
                }
            }
            step.actual_flow = final_amount;
            if (final_amount < step.proposed_flow) {
                std::strncpy(step.log, "Flow clamped by shock bounds", sizeof(step.log) - 1);
                step.log[sizeof(step.log) - 1] = '\0';
            }
        }
    }

    return result;
}

} // namespace AILLE
