/*
 * AILLE Framework - Market Stabilization Governor Advisory Module (MSGAM) (Layer 7.9)
 * AI-Load Integrity and Layered Evaluation
 *
 * Standalone advisory-only module for market stabilization and volatility governing.
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#ifndef AILLE_STABILIZER_HPP
#define AILLE_STABILIZER_HPP

#include <cstdint>
#include <cstddef>
#include "../aille.hpp"
#include "aille_math.hpp"

namespace AILLE {

struct alignas(64) MarketStabilizerObservabilityMetrics final {
    float stabilization_risk_score;
    float stabilization_factor;
    float spread_deviation;
    float systemic_vol;
    std::uint8_t _padding[48]; // 64 - 4*4 = 48 bytes padding

    constexpr MarketStabilizerObservabilityMetrics()
        : stabilization_risk_score(0.0f), stabilization_factor(1.0f),
          spread_deviation(0.0f), systemic_vol(0.0f), _padding{} {}
};
static_assert(sizeof(MarketStabilizerObservabilityMetrics) == 64, "MarketStabilizerObservabilityMetrics must be exactly 64 bytes");

// ============================================================================
// ADVISORY LOGIC
// ============================================================================

[[nodiscard]] constexpr MarketStabilizerAdvisory evaluate_market_stabilizer_advisory(
    const MarketStabilizerState& state,
    const SafetyState* safety
) noexcept {
    MarketStabilizerAdvisory advisory{};

    auto clamp01 = [](float v) constexpr {
        return (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);
    };

    float sys_vol = clamp01(state.systemic_volatility);
    float spread_dev = clamp01(state.bid_ask_spread_deviation);
    float depth_def = clamp01(state.order_book_depth_deficit);
    float crash_cnt = clamp01(state.consecutive_crash_count / 10.0f); // normalize count
    float regime_stress = clamp01(state.regime_stress_factor);

    // Compute pressure based on weights
    float stress_values[4] = {
        sys_vol * 100.0f,
        spread_dev * 100.0f,
        depth_def * 100.0f,
        (crash_cnt + regime_stress) * 50.0f
    };
    float confidences[4] = {1.0f, 1.0f, 0.9f, 0.8f};

    float smoothed_stress = AILLE::Math::fibonacci_weighted_average(stress_values, confidences, 4);

    if (smoothed_stress < 0.0f) smoothed_stress = 0.0f;
    if (smoothed_stress > 100.0f) smoothed_stress = 100.0f;

    advisory.stabilization_risk_score = smoothed_stress;

    // Define stabilization factor and dynamic clamp limit based on stress score
    if (advisory.stabilization_risk_score > 75.0f) {
        advisory.risk_elevated = 1;
        advisory.governor_active = 1;
        advisory.stabilization_factor = 0.15f;
        advisory.dynamic_clamp_limit = 0.20f;
    } else if (advisory.stabilization_risk_score > 40.0f) {
        advisory.risk_elevated = 0;
        advisory.governor_active = 1;
        advisory.stabilization_factor = 0.50f;
        advisory.dynamic_clamp_limit = 0.50f;
    } else {
        advisory.risk_elevated = 0;
        advisory.governor_active = 0;
        advisory.stabilization_factor = 1.0f;
        advisory.dynamic_clamp_limit = 1.0f;
    }

    if (spread_dev > 0.6f) {
        advisory.spread_guard_active = 1;
        advisory.stabilization_factor *= 0.8f;
    }

    // Safety integration (Advisory only!)
    if (safety != nullptr) {
        if (safety->hardware_fault || safety->kill_switch) {
            advisory.risk_elevated = 1;
            advisory.governor_active = 1;
            advisory.spread_guard_active = 1;
            advisory.stabilization_risk_score = 100.0f;
            advisory.stabilization_factor = 0.0f;
            advisory.dynamic_clamp_limit = 0.0f;
        }
    }

    return advisory;
}

} // namespace AILLE

#endif // AILLE_STABILIZER_HPP
