#include "aille_validation.hpp"
#include "aille_stress_regime_override.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace AILLE {

static void apply_stress_regime_override_impl(
    const StressOverrideRules& rules,
    const StressPortfolioState& state,
    const AssetAllocations& prev_allocations,
    AssetAllocations& allocations,
    const SafeBaselineContainer& baselines,
    StressTraceSteps& trace,
    bool normal_safety_failed
) {
    trace.count = 0;

    // 1. Stress Regime Evaluation
    uint8_t effective_mode = rules.mode;

    bool exceeds_crisis_thresholds = (
        state.volatility_index > rules.volatility_threshold ||
        state.drawdown_index > rules.drawdown_threshold ||
        state.correlation_index > rules.correlation_threshold ||
        state.temporal_residual_sum > rules.residual_threshold
    );

    bool exceeds_stress_thresholds = (
        state.volatility_index > 0.5 * rules.volatility_threshold ||
        state.drawdown_index > 0.5 * rules.drawdown_threshold ||
        state.correlation_index > 0.5 * rules.correlation_threshold ||
        state.temporal_residual_sum > 0.5 * rules.residual_threshold
    );

    if (state.stress_level == static_cast<uint8_t>(StressMode::CRISIS) || exceeds_crisis_thresholds) {
        effective_mode = static_cast<uint8_t>(StressMode::CRISIS);
    } else if (state.stress_level == static_cast<uint8_t>(StressMode::STRESS) || exceeds_stress_thresholds) {
        effective_mode = static_cast<uint8_t>(StressMode::STRESS);
    }

    // 2. Check if exposure freeze is active (triggered by exceeding lower/stress bounds)
    bool exposure_freeze_active = (
        state.volatility_index > 0.5 * rules.volatility_threshold ||
        state.drawdown_index > 0.5 * rules.drawdown_threshold
    );

    // 3. Sequential Deterministic processing for each asset
    for (size_t i = 0; i < allocations.count; ++i) {
        auto& alloc = allocations.allocations[i];
        double original = alloc.allocation;
        double current_val = original;
        uint8_t step_flags = 0;

        // Retrieve previous allocation for the asset
        double prev_alloc_val = 0.0;
        bool found_prev = false;
        for (size_t j = 0; j < prev_allocations.count; ++j) {
            if (prev_allocations.allocations[j].asset_id == alloc.asset_id) {
                prev_alloc_val = prev_allocations.allocations[j].allocation;
                found_prev = true;
                break;
            }
        }

        // Define risk-bearing asset conditions
        //   - Non-CASH asset
        //   - risk_score > 40.0
        bool is_risk_bearing = (alloc.asset_id != AssetId::CASH && alloc.risk_score > 40.0);

        // Stage A: Exposure Freeze
        if (exposure_freeze_active && is_risk_bearing && found_prev) {
            if (current_val > prev_alloc_val) {
                current_val = prev_alloc_val;
                step_flags |= (1 << 1); // bit 1: frozen
            }
        }

        // Stage B: Crash Dampening
        if ((effective_mode == static_cast<uint8_t>(StressMode::STRESS) ||
             effective_mode == static_cast<uint8_t>(StressMode::CRISIS)) &&
            is_risk_bearing) {
            current_val = current_val * rules.crash_dampening_factor;
            step_flags |= (1 << 0); // bit 0: dampened
        }

        // Stage C: Fallback Compression
        if (normal_safety_failed || effective_mode == static_cast<uint8_t>(StressMode::CRISIS)) {
            // Find baseline allocation
            double baseline_val = 0.0;
            for (size_t k = 0; k < baselines.count; ++k) {
                if (baselines.baselines[k].asset_id == alloc.asset_id) {
                    baseline_val = baselines.baselines[k].baseline_allocation;
                    break;
                }
            }
            current_val = (1.0 - rules.fallback_lambda) * current_val + rules.fallback_lambda * baseline_val;
            step_flags |= (1 << 2); // bit 2: fallback_applied
        }

        // Apply final adjusted allocation
        alloc.allocation = current_val;

        // Record trace step if space is available
        if (trace.count < StressTraceSteps::MAX_STEPS) {
            auto& step = trace.steps[trace.count++];
            step.asset_id = alloc.asset_id;
            step.flags = step_flags;
            step.original_allocation = original;
            step.adjusted_allocation = current_val;
            std::memset(step.reserved, 0, sizeof(step.reserved));
        }
    }
}

void apply_stress_regime_override(const StressOverrideRules& rules, const StressPortfolioState& state,
    const AssetAllocations& prev_allocations, AssetAllocations& allocations,
    const SafeBaselineContainer& baselines, StressTraceSteps& trace, bool normal_safety_failed) {
    validation::count(prev_allocations.count, prev_allocations.allocations);
    validation::count(allocations.count, allocations.allocations);
    validation::count(baselines.count, baselines.baselines);
    validation::nonnegative(rules.volatility_threshold); validation::nonnegative(rules.drawdown_threshold);
    validation::nonnegative(rules.correlation_threshold); validation::nonnegative(rules.residual_threshold);
    validation::unit(rules.crash_dampening_factor); validation::unit(rules.fallback_lambda);
    validation::finite({state.portfolio_risk, state.stress_index, state.volatility_index, state.drawdown_index,
                        state.correlation_index, state.temporal_residual_sum});
    if (rules.mode>2 || state.stress_level>2) throw std::invalid_argument("unknown stress mode");
    for (std::size_t i=0; i<prev_allocations.count; ++i) validation::finite({prev_allocations.allocations[i].allocation, prev_allocations.allocations[i].risk_score});
    for (std::size_t i=0; i<allocations.count; ++i) validation::finite({allocations.allocations[i].allocation, allocations.allocations[i].risk_score});
    for (std::size_t i=0; i<baselines.count; ++i) validation::finite({baselines.baselines[i].baseline_allocation});
    auto staged_allocations=allocations;
    StressTraceSteps staged_trace{};
    apply_stress_regime_override_impl(rules, state, prev_allocations, staged_allocations, baselines, staged_trace, normal_safety_failed);
    for (std::size_t i=0; i<staged_allocations.count; ++i) validation::finite({staged_allocations.allocations[i].allocation});
    allocations=staged_allocations; trace=staged_trace;
}

} // namespace AILLE
