/*
 * AILLE Framework - Commodity Risk & Growth Advisory Module (Indexed)
 * SILVER Module
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#ifndef AILLE_SILVER_HPP
#define AILLE_SILVER_HPP

#include <cstdint>
#include <cstddef>
#include "../aille.hpp"
#include "aille_math.hpp"

namespace AILLE {

struct alignas(64) SILVERState {
    float price;
    float realized_vol;
    float drawdown;
    float trend_score;
    float smoothed_vol;
    std::uint8_t _reserved_padding[44]; // 64 - 5*4 = 44 bytes

    constexpr SILVERState() : price(0.0f), realized_vol(0.0f), drawdown(0.0f), trend_score(0.0f), smoothed_vol(0.0f), _reserved_padding{} {}
};
static_assert(sizeof(SILVERState) == 64, "SILVERState must be exactly 64 bytes");

struct alignas(64) SILVERAdvisory {
    float recommended_weight;
    float risk_score;
    bool risk_elevated;
    bool growth_favorable;
    std::uint8_t _reserved_padding[54]; // 64 - (4 + 4 + 1 + 1) = 54 bytes

    constexpr SILVERAdvisory() : recommended_weight(1.0f), risk_score(0.0f), risk_elevated(false), growth_favorable(true), _reserved_padding{} {}
};
static_assert(sizeof(SILVERAdvisory) == 64, "SILVERAdvisory must be exactly 64 bytes");

struct alignas(64) SILVERObservabilityMetrics {
    float risk_score;
    float volatility_band;
    float advisory_weight;
    float trend_score;
    std::uint8_t _reserved_padding[48]; // 64 - 4*4 = 48 bytes

    constexpr SILVERObservabilityMetrics() : risk_score(0.0f), volatility_band(0.0f), advisory_weight(0.0f), trend_score(0.0f), _reserved_padding{} {}
};
static_assert(sizeof(SILVERObservabilityMetrics) == 64, "SILVERObservabilityMetrics must be exactly 64 bytes");

[[nodiscard]] constexpr SILVERAdvisory evaluate_silver_state(
    const SILVERState& state,
    const SafetyState* safety
) noexcept {
    float values[4] = { state.realized_vol, state.drawdown, state.trend_score, state.smoothed_vol };
    float confidences[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    float smoothed_risk = AILLE::Math::fibonacci_weighted_average(values, confidences, 4);

    SILVERAdvisory advisory{};

    // Scale risk conceptually between 0 and 100 for consistency
    float scaled_risk = smoothed_risk * 100.0f;
    if (scaled_risk < 0.0f) scaled_risk = 0.0f;
    if (scaled_risk > 100.0f) scaled_risk = 100.0f;

    advisory.risk_score = scaled_risk;
    advisory.risk_elevated = (advisory.risk_score > 60.0f) || (state.drawdown > 0.15f);
    advisory.growth_favorable = (!advisory.risk_elevated && state.trend_score > 0.1f);

    advisory.recommended_weight = 1.0f - (advisory.risk_score / 100.0f);
    if (advisory.recommended_weight < 0.0f) advisory.recommended_weight = 0.0f;
    if (advisory.recommended_weight > 1.0f) advisory.recommended_weight = 1.0f;

    if (safety != nullptr) {
        if (safety->hardware_fault || safety->kill_switch) {
            advisory.risk_elevated = true;
            advisory.growth_favorable = false;
            advisory.risk_score = 100.0f;
            advisory.recommended_weight = 0.0f;
        }
    }

    return advisory;
}

} // namespace AILLE

#endif // AILLE_SILVER_HPP
