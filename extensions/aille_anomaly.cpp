/*
 * AILLE Framework - Real-Time Market Condition Intelligence & Anomaly Detection (Layer 16)
 * AI-Load Integrity and Layered Evaluation
 *
 * Deterministic, allocator-free implementation for Layer 16.
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#include "../aille.hpp"
#include "aille_anomaly.hpp"

namespace AILLE {

void AILLEEngine::evaluate_anomaly_advisory() {
    if (anomaly_state_ != nullptr && anomaly_advisory_ != nullptr) {
        AnomalyConfig cfg = (anomaly_config_ != nullptr) ? *anomaly_config_ : AnomalyConfig();
        *anomaly_advisory_ = AILLE::evaluate_anomaly_advisory(*anomaly_state_, cfg, safety_state_);
    }
}

[[nodiscard]] AnomalyAdvisory evaluate_anomaly_advisory(
    const AnomalyState& state,
    const AnomalyConfig& config,
    const SafetyState* safety
) noexcept {
    AnomalyAdvisory advisory{};

    // 1. Safety override: hardware fault or kill switch disables advisories
    if (safety && (safety->hardware_fault || safety->kill_switch)) {
        advisory.volatility_expansion_ratio = 0.0f;
        advisory.depth_thinning_pct = 0.0f;
        advisory.rolling_correlation = 1.0f;
        advisory.anomaly_severity = 100.0f;
        advisory.advisory_active = 0;
        return advisory;
    }

    // 2. Numerical safety check (NaN / Inf inputs)
    if (std::isnan(state.last_price) || std::isinf(state.last_price) ||
        std::isnan(state.ewma_volatility) || std::isinf(state.ewma_volatility) ||
        std::isnan(state.baseline_volatility) || std::isinf(state.baseline_volatility) ||
        std::isnan(state.bid_size) || std::isinf(state.bid_size) ||
        std::isnan(state.ask_size) || std::isinf(state.ask_size) ||
        std::isnan(state.baseline_depth) || std::isinf(state.baseline_depth) ||
        std::isnan(state.rolling_correlation) || std::isinf(state.rolling_correlation) ||
        std::isnan(state.expected_correlation) || std::isinf(state.expected_correlation)) {
        advisory.volatility_expansion_ratio = 1.0f;
        advisory.depth_thinning_pct = 0.0f;
        advisory.rolling_correlation = 1.0f;
        advisory.anomaly_severity = 0.0f;
        advisory.advisory_active = 0;
        return advisory;
    }

    // Sanitize and clamp inputs
    float ewma_vol = std::max(0.0f, state.ewma_volatility);
    float base_vol = std::max(1e-6f, state.baseline_volatility);
    float bid_size = std::max(0.0f, state.bid_size);
    float ask_size = std::max(0.0f, state.ask_size);
    float base_depth = std::max(0.0f, state.baseline_depth);
    float roll_corr = std::clamp(state.rolling_correlation, -1.0f, 1.0f);
    float exp_corr = std::clamp(state.expected_correlation, -1.0f, 1.0f);

    // 3. Volatility Expansion Calculation
    advisory.volatility_expansion_ratio = ewma_vol / base_vol;

    bool vol_raw_anomaly = (advisory.volatility_expansion_ratio >= config.volatility_threshold);
    if (state.vol_debounce_count >= config.vol_debounce_target) {
        advisory.vol_debounce_active = 1;
        advisory.volatility_anomaly = vol_raw_anomaly ? 1 : 0;
    } else {
        advisory.vol_debounce_active = 0;
        advisory.volatility_anomaly = 0;
    }

    // 4. Liquidity Displacement / Order Book Thinning Calculation
    float current_depth = bid_size + ask_size;
    if (base_depth > 1e-6f) {
        float depth_ratio = current_depth / base_depth;
        if (depth_ratio < 1.0f) {
            advisory.depth_thinning_pct = std::clamp(1.0f - depth_ratio, 0.0f, 1.0f);
        } else {
            advisory.depth_thinning_pct = 0.0f;
        }
    } else {
        advisory.depth_thinning_pct = 0.0f;
    }

    bool liq_raw_anomaly = (advisory.depth_thinning_pct >= config.depth_thinning_threshold);
    if (state.liq_debounce_count >= config.liq_debounce_target) {
        advisory.liq_debounce_active = 1;
        advisory.liquidity_anomaly = liq_raw_anomaly ? 1 : 0;
    } else {
        advisory.liq_debounce_active = 0;
        advisory.liquidity_anomaly = 0;
    }

    // 5. Pair Correlation Break Calculation
    advisory.rolling_correlation = roll_corr;
    float corr_drop = exp_corr - roll_corr;
    bool corr_raw_anomaly = (corr_drop >= config.min_expected_correlation);

    if (state.corr_debounce_count >= config.corr_debounce_target) {
        advisory.corr_debounce_active = 1;
        advisory.correlation_break = corr_raw_anomaly ? 1 : 0;
    } else {
        advisory.corr_debounce_active = 0;
        advisory.correlation_break = 0;
    }

    // 6. Severity Score & Active Advisory Flag
    float vol_score = std::clamp((advisory.volatility_expansion_ratio - 1.0f) * 20.0f, 0.0f, 40.0f);
    float liq_score = advisory.depth_thinning_pct * 30.0f;
    float corr_score = std::clamp(corr_drop * 30.0f, 0.0f, 30.0f);

    advisory.anomaly_severity = std::clamp(vol_score + liq_score + corr_score, 0.0f, 100.0f);

    if (advisory.volatility_anomaly || advisory.liquidity_anomaly || advisory.correlation_break) {
        advisory.advisory_active = 1;
    } else {
        advisory.advisory_active = 0;
    }

    return advisory;
}

} // namespace AILLE
