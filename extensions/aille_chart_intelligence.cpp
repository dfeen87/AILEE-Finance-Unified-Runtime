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
        case ChartIndicatorType::VolatilityInstability: return "VolatilityInstability";
        case ChartIndicatorType::LiquidityErosion: return "LiquidityErosion";
        case ChartIndicatorType::CorrelationBreakdown: return "CorrelationBreakdown";
        case ChartIndicatorType::BaselineDeterioration: return "BaselineDeterioration";
        case ChartIndicatorType::StructuralFatigue: return "StructuralFatigue";
        case ChartIndicatorType::WaveNativeFinanceStream: return "WaveNativeFinanceStream";
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
        case ChartConditionState::StateStable: return "StateStable";
        case ChartConditionState::StateUnstable: return "StateUnstable";
        case ChartConditionState::StateChaotic: return "StateChaotic";
        case ChartConditionState::StatePreserved: return "StatePreserved";
        case ChartConditionState::StateEroding: return "StateEroding";
        case ChartConditionState::StateDepleted: return "StateDepleted";
        case ChartConditionState::StateWeakening: return "StateWeakening";
        case ChartConditionState::StateDeteriorating: return "StateDeteriorating";
        case ChartConditionState::StateLowFatigue: return "StateLowFatigue";
        case ChartConditionState::StateMediumFatigue: return "StateMediumFatigue";
        case ChartConditionState::StateHighFatigue: return "StateHighFatigue";
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
        case PatternHint::BreakdownLike: return "BreakdownLike";
        case PatternHint::ExhaustionLike: return "ExhaustionLike";
        case PatternHint::StressConsolidationLike: return "StressConsolidationLike";
        default: return "None";
    }
}

const char* pattern_hint_code_to_string(std::uint8_t hint_code) noexcept {
    return pattern_hint_to_string(static_cast<PatternHint>(hint_code));
}

const char* pattern_hint_group_to_string(PatternHintGroup group) noexcept {
    switch (group) {
        case PatternHintGroup::ExpansionGroup: return "ExpansionGroup";
        case PatternHintGroup::StressGroup: return "StressGroup";
        default: return "ExpansionGroup";
    }
}

const char* volatility_regime_to_string(VolatilityRegime regime) noexcept {
    switch (regime) {
        case VolatilityRegime::Low: return "Low";
        case VolatilityRegime::Medium: return "Medium";
        case VolatilityRegime::High: return "High";
        default: return "Medium";
    }
}

const char* liquidity_regime_to_string(LiquidityRegime regime) noexcept {
    switch (regime) {
        case LiquidityRegime::Thin: return "Thin";
        case LiquidityRegime::Normal: return "Normal";
        case LiquidityRegime::Deep: return "Deep";
        default: return "Normal";
    }
}

const char* correlation_regime_to_string(CorrelationRegime regime) noexcept {
    switch (regime) {
        case CorrelationRegime::Stable: return "Stable";
        case CorrelationRegime::Transitional: return "Transitional";
        case CorrelationRegime::Unstable: return "Unstable";
        default: return "Stable";
    }
}

// ============================================================================
// STRUCTURAL INDICATOR EVALUATIONS
// ============================================================================

ChartConditionPayload evaluate_wnfs_stream_indicator(
    const WNFSAdvisory& wnfs_advisory,
    std::uint64_t timestamp_ns
) noexcept {
    ChartConditionPayload payload{};
    payload.timestamp_ns = timestamp_ns;
    payload.symbol_id = wnfs_advisory.channel_id;
    payload.indicator_type = static_cast<std::uint8_t>(ChartIndicatorType::WaveNativeFinanceStream);

    float energy = std::clamp(wnfs_advisory.wave_energy_factor, 0.0f, 10.0f);
    float accel = std::clamp(wnfs_advisory.tick_acceleration, -1.0f, 1.0f);
    float conf = std::clamp(wnfs_advisory.ingestion_confidence, 0.0f, 1.0f);

    payload.raw_metric_0 = energy;
    payload.raw_metric_1 = accel;
    payload.raw_metric_2 = conf;

    float score = conf * 100.0f;
    payload.normalized_score = score;

    if (wnfs_advisory.stream_degraded || wnfs_advisory.hft_freeze_required) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::StateChaotic);
    } else if (energy > 1.5f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::Expansion);
    } else {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::StateStable);
    }

    return payload;
}

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
// REGIME DIAGNOSTICS & STRUCTURAL-STRESS EVALUATIONS (V15 EXPANSION)
// ============================================================================

