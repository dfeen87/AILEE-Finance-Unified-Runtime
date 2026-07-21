/*
 * AILLE Framework - Single Header Implementation
 * AI-Load Integrity and Layered Evaluation
 * * PLUG AND PLAY - Just #include this file
 * * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 *
 * Implements deterministic, allocator-free core structures
 * Compatible with SEC, EU AI Act, and MiFID II requirements
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
#include <vector>

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

constexpr const char* AILLE_VERSION = "3.3.1";
constexpr int AILLE_VERSION_MAJOR = 3;
constexpr int AILLE_VERSION_MINOR = 3;
constexpr int AILLE_VERSION_PATCH = 1;

// ============================================================================
// ADVISORY FORWARD STRUCTS
// ============================================================================

struct BTCState;
struct BTCAdvisory;
struct ETHState;
struct ETHAdvisory;
struct OILState;
struct OILAdvisory;
struct GOLDState;
struct GOLDAdvisory;
struct SILVERState;
struct SILVERAdvisory;
struct COPPERState;
struct COPPERAdvisory;
struct NATGASState;
struct NATGASAdvisory;
struct PLATINUMState;
struct PLATINUMAdvisory;
struct ForexUSDState;
struct ForexUSDAdvisory;
struct MacroSignalState;
struct MacroSignalAdvisory;

struct alignas(64) MarketStabilizerState final {
    float systemic_volatility;
    float bid_ask_spread_deviation;
    float order_book_depth_deficit;
    float consecutive_crash_count;
    float regime_stress_factor;
    float arbitrage_discrepancy;
    float historical_stabilizer_weight;
    std::uint8_t _padding[36]; // 64 - 7*4 = 36 bytes padding

    constexpr MarketStabilizerState()
        : systemic_volatility(0.0f), bid_ask_spread_deviation(0.0f),
          order_book_depth_deficit(0.0f), consecutive_crash_count(0.0f),
          regime_stress_factor(0.0f), arbitrage_discrepancy(0.0f),
          historical_stabilizer_weight(0.5f), _padding{} {}
};
static_assert(sizeof(MarketStabilizerState) == 64, "MarketStabilizerState must be exactly 64 bytes");

struct alignas(64) MarketStabilizerAdvisory final {
    float stabilization_risk_score;  // 4
    float stabilization_factor;      // 4
    float dynamic_clamp_limit;       // 4
    std::uint8_t risk_elevated;      // 1 (0 or 1)
    std::uint8_t governor_active;     // 1 (0 or 1)
    std::uint8_t spread_guard_active; // 1 (0 or 1)
    std::uint8_t _reserved0;          // 1

    std::uint8_t _padding[48];        // 48 bytes padding -> 64 bytes total

    constexpr MarketStabilizerAdvisory()
        : stabilization_risk_score(0.0f), stabilization_factor(1.0f),
          dynamic_clamp_limit(1.0f), risk_elevated(0), governor_active(0),
          spread_guard_active(0), _reserved0(0), _padding{} {}
};
static_assert(sizeof(MarketStabilizerAdvisory) == 64, "MarketStabilizerAdvisory must be exactly 64 bytes");

// ============================================================================
// CORE DATA STRUCTURES AND CONTRACTS
// ============================================================================

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

#pragma pack(push, 1)
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
#pragma pack(pop)
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
    
    // Dynamic rule options and configuration version tag
    bool enable_dynamic_fallback;
    float fallback_alpha;
    float fallback_beta;
    const char* config_version;

    AILLEConfig() :
        min_confidence_threshold(0.20f),     // Optimized from 0.35f
        grace_confidence_threshold(0.10f),   // Optimized from 0.25f
        min_models_required(2),
        sign_agreement_threshold(0.66f),
        fallback_window_size(50),
        fallback_position_scale(0.1f),
        max_model_count(10),
        max_signal_age_ns(1000000000ULL),
        max_position_abs(1.0f),
        enable_dynamic_fallback(false),
        fallback_alpha(0.05f),
        fallback_beta(0.05f),
        config_version("4.1.0") {}
};

// ============================================================================
// PERFORMANCE LAYER DATA STRUCTURES
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
    alignas(64) float confidence_buffer[AILLE_MAX_FALLBACK_WINDOW]{};
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
    
    void updateFallbackBuffer(float value, float confidence) {
        size_t window = static_cast<size_t>(std::max(1, config.fallback_window_size));
        if (window > AILLE_MAX_FALLBACK_WINDOW) {
            window = AILLE_MAX_FALLBACK_WINDOW;
        }

        if (fallback_count_ > window) {
            fallback_count_ = window;
        }

        fallback_buffer[fallback_head_] = value;
        confidence_buffer[fallback_head_] = confidence;
        fallback_head_ = (fallback_head_ + 1) % window;
        if (fallback_count_ < window) {
            fallback_count_++;
        }
    }
    
    float smoothPosition(float signal, float scale = 100.0f) const {
        float bounded = std::tanh(signal * scale);
        float clamp_limit = config.max_position_abs;
        if (stabilizer_advisory_ && stabilizer_advisory_->stabilization_risk_score > 75.0f) {
            clamp_limit = std::min(clamp_limit, stabilizer_advisory_->dynamic_clamp_limit);
        }
        return std::clamp(bounded, -clamp_limit, clamp_limit);
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
            decision.num_contributing_models = 0;
            decision.setReasoning("Invalid config: min_confidence_threshold must be in (0, 1]");
            return false;
        }
        if (cfg.grace_confidence_threshold < 0.0f || cfg.grace_confidence_threshold > cfg.min_confidence_threshold) {
            decision.status = ERROR_NO_MODELS;
            decision.num_contributing_models = 0;
            decision.setReasoning("Invalid config: grace_confidence_threshold must be in [0, min_confidence_threshold]");
            return false;
        }
        if (cfg.min_models_required < 1) {
            decision.status = ERROR_NO_MODELS;
            decision.num_contributing_models = 0;
            decision.setReasoning("Invalid config: min_models_required must be >= 1");
            return false;
        }
        if (cfg.fallback_window_size < 1) {
            decision.status = ERROR_NO_MODELS;
            decision.num_contributing_models = 0;
            decision.setReasoning("Invalid config: fallback_window_size must be >= 1");
            return false;
        }
        if (cfg.sign_agreement_threshold <= 0.0f || cfg.sign_agreement_threshold > 1.0f) {
            decision.status = ERROR_NO_MODELS;
            decision.num_contributing_models = 0;
            decision.setReasoning("Invalid config: sign_agreement_threshold must be in (0, 1]");
            return false;
        }
        if (cfg.max_model_count < 1) {
            decision.status = ERROR_NO_MODELS;
            decision.num_contributing_models = 0;
            decision.setReasoning("Invalid config: max_model_count must be >= 1");
            return false;
        }
        if (cfg.max_position_abs <= 0.0f || cfg.max_position_abs > 1.0f ||
            std::isnan(cfg.max_position_abs) || std::isinf(cfg.max_position_abs)) {
            decision.status = ERROR_NO_MODELS;
            decision.num_contributing_models = 0;
            decision.setReasoning("Invalid config: max_position_abs must be in (0, 1]");
            return false;
        }
        return true;
    }

    float getFallbackValue() const {
        if (fallback_count_ == 0) return 0.0f;
        float fb = calculateFallbackValue();
        float sign = (fb >= 0.0f) ? 1.0f : -1.0f;

        float scale = config.fallback_position_scale;
        if (config.enable_dynamic_fallback) {
            float sum_conf = 0.0f;
            for (size_t i = 0; i < fallback_count_; ++i) {
                sum_conf += confidence_buffer[i];
            }
            float ma_conf = sum_conf / static_cast<float>(fallback_count_);
            scale = config.fallback_alpha * ma_conf + config.fallback_beta;
            if (scale < 0.1f) scale = 0.1f;
            if (scale > 0.5f) scale = 0.5f;
        }
        return sign * scale;
    }
    
    SafetyState* safety_state_ = nullptr;
    BTCState* btc_state_ = nullptr;
    BTCAdvisory* btc_advisory_ = nullptr;
    const ETHState* eth_state_ = nullptr;
    ETHAdvisory* eth_advisory_ = nullptr;
    const OILState* oil_state_ = nullptr;
    OILAdvisory* oil_advisory_ = nullptr;
    const GOLDState* gold_state_ = nullptr;
    GOLDAdvisory* gold_advisory_ = nullptr;
    const SILVERState* silver_state_ = nullptr;
    SILVERAdvisory* silver_advisory_ = nullptr;
    const COPPERState* copper_state_ = nullptr;
    COPPERAdvisory* copper_advisory_ = nullptr;
    const NATGASState* natgas_state_ = nullptr;
    NATGASAdvisory* natgas_advisory_ = nullptr;
    const PLATINUMState* platinum_state_ = nullptr;
    PLATINUMAdvisory* platinum_advisory_ = nullptr;
    const ForexUSDState* forex_usd_state_ = nullptr;
    ForexUSDAdvisory* forex_usd_advisory_ = nullptr;
    const MacroSignalState* macro_state_ = nullptr;
    MacroSignalAdvisory* macro_advisory_ = nullptr;
    const MarketStabilizerState* stabilizer_state_ = nullptr;
    MarketStabilizerAdvisory* stabilizer_advisory_ = nullptr;

public:
    AILLEEngine() : fallback_head_(0), fallback_count_(0), safety_state_(nullptr), btc_state_(nullptr), btc_advisory_(nullptr), eth_state_(nullptr), eth_advisory_(nullptr), oil_state_(nullptr), oil_advisory_(nullptr), gold_state_(nullptr), gold_advisory_(nullptr), silver_state_(nullptr), silver_advisory_(nullptr), copper_state_(nullptr), copper_advisory_(nullptr), natgas_state_(nullptr), natgas_advisory_(nullptr), platinum_state_(nullptr), platinum_advisory_(nullptr), forex_usd_state_(nullptr), forex_usd_advisory_(nullptr), macro_state_(nullptr), macro_advisory_(nullptr), stabilizer_state_(nullptr), stabilizer_advisory_(nullptr) {}
    explicit AILLEEngine(const AILLEConfig& cfg) : config(cfg), fallback_head_(0), fallback_count_(0), safety_state_(nullptr), btc_state_(nullptr), btc_advisory_(nullptr), eth_state_(nullptr), eth_advisory_(nullptr), oil_state_(nullptr), oil_advisory_(nullptr), gold_state_(nullptr), gold_advisory_(nullptr), silver_state_(nullptr), silver_advisory_(nullptr), copper_state_(nullptr), copper_advisory_(nullptr), natgas_state_(nullptr), natgas_advisory_(nullptr), platinum_state_(nullptr), platinum_advisory_(nullptr), forex_usd_state_(nullptr), forex_usd_advisory_(nullptr), macro_state_(nullptr), macro_advisory_(nullptr), stabilizer_state_(nullptr), stabilizer_advisory_(nullptr) {}
    
    void setSafetyState(SafetyState* state) { safety_state_ = state; }
    void set_btc_state(BTCState* state) { btc_state_ = state; }
    void set_btc_advisory(BTCAdvisory* advisory) { btc_advisory_ = advisory; }
    void evaluate_btc_advisory();

    void set_eth_state(const ETHState* state) { eth_state_ = state; }
    void set_eth_advisory(ETHAdvisory* advisory) { eth_advisory_ = advisory; }
    void evaluate_eth_advisory();

    void set_oil_state(const OILState* state) { oil_state_ = state; }
    void set_oil_advisory(OILAdvisory* advisory) { oil_advisory_ = advisory; }
    void evaluate_oil_advisory();

    void set_gold_state(const GOLDState* state) { gold_state_ = state; }
    void set_gold_advisory(GOLDAdvisory* advisory) { gold_advisory_ = advisory; }
    void evaluate_gold_advisory();

    void set_silver_state(const SILVERState* state) { silver_state_ = state; }
    void set_silver_advisory(SILVERAdvisory* advisory) { silver_advisory_ = advisory; }
    void evaluate_silver_advisory();

    void set_copper_state(const COPPERState* state) { copper_state_ = state; }
    void set_copper_advisory(COPPERAdvisory* advisory) { copper_advisory_ = advisory; }
    void evaluate_copper_advisory();

    void set_natgas_state(const NATGASState* state) { natgas_state_ = state; }
    void set_natgas_advisory(NATGASAdvisory* advisory) { natgas_advisory_ = advisory; }
    void evaluate_natgas_advisory();

    void set_platinum_state(const PLATINUMState* state) { platinum_state_ = state; }
    void set_platinum_advisory(PLATINUMAdvisory* advisory) { platinum_advisory_ = advisory; }
    void evaluate_platinum_advisory();

    void set_forex_usd_state(const ForexUSDState* state) { forex_usd_state_ = state; }
    void set_forex_usd_advisory(ForexUSDAdvisory* advisory) { forex_usd_advisory_ = advisory; }
    void evaluate_forex_usd_advisory();

    void set_macro_state(const MacroSignalState* s) noexcept { macro_state_ = s; }
    void set_macro_advisory(MacroSignalAdvisory* a) noexcept { macro_advisory_ = a; }
    void evaluate_macro_advisory() noexcept;

    void set_stabilizer_state(const MarketStabilizerState* s) noexcept { stabilizer_state_ = s; }
    void set_stabilizer_advisory(MarketStabilizerAdvisory* a) noexcept { stabilizer_advisory_ = a; }
    void evaluate_stabilizer_advisory() noexcept;

    [[nodiscard]] Decision makeDecision(const ModelSignal* model_signals, size_t count) {
        evaluate_btc_advisory();
        evaluate_eth_advisory();
        evaluate_oil_advisory();
        evaluate_gold_advisory();
        evaluate_silver_advisory();
        evaluate_copper_advisory();
        evaluate_natgas_advisory();
        evaluate_platinum_advisory();
        evaluate_forex_usd_advisory();
        evaluate_macro_advisory();
        evaluate_stabilizer_advisory();

        std::lock_guard<std::mutex> lock(engine_mtx_);

        Decision decision;
        decision.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
        decision.num_contributing_models = 0;

        if (safety_state_ && (safety_state_->kill_switch || safety_state_->hardware_fault)) {
            decision.status = FALLBACK_ACTIVATED;
            decision.final_value = 0.0f;
            decision.confidence = 0.0f;
            decision.fallback_used = true;
            decision.num_contributing_models = 0;
            decision.setReasoning(safety_state_->hardware_fault ? "Hardware fault detected - fallback to zero" : "Kill switch engaged - fallback to zero");
            return decision;
        }

        if (!validateConfig(config, decision)) {
            decision.num_contributing_models = 0;
            return decision;
        }
        
        const uint64_t decision_time_ns = decision.timestamp_ns;

        if (count == 0) {
            decision.status = ERROR_NO_MODELS;
            decision.num_contributing_models = 0;
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
                decision.num_contributing_models = 0;
                return decision;
            }
            if (sig.confidence < 0.0f || sig.confidence > 1.0f) {
                decision.status = REJECTED_LOW_CONFIDENCE;
                decision.setReasoning("Signal rejected: confidence out of range [0,1]");
                decision.final_value = getFallbackValue();
                decision.confidence = 0.0f;
                decision.fallback_used = true;
                decision.num_contributing_models = 0;
                return decision;
            }
            if (sig.timestamp_ns > decision_time_ns) {
                decision.status = REJECTED_LOW_CONFIDENCE;
                decision.setReasoning("Signal rejected: timestamp is in the future");
                decision.final_value = getFallbackValue();
                decision.confidence = 0.0f;
                decision.fallback_used = true;
                decision.num_contributing_models = 0;
                return decision;
            }
            if (config.max_signal_age_ns > 0 &&
                decision_time_ns - sig.timestamp_ns > config.max_signal_age_ns) {
                decision.status = REJECTED_LOW_CONFIDENCE;
                decision.setReasoning("Signal rejected: stale timestamp");
                decision.final_value = getFallbackValue();
                decision.confidence = 0.0f;
                decision.fallback_used = true;
                decision.num_contributing_models = 0;
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
                decision.num_contributing_models = 0;
                return decision;
            }
            if (sig.value < -MAX_SIGNAL_VALUE || sig.value > MAX_SIGNAL_VALUE) {
                decision.status = REJECTED_LOW_CONFIDENCE;
                decision.setReasoning("Signal rejected: value out of reasonable bounds");
                decision.final_value = getFallbackValue();
                decision.confidence = 0.0f;
                decision.fallback_used = true;
                decision.num_contributing_models = 0;
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
            decision.num_contributing_models = 0;
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
            decision.num_contributing_models = 0;
            decision.setReasoning("No consensus - fallback");
            return decision;
        }
        
        decision.status = DECISION_VALID;
        decision.final_value = smoothPosition(consensus_value);

        decision.num_contributing_models = valid.count;

        float total_conf = 0.0f;
        for (size_t i = 0; i < valid.count; ++i) {
            total_conf += valid.confidences[i];
            decision.contributing_models[i] = valid.model_ids[i];
        }
        decision.confidence = (valid.count == 0) ? 0.0f : (total_conf / valid.count);
        decision.models_agreed = models_agreed;
        decision.fallback_used = false;

        char reasoning_buf[128];
        snprintf(reasoning_buf, sizeof(reasoning_buf), "Consensus: %d models", models_agreed);
        decision.setReasoning(reasoning_buf);
        
        updateFallbackBuffer(decision.final_value, decision.confidence);
        return decision;
    }
    
    void reset() noexcept {
        std::lock_guard<std::mutex> lock(engine_mtx_);
        fallback_head_ = 0;
        fallback_count_ = 0;
        std::memset(fallback_buffer, 0, sizeof(fallback_buffer));
        std::memset(confidence_buffer, 0, sizeof(confidence_buffer));
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
    std::vector<AuditRecord> audit_trail;

    // Helper functions for internal cryptographic tracking
    uint32_t rotateRight(uint32_t value, uint32_t bits);
    std::string sha256(const std::string& input);
    std::string serializeRecord(const AuditRecord& record) const;
    std::string computeHash(const AuditRecord& record) const;
    std::string getTimestamp(uint64_t ns) const;
    std::string csvEscape(const std::string& value) const;
    std::string statusToString(DecisionStatus status) const;

public:
    AuditLogger();
    explicit AuditLogger(const std::string& log_filename);
    ~AuditLogger();

    bool open(const std::string& filename);
    void close() {
        if (log_file.is_open()) {
            log_file.close();
        }
    }

    void logDecision(const Decision& decision,
                     const std::string& symbol,
                     const std::string& strategy_id,
                     const std::string& user_id = "default_user");

    bool verifyIntegrity() const;
    void generateReport(const std::string& output_file, uint64_t start_ns, uint64_t end_ns) const;
    size_t getAuditTrailSize() const;
    const std::vector<AuditRecord>& getAuditTrail() const;
};

} // namespace AILLE

#endif // AILLE_HPP
