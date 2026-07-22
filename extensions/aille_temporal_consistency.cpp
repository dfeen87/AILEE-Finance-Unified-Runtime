#include "aille_temporal_consistency.hpp"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace AILLE {

void enforce_temporal_consistency(
    const TemporalStates& prev_states,
    TemporalStates& curr_states,
    const TemporalPortfolioState& prev_portfolio,
    TemporalPortfolioState& curr_portfolio,
    TemporalResiduals& residuals,
    TemporalTraceSteps& trace,
    double max_drift_threshold
) {
    (void)prev_portfolio; // Suppress unused parameter warning if not needed for direct calculations

    residuals.count = 0;
    trace.count = 0;

    double total_residual = 0.0;
    double current_portfolio_risk = 0.0;

    for (size_t i = 0; i < curr_states.count; ++i) {
        auto& curr_asset = curr_states.states[i];

        // Find matching asset in previous states to retrieve w_{i,t} and w_{i,t-1}
        double w_i_t = 0.0;
        double w_i_t_minus_1 = 0.0;
        bool found_prev = false;

        for (size_t j = 0; j < prev_states.count; ++j) {
            if (prev_states.states[j].asset_id == curr_asset.asset_id) {
                w_i_t = prev_states.states[j].prev_allocation;
                w_i_t_minus_1 = prev_states.states[j].prev_prev_allocation;
                found_prev = true;
                break;
            }
        }

        double original_proposed = curr_asset.prev_allocation;
        double current_val = original_proposed;
        bool has_oscillation = false;
        bool has_clamp = false;

        // If we have history, we can perform oscillation detection
        if (found_prev) {
            double delta_t = w_i_t - w_i_t_minus_1;
            double delta_t1 = current_val - w_i_t;

            // Oscillation detection threshold: both absolute step sizes must exceed a small tolerance (1e-4)
            if (((delta_t > 0.0001 && delta_t1 < -0.0001) || (delta_t < -0.0001 && delta_t1 > 0.0001))) {
                has_oscillation = true;
                // Apply oscillation dampening: half the step size
                current_val = w_i_t + 0.5 * delta_t1;
                curr_asset.flags |= 1; // set oscillation_detected flag (bit 0)
            } else {
                curr_asset.flags &= ~1; // clear oscillation_detected flag
            }
        } else {
            curr_asset.flags &= ~1;
        }

        // Apply temporal drift clamping relative to baseline expectation \hat{w}_{i,t+1} = w_{i,t}
        double drift = std::abs(current_val - w_i_t);
        if (drift > max_drift_threshold) {
            has_clamp = true;
            if (current_val > w_i_t) {
                current_val = w_i_t + max_drift_threshold;
            } else {
                current_val = w_i_t - max_drift_threshold;
            }
        }

        // Determine Action Taken
        TemporalAction action = TemporalAction::NO_CHANGE;
        const char* log_msg = "No temporal change";

        if (has_oscillation && has_clamp) {
            action = TemporalAction::OSCILLATED_AND_CLAMPED;
            log_msg = "Oscillated & clamped";
        } else if (has_oscillation) {
            action = TemporalAction::DAMPENED;
            log_msg = "Oscillation dampened";
        } else if (has_clamp) {
            action = TemporalAction::CLAMPED;
            log_msg = "Drift clamped";
        }

        // Update state in-place with resolved allocation and propagate previous allocation
        curr_asset.prev_allocation = current_val;
        curr_asset.prev_prev_allocation = w_i_t;

        // Capture residual metrics
        double final_residual = std::abs(current_val - w_i_t);
        total_residual += final_residual;

        if (residuals.count < TemporalResiduals::MAX_ASSETS) {
            auto& res = residuals.residuals[residuals.count++];
            res.asset_id = curr_asset.asset_id;
            res.expected_allocation = w_i_t;
            res.actual_allocation = current_val;
            res.residual = final_residual;
            std::memset(res.reserved, 0, sizeof(res.reserved));
        }

        // Capture trace steps
        if (trace.count < TemporalTraceSteps::MAX_STEPS) {
            auto& step = trace.steps[trace.count++];
            step.asset_id = curr_asset.asset_id;
            step.action_taken = static_cast<uint8_t>(action);
            step.before_value = original_proposed;
            step.after_value = current_val;
            step.residual = final_residual;
            std::strncpy(step.log, log_msg, sizeof(step.log) - 1);
            step.log[sizeof(step.log) - 1] = '\0';
        }

        // Keep running sum of resolved portfolio risk
        current_portfolio_risk += std::abs(current_val) * curr_asset.prev_risk_score;
    }

    // Update global portfolio states
    curr_portfolio.residual_sum = total_residual;
    curr_portfolio.portfolio_risk = current_portfolio_risk;
    std::memset(curr_portfolio.reserved, 0, sizeof(curr_portfolio.reserved));
}

} // namespace AILLE
