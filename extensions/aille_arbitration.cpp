#include "aille_arbitration.hpp"
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace AILLE {

ArbitrationResult arbitrate(
    const std::vector<Advisory>& advisories,
    const Ladder& ladder,
    const ScalingRules& rules
) {
    ArbitrationResult result{};
    if (advisories.empty()) {
        return result;
    }

    size_t asset_count = std::min(advisories.size(), ArbitrationResult::MAX_ASSETS);
    result.decision_count = asset_count;

    // Fixed arrays on the stack (no heap allocation in the hot path)
    double weights[ArbitrationResult::MAX_ASSETS];
    double safety_hurdles[ArbitrationResult::MAX_ASSETS];
    double liquidity_factors[ArbitrationResult::MAX_ASSETS];
    double reg_caps[ArbitrationResult::MAX_ASSETS];
    double risk_scores[ArbitrationResult::MAX_ASSETS];
    double return_scores[ArbitrationResult::MAX_ASSETS];

    // Phase 1: Canonical Scaling Ruleset (SCALING_RULESET_V1)
    for (size_t i = 0; i < asset_count; ++i) {
        const auto& adv = advisories[i];
        result.decisions[i].asset_id = adv.asset_id;
        weights[i] = 1.0;

        safety_hurdles[i] = rules.clamp(adv.safety_level, 0.0, 1.0);
        liquidity_factors[i] = rules.normalize_liquidity(adv.liquidity_level);
        reg_caps[i] = rules.normalize_regulatory(adv.regulatory_level);
        risk_scores[i] = rules.normalize_risk(adv.risk_score);
        return_scores[i] = rules.clamp(adv.return_score, 0.0, 1.0);
    }

    // Helper to add trace steps to result.trace
    auto add_trace_step = [&](AssetId id, LadderDimension dim, double input_val, double result_w, const char* msg) {
        if (result.trace.step_count < ArbitrationTrace::MAX_STEPS) {
            auto& step = result.trace.steps[result.trace.step_count++];
            step.asset_id = id;
            step.dimension = static_cast<uint16_t>(dim);
            step.reserved_flags = 0;
            step.input_value = input_val;
            step.result_weight = result_w;
            std::strncpy(step.log, msg, sizeof(step.log) - 1);
            step.log[sizeof(step.log) - 1] = '\0';
        }
    };

    // Phase 2: Ladder Traversal (LADDER_V1)
    for (int stage = 0; stage < 5; ++stage) {
        LadderDimension dim = ladder.dimensions[stage];
        for (size_t i = 0; i < asset_count; ++i) {
            if (weights[i] == 0.0) continue;

            const auto& adv = advisories[i];

            if (dim == LadderDimension::SAFETY) {
                if (safety_hurdles[i] < 0.35) {
                    weights[i] = 0.0;
                    add_trace_step(adv.asset_id, dim, safety_hurdles[i], 0.0, "Safety failure");
                } else if (safety_hurdles[i] < 0.6) {
                    weights[i] = weights[i] * 0.5;
                    add_trace_step(adv.asset_id, dim, safety_hurdles[i], weights[i], "Marginal safety cap");
                } else {
                    add_trace_step(adv.asset_id, dim, safety_hurdles[i], weights[i], "Safety pass");
                }
            }
            else if (dim == LadderDimension::LIQUIDITY) {
                weights[i] = weights[i] * liquidity_factors[i];
                add_trace_step(adv.asset_id, dim, liquidity_factors[i], weights[i], "Liquidity scaled");
            }
            else if (dim == LadderDimension::REGULATORY) {
                if (adv.raw_flags & static_cast<uint32_t>(AdvisoryFlags::HARD_BLOCK)) {
                    weights[i] = 0.0;
                    add_trace_step(adv.asset_id, dim, 0.0, 0.0, "Hard block");
                } else if (reg_caps[i] < 0.3) {
                    weights[i] = weights[i] * 0.2;
                    add_trace_step(adv.asset_id, dim, reg_caps[i], weights[i], "Reg soft cap");
                } else if (adv.raw_flags & static_cast<uint32_t>(AdvisoryFlags::PREFERRED)) {
                    weights[i] = weights[i] * 1.2;
                    add_trace_step(adv.asset_id, dim, reg_caps[i], weights[i], "Preferred mult");
                } else {
                    add_trace_step(adv.asset_id, dim, reg_caps[i], weights[i], "Reg neutral");
                }
            }
            else if (dim == LadderDimension::RISK) {
                double risk_damp = 1.0 - risk_scores[i];
                weights[i] = weights[i] * risk_damp;
                add_trace_step(adv.asset_id, dim, risk_scores[i], weights[i], "Risk dampened");
            }
            else if (dim == LadderDimension::RETURN) {
                double return_factor = 0.5 + 0.5 * return_scores[i];
                weights[i] = weights[i] * return_factor;
                add_trace_step(adv.asset_id, dim, return_scores[i], weights[i], "Return scaled");
            }
        }
    }

    // Phase 3: Deterministic Normalization
    double total_weight = 0.0;
    for (size_t i = 0; i < asset_count; ++i) {
        total_weight += weights[i];
    }

    for (size_t i = 0; i < asset_count; ++i) {
        double final_alloc = 0.0;
        if (total_weight > 0.0) {
            final_alloc = weights[i] / total_weight;
        }
        result.decisions[i].recommended_allocation = final_alloc;
    }

    return result;
}

} // namespace AILLE
