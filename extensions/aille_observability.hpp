/*
 * AILLE Observability Extension
 * Lock-free atomic counters (per-core sharded), OTel + Prometheus export plane,
 * and real-time health stream (ring-buffer snapshot channel).
 *
 * License: MIT (see LICENSE)
 * Copyright (c) 2026 Don Michael Feeney Jr
 */

#ifndef AILLE_OBSERVABILITY_HPP
#define AILLE_OBSERVABILITY_HPP

#include <atomic>
#include <array>
#include <cstdint>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include "../aille.hpp"

namespace AILLE {
namespace Observability {

constexpr size_t MAX_CORES = 64;

// Lock-free atomic counters, per-core sharded, aligned to cache line
struct alignas(64) CoreMetrics {
    std::atomic<uint64_t> sequence{0};
    std::atomic<uint64_t> total_decisions{0};
    std::atomic<uint64_t> valid_decisions{0};
    std::atomic<uint64_t> fallback_activations{0};
    std::atomic<uint64_t> rejected_confidence{0};
    std::atomic<uint64_t> rejected_consensus{0};
    std::atomic<uint64_t> invalid_inputs{0};

    // Seqlock write
    void begin_write() {
        // Prevent subsequent writes from being moved before the sequence bump
        sequence.fetch_add(1, std::memory_order_acquire);
    }
    void end_write() {
        // Ensure all prior writes are visible before sequence is finalized
        sequence.fetch_add(1, std::memory_order_release);
    }
};

// Snapshot structure
struct MetricsSnapshot {
    uint64_t total_decisions = 0;
    uint64_t valid_decisions = 0;
    uint64_t fallback_activations = 0;
    uint64_t rejected_confidence = 0;
    uint64_t rejected_consensus = 0;
    uint64_t invalid_inputs = 0;
};

// Backpressure-safe ring buffer for health stream snapshots
template <size_t Capacity = 1024>
class HealthStreamRingBuffer {
private:
    std::array<MetricsSnapshot, Capacity> buffer;
    alignas(64) std::atomic<size_t> write_idx{0};
    alignas(64) std::atomic<size_t> read_idx{0};

public:
    // Try to write (drop if full - backpressure safe)
    bool try_push(const MetricsSnapshot& snapshot) {
        size_t current_write = write_idx.load(std::memory_order_relaxed);
        size_t next_write = (current_write + 1) % Capacity;
        if (next_write == read_idx.load(std::memory_order_acquire)) {
            return false; // Full, drop
        }
        buffer[current_write] = snapshot;
        write_idx.store(next_write, std::memory_order_release);
        return true;
    }

    bool try_pop(MetricsSnapshot& snapshot) {
        size_t current_read = read_idx.load(std::memory_order_relaxed);
        if (current_read == write_idx.load(std::memory_order_acquire)) {
            return false; // Empty
        }
        snapshot = buffer[current_read];
        read_idx.store((current_read + 1) % Capacity, std::memory_order_release);
        return true;
    }
};

class ExportPlane {
private:
    std::array<CoreMetrics, MAX_CORES> sharded_metrics;

    std::atomic<bool> kill_switch_engaged_{false};
    std::atomic<bool> hardware_fault_detected_{false};

    std::thread export_thread;
    std::atomic<bool> running_{false};

public:
    HealthStreamRingBuffer<1024> health_stream;

    ExportPlane() {}
    ~ExportPlane() { stop(); }

    void start() {
        if (!running_.exchange(true)) {
            export_thread = std::thread(&ExportPlane::exportLoop, this);
        }
    }

    void stop() {
        if (running_.exchange(false)) {
            if (export_thread.joinable()) {
                export_thread.join();
            }
        }
    }

    void recordDecision(size_t core_id, const AILLE::Decision& decision) {
        if (core_id >= MAX_CORES) core_id = 0;
        auto& metrics = sharded_metrics[core_id];

        metrics.begin_write();
        metrics.total_decisions.fetch_add(1, std::memory_order_relaxed);
        if (decision.status == AILLE::DECISION_VALID) {
            metrics.valid_decisions.fetch_add(1, std::memory_order_relaxed);
        } else if (decision.status == AILLE::REJECTED_LOW_CONFIDENCE) {
            metrics.rejected_confidence.fetch_add(1, std::memory_order_relaxed);
            metrics.fallback_activations.fetch_add(1, std::memory_order_relaxed);
        } else if (decision.status == AILLE::REJECTED_NO_CONSENSUS) {
            metrics.rejected_consensus.fetch_add(1, std::memory_order_relaxed);
            metrics.fallback_activations.fetch_add(1, std::memory_order_relaxed);
        } else if (decision.status == AILLE::FALLBACK_ACTIVATED) {
            metrics.fallback_activations.fetch_add(1, std::memory_order_relaxed);
        } else {
            metrics.invalid_inputs.fetch_add(1, std::memory_order_relaxed);
        }
        metrics.end_write();
    }

