#include "aille_governor_reconciliation.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace AILLE {

// Helper to fill in static weights based on GovernorType
static double get_governor_weight(GovernorType type) {
    switch (type) {
        case GovernorType::COMPLIANCE: return 1.0;
        case GovernorType::RISK:       return 0.8;
        case GovernorType::LIQUIDITY:  return 0.6;
        case GovernorType::RETURN:     return 0.5;
        case GovernorType::STRATEGY:   return 0.5;
    }
    return 0.0;
}

// Phase angles for the 5 governors on a 5-star circle (in radians)
static double get_governor_phase_angle(GovernorType type) {
    // 2 * M_PI / 5 is approx 1.2566370614
    static constexpr double TWO_PI_5 = 1.2566370614359172;
    switch (type) {
        case GovernorType::COMPLIANCE: return 0.0 * TWO_PI_5;
        case GovernorType::RISK:       return 1.0 * TWO_PI_5;
        case GovernorType::LIQUIDITY:  return 2.0 * TWO_PI_5;
        case GovernorType::RETURN:     return 3.0 * TWO_PI_5;
        case GovernorType::STRATEGY:   return 4.0 * TWO_PI_5;
    }
    return 0.0;
}

ReconciledResult reconcile_governors(
    const std::vector<GovernorProposal>& proposals,
    AssetId asset_id
) {
    ReconciledResult result{};
    result.decision.asset_id = asset_id;
    result.residual.asset_id = asset_id;

    if (proposals.empty()) {
        result.decision.resolved_type = static_cast<uint8_t>(GovernorType::STRATEGY);
        result.decision.final_value = 0.0;
        result.summary.lyapunov_energy = 0.0;
        result.summary.tension = 0.0;
        return result;
    }

    // Identify proposal inputs for each GovernorType for this specific asset
    const GovernorProposal* comp_prop = nullptr;
    const GovernorProposal* risk_prop = nullptr;
    const GovernorProposal* liq_prop = nullptr;
    const GovernorProposal* ret_prop = nullptr;
    const GovernorProposal* strat_prop = nullptr;

    double max_abs_val = 0.0;

    for (const auto& prop : proposals) {
        if (prop.asset_id != asset_id) {
            continue;
        }
        double abs_val = std::abs(prop.proposed_value);
        if (abs_val > max_abs_val) {
            max_abs_val = abs_val;
        }
        switch (static_cast<GovernorType>(prop.governor_type)) {
            case GovernorType::COMPLIANCE: comp_prop = &prop; break;
            case GovernorType::RISK:       risk_prop = &prop; break;
            case GovernorType::LIQUIDITY:  liq_prop = &prop; break;
            case GovernorType::RETURN:     ret_prop = &prop; break;
            case GovernorType::STRATEGY:   strat_prop = &prop; break;
        }
    }

    // Initialize tracking variables
    double current_val = 0.0;
    bool has_active_base = false;
    uint8_t resolved_type = static_cast<uint8_t>(GovernorType::STRATEGY);
    uint8_t flags_applied = static_cast<uint8_t>(ReconciliationFlags::NONE);

    // Helper to log steps
    auto log_step = [&](GovernorType type, uint8_t action, double proposed, double interim, const char* msg) {
        if (result.trace.step_count < ReconciliationTrace::MAX_STEPS) {
            auto& step = result.trace.steps[result.trace.step_count++];
            step.asset_id = asset_id;
            step.governor_type = static_cast<uint8_t>(type);
            step.action_taken = action;
            step.proposed_value = proposed;
            step.interim_value = interim;
            std::strncpy(step.log, msg, sizeof(step.log) - 1);
            step.log[sizeof(step.log) - 1] = '\0';
        }
    };

    // Traversal Order (GOVERNOR_LADDER_V1):
    // 1. STRATEGY (Base level proposal)
    if (strat_prop) {
        current_val = strat_prop->proposed_value;
        has_active_base = true;
        resolved_type = static_cast<uint8_t>(GovernorType::STRATEGY);
        log_step(GovernorType::STRATEGY, 0, strat_prop->proposed_value, current_val, "Base strategy proposal");
    }

    // 2. RETURN (Can update or refine STRATEGY if no override active)
    if (ret_prop) {
        double proposed = ret_prop->proposed_value;
        if (!has_active_base) {
            current_val = proposed;
            has_active_base = true;
            resolved_type = static_cast<uint8_t>(GovernorType::RETURN);
            log_step(GovernorType::RETURN, 0, proposed, current_val, "Return primary proposal");
        } else {
            // Refine strat if return governor is aligned
            current_val = proposed; // Return takes preference over general Strategy
            resolved_type = static_cast<uint8_t>(GovernorType::RETURN);
            log_step(GovernorType::RETURN, 0, proposed, current_val, "Return override/refinement");
        }
    }

    // 3. LIQUIDITY (Magnitude limits based on liquidity caps)
    if (liq_prop) {
        double proposed_limit = liq_prop->proposed_value;
        if (has_active_base) {
            double abs_val = std::abs(current_val);
            if (abs_val > proposed_limit) {
                double sign = (current_val >= 0.0) ? 1.0 : -1.0;
                current_val = sign * proposed_limit;
                flags_applied |= static_cast<uint8_t>(ReconciliationFlags::VOL_CLAMP);
                log_step(GovernorType::LIQUIDITY, 2, proposed_limit, current_val, "Liquidity magnitude clamp");
            } else {
                log_step(GovernorType::LIQUIDITY, 0, proposed_limit, current_val, "Liquidity limit pass");
            }
        } else {
            current_val = proposed_limit;
            has_active_base = true;
            resolved_type = static_cast<uint8_t>(GovernorType::LIQUIDITY);
            log_step(GovernorType::LIQUIDITY, 0, proposed_limit, current_val, "Liquidity base proposal");
        }
    }

    // 4. RISK (Risk score thresholds can override RETURN/STRATEGY)
    if (risk_prop) {
        double proposed_risk_val = risk_prop->proposed_value;
        bool risk_increase = (std::abs(proposed_risk_val) < std::abs(current_val)); // e.g. RISK wants lower exposure
        (void)risk_increase;

        if (risk_prop->risk_score > 75.0f) {
            // Severe Risk threshold: Override lower governors by clamping or enforcing risk proposal
            if (has_active_base) {
                // If RISK wants less exposure than STRATEGY/RETURN, enforce RISK value
                if (std::abs(current_val) > std::abs(proposed_risk_val)) {
                    current_val = proposed_risk_val;
                    resolved_type = static_cast<uint8_t>(GovernorType::RISK);
                    flags_applied |= static_cast<uint8_t>(ReconciliationFlags::RISK_LIMIT);
                    log_step(GovernorType::RISK, 1, proposed_risk_val, current_val, "Risk severe threshold clamp");
                } else {
                    log_step(GovernorType::RISK, 0, proposed_risk_val, current_val, "Risk limits within threshold");
                }
            } else {
                current_val = proposed_risk_val;
                has_active_base = true;
                resolved_type = static_cast<uint8_t>(GovernorType::RISK);
                log_step(GovernorType::RISK, 0, proposed_risk_val, current_val, "Risk primary proposal");
            }
        } else {
            // Moderate Risk evaluation: check if RISK flags dictate clamping
            if (risk_prop->flags & static_cast<uint8_t>(ReconciliationFlags::RISK_LIMIT)) {
                if (std::abs(current_val) > std::abs(proposed_risk_val)) {
                    current_val = proposed_risk_val;
                    flags_applied |= static_cast<uint8_t>(ReconciliationFlags::RISK_LIMIT);
                    log_step(GovernorType::RISK, 2, proposed_risk_val, current_val, "Risk flag limit clamp");
                } else {
                    log_step(GovernorType::RISK, 0, proposed_risk_val, current_val, "Risk flags pass");
                }
            } else {
                log_step(GovernorType::RISK, 0, proposed_risk_val, current_val, "Risk moderate status pass");
            }
        }
    }

    // 5. COMPLIANCE (Ultimate authority - HARD_BLOCK)
    if (comp_prop) {
        if (comp_prop->flags & static_cast<uint8_t>(ReconciliationFlags::HARD_BLOCK)) {
            current_val = 0.0;
            resolved_type = static_cast<uint8_t>(GovernorType::COMPLIANCE);
            flags_applied |= static_cast<uint8_t>(ReconciliationFlags::HARD_BLOCK);
            log_step(GovernorType::COMPLIANCE, 3, comp_prop->proposed_value, current_val, "Compliance HARD_BLOCK triggered");
        } else {
            log_step(GovernorType::COMPLIANCE, 0, comp_prop->proposed_value, current_val, "Compliance pass");
        }
    }

    // ============================================================================
    // LYAPUNOV STABILIZATION & TENSION MAPPING
    // ============================================================================
    double Tx = 0.0;
    double Ty = 0.0;

    // Add phase components for active proposals
    if (comp_prop) {
        double theta = get_governor_phase_angle(GovernorType::COMPLIANCE);
        Tx += comp_prop->proposed_value * std::cos(theta);
        Ty += comp_prop->proposed_value * std::sin(theta);
    }
    if (risk_prop) {
        double theta = get_governor_phase_angle(GovernorType::RISK);
        Tx += risk_prop->proposed_value * std::cos(theta);
        Ty += risk_prop->proposed_value * std::sin(theta);
    }
    if (liq_prop) {
        double theta = get_governor_phase_angle(GovernorType::LIQUIDITY);
        Tx += liq_prop->proposed_value * std::cos(theta);
        Ty += liq_prop->proposed_value * std::sin(theta);
    }
    if (ret_prop) {
        double theta = get_governor_phase_angle(GovernorType::RETURN);
        Tx += ret_prop->proposed_value * std::cos(theta);
        Ty += ret_prop->proposed_value * std::sin(theta);
    }
    if (strat_prop) {
        double theta = get_governor_phase_angle(GovernorType::STRATEGY);
        Tx += strat_prop->proposed_value * std::cos(theta);
        Ty += strat_prop->proposed_value * std::sin(theta);
    }

    double tension = std::sqrt(Tx * Tx + Ty * Ty);

    // Compute Lyapunov energy V
    double sum_weighted_sq_diff = 0.0;
    for (const auto& prop : proposals) {
        if (prop.asset_id != asset_id) {
            continue;
        }
        double w = get_governor_weight(static_cast<GovernorType>(prop.governor_type));
        double diff = prop.proposed_value - current_val;
        sum_weighted_sq_diff += w * (diff * diff);
    }

    double lyapunov_energy = (tension * tension) + (0.5 * sum_weighted_sq_diff);

    // Scale-invariant Lyapunov Damping threshold
    double lyapunov_threshold = std::max(100.0, 10.0 * max_abs_val * max_abs_val);
    if (lyapunov_energy > lyapunov_threshold) {
        double multiplier = lyapunov_threshold / lyapunov_energy;
        double damped_val = current_val * multiplier;
        current_val = damped_val;
        log_step(GovernorType::COMPLIANCE, 4, current_val / multiplier, current_val, "Lyapunov energy damping applied");
    }

    // Assign final decision values
    result.decision.final_value = current_val;
    result.decision.resolved_type = resolved_type;
    result.decision.flags_applied = flags_applied;

    // Calculate Reconciliation Residual Metric: residual = sum(w_i * |p_i - d|)
    double total_residual = 0.0;
    for (const auto& prop : proposals) {
        if (prop.asset_id != asset_id) {
            continue;
        }
        double w = get_governor_weight(static_cast<GovernorType>(prop.governor_type));
        double diff = std::abs(prop.proposed_value - current_val);
        total_residual += w * diff;
    }

    result.residual.residual_value = total_residual;
    result.summary.total_residual = total_residual;
    result.summary.trace_count = static_cast<uint32_t>(result.trace.step_count);
    result.summary.lyapunov_energy = lyapunov_energy;
    result.summary.tension = tension;

    return result;
}

} // namespace AILLE
