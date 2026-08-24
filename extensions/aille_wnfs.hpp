/*
 * AILLE Framework - WaveNativeFinanceStream (WNFS) Layer 18 Extension
 * AI-Load Integrity and Layered Evaluation
 *
 * Deterministic, allocator-free real-time streaming data ingestion and wave synchronization transport.
 * Operates with lock-free wave channel ring-buffers, strictly 64-byte cache-aligned structs,
 * and sub-microsecond latency SLAs (< 350 ns p50 / < 900 ns p99).
 *
 * Version Tag: WAVE_NATIVE_FINANCE_STREAM_V1
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#ifndef AILLE_WNFS_HPP
#define AILLE_WNFS_HPP

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <cstring>
#include "../aille.hpp"

namespace AILLE {

// ============================================================================
// VERSION TAG & CONSTANTS
// ============================================================================

constexpr const char* WNFS_VERSION = "WAVE_NATIVE_FINANCE_STREAM_V1";
constexpr std::size_t WNFS_RING_CAPACITY = 1024; // Power-of-two for fast bitwise modulo

constexpr std::uint8_t WNFS_FLAG_GAP = 0x01;
constexpr std::uint8_t WNFS_FLAG_OUT_OF_ORDER = 0x02;
constexpr std::uint8_t WNFS_FLAG_CORRUPTED = 0x04;

constexpr std::uint8_t WNFS_STATUS_HEALTHY = 0;
constexpr std::uint8_t WNFS_STATUS_DEGRADED = 1;
constexpr std::uint8_t WNFS_STATUS_CORRUPTED = 2;
constexpr std::uint8_t WNFS_STATUS_LOCKED = 3;

// ============================================================================
// CORE DATA STRUCTURES (STRICTLY 64-BYTE CACHE-ALIGNED & ALLOCATOR-FREE)
// ============================================================================

struct alignas(64) WNFSFrame final {
    std::uint64_t sequence_id;        ///< Monotonic WNN wave sequence index
    std::uint64_t timestamp_ns;      ///< High-resolution ingest timestamp
    float bid_price;                 ///< Top-of-book L1 bid price
    float ask_price;                 ///< Top-of-book L1 ask price
    float bid_size;                  ///< Top-of-book L1 bid size
    float ask_size;                  ///< Top-of-book L1 ask size
    float last_price;                ///< Executed trade price
    float last_size;                 ///< Executed trade volume
    float vwap_delta;                ///< Intraday VWAP divergence delta
    std::uint32_t symbol_id;         ///< Numeric symbol identifier
    std::uint8_t wave_channel_id;    ///< Dedicated WNN channel ID
    std::uint8_t frame_flags;        ///< Bit flags: GAP, OUT_OF_ORDER, CORRUPTED
    std::uint8_t _padding[10];

    constexpr WNFSFrame()
        : sequence_id(0), timestamp_ns(0), bid_price(0.0f), ask_price(0.0f),
          bid_size(0.0f), ask_size(0.0f), last_price(0.0f), last_size(0.0f),
          vwap_delta(0.0f), symbol_id(0), wave_channel_id(0), frame_flags(0),
          _padding{} {}
};
static_assert(sizeof(WNFSFrame) == 64, "WNFSFrame must be exactly 64 bytes");

struct alignas(64) WNFSState final {
    std::uint64_t expected_sequence;  ///< Expected next monotonic sequence ID
    std::uint64_t processed_frames;   ///< Total frames successfully processed
    std::uint64_t gap_count;          ///< Total sequence gaps detected
    float wave_phase;                ///< Calculated WNN wave phase [0.0, 2π]
    float wave_amplitude;            ///< Ingestion energy / volume impulse amplitude
    std::uint32_t symbol_id;         ///< Channel symbol identifier
    std::uint32_t clone_status_mask;  ///< Bitmask of active cluster clone health statuses
    std::uint8_t degraded_clone_count;///< Total number of clones currently degraded
    std::uint8_t channel_status;     ///< 0: HEALTHY, 1: DEGRADED, 2: CORRUPTED, 3: LOCKED
    std::uint8_t _padding[22];

    constexpr WNFSState()
        : expected_sequence(1), processed_frames(0), gap_count(0),
          wave_phase(0.0f), wave_amplitude(1.0f), symbol_id(0),
          clone_status_mask(0), degraded_clone_count(0),
          channel_status(WNFS_STATUS_HEALTHY), _padding{} {}
};
static_assert(sizeof(WNFSState) == 64, "WNFSState must be exactly 64 bytes");

struct alignas(64) WNFSAdvisory final {
    float ingestion_confidence;      ///< Transport integrity score [0.0, 1.0]
    float wave_energy_factor;        ///< Normalized stream impulse scaling factor
    float tick_acceleration;         ///< Micro-tick frequency derivative
    std::uint8_t stream_degraded;    ///< 1 if gap or corrupt frame active
    std::uint8_t trigger_stress_escalation; ///< 1 if Layer 13/14 override required
    std::uint8_t hft_freeze_required;  ///< 1 if HFT Delta-V engine should freeze
    std::uint8_t channel_id;         ///< Active WNN channel ID
    std::uint8_t _padding[48];

    constexpr WNFSAdvisory()
        : ingestion_confidence(1.0f), wave_energy_factor(1.0f),
          tick_acceleration(0.0f), stream_degraded(0),
          trigger_stress_escalation(0), hft_freeze_required(0),
          channel_id(0), _padding{} {}
};
static_assert(sizeof(WNFSAdvisory) == 64, "WNFSAdvisory must be exactly 64 bytes");

struct alignas(64) WNFSObservabilityMetrics final {
    std::uint64_t processed_frames;
    std::uint64_t gap_count;
    float ingestion_confidence;
    float wave_energy_factor;
    std::uint32_t clone_status_mask;
    std::uint8_t degraded_clone_count;
    std::uint8_t channel_status;
    std::uint8_t stream_degraded;
    std::uint8_t _padding[33];

    constexpr WNFSObservabilityMetrics()
        : processed_frames(0), gap_count(0), ingestion_confidence(1.0f),
          wave_energy_factor(1.0f), clone_status_mask(0),
          degraded_clone_count(0), channel_status(WNFS_STATUS_HEALTHY),
          stream_degraded(0), _padding{} {}
};
static_assert(sizeof(WNFSObservabilityMetrics) == 64, "WNFSObservabilityMetrics must be exactly 64 bytes");

struct alignas(64) WNFSTraceStep final {
    std::uint64_t timestamp_ns;
    std::uint64_t sequence_id;
    float wave_phase;
    float wave_amplitude;
    std::uint32_t symbol_id;
    std::uint8_t channel_status;
    std::uint8_t frame_flags;
    std::uint8_t _padding[30];

    constexpr WNFSTraceStep()
        : timestamp_ns(0), sequence_id(0), wave_phase(0.0f),
          wave_amplitude(1.0f), symbol_id(0), channel_status(0),
          frame_flags(0), _padding{} {}
};
static_assert(sizeof(WNFSTraceStep) == 64, "WNFSTraceStep must be exactly 64 bytes");

struct alignas(64) WNFSConfig final {
    std::uint64_t max_sequence_gaps;   ///< Threshold of gaps before triggering CRISIS escalation
    float min_confidence_threshold;   ///< Ingestion confidence floor
    std::uint8_t enable_wave_sync;    ///< 1 to enable WNN multi-channel wave phase sync
    std::uint8_t _padding[47];

    constexpr WNFSConfig()
        : max_sequence_gaps(5), min_confidence_threshold(0.70f),
          enable_wave_sync(1), _padding{} {}
};
static_assert(sizeof(WNFSConfig) == 64, "WNFSConfig must be exactly 64 bytes");

// ============================================================================
// LOCK-FREE RING-BUFFER WAVE CHANNEL
// ============================================================================

class alignas(64) WNFSChannel final {
private:
    alignas(64) WNFSFrame ring_buffer_[WNFS_RING_CAPACITY]{};
    alignas(64) std::atomic<std::size_t> write_head_{0};
    alignas(64) std::atomic<std::size_t> read_tail_{0};

public:
    WNFSChannel() noexcept = default;

    [[nodiscard]] bool push_frame(const WNFSFrame& frame) noexcept {
        const std::size_t head = write_head_.load(std::memory_order_relaxed);
        const std::size_t tail = read_tail_.load(std::memory_order_acquire);

        if ((head - tail) >= WNFS_RING_CAPACITY) {
            return false; // Ring buffer full
        }

        ring_buffer_[head & (WNFS_RING_CAPACITY - 1)] = frame;
        write_head_.store(head + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool pop_frame(WNFSFrame& out_frame) noexcept {
        const std::size_t tail = read_tail_.load(std::memory_order_relaxed);
        const std::size_t head = write_head_.load(std::memory_order_acquire);

        if (tail == head) {
            return false; // Ring buffer empty
        }

        out_frame = ring_buffer_[tail & (WNFS_RING_CAPACITY - 1)];
        read_tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        const std::size_t head = write_head_.load(std::memory_order_relaxed);
        const std::size_t tail = read_tail_.load(std::memory_order_relaxed);
        return (head >= tail) ? (head - tail) : 0;
    }

    void reset() noexcept {
        write_head_.store(0, std::memory_order_relaxed);
        read_tail_.store(0, std::memory_order_relaxed);
    }
};

// ============================================================================
// EVALUATION FUNCTION
// ============================================================================

/**
 * @brief Evaluates incoming WNFS Frame against Wave State and Config.
 * Pure functional, zero-allocation, lock-free evaluation.
 */
[[nodiscard]] WNFSAdvisory evaluate_wnfs_advisory(
    const WNFSFrame& frame,
    WNFSState& state,
    const WNFSConfig& config = WNFSConfig(),
    const SafetyState* safety = nullptr
) noexcept;

} // namespace AILLE

#endif // AILLE_WNFS_HPP