RegimeModifier compute_regime_modifier(
    const AnomalyState& anomaly,
    const VolumeState& volume,
    const BaselineState& baseline,
    const RegimeModifier* prev_modifier
) noexcept {
    (void)volume;
    RegimeModifier modifier{};

    // 1. Volatility Regime with Hysteresis
    float ewma_vol = sanitize_float(anomaly.ewma_volatility, 0.0f);
    float base_vol = sanitize_float(baseline.vol_30d > 1e-6f ? baseline.vol_30d : anomaly.baseline_volatility, 1e-6f);
    float vol_ratio = (base_vol > 1e-6f) ? (ewma_vol / base_vol) : 1.0f;

    VolatilityRegime prev_v = prev_modifier ? static_cast<VolatilityRegime>(prev_modifier->volatility_regime) : VolatilityRegime::Medium;

    if (prev_v == VolatilityRegime::Low) {
        if (vol_ratio >= 0.75f) {
            modifier.volatility_regime = static_cast<std::uint8_t>(vol_ratio >= 1.85f ? VolatilityRegime::High : VolatilityRegime::Medium);
        } else {
            modifier.volatility_regime = static_cast<std::uint8_t>(VolatilityRegime::Low);
        }
    } else if (prev_v == VolatilityRegime::High) {
        if (vol_ratio <= 1.70f) {
            modifier.volatility_regime = static_cast<std::uint8_t>(vol_ratio <= 0.65f ? VolatilityRegime::Low : VolatilityRegime::Medium);
        } else {
            modifier.volatility_regime = static_cast<std::uint8_t>(VolatilityRegime::High);
        }
    } else { // Medium
        if (vol_ratio >= 1.85f) {
            modifier.volatility_regime = static_cast<std::uint8_t>(VolatilityRegime::High);
        } else if (vol_ratio <= 0.65f) {
            modifier.volatility_regime = static_cast<std::uint8_t>(VolatilityRegime::Low);
        } else {
            modifier.volatility_regime = static_cast<std::uint8_t>(VolatilityRegime::Medium);
        }
    }

    auto v_enum = static_cast<VolatilityRegime>(modifier.volatility_regime);
    modifier.volatility_regime_factor = (v_enum == VolatilityRegime::High) ? 1.30f : ((v_enum == VolatilityRegime::Low) ? 0.85f : 1.00f);

    // 2. Liquidity Regime with Hysteresis
    float bid_sz = sanitize_float(anomaly.bid_size, 0.0f);
    float ask_sz = sanitize_float(anomaly.ask_size, 0.0f);
    float curr_depth = bid_sz + ask_sz;
    float base_depth = sanitize_float(baseline.liq_30d > 1e-6f ? baseline.liq_30d : anomaly.baseline_depth, 1000.0f);
    float depth_ratio = (base_depth > 1e-6f) ? (curr_depth / base_depth) : 1.0f;

    LiquidityRegime prev_l = prev_modifier ? static_cast<LiquidityRegime>(prev_modifier->liquidity_regime) : LiquidityRegime::Normal;

    if (prev_l == LiquidityRegime::Thin) {
        if (depth_ratio >= 0.55f) {
            modifier.liquidity_regime = static_cast<std::uint8_t>(depth_ratio >= 1.55f ? LiquidityRegime::Deep : LiquidityRegime::Normal);
        } else {
            modifier.liquidity_regime = static_cast<std::uint8_t>(LiquidityRegime::Thin);
        }
    } else if (prev_l == LiquidityRegime::Deep) {
        if (depth_ratio <= 1.40f) {
            modifier.liquidity_regime = static_cast<std::uint8_t>(depth_ratio <= 0.45f ? LiquidityRegime::Thin : LiquidityRegime::Normal);
        } else {
            modifier.liquidity_regime = static_cast<std::uint8_t>(LiquidityRegime::Deep);
        }
    } else { // Normal
        if (depth_ratio <= 0.45f) {
            modifier.liquidity_regime = static_cast<std::uint8_t>(LiquidityRegime::Thin);
        } else if (depth_ratio >= 1.55f) {
            modifier.liquidity_regime = static_cast<std::uint8_t>(LiquidityRegime::Deep);
        } else {
            modifier.liquidity_regime = static_cast<std::uint8_t>(LiquidityRegime::Normal);
        }
    }

    auto l_enum = static_cast<LiquidityRegime>(modifier.liquidity_regime);
    modifier.liquidity_regime_factor = (l_enum == LiquidityRegime::Thin) ? 1.25f : ((l_enum == LiquidityRegime::Deep) ? 0.85f : 1.00f);

    // 3. Correlation Regime with Hysteresis
    float roll_corr = std::clamp(sanitize_float(anomaly.rolling_correlation, 1.0f), -1.0f, 1.0f);
    float exp_corr = std::clamp(sanitize_float(baseline.vol_corr_30d < 1.0f ? baseline.vol_corr_30d : anomaly.expected_correlation, 1.0f), -1.0f, 1.0f);
    float corr_drop = std::max(0.0f, exp_corr - roll_corr);

    CorrelationRegime prev_c = prev_modifier ? static_cast<CorrelationRegime>(prev_modifier->correlation_regime) : CorrelationRegime::Stable;

    if (prev_c == CorrelationRegime::Stable) {
        if (corr_drop >= 0.28f || roll_corr <= 0.0f) {
            modifier.correlation_regime = static_cast<std::uint8_t>((corr_drop >= 0.53f || roll_corr <= 0.0f) ? CorrelationRegime::Unstable : CorrelationRegime::Transitional);
        } else {
            modifier.correlation_regime = static_cast<std::uint8_t>(CorrelationRegime::Stable);
        }
    } else if (prev_c == CorrelationRegime::Unstable) {
        if (corr_drop <= 0.47f && roll_corr > 0.0f) {
            modifier.correlation_regime = static_cast<std::uint8_t>(corr_drop <= 0.22f ? CorrelationRegime::Stable : CorrelationRegime::Transitional);
        } else {
            modifier.correlation_regime = static_cast<std::uint8_t>(CorrelationRegime::Unstable);
        }
    } else { // Transitional
        if (corr_drop >= 0.53f || roll_corr <= 0.0f) {
            modifier.correlation_regime = static_cast<std::uint8_t>(CorrelationRegime::Unstable);
        } else if (corr_drop <= 0.22f) {
            modifier.correlation_regime = static_cast<std::uint8_t>(CorrelationRegime::Stable);
        } else {
            modifier.correlation_regime = static_cast<std::uint8_t>(CorrelationRegime::Transitional);
        }
    }

    auto c_enum = static_cast<CorrelationRegime>(modifier.correlation_regime);
    modifier.correlation_regime_factor = (c_enum == CorrelationRegime::Unstable) ? 1.30f : ((c_enum == CorrelationRegime::Transitional) ? 1.15f : 1.00f);

    return modifier;
}

