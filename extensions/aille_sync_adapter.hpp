/*
 * AILLE Framework - Sync Adapter Extension
 * AI-Load Integrity and Layered Evaluation
 *
 * Deterministic, allocator-free temporal clock synchronization adapter binding
 * AILEE Finance runtime to AILEE Runtime Protocol authoritative clock.
 * Operates with strictly 64-byte cache-aligned structs and zero-jitter tick cadence.
 *
 * Version Tag: SYNC_ADAPTER_V1
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#ifndef AILLE_SYNC_ADAPTER_HPP
#define AILLE_SYNC_ADAPTER_HPP

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

constexpr const char* SYNC_ADAPTER_VERSION = "SYNC_ADAPTER_V1";

constexpr std::uint8_t SYNC_FLAG_ALIGNED = 0x01;
constexpr std::uint8_t SYNC_FLAG_EXTERNAL = 0x02;
constexpr std::uint8_t SYNC_FLAG_FALLBACK = 0x04;
constexpr std::uint8_t SYNC_FLAG_DRIFT_WARN = 0x08;
constexpr std::uint8_t SYNC_FLAG_GAP_DETECTED = 0x10;

// ============================================================================
// CORE DATA STRUCTURES (STRICTLY 64-BYTE CACHE-ALIGNED & ALLOCATOR-FREE)
// ============================================================================

struct alignas(64) SyncTick final {
    std::uint64_t tick_index;          ///< Monotonic synchronized tick counter
    std::uint64_t timestamp_ns;       ///< Authoritative nanosecond timestamp
    std::int64_t drift_ns;            ///< Measured temporal drift relative to cadence
    std::uint64_t tick_cadence_ns;    ///< Standard cadence delta in nanoseconds
    float wave_phase;                 ///< Authoritative wave phase [0.0, 2π]
    float confidence;                 ///< Temporal synchronization confidence [0.0, 1.0]
    std::uint8_t alignment_flags;     ///< Bitmask of synchronization state flags
    std::uint8_t degraded;            ///< 1 if timing jitter or drift degraded clock integrity
    std::uint8_t escalate_stress;     ///< 1 to trigger Layer 13 Stress Override escalation
    std::uint8_t escalate_meta_lock;  ///< 1 to trigger Layer 14 Meta-Governance Lock
    std::uint8_t _padding[20];

    constexpr SyncTick()
        : tick_index(0), timestamp_ns(0), drift_ns(0), tick_cadence_ns(10000000ULL),
          wave_phase(0.0f), confidence(1.0f), alignment_flags(SYNC_FLAG_ALIGNED),
          degraded(0), escalate_stress(0), escalate_meta_lock(0), _padding{} {}
};
static_assert(sizeof(SyncTick) == 64, "SyncTick must be exactly 64 bytes");
static_assert(alignof(SyncTick) == 64, "SyncTick must be alignas(64)");

struct alignas(64) SyncAdapterState final {
    std::uint64_t current_tick_index;  ///< Current tick index
    std::uint64_t last_timestamp_ns;   ///< Last tick nanosecond timestamp
    std::uint64_t expected_cadence_ns; ///< Configured baseline tick cadence
    std::int64_t max_observed_drift_ns;///< Maximum drift observed in window
    float current_wave_phase;         ///< Active wave phase [0.0, 2π]
    float clock_confidence;           ///< Active clock confidence
    std::uint8_t external_clock_active;///< 1 if bound to external ARP protocol node clock
    std::uint8_t sync_degraded;        ///< 1 if sync is degraded
    std::uint8_t _padding[22];

    constexpr SyncAdapterState()
        : current_tick_index(0), last_timestamp_ns(0), expected_cadence_ns(10000000ULL),
          max_observed_drift_ns(0), current_wave_phase(0.0f), clock_confidence(1.0f),
          external_clock_active(0), sync_degraded(0), _padding{} {}
};
static_assert(sizeof(SyncAdapterState) == 64, "SyncAdapterState must be exactly 64 bytes");

struct alignas(64) SyncAdapterConfig final {
    std::uint64_t target_cadence_ns;     ///< Baseline tick cadence (default: 10ms / 10,000,000 ns)
    std::uint64_t max_drift_threshold_ns;///< Max drift threshold before degradation (5ms default)
    float min_confidence_threshold;     ///< Min confidence floor (default: 0.80)
    std::uint8_t auto_escalate_stress;   ///< 1 to auto-escalate drift breach to Layer 13
    std::uint8_t auto_escalate_meta_lock;///< 1 to auto-escalate severe breach to Layer 14
    std::uint8_t _padding[42];

    constexpr SyncAdapterConfig()
        : target_cadence_ns(10000000ULL), max_drift_threshold_ns(5000000ULL),
          min_confidence_threshold(0.80f), auto_escalate_stress(1),
          auto_escalate_meta_lock(1), _padding{} {}
};
static_assert(sizeof(SyncAdapterConfig) == 64, "SyncAdapterConfig must be exactly 64 bytes");

struct alignas(64) SyncAdapterObservabilityMetrics final {
    std::uint64_t total_ticks_processed;
    std::uint64_t external_sync_ticks;
    std::uint64_t fallback_ticks;
    std::int64_t last_drift_ns;
    float current_confidence;
    std::uint8_t external_clock_active;
    std::uint8_t sync_degraded;
    std::uint8_t stress_escalations;
    std::uint8_t meta_lock_escalations;
    std::uint8_t _padding[24];

    constexpr SyncAdapterObservabilityMetrics()
        : total_ticks_processed(0), external_sync_ticks(0), fallback_ticks(0),
          last_drift_ns(0), current_confidence(1.0f), external_clock_active(0),
          sync_degraded(0), stress_escalations(0), meta_lock_escalations(0),
          _padding{} {}
};
static_assert(sizeof(SyncAdapterObservabilityMetrics) == 64, "SyncAdapterObservabilityMetrics must be exactly 64 bytes");

// ============================================================================
// SYNC ADAPTER ENGINE CLASS
// ============================================================================

class alignas(64) SyncAdapter final {
private:
    SyncAdapterState state_;
    SyncAdapterConfig config_;
    SyncAdapterObservabilityMetrics metrics_;
    SyncTick current_tick_;

public:
    SyncAdapter() noexcept;
    explicit SyncAdapter(const SyncAdapterConfig& config) noexcept;

    /**
     * @brief Ingests external authoritative clock temporal output from AILEE Runtime Protocol.
     */
    SyncTick ingest_protocol_clock(
        std::uint64_t tick_index,
        std::uint64_t timestamp_ns,
        float wave_phase,
        float confidence = 1.0f
    ) noexcept;

    /**
     * @brief Advances tick deterministically in standalone mode using zero-jitter fallback.
     */
    SyncTick advance_tick() noexcept;

    /**
     * @brief Returns current cached SyncTick temporal snapshot.
     */
    [[nodiscard]] SyncTick get_sync_tick() const noexcept { return current_tick_; }

    /**
     * @brief Returns active state.
     */
    [[nodiscard]] SyncAdapterState get_state() const noexcept { return state_; }

    /**
     * @brief Returns active observability metrics.
     */
    [[nodiscard]] SyncAdapterObservabilityMetrics get_metrics() const noexcept { return metrics_; }

    /**
     * @brief Reset sync adapter state to initial conditions.
     */
    void reset() noexcept;

    /**
     * @brief Configure sync adapter.
     */
    void set_config(const SyncAdapterConfig& config) noexcept { config_ = config; }
};

} // namespace AILLE

#endif // AILLE_SYNC_ADAPTER_HPP