    MetricsSnapshot getAggregatedSnapshot() const {
        MetricsSnapshot agg;
        for (size_t i = 0; i < MAX_CORES; ++i) {
            const auto& m = sharded_metrics[i];
            uint64_t seq1, seq2;
            uint64_t total, valid, fallback, conf, cons, invalid;
            while (true) {
                seq1 = m.sequence.load(std::memory_order_acquire);
                if (seq1 % 2 != 0) {
                    std::this_thread::yield();
                    continue; // Write in progress
                }

                total = m.total_decisions.load(std::memory_order_relaxed);
                valid = m.valid_decisions.load(std::memory_order_relaxed);
                fallback = m.fallback_activations.load(std::memory_order_relaxed);
                conf = m.rejected_confidence.load(std::memory_order_relaxed);
                cons = m.rejected_consensus.load(std::memory_order_relaxed);
                invalid = m.invalid_inputs.load(std::memory_order_relaxed);

                seq2 = m.sequence.load(std::memory_order_acquire);
                if (seq1 == seq2) {
                    break;
                }
            }

            agg.total_decisions += total;
            agg.valid_decisions += valid;
            agg.fallback_activations += fallback;
            agg.rejected_confidence += conf;
            agg.rejected_consensus += cons;
            agg.invalid_inputs += invalid;
        }
        return agg;
    }

    std::string generatePrometheusExport() const {
        MetricsSnapshot agg = getAggregatedSnapshot();
        std::stringstream ss;
        ss << "# HELP aille_total_decisions Total number of decisions processed\n";
        ss << "# TYPE aille_total_decisions counter\n";
        ss << "aille_total_decisions " << agg.total_decisions << "\n";

        ss << "# HELP aille_valid_decisions Total valid decisions\n";
        ss << "# TYPE aille_valid_decisions counter\n";
        ss << "aille_valid_decisions " << agg.valid_decisions << "\n";

        ss << "# HELP aille_fallback_activations Total fallback activations\n";
        ss << "# TYPE aille_fallback_activations counter\n";
        ss << "aille_fallback_activations " << agg.fallback_activations << "\n";

        ss << "# HELP aille_rejected_confidence Total rejected by confidence\n";
        ss << "# TYPE aille_rejected_confidence counter\n";
        ss << "aille_rejected_confidence " << agg.rejected_confidence << "\n";

        ss << "# HELP aille_rejected_consensus Total rejected by consensus\n";
        ss << "# TYPE aille_rejected_consensus counter\n";
        ss << "aille_rejected_consensus " << agg.rejected_consensus << "\n";

        ss << "# HELP aille_invalid_inputs Total invalid inputs\n";
        ss << "# TYPE aille_invalid_inputs counter\n";
        ss << "aille_invalid_inputs " << agg.invalid_inputs << "\n";

        return ss.str();
    }

    // Safety Invariants
    bool isAdvisoryOnly() const noexcept { return true; }
    bool requiresHumanConfirmation() const noexcept { return true; }

    void engageKillSwitch() noexcept { kill_switch_engaged_.store(true, std::memory_order_release); }
    void declareHardwareFault() noexcept { hardware_fault_detected_.store(true, std::memory_order_release); }

    AILLE::Decision enforceSafetyLayerVeto(AILLE::Decision input_decision) const {
        // Enforce fail-closed behavior on hardware fault
        if (hardware_fault_detected_.load(std::memory_order_acquire)) {
            AILLE::Decision safe_decision = input_decision;
            safe_decision.status = AILLE::FALLBACK_ACTIVATED;
            safe_decision.final_value = 0.0f; // zero-position advisory
            safe_decision.fallback_used = true;
            safe_decision.setReasoning("Hardware fault detected - safe fallback/zero-position advisory");
            return safe_decision;
        }

        // Enforce kill switch: can only reduce/neutralize advisory
        if (kill_switch_engaged_.load(std::memory_order_acquire)) {
            AILLE::Decision safe_decision = input_decision;
            safe_decision.status = AILLE::FALLBACK_ACTIVATED;
            safe_decision.final_value = 0.0f; // reduce risk to zero
            safe_decision.fallback_used = true;
            safe_decision.setReasoning("Kill switch engaged - reduced risk advisory");
            return safe_decision;
        }

        // Ensure safety layer has final veto
        if (input_decision.status == AILLE::REJECTED_LOW_CONFIDENCE ||
            input_decision.status == AILLE::REJECTED_NO_CONSENSUS ||
            input_decision.status == AILLE::ERROR_NO_MODELS) {

            AILLE::Decision veto_decision = input_decision;
            veto_decision.final_value = 0.0f;
            veto_decision.fallback_used = true;
            veto_decision.setReasoning(std::string("Safety layer veto: ") + input_decision.getReasoningString());
            return veto_decision;
        }

        return input_decision;
    }

private:
    void exportLoop() {
        while (running_.load(std::memory_order_acquire)) {
            // Take snapshot and push to health stream
            MetricsSnapshot snap = getAggregatedSnapshot();
            health_stream.try_push(snap);

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
};

} // namespace Observability
} // namespace AILLE

#endif // AILLE_OBSERVABILITY_HPP