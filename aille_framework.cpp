/*
 * AILLE Framework - AI-Load Integrity and Layered Evaluation
 * Production-Grade Algorithmic Trading Safety System
 *
 * License: MIT (see LICENSE)
 * Copyright (c) 2025 Don Michael Feeney Jr
 *
 * Five-Stage Decision Architecture:
 * 1. Model Layer      - Multi-source prediction generation
 * 2. Safety Layer     - Confidence-based filtering
 * 3. Consensus Layer  - Agreement validation
 * 4. Fallback Layer   - Stability guarantee
 * 5. Final Decision   - Auditable output
 */

#include <vector>
#include <deque>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <string>
#include <chrono>

namespace AILLE {

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct ModelSignal {
    float value;           // Prediction value (e.g., expected return)
    float confidence;      // 0.0–1.0 confidence score
    uint64_t timestamp_ns; // Nanosecond timestamp
    int model_id;          // Which model generated this signal

    ModelSignal()
        : value(0.0f), confidence(0.0f), timestamp_ns(0), model_id(-1) {}

    ModelSignal(float v, float c, int id)
        : value(v), confidence(c), model_id(id) {
        timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
    }
};

enum DecisionStatus {
    DECISION_VALID,           // Passed all checks
    REJECTED_LOW_CONFIDENCE,  // Failed safety layer
    REJECTED_NO_CONSENSUS,    // Failed consensus layer
    FALLBACK_ACTIVATED,       // Conservative fallback used
    ERROR_NO_MODELS           // No model inputs available
};

struct Decision {
    float final_value;        // Output decision (e.g., position size)
    DecisionStatus status;    // Decision outcome
    float confidence;         // Aggregate confidence
    int models_agreed;        // Number of agreeing models
    bool fallback_used;       // Whether fallback was applied
    uint64_t timestamp_ns;    // Decision timestamp

    // Audit metadata
    std::vector<int> contributing_models;
    std::string reasoning;    // Human-readable explanation

    Decision()
        : final_value(0.0f),
          status(ERROR_NO_MODELS),
          confidence(0.0f),
          models_agreed(0),
          fallback_used(false),
          timestamp_ns(0) {}
};

// ============================================================================
// CONFIGURATION
// ============================================================================

struct AILLEConfig {
    // Safety Layer
    float min_confidence_threshold;    // Default: 0.35
    float grace_confidence_threshold;  // Default: 0.25

    // Consensus Layer
    int min_models_required;           // Default: 2
    float sign_agreement_threshold;    // Default: 0.66

    // Fallback Layer
    int fallback_window_size;          // Default: 50
    float fallback_position_scale;     // Default: 0.1

    // Performance
    int max_model_count;               // Default: 10

    AILLEConfig()
        : min_confidence_threshold(0.35f),
          grace_confidence_threshold(0.25f),
          min_models_required(2),
          sign_agreement_threshold(0.66f),
          fallback_window_size(50),
          fallback_position_scale(0.1f),
          max_model_count(10) {}
};

// ============================================================================
// AILLE ENGINE - CORE LOGIC
// ============================================================================

class AILLEEngine {
private:
    AILLEConfig config;
    std::deque<float> fallback_buffer;

    // Rolling mean of validated decisions
    float calculateFallbackValue() const {
        if (fallback_buffer.empty()) return 0.0f;
        float sum = 0.0f;
        for (float v : fallback_buffer) sum += v;
        return sum / fallback_buffer.size();
    }

    void updateFallbackBuffer(float value) {
        fallback_buffer.push_back(value);
        while (fallback_buffer.size() >
               static_cast<size_t>(config.fallback_window_size)) {
            fallback_buffer.pop_front();
        }
    }

    float smoothPosition(float signal, float scale = 100.0f) const {
        return std::tanh(signal * scale);
    }

public:
    AILLEEngine() {}
    explicit AILLEEngine(const AILLEConfig& cfg) : config(cfg) {}

    // ========================================================================
    // SAFETY LAYER
    // ========================================================================

    std::vector<ModelSignal> applySafetyLayer(
        const std::vector<ModelSignal>& signals) const {

        std::vector<ModelSignal> valid;
        for (const auto& sig : signals) {
            if (sig.confidence >= config.min_confidence_threshold) {
                valid.push_back(sig);
            } else if (sig.confidence >= config.grace_confidence_threshold) {
                ModelSignal grace = sig;
                grace.confidence *= 0.8f; // degrade borderline confidence
                valid.push_back(grace);
            }
        }
        return valid;
    }