ChartConditionPayload evaluate_volatility_instability(
    const AnomalyState& anomaly,
    const VolumeState& volume,
    const BaselineState& baseline,
    const RegimeModifier& modifier,
    std::uint64_t timestamp_ns
) noexcept {
    (void)volume;
    ChartConditionPayload payload{};
    payload.timestamp_ns = timestamp_ns;
    payload.symbol_id = anomaly.symbol_id;
    payload.indicator_type = static_cast<std::uint8_t>(ChartIndicatorType::VolatilityInstability);

    float ewma_vol = sanitize_float(anomaly.ewma_volatility, 0.0f);
    float base_vol = sanitize_float(baseline.vol_30d > 1e-6f ? baseline.vol_30d : anomaly.baseline_volatility, 1e-6f);
    float vol_spike = std::abs(ewma_vol - base_vol);

    float raw_instability = (base_vol > 1e-6f) ? (vol_spike / base_vol) : 0.0f;
    raw_instability = std::clamp(raw_instability, 0.0f, 10.0f); // Spike clamping

    payload.raw_metric_0 = ewma_vol;
    payload.raw_metric_1 = base_vol;
    payload.raw_metric_2 = raw_instability;

    float score = raw_instability * 40.0f * modifier.volatility_regime_factor;
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
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::StateChaotic);
    } else if (score >= 35.0f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::StateUnstable);
    } else {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::StateStable);
    }

    return payload;
}

