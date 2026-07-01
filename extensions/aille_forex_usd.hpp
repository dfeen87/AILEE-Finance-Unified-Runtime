/*
 * AILLE Framework - FOREX-USD Advisory Module (FRGAM)
 * AI-Load Integrity and Layered Evaluation
 *
 * Advisory-only deterministic risk evaluation for USD FX behavior.
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#ifndef AILLE_FOREX_USD_HPP
#define AILLE_FOREX_USD_HPP

#include <cstdint>
#include <cstddef>
#include "../aille.hpp"
#include "aille_math.hpp"

namespace AILLE {

// ============================================================================
// CORE DATA STRUCTURES
// ============================================================================

struct alignas(64) ForexUSDState {
    float usd_index;          // e.g., DXY or synthetic USD strength
    float realized_vol;
    float drawdown;
    float trend_score;
    float smoothed_vol;
    std::uint8_t _reserved_padding[64 - 5 * sizeof(float)];

    constexpr ForexUSDState() : usd_index(0.0f), realized_vol(0.0f), drawdown(0.0f), trend_score(0.0f), smoothed_vol(0.0f), _reserved_padding{} {}
};
static_assert(sizeof(ForexUSDState) == 64, "ForexUSDState must be exactly 64 bytes");

struct alignas(64) ForexUSDAdvisory {
    float recommended_weight;   // 0.0–1.0 (USD exposure weight)
    float risk_score;           // 0–100
    bool risk_elevated;
    bool growth_favorable;
    std::uint8_t _reserved_padding[64 - 2 * sizeof(float) - 2 * sizeof(bool)];

    constexpr ForexUSDAdvisory() : recommended_weight(1.0f), risk_score(0.0f), risk_elevated(false), growth_favorable(true), _reserved_padding{} {}
};
static_assert(sizeof(ForexUSDAdvisory) == 64, "ForexUSDAdvisory must be exactly 64 bytes");

struct alignas(64) ForexUSDObservabilityMetrics {
    float risk_score;
    float volatility_band;
    float advisory_weight;
    float trend_score;
    std::uint8_t _reserved_padding[64 - 4 * sizeof(float)];

    constexpr ForexUSDObservabilityMetrics() : risk_score(0.0f), volatility_band(0.0f), advisory_weight(1.0f), trend_score(0.0f), _reserved_padding{} {}
};
static_assert(sizeof(ForexUSDObservabilityMetrics) == 64, "ForexUSDObservabilityMetrics must be exactly 64 bytes");

// ============================================================================
// ADVISORY EVALUATION
// ============================================================================

[[nodiscard]] constexpr ForexUSDAdvisory evaluate_forex_usd_state(
    const ForexUSDState& state,
    const SafetyState* safety
) noexcept {
    ForexUSDAdvisory advisory{};

    // If hardware fault or kill switch is active, fallback safely
    if (safety && (safety->hardware_fault || safety->kill_switch)) {
        advisory.recommended_weight = 0.0f;
        advisory.risk_score = 100.0f;
        advisory.risk_elevated = true;
        advisory.growth_favorable = false;
        return advisory;
    }

    float values[4] = {
        state.realized_vol,
        state.drawdown,
        state.trend_score,
        state.smoothed_vol
    };

    float confidences[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    float smoothed_risk = AILLE::Math::fibonacci_weighted_average(values, confidences, 4);

    advisory.risk_score = smoothed_risk * 100.0f;

    if (advisory.risk_score > 100.0f) advisory.risk_score = 100.0f;
    if (advisory.risk_score < 0.0f) advisory.risk_score = 0.0f;

    advisory.recommended_weight = 1.0f - (advisory.risk_score / 100.0f);

    if (advisory.recommended_weight > 1.0f) advisory.recommended_weight = 1.0f;
    if (advisory.recommended_weight < 0.0f) advisory.recommended_weight = 0.0f;

    advisory.risk_elevated = (advisory.risk_score > 75.0f);
    advisory.growth_favorable = (advisory.risk_score < 40.0f) && (state.trend_score > 0.0f);

    return advisory;
}

} // namespace AILLE

#endif // AILLE_FOREX_USD_HPP