    // ========================================================================
    // CONSENSUS LAYER
    // ========================================================================

    bool checkConsensus(const std::vector<ModelSignal>& valid,
                        float& consensus_value,
                        int& models_agreed) const {

        if (valid.size() < static_cast<size_t>(config.min_models_required)) {
            models_agreed = 0;
            return false;
        }

        std::vector<float> values;
        for (const auto& s : valid) values.push_back(s.value);

        std::vector<float> sorted = values;
        std::sort(sorted.begin(), sorted.end());
        float median = sorted[sorted.size() / 2];
        float median_sign = (median >= 0) ? 1.0f : -1.0f;

        int agree = 0;
        for (float v : values) {
            if (((v >= 0) ? 1.0f : -1.0f) == median_sign) agree++;
        }

        models_agreed = agree;
        float ratio = static_cast<float>(agree) / values.size();

        if (ratio >= config.sign_agreement_threshold &&
            agree >= config.min_models_required) {

            float sum = 0.0f;
            int count = 0;
            for (const auto& s : valid) {
                if (((s.value >= 0) ? 1.0f : -1.0f) == median_sign) {
                    sum += s.value;
                    count++;
                }
            }
            consensus_value = sum / count;
            return true;
        }

        return false;
    }

    // ========================================================================
    // FALLBACK LAYER
    // ========================================================================

    float getFallbackValue() const {
        if (fallback_buffer.empty()) {
            return 0.0f;
        }
        float fb = calculateFallbackValue();
        // Direction-only fallback (intentionally conservative)
        float sign = (fb >= 0) ? 1.0f : -1.0f;
        return sign * config.fallback_position_scale;
    }

    // ========================================================================
    // FINAL DECISION
    // ========================================================================

    Decision makeDecision(const std::vector<ModelSignal>& model_signals) {
        Decision decision;
        decision.timestamp_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count();

        if (model_signals.empty()) {
            decision.reasoning = "No model inputs available";
            return decision;
        }

        if (config.max_model_count <= 0) {
            decision.reasoning = "Invalid max model count configuration";
            return decision;
        }

        std::vector<ModelSignal> scoped_signals;
        size_t max_models = static_cast<size_t>(config.max_model_count);
        if (model_signals.size() > max_models) {
            scoped_signals.assign(model_signals.begin(),
                                  model_signals.begin() + max_models);
        } else {
            scoped_signals = model_signals;
        }

        auto valid = applySafetyLayer(scoped_signals);

        if (valid.empty()) {
            decision.status = REJECTED_LOW_CONFIDENCE;
            decision.final_value = getFallbackValue();
            decision.confidence = 0.1f;
            decision.fallback_used = true;
            decision.reasoning =
                "All models failed confidence threshold – fallback activated";
            return decision;
        }

        float consensus_value;
        int agreed;
        if (!checkConsensus(valid, consensus_value, agreed)) {
            decision.status = REJECTED_NO_CONSENSUS;
            decision.final_value = getFallbackValue();
            decision.confidence = 0.2f;
            decision.fallback_used = true;
            decision.models_agreed = agreed;
            decision.reasoning =
                "Models failed to reach consensus – fallback activated";
            return decision;
        }

        decision.status = DECISION_VALID;
        decision.final_value = smoothPosition(consensus_value);

        float total_conf = 0.0f;
        for (const auto& s : valid) {
            total_conf += s.confidence;
            decision.contributing_models.push_back(s.model_id);
        }

        decision.confidence = total_conf / valid.size();
        decision.models_agreed = agreed;
        decision.reasoning =
            "Consensus achieved with " + std::to_string(agreed) + " models";

        updateFallbackBuffer(decision.final_value);
        return decision;
    }

    void reset() { fallback_buffer.clear(); }
    AILLEConfig getConfig() const { return config; }
    void setConfig(const AILLEConfig& cfg) { config = cfg; }
};

} // namespace AILLE

/*
 * LICENSE:
 * Released under the MIT License (see LICENSE).
 * Support / contact: dfeen87@gmail.com
 */
