/*
 * AILLE Framework - Real-Time Market Condition Intelligence & Anomaly Detection (Layer 16)
 * AI-Load Integrity and Layered Evaluation
 *
 * Deterministic, allocator-free real-time market condition monitoring layer.
 * Focuses on volatility expansion, liquidity displacement, and correlation break detection.
 * Emits non-directive, cautionary advisories with zero execution recommendations.
 *
 * Version Tag: ANOMALY_DETECTION_V1
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#ifndef AILLE_ANOMALY_HPP
#define AILLE_ANOMALY_HPP

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>
#include <cstring>
#include "../aille.hpp"

namespace AILLE {

// ============================================================================
// VERSION TAG
// ============================================================================

constexpr const char* ANOMALY_DETECTION_VERSION = "ANOMALY_DETECTION_V1";

// ============================================================================
// CORE STRUCTS (STRICTLY 64-BYTE CACHE-ALIGNED & ALLOCATOR-FREE)
// ============================================================================

struct alignas(64) AnomalyState final {
    float last_price;             ///< Latest asset trade price
    float volume;                 ///< Latest interval volume
    float bid_size;               ///< Top-of-book L1 bid size
    float ask_size;               ///< Top-of-book L1 ask size
    float ewma_volatility;        ///< Fast intraday EWMA volatility estimate
    float baseline_volatility;    ///< Historical 30-day baseline volatility
    float baseline_depth;         ///< Historical average top-of-book total depth (bid_size + ask_size)
    float pair_last_price;        ///< Correlated pair asset trade price
    float rolling_correlation;    ///< Calculated rolling Pearson correlation with pair asset
    float expected_correlation;   ///< Historical benchmark correlation for asset pair
    std::uint8_t vol_debounce_count;  ///< Consecutive volatility anomaly observations
    std::uint8_t liq_debounce_count;  ///< Consecutive liquidity displacement observations
    std::uint8_t corr_debounce_count; ///< Consecutive correlation break observations
    std::uint8_t symbol_id;           ///< Numeric symbol identifier
    std::uint8_t _padding[20];

    constexpr AnomalyState()
        : last_price(0.0f), volume(0.0f), bid_size(0.0f), ask_size(0.0f),
          ewma_volatility(0.0f), baseline_volatility(0.0f), baseline_depth(0.0f),
          pair_last_price(0.0f), rolling_correlation(1.0f), expected_correlation(1.0f),
          vol_debounce_count(0), liq_debounce_count(0), corr_debounce_count(0),
          symbol_id(0), _padding{} {}
};
static_assert(sizeof(AnomalyState) == 64, "AnomalyState must be exactly 64 bytes");

struct alignas(64) AnomalyAdvisory final {
    float volatility_expansion_ratio; ///< Ratio of intraday EWMA to 30-day baseline volatility
    float depth_thinning_pct;        ///< Depth thinning percentage relative to baseline [0.0, 1.0]
    float rolling_correlation;       ///< Active rolling correlation
    float anomaly_severity;          ///< Composite anomaly risk score [0.0, 100.0]
    std::uint8_t volatility_anomaly; ///< 1 if volatility expansion exceeds threshold, 0 otherwise
    std::uint8_t liquidity_anomaly;  ///< 1 if liquidity displacement detected, 0 otherwise
    std::uint8_t correlation_break;  ///< 1 if correlation break detected, 0 otherwise
    std::uint8_t advisory_active;   ///< 1 if any anomaly is active post-debouncing, 0 otherwise
    std::uint8_t vol_debounce_active; ///< 1 if volatility debouncing active
    std::uint8_t liq_debounce_active; ///< 1 if liquidity debouncing active
    std::uint8_t corr_debounce_active;///< 1 if correlation debouncing active
    std::uint8_t _reserved0;
    std::uint8_t _padding[40];

    constexpr AnomalyAdvisory()
        : volatility_expansion_ratio(1.0f), depth_thinning_pct(0.0f),
          rolling_correlation(1.0f), anomaly_severity(0.0f),
          volatility_anomaly(0), liquidity_anomaly(0), correlation_break(0),
          advisory_active(0), vol_debounce_active(0), liq_debounce_active(0),
          corr_debounce_active(0), _reserved0(0), _padding{} {}
};
static_assert(sizeof(AnomalyAdvisory) == 64, "AnomalyAdvisory must be exactly 64 bytes");

struct alignas(64) AnomalyObservabilityMetrics final {
    float volatility_expansion_ratio;
    float depth_thinning_pct;
    float rolling_correlation;
    float anomaly_severity;
    std::uint8_t advisory_active;
    std::uint8_t _reserved[3];
    std::uint8_t _padding[44];

    constexpr AnomalyObservabilityMetrics()
        : volatility_expansion_ratio(1.0f), depth_thinning_pct(0.0f),
          rolling_correlation(1.0f), anomaly_severity(0.0f),
          advisory_active(0), _reserved{}, _padding{} {}
};
static_assert(sizeof(AnomalyObservabilityMetrics) == 64, "AnomalyObservabilityMetrics must be exactly 64 bytes");

struct alignas(64) AnomalyTraceStep final {
    std::uint64_t timestamp_ns;        ///< Nanosecond resolution evaluation timestamp
    float volatility_expansion_ratio;  ///< Logged volatility expansion ratio
    float depth_thinning_pct;         ///< Logged depth thinning percentage
    float rolling_correlation;        ///< Logged rolling correlation
    float anomaly_severity;           ///< Logged anomaly severity score
    std::uint32_t anomaly_type_mask;  ///< Bit 0: vol, Bit 1: liq, Bit 2: corr
    std::uint32_t symbol_id;          ///< Symbol ID evaluated
    std::uint8_t _padding[24];

    constexpr AnomalyTraceStep()
        : timestamp_ns(0), volatility_expansion_ratio(1.0f), depth_thinning_pct(0.0f),
          rolling_correlation(1.0f), anomaly_severity(0.0f),
          anomaly_type_mask(0), symbol_id(0), _padding{} {}
};
static_assert(sizeof(AnomalyTraceStep) == 64, "AnomalyTraceStep must be exactly 64 bytes");

struct alignas(64) AnomalyConfig final {
    float volatility_threshold;       ///< Multiple above baseline to trigger vol anomaly (e.g., 2.5x)
    float depth_thinning_threshold;   ///< Fractional depth drop to trigger liq anomaly (e.g., 0.50 = 50% drop)
    float min_expected_correlation;   ///< Absolute drop in correlation from benchmark to trigger break (e.g., 0.40)
    std::uint8_t vol_debounce_target;  ///< Required consecutive ticks for volatility advisory (e.g., 3)
    std::uint8_t liq_debounce_target;  ///< Required consecutive ticks for liquidity advisory (e.g., 3)
    std::uint8_t corr_debounce_target; ///< Required consecutive ticks for correlation advisory (e.g., 5)
    std::uint8_t _reserved[1];
    std::uint8_t _padding[48];

    constexpr AnomalyConfig()
        : volatility_threshold(2.5f), depth_thinning_threshold(0.50f),
          min_expected_correlation(0.40f), vol_debounce_target(3),
          liq_debounce_target(3), corr_debounce_target(5),
          _reserved{}, _padding{} {}
};
static_assert(sizeof(AnomalyConfig) == 64, "AnomalyConfig must be exactly 64 bytes");

// ============================================================================
// STREAM ALGORITHMS & ADVISORY EVALUATION FUNCTION
// ============================================================================

/**
 * @brief Pure functional, allocator-free evaluation of Layer 16 Market Condition Anomalies.
 *
 * @param state Input market state snapshot and rolling metrics.
 * @param config Threshold and sensitivity parameters.
 * @param safety Optional safety state override.
 * @return AnomalyAdvisory Evaluated non-directive advisory posture.
 */
[[nodiscard]] AnomalyAdvisory evaluate_anomaly_advisory(
    const AnomalyState& state,
    const AnomalyConfig& config = AnomalyConfig(),
    const SafetyState* safety = nullptr
) noexcept;

} // namespace AILLE

#endif // AILLE_ANOMALY_HPP
