/*
 * AILLE Framework - Real-Time Chart Intelligence Subsystem (Layer 17)
 * AI-Load Integrity and Layered Evaluation
 *
 * Implementation of deterministic, allocator-free chart intelligence indicators,
 * pattern diagnostics, and JSON serialization.
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#include "aille_chart_intelligence.hpp"

namespace AILLE {

// Helper to sanitize float input against NaN/Inf
static inline float sanitize_float(float val, float fallback = 0.0f) noexcept {
    if (std::isnan(val) || std::isinf(val)) {
        return fallback;
    }
    return val;
}

// Bitmask constants for clamped_flags
constexpr std::uint32_t CLAMP_FLAG_VOLATILITY = (1U << 0);
constexpr std::uint32_t CLAMP_FLAG_LIQUIDITY  = (1U << 1);
constexpr std::uint32_t CLAMP_FLAG_CORRELATION= (1U << 2);
constexpr std::uint32_t CLAMP_FLAG_BASELINE   = (1U << 3);
constexpr std::uint32_t CLAMP_FLAG_SCORE      = (1U << 4);

// ============================================================================
// STRING MAPPING HELPERS
// ============================================================================

const char* chart_indicator_type_to_string(ChartIndicatorType type) noexcept {
    switch (type) {
        case ChartIndicatorType::VolatilityExpansionBands: return "VolatilityExpansionBands";
        case ChartIndicatorType::LiquidityDisplacementZones: return "LiquidityDisplacementZones";
        case ChartIndicatorType::CorrelationDivergenceIndex: return "CorrelationDivergenceIndex";
        case ChartIndicatorType::BaselineStrengthMeter: return "BaselineStrengthMeter";
        case ChartIndicatorType::PatternEnvironmentScore: return "PatternEnvironmentScore";
        default: return "Unknown";
    }
}

const char* chart_indicator_type_code_to_string(std::uint8_t type_code) noexcept {
    return chart_indicator_type_to_string(static_cast<ChartIndicatorType>(type_code));
}

const char* chart_condition_state_to_string(ChartConditionState state) noexcept {
    switch (state) {
        case ChartConditionState::Neutral: return "Neutral";
        case ChartConditionState::Compression: return "Compression";
        case ChartConditionState::Expansion: return "Expansion";
        case ChartConditionState::Displaced: return "Displaced";
        case ChartConditionState::Diverging: return "Diverging";
        case ChartConditionState::Broken: return "Broken";
        case ChartConditionState::StrongSupport: return "StrongSupport";
        case ChartConditionState::Consolidation: return "Consolidation";
        case ChartConditionState::Stressed: return "Stressed";
        case ChartConditionState::Weak: return "Weak";
        case ChartConditionState::Average: return "Average";
        case ChartConditionState::Strong: return "Strong";
        default: return "Unknown";
    }
}

const char* chart_condition_state_code_to_string(std::uint8_t state_code) noexcept {
    return chart_condition_state_to_string(static_cast<ChartConditionState>(state_code));
}

const char* pattern_hint_to_string(PatternHint hint) noexcept {
    switch (hint) {
        case PatternHint::None: return "None";
        case PatternHint::CupHandleLike: return "CupHandleLike";
        case PatternHint::PennantLike: return "PennantLike";
        case PatternHint::FlagLike: return "FlagLike";
        default: return "None";
    }
}

const char* pattern_hint_code_to_string(std::uint8_t hint_code) noexcept {
    return pattern_hint_to_string(static_cast<PatternHint>(hint_code));
}

// ============================================================================
// STRUCTURAL INDICATOR EVALUATIONS
// ============================================================================

ChartConditionPayload evaluate_volatility_expansion_bands(
    const AnomalyState& anomaly,
    const VolumeState& volume,
    const BaselineState& baseline,
    std::uint64_t timestamp_ns
) noexcept {
    (void)volume;
    ChartConditionPayload payload{};
    payload.timestamp_ns = timestamp_ns;
    payload.symbol_id = anomaly.symbol_id;
    payload.indicator_type = static_cast<std::uint8_t>(ChartIndicatorType::VolatilityExpansionBands);

    float ewma_vol = sanitize_float(anomaly.ewma_volatility, 0.0f);
    float base_vol = sanitize_float(baseline.vol_30d > 1e-6f ? baseline.vol_30d : anomaly.baseline_volatility, 1e-6f);

    if (ewma_vol < 0.0f) {
        ewma_vol = 0.0f;
        payload.clamped_flags |= CLAMP_FLAG_VOLATILITY;
    }
    if (base_vol <= 1e-6f) {
        base_vol = 1e-6f;
        payload.clamped_flags |= CLAMP_FLAG_BASELINE;
    }

    float ratio = ewma_vol / base_vol;
    if (std::isnan(ratio) || std::isinf(ratio)) {
        ratio = 1.0f;
        payload.clamped_flags |= CLAMP_FLAG_VOLATILITY;
    }

    payload.raw_metric_0 = ratio;     ///< Volatility Expansion Ratio
    payload.raw_metric_1 = ewma_vol;  ///< Fast Intraday Volatility
    payload.raw_metric_2 = base_vol;  ///< 30-day Baseline Volatility

    // Normalized score calculation [0.0, 100.0]
    float score = (ratio - 1.0f) * 50.0f + 50.0f;
    if (score > 100.0f) {
        score = 100.0f;
        payload.clamped_flags |= CLAMP_FLAG_SCORE;
    }
    if (score < 0.0f) {
        score = 0.0f;
        payload.clamped_flags |= CLAMP_FLAG_SCORE;
    }
    payload.normalized_score = score;

    if (ratio >= 2.0f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::Expansion);
    } else if (ratio <= 0.60f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::Compression);
    } else {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::Neutral);
    }

    return payload;
}

ChartConditionPayload evaluate_liquidity_displacement_zones(
    const AnomalyState& anomaly,
    const VolumeState& volume,
    const BaselineState& baseline,
    std::uint64_t timestamp_ns
) noexcept {
    ChartConditionPayload payload{};
    payload.timestamp_ns = timestamp_ns;
    payload.symbol_id = anomaly.symbol_id;
    payload.indicator_type = static_cast<std::uint8_t>(ChartIndicatorType::LiquidityDisplacementZones);

    float bid_sz = sanitize_float(anomaly.bid_size, 0.0f);
    float ask_sz = sanitize_float(anomaly.ask_size, 0.0f);
    float curr_depth = bid_sz + ask_sz;

    float base_depth = sanitize_float(baseline.liq_30d > 1e-6f ? baseline.liq_30d : anomaly.baseline_depth, 0.0f);
    float vol_ratio = sanitize_float(volume.volume_anomaly_ratio, 1.0f);

    float thinning_pct = 0.0f;
    if (base_depth > 1e-6f) {
        float depth_ratio = curr_depth / base_depth;
        if (depth_ratio < 1.0f) {
            thinning_pct = 1.0f - depth_ratio;
        }
    }

    if (thinning_pct > 1.0f) {
        thinning_pct = 1.0f;
        payload.clamped_flags |= CLAMP_FLAG_LIQUIDITY;
    }
    if (thinning_pct < 0.0f) {
        thinning_pct = 0.0f;
        payload.clamped_flags |= CLAMP_FLAG_LIQUIDITY;
    }

    payload.raw_metric_0 = thinning_pct; ///< Order Book Thinning Fractional Percentage [0.0, 1.0]
    payload.raw_metric_1 = curr_depth;   ///< Current Book Depth (bid + ask)
    payload.raw_metric_2 = vol_ratio;    ///< Volume Acceleration Ratio

    float score = thinning_pct * 70.0f + (vol_ratio > 2.0f ? (vol_ratio - 2.0f) * 15.0f : 0.0f);
    if (score > 100.0f) {
        score = 100.0f;
        payload.clamped_flags |= CLAMP_FLAG_SCORE;
    }
    if (score < 0.0f) {
        score = 0.0f;
        payload.clamped_flags |= CLAMP_FLAG_SCORE;
    }
    payload.normalized_score = score;

    if (thinning_pct >= 0.50f || score >= 60.0f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::Displaced);
    } else if (thinning_pct >= 0.25f || score >= 35.0f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::Stressed);
    } else {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::Neutral);
    }

    return payload;
}

ChartConditionPayload evaluate_correlation_divergence_index(
    const AnomalyState& anomaly,
    const VolumeState& volume,
    const BaselineState& baseline,
    std::uint64_t timestamp_ns
) noexcept {
    (void)volume;
    ChartConditionPayload payload{};
    payload.timestamp_ns = timestamp_ns;
    payload.symbol_id = anomaly.symbol_id;
    payload.indicator_type = static_cast<std::uint8_t>(ChartIndicatorType::CorrelationDivergenceIndex);

    float roll_corr = sanitize_float(anomaly.rolling_correlation, 1.0f);
    float exp_corr = sanitize_float(baseline.vol_corr_30d < 1.0f ? baseline.vol_corr_30d : anomaly.expected_correlation, 1.0f);

    roll_corr = std::clamp(roll_corr, -1.0f, 1.0f);
    exp_corr = std::clamp(exp_corr, -1.0f, 1.0f);

    float drop = exp_corr - roll_corr;
    if (drop < 0.0f) drop = 0.0f;

    payload.raw_metric_0 = roll_corr; ///< Rolling Correlation
    payload.raw_metric_1 = exp_corr;  ///< Benchmark Expected Correlation
    payload.raw_metric_2 = drop;      ///< Absolute Correlation Drop

    float score = drop * 50.0f;
    if (score > 100.0f) {
        score = 100.0f;
        payload.clamped_flags |= CLAMP_FLAG_SCORE;
    }
    if (score < 0.0f) {
        score = 0.0f;
        payload.clamped_flags |= CLAMP_FLAG_SCORE;
    }
    payload.normalized_score = score;

    if (drop >= 0.70f || roll_corr <= 0.0f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::Broken);
    } else if (drop >= 0.35f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::Diverging);
    } else {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::Neutral);
    }

    return payload;
}

ChartConditionPayload evaluate_baseline_strength_meter(
    const AnomalyState& anomaly,
    const VolumeState& volume,
    const BaselineState& baseline,
    std::uint64_t timestamp_ns
) noexcept {
    (void)anomaly;
    (void)volume;
    ChartConditionPayload payload{};
    payload.timestamp_ns = timestamp_ns;
    payload.symbol_id = anomaly.symbol_id;
    payload.indicator_type = static_cast<std::uint8_t>(ChartIndicatorType::BaselineStrengthMeter);

    float vol_5m = sanitize_float(baseline.vol_5m, 0.0f);
    float vol_1h = sanitize_float(baseline.vol_1h, 0.0f);
    float vol_30d = sanitize_float(baseline.vol_30d, 1e-6f);

    float ratio_short = (vol_30d > 1e-6f) ? (vol_5m / vol_30d) : 1.0f;
    float ratio_med   = (vol_30d > 1e-6f) ? (vol_1h / vol_30d) : 1.0f;

    payload.raw_metric_0 = ratio_short;
    payload.raw_metric_1 = ratio_med;
    payload.raw_metric_2 = vol_30d;

    // Stability score: lower short/medium volatility expansion relative to 30d => stronger baseline support
    float instability = (std::abs(ratio_short - 1.0f) + std::abs(ratio_med - 1.0f)) * 50.0f;
    float score = 100.0f - instability;
    if (score > 100.0f) {
        score = 100.0f;
        payload.clamped_flags |= CLAMP_FLAG_SCORE;
    }
    if (score < 0.0f) {
        score = 0.0f;
        payload.clamped_flags |= CLAMP_FLAG_SCORE;
    }
    payload.normalized_score = score;

    if (score >= 70.0f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::Strong);
    } else if (score >= 40.0f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::Average);
    } else {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::Weak);
    }

    return payload;
}

// ============================================================================
// PATTERN DIAGNOSTIC ENGINE
// ============================================================================

PatternConditionPayload evaluate_pattern_diagnostics(
    const ChartConditionPayload* payloads,
    std::size_t payload_count,
    const BaselineState& baseline,
    std::uint64_t timestamp_ns
) noexcept {
    PatternConditionPayload pattern{};
    pattern.timestamp_ns = timestamp_ns;

    if (payloads == nullptr || payload_count == 0) {
        pattern.pattern_hint = static_cast<std::uint8_t>(PatternHint::None);
        return pattern;
    }

    float vol_expansion = 0.0f;
    float liq_thinning = 0.0f;
    float corr_divergence = 0.0f;
    float baseline_strength = 50.0f;

    for (std::size_t i = 0; i < payload_count; ++i) {
        const auto& p = payloads[i];
        if (p.indicator_type == static_cast<std::uint8_t>(ChartIndicatorType::VolatilityExpansionBands)) {
            vol_expansion = p.raw_metric_0; // Volatility expansion ratio
        } else if (p.indicator_type == static_cast<std::uint8_t>(ChartIndicatorType::LiquidityDisplacementZones)) {
            liq_thinning = p.raw_metric_0;  // Liquidity depth thinning pct
        } else if (p.indicator_type == static_cast<std::uint8_t>(ChartIndicatorType::CorrelationDivergenceIndex)) {
            corr_divergence = p.raw_metric_2; // Correlation drop
        } else if (p.indicator_type == static_cast<std::uint8_t>(ChartIndicatorType::BaselineStrengthMeter)) {
            baseline_strength = p.normalized_score;
        }
    }

    // 1. Prior Expansion Score
    float prior_exp = (vol_expansion > 1.0f) ? (vol_expansion - 1.0f) * 60.0f : 0.0f;
    pattern.prior_expansion_score = std::clamp(prior_exp, 0.0f, 100.0f);

    // 2. Consolidation Score (Low volatility expansion + preserved depth)
    float compression_term = (vol_expansion <= 1.0f) ? (1.0f - vol_expansion) * 100.0f : 0.0f;
    float depth_term = (1.0f - liq_thinning) * 50.0f;
    pattern.consolidation_score = std::clamp(compression_term * 0.5f + depth_term, 0.0f, 100.0f);

    // 3. Support Strength Score
    float supp = baseline_strength * 0.7f + (1.0f - corr_divergence) * 30.0f;
    pattern.support_strength_score = std::clamp(supp, 0.0f, 100.0f);

    // 4. Structural Symmetry Score
    float sym = 100.0f - (std::abs(vol_expansion - 1.0f) * 30.0f + liq_thinning * 40.0f);
    pattern.symmetry_score = std::clamp(sym, 0.0f, 100.0f);

    // Evaluate Pattern Diagnostic Resemblance
    // Cup & Handle: Strong baseline support, rounded low-vol consolidation, preserved liquidity
    if (pattern.support_strength_score >= 65.0f &&
        pattern.consolidation_score >= 50.0f &&
        liq_thinning < 0.30f) {
        pattern.pattern_hint = static_cast<std::uint8_t>(PatternHint::CupHandleLike);
    }
    // Pennant: Strong prior expansion followed by tight compression and high symmetry
    else if (pattern.prior_expansion_score >= 40.0f &&
             pattern.consolidation_score >= 60.0f &&
             pattern.symmetry_score >= 60.0f) {
        pattern.pattern_hint = static_cast<std::uint8_t>(PatternHint::PennantLike);
    }
    // Flag: Prior expansion with sideways consolidation and preserved depth
    else if (pattern.prior_expansion_score >= 35.0f &&
             pattern.consolidation_score >= 40.0f &&
             pattern.support_strength_score >= 45.0f) {
        pattern.pattern_hint = static_cast<std::uint8_t>(PatternHint::FlagLike);
    }
    else {
        pattern.pattern_hint = static_cast<std::uint8_t>(PatternHint::None);
    }

    (void)baseline;
    return pattern;
}

// ============================================================================
// JSON SERIALIZATION HELPERS
// ============================================================================

std::size_t serialize_chart_payload_json(
    const ChartConditionPayload& payload,
    char* buffer,
    std::size_t buffer_size
) noexcept {
    if (buffer == nullptr || buffer_size == 0) return 0;

    const char* indicator_str = chart_indicator_type_code_to_string(payload.indicator_type);
    const char* state_str = chart_condition_state_code_to_string(payload.condition_state);

    int written = std::snprintf(
        buffer,
        buffer_size,
        "{"
        "\"indicator\":\"%s\","
        "\"state\":\"%s\","
        "\"score\":%.2f,"
        "\"metrics\":[%.4f,%.4f,%.4f],"
        "\"timestamp_ns\":%llu,"
        "\"clamped_flags\":%u"
        "}",
        indicator_str,
        state_str,
        static_cast<double>(payload.normalized_score),
        static_cast<double>(payload.raw_metric_0),
        static_cast<double>(payload.raw_metric_1),
        static_cast<double>(payload.raw_metric_2),
        static_cast<unsigned long long>(payload.timestamp_ns),
        payload.clamped_flags
    );

    if (written < 0 || static_cast<std::size_t>(written) >= buffer_size) {
        buffer[0] = '\0';
        return 0;
    }
    return static_cast<std::size_t>(written);
}

std::size_t serialize_pattern_payload_json(
    const PatternConditionPayload& payload,
    char* buffer,
    std::size_t buffer_size
) noexcept {
    if (buffer == nullptr || buffer_size == 0) return 0;

    const char* pattern_str = pattern_hint_code_to_string(payload.pattern_hint);

    int written = std::snprintf(
        buffer,
        buffer_size,
        "{"
        "\"pattern_hint\":\"%s\","
        "\"scores\":{"
        "\"prior_expansion\":%.2f,"
        "\"consolidation\":%.2f,"
        "\"support_strength\":%.2f,"
        "\"symmetry\":%.2f"
        "},"
        "\"timestamp_ns\":%llu"
        "}",
        pattern_str,
        static_cast<double>(payload.prior_expansion_score),
        static_cast<double>(payload.consolidation_score),
        static_cast<double>(payload.support_strength_score),
        static_cast<double>(payload.symmetry_score),
        static_cast<unsigned long long>(payload.timestamp_ns)
    );

    if (written < 0 || static_cast<std::size_t>(written) >= buffer_size) {
        buffer[0] = '\0';
        return 0;
    }
    return static_cast<std::size_t>(written);
}

} // namespace AILLE