ChartConditionPayload evaluate_liquidity_erosion(
    const AnomalyState& anomaly,
    const VolumeState& volume,
    const BaselineState& baseline,
    const RegimeModifier& modifier,
    std::uint64_t timestamp_ns
) noexcept {
    ChartConditionPayload payload{};
    payload.timestamp_ns = timestamp_ns;
    payload.symbol_id = anomaly.symbol_id;
    payload.indicator_type = static_cast<std::uint8_t>(ChartIndicatorType::LiquidityErosion);

    float bid_sz = sanitize_float(anomaly.bid_size, 0.0f);
    float ask_sz = sanitize_float(anomaly.ask_size, 0.0f);
    float curr_depth = std::max(0.001f, bid_sz + ask_sz); // Depth floor hardening
    float base_depth = std::max(0.001f, sanitize_float(baseline.liq_30d > 1e-6f ? baseline.liq_30d : anomaly.baseline_depth, 1000.0f));

    float thinning_rate = 0.0f;
    if (curr_depth < base_depth) {
        thinning_rate = (base_depth - curr_depth) / base_depth;
    }
    thinning_rate = std::clamp(thinning_rate, 0.0f, 1.0f);

    float vol_anomaly = sanitize_float(volume.volume_anomaly_ratio, 1.0f);
    vol_anomaly = std::clamp(vol_anomaly, 0.1f, 20.0f); // Vacuum detection ceiling clamp

    payload.raw_metric_0 = thinning_rate;
    payload.raw_metric_1 = curr_depth;
    payload.raw_metric_2 = vol_anomaly;

    float score = thinning_rate * 80.0f * modifier.liquidity_regime_factor;
    if (score > 100.0f) {
        score = 100.0f;
        payload.clamped_flags |= CLAMP_FLAG_SCORE;
    }
    if (score < 0.0f) {
        score = 0.0f;
        payload.clamped_flags |= CLAMP_FLAG_SCORE;
    }
    payload.normalized_score = score;

    if (score >= 60.0f || thinning_rate >= 0.60f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::StateDepleted);
    } else if (score >= 30.0f || thinning_rate >= 0.25f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::StateEroding);
    } else {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::StatePreserved);
    }

    return payload;
}

ChartConditionPayload evaluate_correlation_breakdown(
    const AnomalyState& anomaly,
    const VolumeState& volume,
    const BaselineState& baseline,
    const RegimeModifier& modifier,
    std::uint64_t timestamp_ns
) noexcept {
    (void)volume;
    ChartConditionPayload payload{};
    payload.timestamp_ns = timestamp_ns;
    payload.symbol_id = anomaly.symbol_id;
    payload.indicator_type = static_cast<std::uint8_t>(ChartIndicatorType::CorrelationBreakdown);

    float roll_corr = std::clamp(sanitize_float(anomaly.rolling_correlation, 1.0f), -1.0f, 1.0f);
    float exp_corr = std::clamp(sanitize_float(baseline.vol_corr_30d < 1.0f ? baseline.vol_corr_30d : anomaly.expected_correlation, 1.0f), -1.0f, 1.0f);
    float corr_drop = std::max(0.0f, exp_corr - roll_corr);

    payload.raw_metric_0 = roll_corr;
    payload.raw_metric_1 = exp_corr;
    payload.raw_metric_2 = corr_drop;

    float score = corr_drop * 60.0f * modifier.correlation_regime_factor;
    if (score > 100.0f) {
        score = 100.0f;
        payload.clamped_flags |= CLAMP_FLAG_SCORE;
    }
    if (score < 0.0f) {
        score = 0.0f;
        payload.clamped_flags |= CLAMP_FLAG_SCORE;
    }
    payload.normalized_score = score;

    if (score >= 65.0f || corr_drop >= 0.60f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::StateDeteriorating);
    } else if (score >= 30.0f || corr_drop >= 0.25f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::StateWeakening);
    } else {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::StateStable);
    }

    return payload;
}

