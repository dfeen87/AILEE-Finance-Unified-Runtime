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
        *anomaly_advisory_ = AILLE::evaluate_anomaly_advisory(*anomaly_state_, AnomalyConfig(), safety_state_);
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

    // 2. Volatility Expansion Calculation
    if (state.baseline_volatility > 1e-6f) {
        advisory.volatility_expansion_ratio = state.ewma_volatility / state.baseline_volatility;
    } else {
        advisory.volatility_expansion_ratio = 1.0f;
    }

    bool vol_raw_anomaly = (advisory.volatility_expansion_ratio >= config.volatility_threshold);
    if (state.vol_debounce_count >= config.vol_debounce_target) {
        advisory.vol_debounce_active = 1;
        advisory.volatility_anomaly = vol_raw_anomaly ? 1 : 0;
    } else {
        advisory.vol_debounce_active = 0;
        advisory.volatility_anomaly = 0;
    }

    // 3. Liquidity Displacement / Order Book Thinning Calculation
    float current_depth = state.bid_size + state.ask_size;
    if (state.baseline_depth > 1e-6f) {
        float depth_ratio = current_depth / state.baseline_depth;
        if (depth_ratio < 1.0f) {
            advisory.depth_thinning_pct = 1.0f - depth_ratio;
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

    // 4. Pair Correlation Break Calculation
    advisory.rolling_correlation = state.rolling_correlation;
    float corr_drop = state.expected_correlation - state.rolling_correlation;
    bool corr_raw_anomaly = (corr_drop >= config.min_expected_correlation);

    if (state.corr_debounce_count >= config.corr_debounce_target) {
        advisory.corr_debounce_active = 1;
        advisory.correlation_break = corr_raw_anomaly ? 1 : 0;
    } else {
        advisory.corr_debounce_active = 0;
        advisory.correlation_break = 0;
    }

    // 5. Severity Score & Active Advisory Flag
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
