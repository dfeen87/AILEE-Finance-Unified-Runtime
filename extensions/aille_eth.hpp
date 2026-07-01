/*
 * AILLE Framework - ETH Risk & Growth Advisory Module (ERGAM)
 * AI-Load Integrity and Layered Evaluation
 *
 * Standalone advisory-only module for ETH volatility exposure reduction,
 * fast drawdown mitigation, and controlled growth.
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#ifndef AILLE_ETH_HPP
#define AILLE_ETH_HPP

#include <cstdint>
#include <cstddef>
#include "../aille.hpp"
#include "aille_math.hpp"

namespace AILLE {

// ============================================================================
// CORE DATA STRUCTURES
// ============================================================================

struct alignas(64) ETHState {
    float price;
    float realized_vol;
    float drawdown;
    float trend_score;
    float smoothed_vol;
    std::uint8_t _reserved_padding[44]; // 64 - 5*4 = 44 bytes

    constexpr ETHState() : price(0.0f), realized_vol(0.0f), drawdown(0.0f), trend_score(0.0f), smoothed_vol(0.0f), _reserved_padding{} {}
};
static_assert(sizeof(ETHState) == 64, "ETHState must be exactly 64 bytes");

struct alignas(64) ETHAdvisory {
    float recommended_weight; // 0.0 - 1.0
    float risk_score;         // 0 - 100
    bool risk_elevated;
    bool growth_favorable;
    std::uint8_t _reserved_padding[54]; // 64 - (4 + 4 + 1 + 1) = 54 bytes

    constexpr ETHAdvisory() : recommended_weight(1.0f), risk_score(0.0f), risk_elevated(false), growth_favorable(true), _reserved_padding{} {}
};
static_assert(sizeof(ETHAdvisory) == 64, "ETHAdvisory must be exactly 64 bytes");

struct alignas(64) ETHObservabilityMetrics {
    float risk_score;
    float volatility_band;
    float advisory_weight;
    float trend_score;
    std::uint8_t _reserved_padding[48]; // 64 - 4*4 = 48 bytes

    constexpr ETHObservabilityMetrics() : risk_score(0.0f), volatility_band(0.0f), advisory_weight(0.0f), trend_score(0.0f), _reserved_padding{} {}
};
static_assert(sizeof(ETHObservabilityMetrics) == 64, "ETHObservabilityMetrics must be exactly 64 bytes");


// ============================================================================
// ADVISORY LOGIC
// ============================================================================

[[nodiscard]] constexpr ETHAdvisory evaluate_eth_state(
    const ETHState& state,
    const SafetyState* safety
) noexcept {
    // Construct local arrays from scalar state
    float risk_values[4] = {
        state.realized_vol * 100.0f,  // Scale vol up for score
        state.drawdown * 100.0f,      // Scale drawdown
        (state.trend_score < 0.0f ? -state.trend_score * 50.0f : 0.0f), // Penalize negative trend
        state.smoothed_vol * 100.0f
    };

    float confidences[4] = {1.0f, 1.0f, 0.8f, 1.0f};

    // Calculate smoothed risk deterministically
    float smoothed_risk = AILLE::Math::fibonacci_weighted_average(risk_values, confidences, 4);

    ETHAdvisory advisory{};

    // Scale risk score bounded to 0-100
    if (smoothed_risk < 0.0f) smoothed_risk = 0.0f;
    if (smoothed_risk > 100.0f) smoothed_risk = 100.0f;

    advisory.risk_score = smoothed_risk;

    // Determine flags
    advisory.risk_elevated = (advisory.risk_score > 70.0f) || (state.drawdown > 0.20f);
    advisory.growth_favorable = (!advisory.risk_elevated && state.trend_score > 0.05f);

    // Calculate weight: invert risk
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

#endif // AILLE_ETH_HPP