ChartConditionPayload evaluate_baseline_deterioration(
    const AnomalyState& anomaly,
    const VolumeState& volume,
    const BaselineState& baseline,
    const RegimeModifier& modifier,
    std::uint64_t timestamp_ns
) noexcept {
    (void)anomaly;
    (void)volume;
    ChartConditionPayload payload{};
    payload.timestamp_ns = timestamp_ns;
    payload.symbol_id = anomaly.symbol_id;
    payload.indicator_type = static_cast<std::uint8_t>(ChartIndicatorType::BaselineDeterioration);

    float vol_5m = std::clamp(sanitize_float(baseline.vol_5m, 0.0f), 0.0f, 100.0f);
    float vol_1h = std::clamp(sanitize_float(baseline.vol_1h, 0.0f), 0.0f, 100.0f);
    float vol_30d = std::clamp(sanitize_float(baseline.vol_30d, 1e-6f), 1e-6f, 100.0f);

    float drift_short = std::abs(vol_5m - vol_30d) / vol_30d;
    float drift_med = std::abs(vol_1h - vol_30d) / vol_30d;

    payload.raw_metric_0 = drift_short;
    payload.raw_metric_1 = drift_med;
    payload.raw_metric_2 = vol_30d;

    float score = (drift_short * 40.0f + drift_med * 30.0f) * modifier.volatility_regime_factor;
    if (score > 100.0f) {
        score = 100.0f;
        payload.clamped_flags |= CLAMP_FLAG_SCORE;
    }
    if (score < 0.0f) {
        score = 0.0f;
        payload.clamped_flags |= CLAMP_FLAG_SCORE;
    }
    payload.normalized_score = score;

    if (score >= 60.0f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::StateDeteriorating);
    } else if (score >= 30.0f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::StateWeakening);
    } else {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::Strong);
    }

    return payload;
}

ChartConditionPayload evaluate_structural_fatigue(
    const ChartConditionPayload& vol_instability,
    const ChartConditionPayload& liq_erosion,
    const ChartConditionPayload& base_deterioration,
    const RegimeModifier& modifier,
    std::uint64_t timestamp_ns
) noexcept {
    ChartConditionPayload payload{};
    payload.timestamp_ns = timestamp_ns;
    payload.symbol_id = vol_instability.symbol_id;
    payload.indicator_type = static_cast<std::uint8_t>(ChartIndicatorType::StructuralFatigue);

    float s_vol = sanitize_float(vol_instability.normalized_score, 0.0f);
    float s_liq = sanitize_float(liq_erosion.normalized_score, 0.0f);
    float s_base = sanitize_float(base_deterioration.normalized_score, 0.0f);

    payload.raw_metric_0 = s_vol;
    payload.raw_metric_1 = s_liq;
    payload.raw_metric_2 = s_base;

    float combined_score = (s_vol * 0.35f + s_liq * 0.40f + s_base * 0.25f) * modifier.volatility_regime_factor;
    if (combined_score > 100.0f) {
        combined_score = 100.0f;
        payload.clamped_flags |= CLAMP_FLAG_SCORE;
    }
    if (combined_score < 0.0f) {
        combined_score = 0.0f;
        payload.clamped_flags |= CLAMP_FLAG_SCORE;
    }
    payload.normalized_score = combined_score;

    if (combined_score >= 65.0f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::StateHighFatigue);
    } else if (combined_score >= 35.0f) {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::StateMediumFatigue);
    } else {
        payload.condition_state = static_cast<std::uint8_t>(ChartConditionState::StateLowFatigue);
    }

    return payload;
}

