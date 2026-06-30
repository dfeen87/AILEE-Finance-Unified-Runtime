/*
 * AILLE Metrics Extension
 * Read-only observability layer for AILLE decisions
 *
 * License: MIT (see LICENSE)
 * Copyright (c) 2026 Don Michael Feeney Jr
 *
 * This extension adds real-time metrics and health insight
 * without modifying AILLE core logic or decision behavior.
 *
 * VERSION: 2.0 (Production-Hardened)
 */

#ifndef AILLE_METRICS_HPP
#define AILLE_METRICS_HPP

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>
#include <chrono>
#include <numeric>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cmath>

#include "aille.hpp"

namespace AILLE {

// ============================================================================
// METRICS SNAPSHOT
// ============================================================================

struct MetricsSnapshot {
    uint64_t total_decisions = 0;
    uint64_t valid_decisions = 0;
    uint64_t fallback_activations = 0;
    uint64_t rejected_confidence = 0;
    uint64_t rejected_consensus = 0;
    uint64_t invalid_inputs = 0;

    float average_confidence = 0.0f;
    float fallback_rate = 0.0f;
    float consensus_failure_rate = 0.0f;
    float min_confidence = 0.0f;
    float max_confidence = 0.0f;
    float stddev_confidence = 0.0f;

    std::unordered_map<int, uint64_t> models_agreed_histogram;

    uint64_t last_decision_timestamp_ns = 0;
    bool overflow_detected = false;
};

// ============================================================================
// METRICS COLLECTOR (PRODUCTION-HARDENED, THREAD-SAFE)
// ============================================================================

class MetricsCollector {
private:
    // Thread safety
    mutable std::mutex mtx;
    
    // Core metrics (protected by mutex)
    MetricsSnapshot snapshot;
    
    // Circular buffer for confidence samples (bounded memory)
    static constexpr size_t MAX_SAMPLES = 10000;
    std::vector<float> confidence_samples;
    size_t sample_write_index = 0;
    bool samples_buffer_full = false;
    
    // Atomic overflow detection
    std::atomic<bool> overflow_flag{false};

public:
    MetricsCollector() {
        confidence_samples.reserve(MAX_SAMPLES);
    }

    // Called externally after each decision
    void observeDecision(const Decision& d) {
        std::lock_guard<std::mutex> lock(mtx);

        // Input validation
        if (!isValidDecision(d)) {
            snapshot.invalid_inputs++;
            return;
        }

        // Overflow protection
        if (snapshot.total_decisions == UINT64_MAX) {
            overflow_flag.store(true);
            snapshot.overflow_detected = true;
            return;
        }

        snapshot.total_decisions++;
        snapshot.last_decision_timestamp_ns = d.timestamp_ns;

        // Circular buffer management (bounded memory)
        addConfidenceSample(d.confidence);

        // Status tracking
        switch (d.status) {
            case DECISION_VALID:
                snapshot.valid_decisions++;
                break;
            case REJECTED_LOW_CONFIDENCE:
                snapshot.rejected_confidence++;
                snapshot.fallback_activations++;
                break;
            case REJECTED_NO_CONSENSUS:
                snapshot.rejected_consensus++;
                snapshot.fallback_activations++;
                break;
            case FALLBACK_ACTIVATED:
                snapshot.fallback_activations++;
                break;
            default:
                // Unknown status - log but don't crash
                break;
        }

        // Histogram tracking with bounds checking
        if (d.models_agreed >= 0 && d.models_agreed < 1000) {
            snapshot.models_agreed_histogram[d.models_agreed]++;
        }

        recomputeStatistics();
    }

    // Thread-safe snapshot retrieval
    MetricsSnapshot getSnapshot() const {
        std::lock_guard<std::mutex> lock(mtx);
        return snapshot;
    }

    // Simple health check for dashboards / alerts
    bool isHealthy(float max_fallback_rate = 0.10f) const {
        std::lock_guard<std::mutex> lock(mtx);
        return snapshot.fallback_rate <= max_fallback_rate && 
               !snapshot.overflow_detected;
    }

    // Reset metrics (useful for testing or periodic resets)
    void reset() {
        std::lock_guard<std::mutex> lock(mtx);
        snapshot = MetricsSnapshot();
        confidence_samples.clear();
        sample_write_index = 0;
        samples_buffer_full = false;
        overflow_flag.store(false);
    }

