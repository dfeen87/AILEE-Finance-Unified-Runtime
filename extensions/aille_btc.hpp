/*
 * AILLE Framework - BTC Risk & Growth Advisory Module (BRGAM)
 * AI-Load Integrity and Layered Evaluation
 *
 * Standalone advisory-only module for BTC volatility exposure reduction,
 * fast drop mitigation, and controlled growth.
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#ifndef AILLE_BTC_HPP
#define AILLE_BTC_HPP

#include <cstdint>
#include <cstddef>
#include "../aille.hpp"
#include "aille_math.hpp"

namespace AILLE {

// ============================================================================
// CORE DATA STRUCTURES
// ============================================================================

struct alignas(64) BTCState {
    float price;
    float realized_vol;
    float drawdown;
    float trend_score;
    float smoothed_vol;
    std::uint8_t _reserved_padding[44]; // 64 - 5*4 = 44 bytes

    constexpr BTCState() : price(0.0f), realized_vol(0.0f), drawdown(0.0f), trend_score(0.0f), smoothed_vol(0.0f), _reserved_padding{} {}
};
static_assert(sizeof(BTCState) == 64, "BTCState must be exactly 64 bytes");

struct alignas(64) BTCAdvisory {
    float recommended_weight; // 0.0 - 1.0
    float risk_score;         // 0 - 100
    bool risk_elevated;
    bool growth_favorable;
    std::uint8_t _reserved_padding[54]; // 64 - (4 + 4 + 1 + 1) = 54 bytes

    constexpr BTCAdvisory() : recommended_weight(1.0f), risk_score(0.0f), risk_elevated(false), growth_favorable(true), _reserved_padding{} {}
};
static_assert(sizeof(BTCAdvisory) == 64, "BTCAdvisory must be exactly 64 bytes");

struct alignas(64) BTCObservabilityMetrics {
    float risk_score;
    float volatility_band;
    float advisory_weight;
    float trend_score;
    std::uint8_t _reserved_padding[48]; // 64 - 4*4 = 48 bytes

    constexpr BTCObservabilityMetrics() : risk_score(0.0f), volatility_band(0.0f), advisory_weight(0.0f), trend_score(0.0f), _reserved_padding{} {}
};
static_assert(sizeof(BTCObservabilityMetrics) == 64, "BTCObservabilityMetrics must be exactly 64 bytes");


// ============================================================================
// ADVISORY LOGIC
// ============================================================================

[[nodiscard]] constexpr BTCAdvisory evaluate_btc_state(
    const BTCState& state,
    const SafetyState* safety
) noexcept {
    // Treat different aspects as a "risk vector" for smoothing
    float risk_values[3] = {
        state.realized_vol * 100.0f,  // Scale vol up for score (e.g. 0.05 -> 5.0)
        state.drawdown * 100.0f,      // Scale drawdown
        (state.trend_score < 0.0f ? -state.trend_score * 50.0f : 0.0f) // Penalize negative trend
    };

    float confidences[3] = {1.0f, 1.0f, 0.8f};

    float smoothed_risk = AILLE::Math::fibonacci_weighted_average(risk_values, confidences, 3);

    BTCAdvisory advisory{};

    // Scale risk score between 0 and 100
    if (smoothed_risk < 0.0f) smoothed_risk = 0.0f;
    if (smoothed_risk > 100.0f) smoothed_risk = 100.0f;

    advisory.risk_score = smoothed_risk;

    // Determine flags based on risk and trend
    advisory.risk_elevated = (advisory.risk_score > 60.0f) || (state.drawdown > 0.15f);
    advisory.growth_favorable = (!advisory.risk_elevated && state.trend_score > 0.1f);

    // Calculate weight: invert risk (e.g., 0 risk = 1.0 weight, 100 risk = 0.0 weight)
    advisory.recommended_weight = 1.0f - (advisory.risk_score / 100.0f);

    if (advisory.recommended_weight < 0.0f) advisory.recommended_weight = 0.0f;
    if (advisory.recommended_weight > 1.0f) advisory.recommended_weight = 1.0f;

    // Safety integration (Advisory only!)
    if (safety != nullptr) {
        if (safety->hardware_fault || safety->kill_switch) {
            advisory.risk_elevated = true;
            advisory.growth_favorable = false;
            advisory.risk_score = 100.0f;
            advisory.recommended_weight = 0.0f; // Recommend no weight during system failure
        }
    }

    return advisory;
}

} // namespace AILLE

#endif // AILLE_BTC_HPP