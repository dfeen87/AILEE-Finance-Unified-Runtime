/*
 * AILLE Framework - Single Header Implementation
 * AI-Load Integrity and Layered Evaluation
 * 
 * PLUG AND PLAY - Just #include this file
 * 
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#ifndef AILLE_HPP
#define AILLE_HPP

#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <mutex>
#include <cstring>
#include <thread>

// For __builtin_prefetch fallback
#ifndef __has_builtin
  #define __has_builtin(x) 0
#endif

#define AILLE_MAX_MODELS 64
#define AILLE_MAX_FALLBACK_WINDOW 256

namespace AILLE {

// ============================================================================
// VERSION
// ============================================================================

constexpr const char* AILLE_VERSION = "3.3.0";
constexpr int AILLE_VERSION_MAJOR = 3;
constexpr int AILLE_VERSION_MINOR = 3;
constexpr int AILLE_VERSION_PATCH = 0;

// ============================================================================
// CORE DATA STRUCTURES AND CONTRACTS
// ============================================================================

struct BTCState;
struct BTCAdvisory;

struct alignas(64) SafetyState {
    bool hardware_fault;
    bool kill_switch;
    std::uint8_t _reserved_padding[62];

    SafetyState() : hardware_fault(false), kill_switch(false) {
        std::memset(_reserved_padding, 0, sizeof(_reserved_padding));
    }
};
static_assert(sizeof(SafetyState) == 64, "SafetyState must be exactly 64 bytes");

struct FPGASafetyContract {
    bool advisory_only;
    const bool execution_capability;
    std::uint8_t _padding[6];

    FPGASafetyContract() : advisory_only(true), execution_capability(false) {
        std::memset(_padding, 0, sizeof(_padding));
    }
};
static_assert(sizeof(FPGASafetyContract) == 8, "FPGASafetyContract must be exactly 8 bytes");

struct ModelSignal {
    float value;           // Prediction value
    float confidence;      // 0.0-1.0
    uint64_t timestamp_ns;
    int model_id;
    std::uint8_t _reserved_padding[12];
    
    ModelSignal() : value(0.0f), confidence(0.0f), timestamp_ns(0), model_id(-1) {
        std::memset(_reserved_padding, 0, sizeof(_reserved_padding));
    }
    
    ModelSignal(float v, float c, int id = 0) 
        : value(v), confidence(c), model_id(id) {
        timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
        std::memset(_reserved_padding, 0, sizeof(_reserved_padding));
    }
};
static_assert(sizeof(ModelSignal) == 32, "ModelSignal must be exactly 32 bytes");

enum DecisionStatus {
    DECISION_VALID,
    REJECTED_LOW_CONFIDENCE,
    REJECTED_NO_CONSENSUS,
    FALLBACK_ACTIVATED,
    ERROR_NO_MODELS
};

struct Decision {
    float final_value;
    DecisionStatus status;
    float confidence;
    int models_agreed;
    bool fallback_used;
    std::uint8_t _pad1[3];
    uint64_t timestamp_ns;

    int contributing_models[AILLE_MAX_MODELS];
    size_t num_contributing_models;
    char reasoning[128];
    
    Decision() : final_value(0.0f), status(ERROR_NO_MODELS), confidence(0.0f),
                 models_agreed(0), fallback_used(false), timestamp_ns(0),
                 num_contributing_models(0) {
        std::memset(_pad1, 0, sizeof(_pad1));
        std::memset(reasoning, 0, sizeof(reasoning));
        std::memset(contributing_models, 0, sizeof(contributing_models));
    }

    void setReasoning(const char* str) {
        size_t len = 0;
        while (str[len] != '\0' && len < sizeof(reasoning) - 1) {
            reasoning[len] = str[len];
            len++;
        }
        reasoning[len] = '\0';
    }

    const char* getReasoningString() const { return reasoning; }
};

struct AuditRecord {
    uint64_t timestamp_ns;        // 8
    uint64_t decision_id;         // 8
    DecisionStatus status;        // 4
    float final_value;            // 4
    float confidence;             // 4
    int models_agreed;            // 4
    bool fallback_used;           // 1
    std::uint8_t _pad1[3];        // 3 -> 36 bytes

    char reasoning[64];           // 64 -> 100 bytes
    int contributing_models[10];  // 40 -> 140 bytes
    char symbol[12];              // 12 -> 152 bytes
    char strategy_id[16];         // 16 -> 168 bytes
    char user_id[16];             // 16 -> 184 bytes

    std::uint8_t prev_hash[32];   // 32 -> 216 bytes
    std::uint8_t _reserved_padding[8]; // 8 -> 224 bytes
    std::uint8_t hash[32];        // 32 -> 256 bytes

    AuditRecord() : timestamp_ns(0), decision_id(0), status(DECISION_VALID),
                   final_value(0.0f), confidence(0.0f), models_agreed(0),
                   fallback_used(false) {
        std::memset(_pad1, 0, sizeof(_pad1));
        std::memset(reasoning, 0, sizeof(reasoning));
        std::memset(contributing_models, 0, sizeof(contributing_models));
        std::memset(symbol, 0, sizeof(symbol));
        std::memset(strategy_id, 0, sizeof(strategy_id));
        std::memset(user_id, 0, sizeof(user_id));
        std::memset(hash, 0, sizeof(hash));
        std::memset(prev_hash, 0, sizeof(prev_hash));
        std::memset(_reserved_padding, 0, sizeof(_reserved_padding));
    }
};
static_assert(sizeof(AuditRecord) == 256, "AuditRecord must be exactly 256 bytes");

struct alignas(64) SignalSoA {
    float values[AILLE_MAX_MODELS];
    float confidences[AILLE_MAX_MODELS];
    uint64_t timestamps_ns[AILLE_MAX_MODELS];
    int model_ids[AILLE_MAX_MODELS];
    size_t count;

    SignalSoA() : count(0) {}
};

struct AILLEConfig {
    float min_confidence_threshold;
    float grace_confidence_threshold;
    int min_models_required;
    float sign_agreement_threshold;
    int fallback_window_size;
    float fallback_position_scale;
    int max_model_count;
    uint64_t max_signal_age_ns;
    float max_position_abs;
    
    AILLEConfig() :
        min_confidence_threshold(0.35f),
        grace_confidence_threshold(0.25f),
        min_models_required(2),
        sign_agreement_threshold(0.66f),
        fallback_window_size(50),
        fallback_position_scale(0.1f),
        max_model_count(10),
        max_signal_age_ns(1000000000ULL),
        max_position_abs(1.0f) {}
};

// ============================================================================
// PERFORMANCE LAYER DATA STRUCTURES (OPTIONAL)
// ============================================================================

enum class IPCTransport {
    SharedMemoryRing,
    MemoryMappedQueue,
    KernelBypassDescriptor,
    InProcessFallback
};

enum class HardwareTarget {
    PortableCPU,
    SIMDCPU,
    FPGA,
    ASIC
};

struct IPCChannelConfig {
    IPCTransport transport;
    uint32_t ring_slots;
    uint32_t cache_line_bytes;
    bool single_writer_single_reader;
    bool advisory_only;

    IPCChannelConfig()
        : transport(IPCTransport::SharedMemoryRing), ring_slots(1024),
          cache_line_bytes(64), single_writer_single_reader(true),
          advisory_only(true) {}
};

struct IPCSignalEnvelope {
    uint64_t sequence;
    uint64_t published_timestamp_ns;
    ModelSignal signal;
    bool advisory_only;

    IPCSignalEnvelope() : sequence(0), published_timestamp_ns(0), advisory_only(true) {}
};

struct SIMDConsensusResult {
    float weighted_sum;
    float total_weight;
    int positive_votes;
    int negative_votes;
    int valid_lanes;
    bool advisory_only;

    SIMDConsensusResult()
        : weighted_sum(0.0f), total_weight(0.0f), positive_votes(0),
          negative_votes(0), valid_lanes(0), advisory_only(true) {}
};

struct HardwareKernelManifest {
    HardwareTarget target;
    char kernel_name[64];
    char execution_model[128];
    bool supports_fixed_point;
    bool supports_streaming_ipc;
    bool emits_orders;
    bool advisory_only;

    HardwareKernelManifest()
        : target(HardwareTarget::PortableCPU), supports_fixed_point(false),
          supports_streaming_ipc(false), emits_orders(false), advisory_only(true) {
        std::memset(kernel_name, 0, sizeof(kernel_name));
        std::memset(execution_model, 0, sizeof(execution_model));
        std::strncpy(kernel_name, "aille_advisory_kernel", sizeof(kernel_name) - 1);
        std::strncpy(execution_model, "passive risk scoring", sizeof(execution_model) - 1);
    }
};

struct PerformanceLayerConfig {
    bool enable_ipc_descriptors;
    bool enable_simd_consensus;
    bool enable_hardware_manifests;
    IPCChannelConfig ipc;
    HardwareTarget preferred_target;
    bool advisory_only;

    PerformanceLayerConfig()
        : enable_ipc_descriptors(true), enable_simd_consensus(true),
          enable_hardware_manifests(true), preferred_target(HardwareTarget::SIMDCPU),
          advisory_only(true) {}
};

class PerformanceLayer {
private:
    PerformanceLayerConfig config_;

    static uint64_t nowNs() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }

public:
    PerformanceLayer() = default;
    explicit PerformanceLayer(const PerformanceLayerConfig& cfg) : config_(cfg) {
        config_.advisory_only = true;
        config_.ipc.advisory_only = true;
    }

    [[nodiscard]] IPCSignalEnvelope publishAdvisorySignal(const ModelSignal& signal,
                                                          uint64_t sequence) const {
        IPCSignalEnvelope envelope;
        envelope.sequence = sequence;
        envelope.published_timestamp_ns = nowNs();
        envelope.signal = signal;
        envelope.advisory_only = true;
        return envelope;
    }


    [[nodiscard]] SIMDConsensusResult evaluateConsensusVector(
        const ModelSignal* signals, size_t count,
        float min_confidence) const {
        SIMDConsensusResult result;
        for (size_t i = 0; i < count; ++i) {
            const auto& sig = signals[i];
            if (sig.confidence < min_confidence || std::isnan(sig.value) ||
                std::isinf(sig.value) || std::isnan(sig.confidence) ||
                std::isinf(sig.confidence)) {
                continue;
            }
            result.valid_lanes++;
            if (sig.value >= 0.0f) result.positive_votes++;
            else result.negative_votes++;
            result.weighted_sum += sig.value * sig.confidence;
            result.total_weight += sig.confidence;
        }
        result.advisory_only = true;
        return result;
    }

    [[nodiscard]] HardwareKernelManifest describeHardwareTarget(HardwareTarget target) const {
        HardwareKernelManifest manifest;
        manifest.target = target;
        manifest.advisory_only = true;
        manifest.emits_orders = false;
        switch (target) {
            case HardwareTarget::SIMDCPU:
                std::strncpy(manifest.kernel_name, "aille_simd_consensus_advisory", sizeof(manifest.kernel_name) - 1);
                std::strncpy(manifest.execution_model, "vectorized CPU lanes for passive consensus scoring", sizeof(manifest.execution_model) - 1);
                break;
            case HardwareTarget::FPGA:
                std::strncpy(manifest.kernel_name, "aille_fpga_streaming_risk_advisory", sizeof(manifest.kernel_name) - 1);
                std::strncpy(manifest.execution_model, "streaming fixed-latency fabric for mitigation and risk scores", sizeof(manifest.execution_model) - 1);
                manifest.supports_fixed_point = true;
                manifest.supports_streaming_ipc = true;
                break;
            case HardwareTarget::ASIC:
                std::strncpy(manifest.kernel_name, "aille_asic_mitigation_advisory", sizeof(manifest.kernel_name) - 1);
                std::strncpy(manifest.execution_model, "synthesizable combinational/sequential advisory scoring pipeline", sizeof(manifest.execution_model) - 1);
                manifest.supports_fixed_point = true;
                manifest.supports_streaming_ipc = true;
                break;
            case HardwareTarget::PortableCPU:
            default:
                std::strncpy(manifest.kernel_name, "aille_portable_cpu_advisory", sizeof(manifest.kernel_name) - 1);
                std::strncpy(manifest.execution_model, "portable scalar passive risk scoring", sizeof(manifest.execution_model) - 1);
                break;
        }
        return manifest;
    }

    [[nodiscard]] bool isAdvisoryOnly() const noexcept { return true; }
    [[nodiscard]] PerformanceLayerConfig getConfig() const noexcept { return config_; }
};

// ============================================================================
// AILLE ENGINE
// ============================================================================

class AILLEEngine {
private:
    static constexpr float MAX_SIGNAL_VALUE = 1e6f;

    AILLEConfig config;
    mutable std::mutex engine_mtx_;
    
    alignas(64) float fallback_buffer[AILLE_MAX_FALLBACK_WINDOW]{};
    size_t fallback_head_;
    size_t fallback_count_;

    [[nodiscard]] float calculateFallbackValue() const {
        if (fallback_count_ == 0) return 0.0f;
        float sum = 0.0f;
        for (size_t i = 0; i < fallback_count_; ++i) {
            sum += fallback_buffer[i];
        }
        return sum / static_cast<float>(fallback_count_);
    }
    
    void updateFallbackBuffer(float value) {
        size_t window = static_cast<size_t>(std::max(1, config.fallback_window_size));
        if (window > AILLE_MAX_FALLBACK_WINDOW) {
            window = AILLE_MAX_FALLBACK_WINDOW;
        }

        if (fallback_count_ > window) {
            fallback_count_ = window;
        }

        fallback_buffer[fallback_head_] = value;
        fallback_head_ = (fallback_head_ + 1) % window;
        if (fallback_count_ < window) {
            fallback_count_++;
        }
    }
    
    float smoothPosition(float signal, float scale = 100.0f) const {
        float bounded = std::tanh(signal * scale);
        return std::clamp(bounded, -config.max_position_abs, config.max_position_abs);
    }
    
    void applySafetyLayerFast(const ModelSignal* signals, size_t count, SignalSoA& valid) const {
        for (size_t i = 0; i < count; ++i) {
            const auto& sig = signals[i];
            if (sig.confidence >= config.min_confidence_threshold) {
                valid.values[valid.count] = sig.value;
                valid.confidences[valid.count] = sig.confidence;
                valid.timestamps_ns[valid.count] = sig.timestamp_ns;
                valid.model_ids[valid.count] = sig.model_id;
                valid.count++;
            } else if (sig.confidence >= config.grace_confidence_threshold) {
                valid.values[valid.count] = sig.value;
                valid.confidences[valid.count] = sig.confidence * 0.8f;
                valid.timestamps_ns[valid.count] = sig.timestamp_ns;
                valid.model_ids[valid.count] = sig.model_id;
                valid.count++;
            }
        }
    }
    
    bool checkConsensusFast(const SignalSoA& valid_signals,
                       float& consensus_value, int& models_agreed) const {
        if (valid_signals.count < static_cast<size_t>(config.min_models_required)) {
            models_agreed = 0;
            return false;
        }

        float values[AILLE_MAX_MODELS];
        for (size_t i = 0; i < valid_signals.count; ++i) {
            values[i] = valid_signals.values[i];
        }

        const size_t median_index = valid_signals.count / 2;
        std::nth_element(values, values + median_index, values + valid_signals.count);
        float median = values[median_index];
        float median_sign = (median >= 0) ? 1.0f : -1.0f;

        int agreement_count = 0;
        for (size_t i = 0; i < valid_signals.count; ++i) {
            if (((valid_signals.values[i] >= 0) ? 1.0f : -1.0f) == median_sign) agreement_count++;
        }

        models_agreed = agreement_count;
        float agreement_ratio = static_cast<float>(agreement_count) / valid_signals.count;

        if (agreement_ratio >= config.sign_agreement_threshold &&
            agreement_count >= config.min_models_required) {
            float weighted_sum = 0.0f;
            float total_weight = 0.0f;
            for (size_t i = 0; i < valid_signals.count; ++i) {
                if (((valid_signals.values[i] >= 0) ? 1.0f : -1.0f) == median_sign) {
                    weighted_sum += valid_signals.values[i] * valid_signals.confidences[i];
                    total_weight += valid_signals.confidences[i];
                }
            }
            if (total_weight > 0.0f) {
                consensus_value = weighted_sum / total_weight;
                return true;
            }
        }
        return false;
    }

    bool validateConfig(const AILLEConfig& cfg, Decision& decision) const {
        if (cfg.min_confidence_threshold <= 0.0f || cfg.min_confidence_threshold > 1.0f) {
            decision.status = ERROR_NO_MODELS;
            decision.setReasoning("Invalid config: min_confidence_threshold must be in (0, 1]");
            return false;
        }
        if (cfg.grace_confidence_threshold < 0.0f || cfg.grace_confidence_threshold > cfg.min_confidence_threshold) {
            decision.status = ERROR_NO_MODELS;
            decision.setReasoning("Invalid config: grace_confidence_threshold must be in [0, min_confidence_threshold]");
            return false;
        }
        if (cfg.min_models_required < 1) {
            decision.status = ERROR_NO_MODELS;
            decision.setReasoning("Invalid config: min_models_required must be >= 1");
            return false;
        }
        if (cfg.fallback_window_size < 1) {
            decision.status = ERROR_NO_MODELS;
            decision.setReasoning("Invalid config: fallback_window_size must be >= 1");
            return false;
        }
        if (cfg.sign_agreement_threshold <= 0.0f || cfg.sign_agreement_threshold > 1.0f) {
            decision.status = ERROR_NO_MODELS;
            decision.setReasoning("Invalid config: sign_agreement_threshold must be in (0, 1]");
            return false;
        }
        if (cfg.max_model_count < 1) {
            decision.status = ERROR_NO_MODELS;
            decision.setReasoning("Invalid config: max_model_count must be >= 1");
            return false;
        }
        if (cfg.max_position_abs <= 0.0f || cfg.max_position_abs > 1.0f ||
            std::isnan(cfg.max_position_abs) || std::isinf(cfg.max_position_abs)) {
            decision.status = ERROR_NO_MODELS;
            decision.setReasoning("Invalid config: max_position_abs must be in (0, 1]");
            return false;
        }
        return true;
    }

    float getFallbackValue() const {
        if (fallback_count_ == 0) return 0.0f;
        float fb = calculateFallbackValue();
        return ((fb >= 0) ? 1.0f : -1.0f) * config.fallback_position_scale;
    }
    
    SafetyState* safety_state_ = nullptr;
    BTCState* btc_state_ = nullptr;
    BTCAdvisory* btc_advisory_ = nullptr;

public:
    AILLEEngine() : fallback_head_(0), fallback_count_(0), safety_state_(nullptr), btc_state_(nullptr), btc_advisory_(nullptr) {}
    explicit AILLEEngine(const AILLEConfig& cfg) : config(cfg), fallback_head_(0), fallback_count_(0), safety_state_(nullptr), btc_state_(nullptr), btc_advisory_(nullptr) {}
    
    void setSafetyState(SafetyState* state) { safety_state_ = state; }
    void set_btc_state(BTCState* state) { btc_state_ = state; }
    void set_btc_advisory(BTCAdvisory* advisory) { btc_advisory_ = advisory; }
    void evaluate_btc_advisory();

    [[nodiscard]] Decision makeDecision(const ModelSignal* model_signals, size_t count) {
        evaluate_btc_advisory();
        std::lock_guard<std::mutex> lock(engine_mtx_);
        Decision decision;
        decision.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();

        if (safety_state_ && (safety_state_->kill_switch || safety_state_->hardware_fault)) {
            decision.status = FALLBACK_ACTIVATED;
            decision.final_value = 0.0f;
            decision.confidence = 0.0f;
            decision.fallback_used = true;
            decision.setReasoning(safety_state_->hardware_fault ? "Hardware fault detected - fallback to zero" : "Kill switch engaged - fallback to zero");
            return decision;
        }

        if (!validateConfig(config, decision)) {
            return decision;
        }
        
        const uint64_t decision_time_ns = decision.timestamp_ns;

        if (count == 0) {
            decision.status = ERROR_NO_MODELS;
            decision.setReasoning("No model inputs");
            return decision;
        }

        size_t process_count = count;
        if (process_count > static_cast<size_t>(config.max_model_count)) {
            process_count = static_cast<size_t>(config.max_model_count);
        }
        if (process_count > AILLE_MAX_MODELS) {
            process_count = AILLE_MAX_MODELS;
        }

        for (size_t i = 0; i < process_count; ++i) {
#if __has_builtin(__builtin_prefetch) || defined(__GNUC__) || defined(__clang__)
            __builtin_prefetch(&model_signals[i + 1], 0, 1);
#endif
            const auto& sig = model_signals[i];

            if (std::isnan(sig.value) || std::isinf(sig.value) ||
                std::isnan(sig.confidence) || std::isinf(sig.confidence)) {
                decision.status = REJECTED_LOW_CONFIDENCE;
                decision.setReasoning("Invalid input (NaN/Inf) detected");
                decision.final_value = getFallbackValue();
                decision.confidence = 0.0f;
                decision.fallback_used = true;
                return decision;
            }
            if (sig.confidence < 0.0f || sig.confidence > 1.0f) {
                decision.status = REJECTED_LOW_CONFIDENCE;
                decision.setReasoning("Signal rejected: confidence out of range [0,1]");
                decision.final_value = getFallbackValue();
                decision.confidence = 0.0f;
                decision.fallback_used = true;
                return decision;
            }
            if (sig.timestamp_ns > decision_time_ns) {
                decision.status = REJECTED_LOW_CONFIDENCE;
                decision.setReasoning("Signal rejected: timestamp is in the future");
                decision.final_value = getFallbackValue();
                decision.confidence = 0.0f;
                decision.fallback_used = true;
                return decision;
            }
            if (config.max_signal_age_ns > 0 &&
                decision_time_ns - sig.timestamp_ns > config.max_signal_age_ns) {
                decision.status = REJECTED_LOW_CONFIDENCE;
                decision.setReasoning("Signal rejected: stale timestamp");
                decision.final_value = getFallbackValue();
                decision.confidence = 0.0f;
                decision.fallback_used = true;
                return decision;
            }

            bool is_duplicate = false;
            for (size_t j = 0; j < i; ++j) {
                if (model_signals[j].model_id == sig.model_id) {
                    is_duplicate = true;
                    break;
                }
            }

            if (is_duplicate) {
                decision.status = REJECTED_NO_CONSENSUS;
                decision.setReasoning("Signal rejected: duplicate model_id");
                decision.final_value = getFallbackValue();
                decision.confidence = 0.0f;
                decision.fallback_used = true;
                return decision;
            }
            if (sig.value < -MAX_SIGNAL_VALUE || sig.value > MAX_SIGNAL_VALUE) {
                decision.status = REJECTED_LOW_CONFIDENCE;
                decision.setReasoning("Signal rejected: value out of reasonable bounds");
                decision.final_value = getFallbackValue();
                decision.confidence = 0.0f;
                decision.fallback_used = true;
                return decision;
            }
        }

        SignalSoA valid;
        applySafetyLayerFast(model_signals, process_count, valid);
        
        if (valid.count == 0) {
            decision.status = REJECTED_LOW_CONFIDENCE;
            decision.final_value = getFallbackValue();
            decision.confidence = 0.1f;
            decision.fallback_used = true;
            decision.setReasoning("All models failed confidence - fallback");
            return decision;
        }
        
        float consensus_value;
        int models_agreed;
        bool consensus_ok = checkConsensusFast(valid, consensus_value, models_agreed);
        
        if (!consensus_ok) {
            decision.status = REJECTED_NO_CONSENSUS;
            decision.final_value = getFallbackValue();
            decision.confidence = 0.2f;
            decision.fallback_used = true;
            decision.models_agreed = models_agreed;
            decision.setReasoning("No consensus - fallback");
            return decision;
        }
        
        decision.status = DECISION_VALID;
        decision.final_value = smoothPosition(consensus_value);
        
        float total_conf = 0.0f;
        for (size_t i = 0; i < valid.count; ++i) {
            total_conf += valid.confidences[i];
            decision.contributing_models[decision.num_contributing_models++] = valid.model_ids[i];
        }
        decision.confidence = (valid.count == 0) ? 0.0f : (total_conf / valid.count);
        decision.models_agreed = models_agreed;
        decision.fallback_used = false;

        char reasoning_buf[128];
        snprintf(reasoning_buf, sizeof(reasoning_buf), "Consensus: %d models", models_agreed);
        decision.setReasoning(reasoning_buf);
        
        updateFallbackBuffer(decision.final_value);
        return decision;
    }
    
    void reset() noexcept {
        std::lock_guard<std::mutex> lock(engine_mtx_);
        fallback_head_ = 0;
        fallback_count_ = 0;
    }
    AILLEConfig getConfig() const noexcept { return config; }
    void setConfig(const AILLEConfig& cfg) {
        std::lock_guard<std::mutex> lock(engine_mtx_);
        config = cfg;
    }
};

// ============================================================================
// AUDIT LOGGER
// ============================================================================

class AuditLogger {
private:
    std::ofstream log_file;
    uint64_t next_decision_id;
    std::uint8_t last_hash[32];
    mutable std::mutex mutex_;
    bool flush_on_write_;

    static uint32_t rotateRight(uint32_t value, uint32_t bits) {
        return (value >> bits) | (value << (32 - bits));
    }

    void computeHashRaw(const AuditRecord& record, std::uint8_t* out_hash) const {
        static constexpr std::array<uint32_t, 64> k = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
            0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
            0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
            0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
            0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
            0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
            0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };

        std::array<uint32_t, 8> h = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
        };

        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&record);
        size_t len = 224; // offset up to hash array // Exact offset up to user_id

        uint8_t data[512];
        std::memcpy(data, ptr, len);

        uint64_t bit_len = static_cast<uint64_t>(len) * 8;
        data[len] = 0x80;
        size_t padded_len = len + 1;

        while ((padded_len % 64) != 56) {
            data[padded_len++] = 0;
        }

        for (int i = 7; i >= 0; --i) {
            data[padded_len++] = static_cast<uint8_t>((bit_len >> (i * 8)) & 0xff);
        }

        for (size_t chunk = 0; chunk < padded_len; chunk += 64) {
            std::array<uint32_t, 64> w{};
            for (size_t i = 0; i < 16; ++i) {
                size_t idx = chunk + i * 4;
                w[i] = (static_cast<uint32_t>(data[idx]) << 24)
                    | (static_cast<uint32_t>(data[idx + 1]) << 16)
                    | (static_cast<uint32_t>(data[idx + 2]) << 8)
                    | static_cast<uint32_t>(data[idx + 3]);
            }
            for (size_t i = 16; i < 64; ++i) {
                uint32_t s0 = rotateRight(w[i - 15], 7) ^
                              rotateRight(w[i - 15], 18) ^
                              (w[i - 15] >> 3);
                uint32_t s1 = rotateRight(w[i - 2], 17) ^
                              rotateRight(w[i - 2], 19) ^
                              (w[i - 2] >> 10);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }

            uint32_t a = h[0];
            uint32_t b = h[1];
            uint32_t c = h[2];
            uint32_t d = h[3];
            uint32_t e = h[4];
            uint32_t f = h[5];
            uint32_t g = h[6];
            uint32_t hh = h[7];

            for (size_t i = 0; i < 64; ++i) {
                uint32_t s1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
                uint32_t ch = (e & f) ^ (~e & g);
                uint32_t temp1 = hh + s1 + ch + k[i] + w[i];
                uint32_t s0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
                uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                uint32_t temp2 = s0 + maj;

                hh = g;
                g = f;
                f = e;
                e = d + temp1;
                d = c;
                c = b;
                b = a;
                a = temp1 + temp2;
            }

            h[0] += a;
            h[1] += b;
            h[2] += c;
            h[3] += d;
            h[4] += e;
            h[5] += f;
            h[6] += g;
            h[7] += hh;
        }

        for (size_t i = 0; i < 8; ++i) {
            out_hash[i*4]   = (h[i] >> 24) & 0xff;
            out_hash[i*4+1] = (h[i] >> 16) & 0xff;
            out_hash[i*4+2] = (h[i] >> 8) & 0xff;
            out_hash[i*4+3] = h[i] & 0xff;
        }
    }

    void writeTimestamp(uint64_t ns) {
        time_t seconds = ns / 1000000000ULL;
        struct tm timeinfo;
#if defined(_WIN32)
        gmtime_s(&timeinfo, &seconds);
#else
        gmtime_r(&seconds, &timeinfo);
#endif
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        log_file << buffer;
    }

    void writeHex(const std::uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            log_file << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]);
        }
        log_file << std::dec;
    }
    
    const char* statusToString(DecisionStatus s) const {
        switch (s) {
            case DECISION_VALID: return "VALID";
            case REJECTED_LOW_CONFIDENCE: return "REJECTED_CONF";
            case REJECTED_NO_CONSENSUS: return "REJECTED_CONSENSUS";
            case FALLBACK_ACTIVATED: return "FALLBACK";
            default: return "ERROR";
        }
    }
    
public:
    AuditLogger(bool flush = true) : next_decision_id(1), flush_on_write_(flush) {
        std::memset(last_hash, 0, sizeof(last_hash));
    }
    
    explicit AuditLogger(const char* filename, bool flush = true)
        : next_decision_id(1), flush_on_write_(flush) {
        std::memset(last_hash, 0, sizeof(last_hash));
        open(filename);
    }

    AuditLogger(const AuditLogger&) = delete;
    AuditLogger& operator=(const AuditLogger&) = delete;
    
    ~AuditLogger() { close(); }
    
    bool open(const char* filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        log_file.open(filename, std::ios::app);
        if (!log_file.is_open()) return false;
        if (log_file.tellp() == 0) {
            log_file << "timestamp,id,status,value,conf,models,fallback,"
                    << "reasoning,contributing_models,symbol,strategy,hash,prev_hash\n";
        }
        return true;
    }
    
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (log_file.is_open()) log_file.close();
    }
    
    void logDecision(const Decision& d, const char* symbol = "", const char* strategy = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        AuditRecord rec;
        rec.timestamp_ns = d.timestamp_ns;
        rec.decision_id = next_decision_id++;
        rec.status = d.status;
        rec.final_value = d.final_value;
        rec.confidence = d.confidence;
        rec.models_agreed = d.models_agreed;
        rec.fallback_used = d.fallback_used;

        std::strncpy(rec.reasoning, d.getReasoningString(), sizeof(rec.reasoning) - 1);

        for (size_t i = 0; i < d.num_contributing_models && i < 10; ++i) {
            rec.contributing_models[i] = d.contributing_models[i];
        }
        
        if (symbol) std::strncpy(rec.symbol, symbol, sizeof(rec.symbol) - 1);
        if (strategy) std::strncpy(rec.strategy_id, strategy, sizeof(rec.strategy_id) - 1);

        std::memcpy(rec.prev_hash, last_hash, sizeof(last_hash));
        computeHashRaw(rec, rec.hash);
        
        if (log_file.is_open()) {
            writeTimestamp(rec.timestamp_ns);
            log_file << "," << rec.decision_id << ","
                    << statusToString(rec.status) << "," << rec.final_value << ","
                    << rec.confidence << "," << rec.models_agreed << ","
                    << (rec.fallback_used ? "true" : "false") << ","
                    << "\"" << rec.reasoning << "\","
                    << "\"[";
            for (size_t i = 0; i < d.num_contributing_models && i < 10; ++i) {
                if (i > 0) log_file << ",";
                log_file << rec.contributing_models[i];
            }
            log_file << "]\",\"" << rec.symbol << "\",\""
                    << rec.strategy_id << "\",";
            writeHex(rec.hash, 32);
            log_file << ",";
            writeHex(rec.prev_hash, 32);
            log_file << "\n";

            if (flush_on_write_) log_file.flush();
        }

        std::memcpy(last_hash, rec.hash, sizeof(last_hash));
    }
};

} // namespace AILLE

#endif // AILLE_HPP
