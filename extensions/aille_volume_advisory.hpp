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
    float prev_volume_anomaly_ratio; ///< Previous interval's volume anomaly ratio
    float prev_recommended_weight;   ///< Previous interval's recommended weight
    bool is_index_etf;             ///< True for SPY/QQQ index ETFs (enables looser oversold thresholding)
    std::int8_t contrarian_override; ///< 0: follow config, 1: force enable, -1: force disable
    std::uint8_t _reserved_padding[64 - 7 * sizeof(float) - sizeof(bool) - sizeof(std::int8_t)];

    constexpr VolumeState()
        : current_volume(0.0f), avg_volume(0.0f), volume_anomaly_ratio(0.0f),
          price_change(0.0f), vwap_deviation(0.0f),
          prev_volume_anomaly_ratio(0.0f), prev_recommended_weight(-1.0f),
          is_index_etf(false), contrarian_override(0),
          _reserved_padding{} {}
};
static_assert(sizeof(VolumeState) == 64, "VolumeState must be exactly 64 bytes");

struct alignas(64) VolumeAdvisory {
    float recommended_weight;      ///< Proportional suggested weight [0.0, 1.0]
    float risk_score;              ///< Risk classification [0.0, 100.0]
    float oversold_score;          ///< Multi-factor oversold score [0.0, +inf)
    bool risk_elevated;            ///< True if high-risk anomalies are active
    bool growth_favorable;         ///< True if strong volume-price accumulation
    bool oversold_state;           ///< True if multi-factor oversold condition triggered
    bool contrarian_buy_signal;    ///< True if contrarian buy signal is active
    std::uint8_t _reserved_padding[64 - 3 * sizeof(float) - 4 * sizeof(bool)];

    constexpr VolumeAdvisory()
        : recommended_weight(1.0f), risk_score(0.0f), oversold_score(0.0f),
          risk_elevated(false), growth_favorable(true),
          oversold_state(false), contrarian_buy_signal(false),
          _reserved_padding{} {}
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
    const SafetyState* safety,
    const MarketStabilizerAdvisory* stabilizer = nullptr,
    bool enable_contrarian = false,
    float aggressiveness = 1.0f
) noexcept {
    VolumeAdvisory advisory{};

    // 1. Safety precedence: Hardware faults or kill switches immediately override and yield zero weight
    if (safety && (safety->hardware_fault || safety->kill_switch)) {
        advisory.recommended_weight = 0.0f;
        advisory.risk_score = 100.0f;
        advisory.risk_elevated = true;
        advisory.growth_favorable = false;
        advisory.oversold_score = 0.0f;
        advisory.oversold_state = false;
        advisory.contrarian_buy_signal = false;
        return advisory;
    }

    // Exponential smoothing on volume anomaly ratio
    float smoothed_ratio = state.volume_anomaly_ratio;
    if (state.prev_volume_anomaly_ratio > 0.0f) {
        smoothed_ratio = 0.2f * state.volume_anomaly_ratio + 0.8f * state.prev_volume_anomaly_ratio;
    }

    // Compute Multi-Factor Oversold Score
    float norm_price = (-state.price_change - 0.007f) / 0.015f;
    if (norm_price < 0.0f) norm_price = 0.0f;

    float norm_vwap = (-state.vwap_deviation - 0.005f) / 0.015f;
    if (norm_vwap < 0.0f) norm_vwap = 0.0f;

    float norm_vol = (smoothed_ratio - 1.5f) / 2.0f;
    if (norm_vol < 0.0f) norm_vol = 0.0f;

    advisory.oversold_score = (0.4f * norm_price + 0.3f * norm_vwap + 0.3f * norm_vol) * aggressiveness;

    // Conditions A and B
    bool cond_a = (state.price_change <= -0.012f) && (state.vwap_deviation <= -0.008f) && (smoothed_ratio >= 2.5f);
    bool cond_b = (state.price_change <= -0.007f) && (state.vwap_deviation <= -0.005f) && (smoothed_ratio >= 1.8f);

    if (state.is_index_etf) {
        advisory.oversold_state = (advisory.oversold_score >= 0.6f) || cond_a || cond_b;
    } else {
        advisory.oversold_state = (advisory.oversold_score >= 1.0f) || cond_a;
    }

    // Determine effective contrarian enabling
    bool contrarian_active = enable_contrarian;
    if (state.contrarian_override == 1) {
        contrarian_active = true;
    } else if (state.contrarian_override == -1) {
        contrarian_active = false;
    }

    advisory.contrarian_buy_signal = contrarian_active && advisory.oversold_state;

    // Honest baseline risk score calculation
    float vol_risk = smoothed_ratio * 15.0f;
    float price_risk = (state.price_change < 0.0f) ? std::abs(state.price_change) * 200.0f : 0.0f;
    float vwap_risk = std::abs(state.vwap_deviation) * 150.0f;

    float raw_risk = vol_risk + price_risk + vwap_risk;
    if (raw_risk > 100.0f) raw_risk = 100.0f;
    if (raw_risk < 0.0f) raw_risk = 0.0f;

    advisory.risk_score = raw_risk;
    advisory.risk_elevated = (advisory.risk_score > 60.0f) || (smoothed_ratio > 4.0f && state.price_change < -0.01f);
    advisory.growth_favorable = (!advisory.risk_elevated) && (smoothed_ratio > 1.2f) && (state.price_change > 0.0f);

    advisory.recommended_weight = 1.0f - (advisory.risk_score / 100.0f);

    // Apply Contrarian Weight Multiplier if contrarian buy signal active
    if (advisory.contrarian_buy_signal) {
        float multiplier = 1.15f;
        if (advisory.oversold_score >= 0.9f || cond_a) {
            multiplier = 1.30f;
        }
        advisory.recommended_weight *= multiplier;
    }

    // Market Stabilizer (MSGAM) Coupling & Clamping
    if (stabilizer != nullptr) {
        advisory.recommended_weight *= stabilizer->stabilization_factor;
        if (stabilizer->risk_elevated) {
            advisory.risk_elevated = true;
            advisory.growth_favorable = false;
            advisory.risk_score = (stabilizer->stabilization_risk_score > advisory.risk_score) ? stabilizer->stabilization_risk_score : advisory.risk_score;
        }
    }

    if (advisory.recommended_weight > 1.0f) advisory.recommended_weight = 1.0f;
    if (advisory.recommended_weight < 0.0f) advisory.recommended_weight = 0.0f;

    // Temporal Step Clamping (Drift Control)
    if (state.prev_recommended_weight >= 0.0f && state.prev_recommended_weight <= 1.0f) {
        float diff = advisory.recommended_weight - state.prev_recommended_weight;
        if (diff > 0.15f) {
            advisory.recommended_weight = state.prev_recommended_weight + 0.15f;
        } else if (diff < -0.15f) {
            advisory.recommended_weight = state.prev_recommended_weight - 0.15f;
        }
    }

    if (advisory.recommended_weight > 1.0f) advisory.recommended_weight = 1.0f;
    if (advisory.recommended_weight < 0.0f) advisory.recommended_weight = 0.0f;

    return advisory;
}

} // namespace AILLE

#endif // AILLE_VOLUME_ADVISORY_HPP
