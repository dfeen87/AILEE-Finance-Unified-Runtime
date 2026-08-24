/*
 * AILLE Framework - Real-Time Chart Intelligence & Environment Diagnostics Subsystem (Layer 17)
 * AI-Load Integrity and Layered Evaluation
 *
 * Deterministic, allocator-free technical indicator and environment diagnostic layer.
 * Operates on AnomalyState, VolumeState, and BaselineState structs to compute diagnostic indicators.
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#ifndef AILLE_CHART_INTELLIGENCE_HPP
#define AILLE_CHART_INTELLIGENCE_HPP

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include "../aille.hpp"
#include "aille_anomaly.hpp"
#include "aille_volume_advisory.hpp"

namespace AILLE {

// ============================================================================
// VERSION TAG & CONSTANTS
// ============================================================================

constexpr const char* CHART_INTELLIGENCE_VERSION = "CHART_INTELLIGENCE_V1";
constexpr std::size_t MAX_INDICATORS = 16;

// ============================================================================
// ENUMS FOR INDICATOR TYPES & STATES
// ============================================================================

enum class ChartIndicatorType : std::uint8_t {
    VolatilityExpansionBands = 0,
    LiquidityDisplacementZones = 1,
    CorrelationDivergenceIndex = 2,
    BaselineStrengthMeter = 3,
    PatternEnvironmentScore = 4
};

enum class ChartConditionState : std::uint8_t {
    Neutral = 0,
    Compression = 1,
    Expansion = 2,
    Displaced = 3,
    Diverging = 4,
    Broken = 5,
    StrongSupport = 6,
    Consolidation = 7,
    Stressed = 8,
    Weak = 9,
    Average = 10,
    Strong = 11
};

enum class PatternHint : std::uint8_t {
    None = 0,
    CupHandleLike = 1,
    PennantLike = 2,
    FlagLike = 3
};

// ============================================================================
// CORE STRUCTS (STRICTLY 64-BYTE CACHE-ALIGNED & ALLOCATOR-FREE)
// ============================================================================

struct alignas(64) BaselineState final {
    float vol_5m;          ///< 5-minute horizon volatility baseline
    float vol_1h;          ///< 1-hour horizon volatility baseline
    float vol_30d;         ///< 30-day horizon volatility baseline
    float liq_5m;          ///< 5-minute horizon liquidity depth baseline
    float liq_1h;          ///< 1-hour horizon liquidity depth baseline
    float liq_30d;         ///< 30-day horizon liquidity depth baseline
    float vol_corr_5m;     ///< 5-minute horizon correlation baseline
    float vol_corr_1h;     ///< 1-hour horizon correlation baseline
    float vol_corr_30d;    ///< 30-day horizon correlation baseline
    std::uint8_t _padding[28];

    constexpr BaselineState()
        : vol_5m(0.0f), vol_1h(0.0f), vol_30d(0.0f),
          liq_5m(0.0f), liq_1h(0.0f), liq_30d(0.0f),
          vol_corr_5m(1.0f), vol_corr_1h(1.0f), vol_corr_30d(1.0f),
          _padding{} {}
};
static_assert(sizeof(BaselineState) == 64, "BaselineState must be exactly 64 bytes");

struct alignas(64) ChartConditionPayload final {
    std::uint64_t timestamp_ns;    ///< Nanosecond resolution evaluation timestamp
    float raw_metric_0;            ///< Primary indicator raw metric value
    float raw_metric_1;            ///< Secondary indicator raw metric value
    float raw_metric_2;            ///< Tertiary indicator raw metric value
    float normalized_score;        ///< Clamped/normalized diagnostic score [0.0, 100.0]
    std::uint32_t clamped_flags;   ///< Bitfield indicating if values were clamped
    std::uint8_t indicator_type;   ///< ChartIndicatorType enum value
    std::uint8_t condition_state;  ///< ChartConditionState enum value
    std::uint8_t symbol_id;        ///< Numeric symbol identifier
    std::uint8_t _padding[29];

    constexpr ChartConditionPayload()
        : timestamp_ns(0), raw_metric_0(0.0f), raw_metric_1(0.0f), raw_metric_2(0.0f),
          normalized_score(0.0f), clamped_flags(0),
          indicator_type(static_cast<std::uint8_t>(ChartIndicatorType::VolatilityExpansionBands)),
          condition_state(static_cast<std::uint8_t>(ChartConditionState::Neutral)),
          symbol_id(0), _padding{} {}
};
static_assert(sizeof(ChartConditionPayload) == 64, "ChartConditionPayload must be exactly 64 bytes");

struct alignas(64) PatternEnvironmentState final {
    float prior_expansion_score;     ///< Strength of recent expansion [0.0, 100.0]
    float consolidation_score;       ///< Volatility compression + stable liquidity [0.0, 100.0]
    float support_strength_score;    ///< Baseline strength + shallow pullback retention [0.0, 100.0]
    float symmetry_score;            ///< Structural symmetry / consolidation ratio [0.0, 100.0]
    std::uint8_t pattern_hint;       ///< PatternHint enum value (None, CupHandleLike, PennantLike, FlagLike)
    std::uint8_t _padding[47];

    constexpr PatternEnvironmentState()
        : prior_expansion_score(0.0f), consolidation_score(0.0f),
          support_strength_score(0.0f), symmetry_score(0.0f),
          pattern_hint(static_cast<std::uint8_t>(PatternHint::None)),
          _padding{} {}
};
static_assert(sizeof(PatternEnvironmentState) == 64, "PatternEnvironmentState must be exactly 64 bytes");

struct alignas(64) PatternConditionPayload final {
    std::uint64_t timestamp_ns;        ///< Nanosecond timestamp
    float prior_expansion_score;     ///< Expansion metric
    float consolidation_score;       ///< Consolidation metric
    float support_strength_score;    ///< Support metric
    float symmetry_score;            ///< Symmetry metric
    std::uint8_t pattern_hint;       ///< PatternHint enum value
    std::uint8_t _padding[35];

    constexpr PatternConditionPayload()
        : timestamp_ns(0), prior_expansion_score(0.0f), consolidation_score(0.0f),
          support_strength_score(0.0f), symmetry_score(0.0f),
          pattern_hint(static_cast<std::uint8_t>(PatternHint::None)),
          _padding{} {}
};
static_assert(sizeof(PatternConditionPayload) == 64, "PatternConditionPayload must be exactly 64 bytes");

// ============================================================================
// ALLOCATOR-FREE REGISTRY DISPATCH FUNCTION POINTERS
// ============================================================================

using IndicatorFn = ChartConditionPayload(*)(const AnomalyState&, const VolumeState&, const BaselineState&, std::uint64_t) noexcept;

struct ChartIndicatorRegistry final {
    IndicatorFn fns[MAX_INDICATORS];
    std::uint8_t active[MAX_INDICATORS]; // 0 = inactive, 1 = active
    std::size_t registered_count;

    constexpr ChartIndicatorRegistry() : fns{}, active{}, registered_count(0) {}

    bool register_indicator(std::size_t index, IndicatorFn fn, bool initially_active = true) noexcept {
        if (index >= MAX_INDICATORS || fn == nullptr) return false;
        fns[index] = fn;
        active[index] = initially_active ? 1 : 0;
        if (index >= registered_count) {
            registered_count = index + 1;
        }
        return true;
    }

    void set_active(std::size_t index, bool is_active) noexcept {
        if (index < MAX_INDICATORS) {
            active[index] = is_active ? 1 : 0;
        }
    }

    void execute_active(const AnomalyState& anomaly,
                        const VolumeState& volume,
                        const BaselineState& baseline,
                        ChartConditionPayload* outputs,
                        std::size_t max_outputs,
                        std::size_t& output_count,
                        std::uint64_t timestamp_ns = 0) const noexcept {
        output_count = 0;
        for (std::size_t i = 0; i < registered_count && i < MAX_INDICATORS; ++i) {
            if (active[i] != 0 && fns[i] != nullptr && output_count < max_outputs) {
                outputs[output_count] = fns[i](anomaly, volume, baseline, timestamp_ns);
                output_count++;
            }
        }
    }
};

// ============================================================================
// STRUCTURAL INDICATOR EVALUATION FUNCTIONS
// ============================================================================

[[nodiscard]] ChartConditionPayload evaluate_volatility_expansion_bands(
    const AnomalyState& anomaly,
    const VolumeState& volume,
    const BaselineState& baseline,
    std::uint64_t timestamp_ns = 0
) noexcept;

[[nodiscard]] ChartConditionPayload evaluate_liquidity_displacement_zones(
    const AnomalyState& anomaly,
    const VolumeState& volume,
    const BaselineState& baseline,
    std::uint64_t timestamp_ns = 0
) noexcept;

[[nodiscard]] ChartConditionPayload evaluate_correlation_divergence_index(
    const AnomalyState& anomaly,
    const VolumeState& volume,
    const BaselineState& baseline,
    std::uint64_t timestamp_ns = 0
) noexcept;

[[nodiscard]] ChartConditionPayload evaluate_baseline_strength_meter(
    const AnomalyState& anomaly,
    const VolumeState& volume,
    const BaselineState& baseline,
    std::uint64_t timestamp_ns = 0
) noexcept;

// ============================================================================
// PATTERN DIAGNOSTIC ENGINE
// ============================================================================

[[nodiscard]] PatternConditionPayload evaluate_pattern_diagnostics(
    const ChartConditionPayload* payloads,
    std::size_t payload_count,
    const BaselineState& baseline,
    std::uint64_t timestamp_ns = 0
) noexcept;

// ============================================================================
// STRING MAPPING & JSON SERIALIZATION HELPERS
// ============================================================================

[[nodiscard]] const char* chart_indicator_type_to_string(ChartIndicatorType type) noexcept;
[[nodiscard]] const char* chart_indicator_type_code_to_string(std::uint8_t type_code) noexcept;
[[nodiscard]] const char* chart_condition_state_to_string(ChartConditionState state) noexcept;
[[nodiscard]] const char* chart_condition_state_code_to_string(std::uint8_t state_code) noexcept;
[[nodiscard]] const char* pattern_hint_to_string(PatternHint hint) noexcept;
[[nodiscard]] const char* pattern_hint_code_to_string(std::uint8_t hint_code) noexcept;

std::size_t serialize_chart_payload_json(
    const ChartConditionPayload& payload,
    char* buffer,
    std::size_t buffer_size
) noexcept;

std::size_t serialize_pattern_payload_json(
    const PatternConditionPayload& payload,
    char* buffer,
    std::size_t buffer_size
) noexcept;

} // namespace AILLE

#endif // AILLE_CHART_INTELLIGENCE_HPP
