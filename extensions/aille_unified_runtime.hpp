/*
 * AILLE Framework - Unified Cohesive Runtime & Resiliency Engine (Layer 19)
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

#ifndef AILLE_UNIFIED_RUNTIME_HPP
#define AILLE_UNIFIED_RUNTIME_HPP

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>
#include <cstring>
#include "../aille.hpp"

namespace AILLE {

// ============================================================================
// VERSION TAG & CONSTANTS
// ============================================================================

constexpr const char* UNIFIED_RUNTIME_VERSION = "UNIFIED_RUNTIME_V1";

constexpr std::uint8_t UNIFIED_STATUS_NOMINAL = 0;
constexpr std::uint8_t UNIFIED_STATUS_DEGRADED = 1;
constexpr std::uint8_t UNIFIED_STATUS_STRESS_OVERRIDE = 2;
constexpr std::uint8_t UNIFIED_STATUS_META_LOCKED = 3;

constexpr std::uint8_t UNIFIED_RESILIENCY_STANDARD = 0;
constexpr std::uint8_t UNIFIED_RESILIENCY_HIGH_STRESS = 1;
constexpr std::uint8_t UNIFIED_RESILIENCY_FAIL_CLOSED = 2;

// ============================================================================
// CORE DATA STRUCTURES (STRICTLY 64-BYTE CACHE-ALIGNED & ALLOCATOR-FREE)
// ============================================================================

struct alignas(64) UnifiedRuntimeState final {
    std::uint64_t cycle_sequence_id;  ///< Monotonic master execution cycle index
    std::uint64_t timestamp_ns;       ///< Nanosecond timestamp for active cycle
    float aggregate_risk_score;       ///< Cross-layer weighted composite risk score [0.0, 1.0]
    float systemic_stability_index;   ///< Overall multi-layer system stability [0.0, 1.0]
    std::uint32_t active_layer_mask;  ///< Bitmask of active operational layers (18 bits)
    std::uint8_t system_status;       ///< 0: NOMINAL, 1: DEGRADED, 2: STRESS_OVERRIDE, 3: META_LOCKED
    std::uint8_t resiliency_mode;     ///< 0: STANDARD, 1: HIGH_STRESS, 2: FAIL_CLOSED
    std::uint8_t execution_ready;     ///< 1 if execution post-meta-governance lock is ready
    std::uint8_t fault_escalated;     ///< 1 if stream/anomaly fault escalated to CRISIS mode
    std::uint8_t _padding[20];

    constexpr UnifiedRuntimeState()
        : cycle_sequence_id(0), timestamp_ns(0), aggregate_risk_score(0.0f),
          systemic_stability_index(1.0f), active_layer_mask(0x3FFFF),
          system_status(UNIFIED_STATUS_NOMINAL), resiliency_mode(UNIFIED_RESILIENCY_STANDARD),
          execution_ready(1), fault_escalated(0), _padding{} {}
};
static_assert(sizeof(UnifiedRuntimeState) == 64, "UnifiedRuntimeState must be exactly 64 bytes");

struct alignas(64) UnifiedRuntimeMetrics final {
    std::uint64_t total_cycles_processed;
    std::uint64_t last_cycle_latency_ns;
    float max_observed_risk;
    float min_observed_stability;
    std::uint32_t total_fault_escalations;
    std::uint8_t stream_degraded;
    std::uint8_t stress_override_active;
    std::uint8_t meta_lock_active;
    std::uint8_t _padding[29];

    constexpr UnifiedRuntimeMetrics()
        : total_cycles_processed(0), last_cycle_latency_ns(0),
          max_observed_risk(0.0f), min_observed_stability(1.0f),
          total_fault_escalations(0), stream_degraded(0),
          stress_override_active(0), meta_lock_active(0), _padding{} {}
};
static_assert(sizeof(UnifiedRuntimeMetrics) == 64, "UnifiedRuntimeMetrics must be exactly 64 bytes");

struct alignas(64) UnifiedRuntimeTraceStep final {
    std::uint64_t timestamp_ns;
    std::uint64_t sequence_id;
    float residual_tension_score;
    float membrane_lyapunov_energy;
    std::uint32_t active_layer_mask;
    std::uint8_t system_status;
    std::uint8_t execution_ready;
    std::uint8_t _padding[30];

    constexpr UnifiedRuntimeTraceStep()
        : timestamp_ns(0), sequence_id(0), residual_tension_score(0.0f),
          membrane_lyapunov_energy(0.0f), active_layer_mask(0),
          system_status(0), execution_ready(0), _padding{} {}
};
static_assert(sizeof(UnifiedRuntimeTraceStep) == 64, "UnifiedRuntimeTraceStep must be exactly 64 bytes");

struct alignas(64) UnifiedRuntimeAdvisory final {
    float system_confidence;          ///< Unified system-wide decision confidence [0.0, 1.0]
    float recommended_execution_scale;///< Master exposure scale factor [0.0, 1.0]
    float resilience_factor;          ///< Fault tolerance & recovery factor
    std::uint8_t system_status;       ///< 0: NOMINAL, 1: DEGRADED, 2: STRESS_OVERRIDE, 3: META_LOCKED
    std::uint8_t execution_permitted; ///< 1 if orders & allocations are authorized
    std::uint8_t hft_freeze_active;   ///< 1 if HFT impulse engine is halted
    std::uint8_t _padding[45];

    constexpr UnifiedRuntimeAdvisory()
        : system_confidence(1.0f), recommended_execution_scale(1.0f),
          resilience_factor(1.0f), system_status(UNIFIED_STATUS_NOMINAL),
          execution_permitted(1), hft_freeze_active(0), _padding{} {}
};
static_assert(sizeof(UnifiedRuntimeAdvisory) == 64, "UnifiedRuntimeAdvisory must be exactly 64 bytes");

struct alignas(64) UnifiedRuntimeConfig final {
    float max_allowed_risk;           ///< Max risk score threshold before de-risking
    float min_stability_threshold;   ///< Stability floor threshold
    std::uint64_t max_cycle_latency_ns;///< Latency SLA threshold
    std::uint8_t enforce_strict_lock; ///< 1 to freeze execution on meta-governance lock
    std::uint8_t auto_escalate_faults;///< 1 to escalate stream/anomaly faults to CRISIS mode
    std::uint8_t _padding[38];

    constexpr UnifiedRuntimeConfig()
        : max_allowed_risk(0.75f), min_stability_threshold(0.50f),
          max_cycle_latency_ns(1000000ULL), enforce_strict_lock(1),
          auto_escalate_faults(1), _padding{} {}
};
static_assert(sizeof(UnifiedRuntimeConfig) == 64, "UnifiedRuntimeConfig must be exactly 64 bytes");

// ============================================================================
// MASTER EVALUATION FUNCTION
// ============================================================================

/**
 * @brief Evaluates the entire multi-layer AILEE Finance Runtime deterministically.
 * Pure functional, zero-allocation master orchestration cycle.
 */
[[nodiscard]] UnifiedRuntimeAdvisory evaluate_unified_runtime(
    UnifiedRuntimeState& state,
    UnifiedRuntimeMetrics& metrics,
    const WNFSAdvisory* wnfs_adv,
    const AnomalyAdvisory* anomaly_adv,
    const MarketStabilizerAdvisory* MSGAM_adv,
    const StressPortfolioState* stress_state,
    const MetaGovernanceState* meta_state,
    const UnifiedRuntimeConfig& config = UnifiedRuntimeConfig(),
    const SafetyState* safety = nullptr,
    const SyncTick* sync_tick = nullptr
) noexcept;

} // namespace AILLE

#endif // AILLE_UNIFIED_RUNTIME_HPP