    // Get current sample count (for diagnostics)
    size_t getSampleCount() const {
        std::lock_guard<std::mutex> lock(mtx);
        return samples_buffer_full ? MAX_SAMPLES : confidence_samples.size();
    }

private:
    // Input validation
    bool isValidDecision(const Decision& d) const {
        // Validate confidence range
        if (std::isnan(d.confidence) || std::isinf(d.confidence)) {
            return false;
        }
        if (d.confidence < 0.0f || d.confidence > 1.0f) {
            return false;
        }
        
        // Validate timestamp (basic sanity check)
        if (d.timestamp_ns == 0) {
            return false;
        }
        
        // Validate models_agreed is reasonable
        if (d.models_agreed < 0) {
            return false;
        }
        
        return true;
    }

    // Circular buffer management
    void addConfidenceSample(float confidence) {
        if (confidence_samples.size() < MAX_SAMPLES) {
            confidence_samples.push_back(confidence);
        } else {
            samples_buffer_full = true;
            confidence_samples[sample_write_index] = confidence;
            sample_write_index = (sample_write_index + 1) % MAX_SAMPLES;
        }
    }

    // Recompute derived statistics
    void recomputeStatistics() {
        if (snapshot.total_decisions == 0) return;

        // Rates
        snapshot.fallback_rate =
            static_cast<float>(snapshot.fallback_activations) /
            static_cast<float>(snapshot.total_decisions);

        snapshot.consensus_failure_rate =
            static_cast<float>(snapshot.rejected_consensus) /
            static_cast<float>(snapshot.total_decisions);

        // Confidence statistics
        if (confidence_samples.empty()) {
            snapshot.average_confidence = 0.0f;
            snapshot.min_confidence = 0.0f;
            snapshot.max_confidence = 0.0f;
            snapshot.stddev_confidence = 0.0f;
            return;
        }

        // Mean
        double sum = std::accumulate(
            confidence_samples.begin(),
            confidence_samples.end(),
            0.0
        );
        snapshot.average_confidence = 
            static_cast<float>(sum / confidence_samples.size());

        // Min/Max
        auto minmax = std::minmax_element(
            confidence_samples.begin(),
            confidence_samples.end()
        );
        snapshot.min_confidence = *minmax.first;
        snapshot.max_confidence = *minmax.second;

        // Standard deviation (using Welford's online algorithm concept)
        double mean = snapshot.average_confidence;
        double sq_sum = 0.0;
        for (float val : confidence_samples) {
            double diff = val - mean;
            sq_sum += diff * diff;
        }
        snapshot.stddev_confidence = 
            static_cast<float>(std::sqrt(sq_sum / confidence_samples.size()));
    }
};

// ============================================================================
// OPTIONAL HELPER: HUMAN-READABLE SUMMARY
// ============================================================================

inline std::string formatMetrics(const MetricsSnapshot& m) {
    std::string out;
    out += "AILLE Metrics Snapshot\n";
    out += "======================\n";
    out += "Total Decisions: " + std::to_string(m.total_decisions) + "\n";
    out += "Valid Decisions: " + std::to_string(m.valid_decisions) + "\n";
    out += "Invalid Inputs:  " + std::to_string(m.invalid_inputs) + "\n";
    out += "Fallback Activations: " + std::to_string(m.fallback_activations) + "\n";
    out += "\n";
    out += "Rates:\n";
    out += "  Fallback Rate: " + std::to_string(m.fallback_rate * 100.0f) + "%\n";
    out += "  Consensus Failure Rate: " +
           std::to_string(m.consensus_failure_rate * 100.0f) + "%\n";
    out += "\n";
    out += "Confidence Statistics:\n";
    out += "  Average: " + std::to_string(m.average_confidence) + "\n";
    out += "  Min:     " + std::to_string(m.min_confidence) + "\n";
    out += "  Max:     " + std::to_string(m.max_confidence) + "\n";
    out += "  StdDev:  " + std::to_string(m.stddev_confidence) + "\n";
    out += "\n";
    
    if (m.overflow_detected) {
        out += "⚠️  WARNING: Counter overflow detected!\n";
    }
    
    return out;
}

} // namespace AILLE

#endif // AILLE_METRICS_HPP
