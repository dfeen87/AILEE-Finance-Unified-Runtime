/*
 * AILLE Framework - Macro Risk & Growth Advisory Module (MSM)
 * AI-Load Integrity and Layered Evaluation
 *
 * Standalone advisory-only module for macro signal evaluation,
 * risk scoring, and recommended macro weights.
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#ifndef AILLE_MACRO_HPP
#define AILLE_MACRO_HPP

#include <cstdint>
#include <cstddef>
#include "../aille.hpp"
#include "aille_math.hpp"

namespace AILLE {

// ============================================================================
// CORE DATA STRUCTURES
// ============================================================================

struct alignas(64) MacroSignalState final {
    float usd_strength;
    float commodity_pressure;
    float crypto_sentiment;
    float macro_volatility;
    float risk_on_score;
    float inflation_pressure;
    float recession_pressure;
    std::uint8_t _padding[64 - 4 * 7]; // 64 - 28 = 36 bytes padding

    constexpr MacroSignalState() : usd_strength(0.0f), commodity_pressure(0.0f),
                                   crypto_sentiment(0.0f), macro_volatility(0.0f),
                                   risk_on_score(0.0f), inflation_pressure(0.0f),
                                   recession_pressure(0.0f), _padding{} {}
};
static_assert(sizeof(MacroSignalState) == 64, "MacroSignalState must be exactly 64 bytes");

struct alignas(64) MacroSignalAdvisory final {
    float        macro_risk_score;        // 4
    float        recommended_macro_weight;// 4
    std::uint8_t risk_elevated;           // 1 (0 or 1)
    std::uint8_t growth_favorable;        // 1 (0 or 1)
    std::uint8_t _reserved0[2];           // 2 (alignment to 4-byte boundary)

    float        smoothed_macro_volatility; // 4
    float        macro_trend_score;         // 4
    float        macro_pressure_index;      // 4
    float        risk_on_intensity;         // 4

    std::uint8_t _padding[64
        - 4  // macro_risk_score
        - 4  // recommended_macro_weight
        - 1  // risk_elevated
        - 1  // growth_favorable
        - 2  // _reserved0
        - 4  // smoothed_macro_volatility
        - 4  // macro_trend_score
        - 4  // macro_pressure_index
        - 4  // risk_on_intensity
    ]; // 64 - 28 = 36 bytes padding

    constexpr MacroSignalAdvisory() : macro_risk_score(0.0f), recommended_macro_weight(1.0f),
                                      risk_elevated(0), growth_favorable(1), _reserved0{},
                                      smoothed_macro_volatility(0.0f), macro_trend_score(0.0f),
                                      macro_pressure_index(0.0f), risk_on_intensity(0.0f), _padding{} {}
};
static_assert(sizeof(MacroSignalAdvisory) == 64, "MacroSignalAdvisory must be exactly 64 bytes");

struct alignas(64) MacroSignalObservabilityMetrics final {
    float macro_risk_score;
    float recommended_macro_weight;
    float macro_pressure_index;
    float risk_on_intensity;
    std::uint8_t _padding[64 - 4 * 4]; // 64 - 16 = 48 bytes padding

    constexpr MacroSignalObservabilityMetrics() : macro_risk_score(0.0f), recommended_macro_weight(0.0f),
                                                  macro_pressure_index(0.0f), risk_on_intensity(0.0f), _padding{} {}
};
static_assert(sizeof(MacroSignalObservabilityMetrics) == 64, "MacroSignalObservabilityMetrics must be exactly 64 bytes");

// ============================================================================
// ADVISORY LOGIC
// ============================================================================

[[nodiscard]] constexpr MacroSignalAdvisory evaluate_macro_advisory(
    const MacroSignalState& state,
    const SafetyState* safety
) noexcept {
    MacroSignalAdvisory advisory{};

    // Normalize inputs (assuming inputs arrive relatively normalized between 0-1)
    auto clamp01 = [](float v) constexpr {
        return (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);
    };

    float u_str = clamp01(state.usd_strength);
    float c_pres = clamp01(state.commodity_pressure);
    float i_pres = clamp01(state.inflation_pressure);
    float r_pres = clamp01(state.recession_pressure);
    float r_on = clamp01(state.risk_on_score);
    float c_sent = clamp01(state.crypto_sentiment);
    float m_vol = clamp01(state.macro_volatility);

    // Basic weights for macro pressure index
    float w_usd = 0.2f;
    float w_commod = 0.15f;
    float w_infl = 0.2f;
    float w_recess = 0.25f;
    float w_riskon = 0.1f;
    float w_crypto = 0.1f;

    // Compute macro pressure index
    float pressure_index =
        w_usd * (1.0f - u_str) + // Weaken USD indicates pressure
        w_commod * c_pres +
        w_infl * i_pres +
        w_recess * r_pres -
        w_riskon * r_on -
        w_crypto * c_sent;

    advisory.macro_pressure_index = pressure_index;
    advisory.risk_on_intensity = r_on;

    // Smooth volatility and pressure index using Fibonacci weighted average
    float risk_values[3] = {
        m_vol * 100.0f,
        pressure_index * 100.0f,
        r_pres * 100.0f
    };

    float confidences[3] = {1.0f, 1.0f, 0.8f};

    float smoothed_risk = AILLE::Math::fibonacci_weighted_average(risk_values, confidences, 3);

    advisory.smoothed_macro_volatility = m_vol;
    advisory.macro_trend_score = pressure_index;

    // Scale risk score between 0 and 100
    if (smoothed_risk < 0.0f) smoothed_risk = 0.0f;
    if (smoothed_risk > 100.0f) smoothed_risk = 100.0f;

    advisory.macro_risk_score = smoothed_risk;

    advisory.risk_elevated = (advisory.macro_risk_score >= 70.0f) ? 1 : 0;
    advisory.growth_favorable = ((state.risk_on_score >= 0.6f) && (advisory.macro_risk_score <= 40.0f)) ? 1 : 0;

    advisory.recommended_macro_weight = 1.0f - (advisory.macro_risk_score / 100.0f);

    if (advisory.recommended_macro_weight < 0.0f) advisory.recommended_macro_weight = 0.0f;
    if (advisory.recommended_macro_weight > 1.0f) advisory.recommended_macro_weight = 1.0f;

    // Safety integration (Advisory only!)
    if (safety != nullptr) {
        if (safety->hardware_fault || safety->kill_switch) {
            advisory.risk_elevated = 1;
            advisory.growth_favorable = 0;
            advisory.macro_risk_score = 100.0f;
            advisory.recommended_macro_weight = 0.0f; // Recommend no weight during system failure
        }
    }

    return advisory;
}

} // namespace AILLE

#endif // AILLE_MACRO_HPP