StressRegimePayload evaluate_stress_regime_payload(
    const ChartConditionPayload* payloads,
    std::size_t payload_count,
    const RegimeModifier& modifier,
    std::uint64_t timestamp_ns
) noexcept {
    StressRegimePayload payload{};
    payload.timestamp_ns = timestamp_ns;
    payload.volatility_regime = modifier.volatility_regime;
    payload.liquidity_regime = modifier.liquidity_regime;
    payload.correlation_regime = modifier.correlation_regime;
    payload.regime_confidence = 1.0f;

    if (payloads == nullptr || payload_count == 0) {
        return payload;
    }

    float vol_instability = 0.0f;
    float liq_erosion = 0.0f;
    float base_deterioration = 0.0f;
    float struct_fatigue = 0.0f;

    for (std::size_t i = 0; i < payload_count; ++i) {
        const auto& p = payloads[i];
        if (p.indicator_type == static_cast<std::uint8_t>(ChartIndicatorType::VolatilityInstability)) {
            vol_instability = p.normalized_score;
        } else if (p.indicator_type == static_cast<std::uint8_t>(ChartIndicatorType::LiquidityErosion)) {
            liq_erosion = p.normalized_score;
        } else if (p.indicator_type == static_cast<std::uint8_t>(ChartIndicatorType::BaselineDeterioration)) {
            base_deterioration = p.normalized_score;
        } else if (p.indicator_type == static_cast<std::uint8_t>(ChartIndicatorType::StructuralFatigue)) {
            struct_fatigue = p.normalized_score;
        } else if (p.indicator_type == static_cast<std::uint8_t>(ChartIndicatorType::WaveNativeFinanceStream)) {
            if (p.condition_state == static_cast<std::uint8_t>(ChartConditionState::StateChaotic)) {
                vol_instability = std::max(vol_instability, 85.0f);
            }
        }
    }

    payload.instability_score = std::clamp(vol_instability, 0.0f, 100.0f);
    payload.deterioration_score = std::clamp(base_deterioration, 0.0f, 100.0f);

    float unified_stress = std::max({vol_instability, liq_erosion, base_deterioration, struct_fatigue});
    payload.stress_score = std::clamp(unified_stress, 0.0f, 100.0f);

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

    // Also check structural-stress indicators if present
    float vol_instability = 0.0f;
    float liq_erosion = 0.0f;
    float base_deterioration = 0.0f;
    float struct_fatigue = 0.0f;

    for (std::size_t i = 0; i < payload_count; ++i) {
        const auto& p = payloads[i];
        if (p.indicator_type == static_cast<std::uint8_t>(ChartIndicatorType::VolatilityInstability)) {
            vol_instability = p.normalized_score;
        } else if (p.indicator_type == static_cast<std::uint8_t>(ChartIndicatorType::LiquidityErosion)) {
            liq_erosion = p.normalized_score;
        } else if (p.indicator_type == static_cast<std::uint8_t>(ChartIndicatorType::BaselineDeterioration)) {
            base_deterioration = p.normalized_score;
        } else if (p.indicator_type == static_cast<std::uint8_t>(ChartIndicatorType::StructuralFatigue)) {
            struct_fatigue = p.normalized_score;
        }
    }

    // Evaluate Pattern Diagnostic Resemblance (Expansion Group vs Stress Group)
    // 1. BreakdownLike: correlation collapse + liquidity erosion + volatility instability
    if (corr_divergence >= 0.50f && liq_erosion >= 40.0f && vol_instability >= 40.0f) {
        pattern.pattern_hint = static_cast<std::uint8_t>(PatternHint::BreakdownLike);
        pattern.pattern_group = static_cast<std::uint8_t>(PatternHintGroup::StressGroup);
    }
    // 2. ExhaustionLike: baseline deterioration + structural fatigue + instability spikes
    else if (base_deterioration >= 50.0f && struct_fatigue >= 50.0f) {
        pattern.pattern_hint = static_cast<std::uint8_t>(PatternHint::ExhaustionLike);
        pattern.pattern_group = static_cast<std::uint8_t>(PatternHintGroup::StressGroup);
    }
    // 3. StressConsolidationLike: tight compression under weakening support + depth erosion
    else if (pattern.consolidation_score >= 45.0f && baseline_strength <= 45.0f && liq_thinning >= 0.30f) {
        pattern.pattern_hint = static_cast<std::uint8_t>(PatternHint::StressConsolidationLike);
        pattern.pattern_group = static_cast<std::uint8_t>(PatternHintGroup::StressGroup);
    }
    // 4. Cup & Handle: Strong baseline support, rounded low-vol consolidation, preserved liquidity
    else if (pattern.support_strength_score >= 65.0f &&
             pattern.consolidation_score >= 50.0f &&
             liq_thinning < 0.30f) {
        pattern.pattern_hint = static_cast<std::uint8_t>(PatternHint::CupHandleLike);
        pattern.pattern_group = static_cast<std::uint8_t>(PatternHintGroup::ExpansionGroup);
    }
    // 5. Pennant: Strong prior expansion followed by tight compression and high symmetry
    else if (pattern.prior_expansion_score >= 40.0f &&
             pattern.consolidation_score >= 60.0f &&
             pattern.symmetry_score >= 60.0f) {
        pattern.pattern_hint = static_cast<std::uint8_t>(PatternHint::PennantLike);
        pattern.pattern_group = static_cast<std::uint8_t>(PatternHintGroup::ExpansionGroup);
    }
    // 6. Flag: Prior expansion with sideways consolidation and preserved depth
    else if (pattern.prior_expansion_score >= 35.0f &&
             pattern.consolidation_score >= 40.0f &&
             pattern.support_strength_score >= 45.0f) {
        pattern.pattern_hint = static_cast<std::uint8_t>(PatternHint::FlagLike);
        pattern.pattern_group = static_cast<std::uint8_t>(PatternHintGroup::ExpansionGroup);
    }
    else {
        pattern.pattern_hint = static_cast<std::uint8_t>(PatternHint::None);
        pattern.pattern_group = static_cast<std::uint8_t>(PatternHintGroup::ExpansionGroup);
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
    const char* group_str = pattern_hint_group_to_string(static_cast<PatternHintGroup>(payload.pattern_group));

    int written = std::snprintf(
        buffer,
        buffer_size,
        "{"
        "\"pattern_hint\":\"%s\","
        "\"pattern_group\":\"%s\","
        "\"scores\":{"
        "\"prior_expansion\":%.2f,"
        "\"consolidation\":%.2f,"
        "\"support_strength\":%.2f,"
        "\"symmetry\":%.2f"
        "},"
        "\"timestamp_ns\":%llu"
        "}",
        pattern_str,
        group_str,
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

std::size_t serialize_stress_regime_payload_json(
    const StressRegimePayload& payload,
    char* buffer,
    std::size_t buffer_size
) noexcept {
    if (buffer == nullptr || buffer_size == 0) return 0;

    const char* vol_regime_str = volatility_regime_to_string(static_cast<VolatilityRegime>(payload.volatility_regime));
    const char* liq_regime_str = liquidity_regime_to_string(static_cast<LiquidityRegime>(payload.liquidity_regime));
    const char* corr_regime_str = correlation_regime_to_string(static_cast<CorrelationRegime>(payload.correlation_regime));

    int written = std::snprintf(
        buffer,
        buffer_size,
        "{"
        "\"volatility_regime\":\"%s\","
        "\"liquidity_regime\":\"%s\","
        "\"correlation_regime\":\"%s\","
        "\"stress_score\":%.2f,"
        "\"deterioration_score\":%.2f,"
        "\"instability_score\":%.2f,"
        "\"regime_confidence\":%.2f,"
        "\"reserved_flags\":%u,"
        "\"timestamp_ns\":%llu"
        "}",
        vol_regime_str,
        liq_regime_str,
        corr_regime_str,
        static_cast<double>(payload.stress_score),
        static_cast<double>(payload.deterioration_score),
        static_cast<double>(payload.instability_score),
        static_cast<double>(payload.regime_confidence),
        payload.reserved_flags,
        static_cast<unsigned long long>(payload.timestamp_ns)
    );

    if (written < 0 || static_cast<std::size_t>(written) >= buffer_size) {
        buffer[0] = '\0';
        return 0;
    }
    return static_cast<std::size_t>(written);
}

} // namespace AILLE
