/*
 * AILLE Framework - Intraday Volume Advisory Module (VAM)
 * AI-Load Integrity and Layered Evaluation
 *
 * Advisory-only deterministic risk evaluation for SPY and QQQ intraday volume behaviors.
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#ifndef AILLE_VOLUME_ADVISORY_HPP
#define AILLE_VOLUME_ADVISORY_HPP

#include <cstdint>
#include <cstddef>
#include "../aille.hpp"
#include "aille_math.hpp"

namespace AILLE {

// ============================================================================
// CORE DATA STRUCTURES
// ============================================================================

struct alignas(64) VolumeState {
    float current_volume;          ///< Intraday active bar volume
    float avg_volume;              ///< Baseline historical average volume for this interval/time of day
    float volume_anomaly_ratio;    ///< Ratio of current_volume to avg_volume
    float price_change;            ///< (Close - Open) / (Open + epsilon) of the intraday interval
    float vwap_deviation;          ///< Percent deviation from Volume-Weighted Average Price
    std::uint8_t _reserved_padding[64 - 5 * sizeof(float)];

    constexpr VolumeState()
        : current_volume(0.0f), avg_volume(0.0f), volume_anomaly_ratio(0.0f),
          price_change(0.0f), vwap_deviation(0.0f), _reserved_padding{} {}
};
static_assert(sizeof(VolumeState) == 64, "VolumeState must be exactly 64 bytes");

struct alignas(64) VolumeAdvisory {
    float recommended_weight;      ///< Proportional suggested weight [0.0, 1.0]
    float risk_score;              ///< Risk classification [0.0, 100.0]
    bool risk_elevated;            ///< True if high-risk anomalies are active
    bool growth_favorable;         ///< True if strong volume-price accumulation
    std::uint8_t _reserved_padding[64 - 2 * sizeof(float) - 2 * sizeof(bool)];

    constexpr VolumeAdvisory()
        : recommended_weight(1.0f), risk_score(0.0f), risk_elevated(false),
          growth_favorable(true), _reserved_padding{} {}
};
static_assert(sizeof(VolumeAdvisory) == 64, "VolumeAdvisory must be exactly 64 bytes");

struct alignas(64) VolumeObservabilityMetrics {
    float risk_score;
    float volume_anomaly_ratio;
    float advisory_weight;
    float vwap_deviation;
    std::uint8_t _reserved_padding[64 - 4 * sizeof(float)];

    constexpr VolumeObservabilityMetrics()
        : risk_score(0.0f), volume_anomaly_ratio(0.0f), advisory_weight(1.0f),
          vwap_deviation(0.0f), _reserved_padding{} {}
};
static_assert(sizeof(VolumeObservabilityMetrics) == 64, "VolumeObservabilityMetrics must be exactly 64 bytes");

// ============================================================================
// ADVISORY EVALUATION
// ============================================================================

[[nodiscard]] constexpr VolumeAdvisory evaluate_volume_state(
    const VolumeState& state,
    const SafetyState* safety
) noexcept {
    VolumeAdvisory advisory{};

    // Handle safety conditions (kill switch or hardware faults)
    if (safety && (safety->hardware_fault || safety->kill_switch)) {
        advisory.recommended_weight = 0.0f;
        advisory.risk_score = 100.0f;
        advisory.risk_elevated = true;
        advisory.growth_favorable = false;
        return advisory;
    }

    // Evaluate risk based on volume anomaly ratio and price behavior/VWAP deviation
    // Highly anomalous volume accompanied by negative price drift or high deviation from VWAP increases risk.
    float vol_risk = state.volume_anomaly_ratio * 15.0f; // e.g. an anomaly ratio of 5.0x yields 75.0% risk before constraints
    float price_risk = (state.price_change < 0.0f) ? std::abs(state.price_change) * 200.0f : 0.0f;
    float vwap_risk = std::abs(state.vwap_deviation) * 150.0f;

    float raw_risk = vol_risk + price_risk + vwap_risk;
    if (raw_risk > 100.0f) raw_risk = 100.0f;
    if (raw_risk < 0.0f) raw_risk = 0.0f;

    advisory.risk_score = raw_risk;
    advisory.risk_elevated = (advisory.risk_score > 60.0f) || (state.volume_anomaly_ratio > 4.0f && state.price_change < -0.01f);
    advisory.growth_favorable = (!advisory.risk_elevated) && (state.volume_anomaly_ratio > 1.2f) && (state.price_change > 0.0f);

    advisory.recommended_weight = 1.0f - (advisory.risk_score / 100.0f);
    if (advisory.recommended_weight > 1.0f) advisory.recommended_weight = 1.0f;
    if (advisory.recommended_weight < 0.0f) advisory.recommended_weight = 0.0f;

    return advisory;
}

} // namespace AILLE

#endif // AILLE_VOLUME_ADVISORY_HPP
