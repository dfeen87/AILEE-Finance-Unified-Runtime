/*
 * AILLE Framework - Unified Cohesive Runtime & Resiliency Engine (Layer 19) Implementation
 * AI-Load Integrity and Layered Evaluation
 *
 * Master deterministic runtime orchestrator tying together all 18 layers into a single
 * allocator-free, cache-aligned master execution cycle with sub-microsecond latency SLAs
 * and fail-closed multi-layer fault escalation.
 *
 * Version Tag: UNIFIED_RUNTIME_V1
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#include "aille_unified_runtime.hpp"
#include "aille_wnfs.hpp"
#include "aille_anomaly.hpp"
#include "aille_stress_regime_override.hpp"
#include "aille_meta_governance.hpp"

namespace AILLE {

[[nodiscard]] UnifiedRuntimeAdvisory evaluate_unified_runtime(
    UnifiedRuntimeState& state,
    UnifiedRuntimeMetrics& metrics,
    const WNFSAdvisory* wnfs_adv,
    const AnomalyAdvisory* anomaly_adv,
    const MarketStabilizerAdvisory* MSGAM_adv,
    const StressPortfolioState* stress_state,
    const MetaGovernanceState* meta_state,
    const UnifiedRuntimeConfig& config,
    const SafetyState* safety
) noexcept {
    UnifiedRuntimeAdvisory advisory;
    state.cycle_sequence_id++;
    state.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();

    // 1. Hardware / Safety State check
    if (safety && (safety->kill_switch || safety->hardware_fault)) {
        state.system_status = UNIFIED_STATUS_META_LOCKED;
        state.resiliency_mode = UNIFIED_RESILIENCY_FAIL_CLOSED;
        state.execution_ready = 0;
        state.fault_escalated = 1;

        advisory.system_confidence = 0.0f;
        advisory.recommended_execution_scale = 0.0f;
        advisory.resilience_factor = 0.0f;
        advisory.system_status = UNIFIED_STATUS_META_LOCKED;
        advisory.execution_permitted = 0;
        advisory.hft_freeze_active = 1;

        metrics.meta_lock_active = 1;
        metrics.total_fault_escalations++;
        metrics.total_cycles_processed++;
        return advisory;
    }

    // 2. Evaluate streaming transport integrity (Layer 18 - WNFS)
    bool wnfs_fault = false;
    if (wnfs_adv) {
        if (wnfs_adv->stream_degraded) {
            metrics.stream_degraded = 1;
            state.system_status = UNIFIED_STATUS_DEGRADED;
            advisory.hft_freeze_active = 1;
            advisory.system_confidence *= 0.5f;
            advisory.recommended_execution_scale *= 0.5f;
        }
        if (wnfs_adv->trigger_stress_escalation) {
            wnfs_fault = true;
        }
    }

    // 3. Evaluate anomaly detection (Layer 16)
    bool anomaly_fault = false;
    if (anomaly_adv) {
        if (anomaly_adv->advisory_active) {
            state.system_status = UNIFIED_STATUS_DEGRADED;
            float anomaly_penalty = std::clamp(anomaly_adv->anomaly_severity / 100.0f, 0.0f, 0.8f);
            advisory.system_confidence *= (1.0f - anomaly_penalty);
            advisory.recommended_execution_scale *= (1.0f - anomaly_penalty);
            if (anomaly_adv->anomaly_severity > 80.0f) {
                anomaly_fault = true;
            }
        }
    }

    // 4. Evaluate MSGAM market stabilizer (Layer 7.9)
    if (MSGAM_adv) {
        if (MSGAM_adv->risk_elevated || MSGAM_adv->governor_active) {
            advisory.recommended_execution_scale *= MSGAM_adv->stabilization_factor;
            advisory.recommended_execution_scale = std::clamp(
                advisory.recommended_execution_scale, 0.0f, MSGAM_adv->dynamic_clamp_limit
            );
        }
    }

    // 5. Evaluate Stress Regime Override (Layer 13) & Fault Escalations
    bool crisis_escalation = (config.auto_escalate_faults && (wnfs_fault || anomaly_fault)) ||
                             (stress_state && stress_state->stress_level == static_cast<std::uint8_t>(StressMode::CRISIS));
    bool stress_mode = (stress_state && stress_state->stress_level == static_cast<std::uint8_t>(StressMode::STRESS));

    if (crisis_escalation) {
        state.system_status = UNIFIED_STATUS_STRESS_OVERRIDE;
        state.resiliency_mode = UNIFIED_RESILIENCY_FAIL_CLOSED;
        state.fault_escalated = 1;
        metrics.stress_override_active = 1;
        metrics.total_fault_escalations++;
        advisory.recommended_execution_scale = 0.0f; // FREEZE EXPOSURE
        advisory.system_confidence = 0.0f;
        advisory.hft_freeze_active = 1;
    } else if (stress_mode) {
        state.system_status = UNIFIED_STATUS_STRESS_OVERRIDE;
        state.resiliency_mode = UNIFIED_RESILIENCY_HIGH_STRESS;
        metrics.stress_override_active = 1;
        advisory.recommended_execution_scale *= 0.30f; // HARD DE-RISK
        advisory.hft_freeze_active = 1;
    }

    // 6. Evaluate Meta-Governance Lock (Layer 14)
    if (meta_state) {
        if (!meta_state->execution_ready && config.enforce_strict_lock) {
            state.system_status = UNIFIED_STATUS_META_LOCKED;
            state.execution_ready = 0;
            metrics.meta_lock_active = 1;
            advisory.execution_permitted = 0;
            advisory.recommended_execution_scale = 0.0f;
            advisory.system_confidence = 0.0f;
        } else {
            state.execution_ready = meta_state->execution_ready;
            metrics.meta_lock_active = 0;
        }
    }

    // Calculate system stability and aggregate risk
    state.aggregate_risk_score = 1.0f - std::clamp(advisory.system_confidence, 0.0f, 1.0f);
    state.systemic_stability_index = std::clamp(advisory.recommended_execution_scale, 0.0f, 1.0f);

    if (state.aggregate_risk_score > config.max_allowed_risk) {
        advisory.recommended_execution_scale *= 0.5f;
    }

    advisory.system_status = state.system_status;
    advisory.execution_permitted = (state.execution_ready && state.system_status != UNIFIED_STATUS_META_LOCKED) ? 1 : 0;
    advisory.resilience_factor = (state.system_status == UNIFIED_STATUS_NOMINAL) ? 1.0f : 0.5f;

    metrics.total_cycles_processed++;
    metrics.max_observed_risk = std::max(metrics.max_observed_risk, state.aggregate_risk_score);
    metrics.min_observed_stability = std::min(metrics.min_observed_stability, state.systemic_stability_index);

    return advisory;
}

void AILLEEngine::evaluate_unified_runtime() {
    if (!unified_state_ || !unified_advisory_) return;
    UnifiedRuntimeConfig default_cfg;
    const UnifiedRuntimeConfig& cfg = unified_config_ ? *unified_config_ : default_cfg;

    UnifiedRuntimeMetrics default_metrics;
    UnifiedRuntimeMetrics& metrics = unified_metrics_ ? *unified_metrics_ : default_metrics;

    UnifiedRuntimeState mutable_state = *unified_state_;
    *unified_advisory_ = ::AILLE::evaluate_unified_runtime(
        mutable_state,
        metrics,
        wnfs_advisory_,
        anomaly_advisory_,
        stabilizer_advisory_,
        stress_state_,
        meta_state_,
        cfg,
        safety_state_
    );
    *unified_state_ = mutable_state;
}

} // namespace AILLE
