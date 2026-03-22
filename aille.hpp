/*
 * AILLE Framework - Single Header Implementation
 * AI-Load Integrity and Layered Evaluation
 * 
 * PLUG AND PLAY - Just #include this file
 * 
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: Non-Commercial (see LICENSE)
 * 
 * USAGE:
 * ------
 * #include "aille.hpp"
 * 
 * AILLE::AILLEEngine engine;
 * std::vector<AILLE::ModelSignal> signals = get_your_model_predictions();
 * AILLE::Decision decision = engine.makeDecision(signals);
 * 
 * No linking, no dependencies, no unnecessary complexity.
 */

#ifndef AILLE_HPP
#define AILLE_HPP

#include <vector>
#include <deque>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <mutex>

namespace AILLE {

// ============================================================================
// VERSION
// ============================================================================

constexpr const char* AILLE_VERSION = "2.1.0";
constexpr int AILLE_VERSION_MAJOR = 2;
constexpr int AILLE_VERSION_MINOR = 1;
constexpr int AILLE_VERSION_PATCH = 0;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

struct ModelSignal;
struct Decision;
struct AuditRecord;
struct AILLEConfig;
class AILLEEngine;
class AuditLogger;

enum DecisionStatus {
    DECISION_VALID,
    REJECTED_LOW_CONFIDENCE,
    REJECTED_NO_CONSENSUS,
    FALLBACK_ACTIVATED,
    ERROR_NO_MODELS
};

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct ModelSignal {
    float value;           // Prediction value
    float confidence;      // 0.0-1.0
    uint64_t timestamp_ns;
    int model_id;
    
    ModelSignal() : value(0.0f), confidence(0.0f), timestamp_ns(0), model_id(-1) {}
    
    ModelSignal(float v, float c, int id = 0) 
        : value(v), confidence(c), model_id(id) {
        timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
    }
};

struct Decision {
    float final_value;
    DecisionStatus status;
    float confidence;
    int models_agreed;
    bool fallback_used;
    uint64_t timestamp_ns;
    std::vector<int> contributing_models;
    std::string reasoning;
    
    Decision() : final_value(0.0f), status(ERROR_NO_MODELS), confidence(0.0f),
                 models_agreed(0), fallback_used(false), timestamp_ns(0) {}
};

struct AILLEConfig {
    float min_confidence_threshold;
    float grace_confidence_threshold;
    int min_models_required;
    float sign_agreement_threshold;
    int fallback_window_size;
    float fallback_position_scale;
    int max_model_count;
    
    AILLEConfig() :
        min_confidence_threshold(0.35f),
        grace_confidence_threshold(0.25f),
        min_models_required(2),
        sign_agreement_threshold(0.66f),
        fallback_window_size(50),
        fallback_position_scale(0.1f),
        max_model_count(10) {}
};

// ============================================================================
// AILLE ENGINE
// ============================================================================

class AILLEEngine {
private:
    static constexpr float MAX_SIGNAL_VALUE = 1e6f;

    AILLEConfig config;
    std::deque<float> fallback_buffer;
    mutable std::mutex engine_mtx_;
    
    [[nodiscard]] float calculateFallbackValue() const {
        if (fallback_buffer.empty()) return 0.0f;
        float sum = 0.0f;
        for (float val : fallback_buffer) sum += val;
        return sum / fallback_buffer.size();
    }
    
    void updateFallbackBuffer(float value) {
        fallback_buffer.push_back(value);
        size_t window = static_cast<size_t>(std::max(1, config.fallback_window_size));
        while (fallback_buffer.size() > window) {
            fallback_buffer.pop_front();
        }
    }
    
    // smoothPosition maps a raw signal to [-1, 1] via tanh.
    // The default scale=100.0f intentionally saturates to ±1.0 for direction-only
    // output. Callers wanting magnitude sensitivity should lower the scale via
    // AILLEConfig settings.
    float smoothPosition(float signal, float scale = 100.0f) const {
        return std::tanh(signal * scale);
    }
    
    std::vector<ModelSignal> applySafetyLayer(const std::vector<ModelSignal>& signals) {
        std::vector<ModelSignal> valid;
        valid.reserve(signals.size());
        for (const auto& sig : signals) {
            if (sig.confidence >= config.min_confidence_threshold) {
                valid.push_back(sig);
            } else if (sig.confidence >= config.grace_confidence_threshold) {
                ModelSignal grace_sig = sig;
                grace_sig.confidence *= 0.8f;
                valid.push_back(grace_sig);
            }
        }
        return valid;
    }
    
    bool checkConsensus(const std::vector<ModelSignal>& valid_signals,
                       float& consensus_value, int& models_agreed) {
        if (valid_signals.size() < static_cast<size_t>(config.min_models_required)) {
            models_agreed = 0;
            return false;
        }
        
        std::vector<float> values;
        values.reserve(valid_signals.size());
        for (const auto& sig : valid_signals) {
            values.push_back(sig.value);
        }
        
        const size_t median_index = values.size() / 2;
        std::nth_element(values.begin(), values.begin() + median_index, values.end());
        float median = values[median_index];
        float median_sign = (median >= 0) ? 1.0f : -1.0f;
        
        int agreement_count = 0;
        for (float val : values) {
            if (((val >= 0) ? 1.0f : -1.0f) == median_sign) agreement_count++;
        }
        
        models_agreed = agreement_count;
        float agreement_ratio = static_cast<float>(agreement_count) / values.size();
        
        if (agreement_ratio >= config.sign_agreement_threshold && 
            agreement_count >= config.min_models_required) {
            float sum = 0.0f;
            int count = 0;
            for (const auto& sig : valid_signals) {
                if (((sig.value >= 0) ? 1.0f : -1.0f) == median_sign) {
                    sum += sig.value;
                    count++;
                }
            }
            if (count > 0) {
                consensus_value = sum / count;
                return true;
            }
        }
        return false;
    }
    
    bool validateConfig(const AILLEConfig& cfg, Decision& decision) const {
        if (cfg.min_confidence_threshold <= 0.0f || cfg.min_confidence_threshold > 1.0f) {
            decision.status = ERROR_NO_MODELS;
            decision.reasoning = "Invalid config: min_confidence_threshold must be in (0, 1]";
            return false;
        }
        if (cfg.grace_confidence_threshold < 0.0f || cfg.grace_confidence_threshold > cfg.min_confidence_threshold) {
            decision.status = ERROR_NO_MODELS;
            decision.reasoning = "Invalid config: grace_confidence_threshold must be in [0, min_confidence_threshold]";
            return false;
        }
        if (cfg.min_models_required < 1) {
            decision.status = ERROR_NO_MODELS;
            decision.reasoning = "Invalid config: min_models_required must be >= 1";
            return false;
        }
        if (cfg.fallback_window_size < 1) {
            decision.status = ERROR_NO_MODELS;
            decision.reasoning = "Invalid config: fallback_window_size must be >= 1";
            return false;
        }
        if (cfg.sign_agreement_threshold <= 0.0f || cfg.sign_agreement_threshold > 1.0f) {
            decision.status = ERROR_NO_MODELS;
            decision.reasoning = "Invalid config: sign_agreement_threshold must be in (0, 1]";
            return false;
        }
        if (cfg.max_model_count < 1) {
            decision.status = ERROR_NO_MODELS;
            decision.reasoning = "Invalid config: max_model_count must be >= 1";
            return false;
        }
        return true;
    }

    float getFallbackValue() const {
        if (fallback_buffer.empty()) return 0.0f;
        float fb = calculateFallbackValue();
        return ((fb >= 0) ? 1.0f : -1.0f) * config.fallback_position_scale;
    }
    
public:
    AILLEEngine() {}
    explicit AILLEEngine(const AILLEConfig& cfg) : config(cfg) {}
    
    [[nodiscard]] Decision makeDecision(const std::vector<ModelSignal>& model_signals) {
        std::lock_guard<std::mutex> lock(engine_mtx_);
        Decision decision;
        decision.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();

        if (!validateConfig(config, decision)) {
            return decision;
        }
        
        if (model_signals.empty()) {
            decision.status = ERROR_NO_MODELS;
            decision.reasoning = "No model inputs";
            return decision;
        }

        // Validate inputs
        for (const auto& sig : model_signals) {
            if (std::isnan(sig.value) || std::isinf(sig.value) ||
                std::isnan(sig.confidence) || std::isinf(sig.confidence)) {
                decision.status = REJECTED_LOW_CONFIDENCE;
                decision.reasoning = "Invalid input (NaN/Inf) detected";
                decision.final_value = getFallbackValue();
                decision.confidence = 0.0f;
                decision.fallback_used = true;
                return decision;
            }
            if (sig.confidence < 0.0f || sig.confidence > 1.0f) {
                decision.status = REJECTED_LOW_CONFIDENCE;
                decision.reasoning = "Signal rejected: confidence out of range [0,1]";
                decision.final_value = getFallbackValue();
                decision.confidence = 0.0f;
                decision.fallback_used = true;
                return decision;
            }
            if (sig.value < -MAX_SIGNAL_VALUE || sig.value > MAX_SIGNAL_VALUE) {
                decision.status = REJECTED_LOW_CONFIDENCE;
                decision.reasoning = "Signal rejected: value out of reasonable bounds";
                decision.final_value = getFallbackValue();
                decision.confidence = 0.0f;
                decision.fallback_used = true;
                return decision;
            }
        }

        std::vector<ModelSignal> scoped_signals;
        const std::vector<ModelSignal>* scoped_signals_ptr = &model_signals;
        size_t max_models = static_cast<size_t>(config.max_model_count);
        if (model_signals.size() > max_models) {
            scoped_signals.assign(model_signals.begin(),
                                  model_signals.begin() + max_models);
            scoped_signals_ptr = &scoped_signals;
        }

        std::vector<ModelSignal> valid = applySafetyLayer(*scoped_signals_ptr);
        
        if (valid.empty()) {
            decision.status = REJECTED_LOW_CONFIDENCE;
            decision.final_value = getFallbackValue();
            decision.confidence = 0.1f;
            decision.fallback_used = true;
            decision.reasoning = "All models failed confidence - fallback";
            return decision;
        }
        
        float consensus_value;
        int models_agreed;
        bool consensus_ok = checkConsensus(valid, consensus_value, models_agreed);
        
        if (!consensus_ok) {
            decision.status = REJECTED_NO_CONSENSUS;
            decision.final_value = getFallbackValue();
            decision.confidence = 0.2f;
            decision.fallback_used = true;
            decision.models_agreed = models_agreed;
            decision.reasoning = "No consensus - fallback";
            return decision;
        }
        
        decision.status = DECISION_VALID;
        decision.final_value = smoothPosition(consensus_value);
        
        float total_conf = 0.0f;
        decision.contributing_models.reserve(valid.size());
        for (const auto& sig : valid) {
            total_conf += sig.confidence;
            decision.contributing_models.push_back(sig.model_id);
        }
        decision.confidence = valid.empty() ? 0.0f : (total_conf / valid.size());
        decision.models_agreed = models_agreed;
        decision.fallback_used = false;
        decision.reasoning = "Consensus: " + std::to_string(models_agreed) + " models";
        
        updateFallbackBuffer(decision.final_value);
        return decision;
    }
    
    void reset() noexcept {
        std::lock_guard<std::mutex> lock(engine_mtx_);
        fallback_buffer.clear();
    }
    AILLEConfig getConfig() const noexcept { return config; }
    void setConfig(const AILLEConfig& cfg) {
        std::lock_guard<std::mutex> lock(engine_mtx_);
        config = cfg;
    }
};

// ============================================================================
// AUDIT LOGGER (OPTIONAL - For Compliance)
// ============================================================================

struct AuditRecord {
    uint64_t timestamp_ns;
    uint64_t decision_id;
    DecisionStatus status;
    float final_value;
    float confidence;
    int models_agreed;
    bool fallback_used;
    std::string reasoning;
    std::vector<int> contributing_models;
    std::string symbol;
    std::string strategy_id;
    std::string hash;
    std::string prev_hash;
    
    AuditRecord() : timestamp_ns(0), decision_id(0), status(DECISION_VALID),
                   final_value(0.0f), confidence(0.0f), models_agreed(0),
                   fallback_used(false) {}
};

class AuditLogger {
private:
    std::ofstream log_file;
    std::vector<AuditRecord> audit_trail;
    uint64_t next_decision_id;
    std::string last_hash;
    mutable std::mutex mutex_;
    bool flush_on_write_;

    static uint32_t rotateRight(uint32_t value, uint32_t bits) {
        return (value >> bits) | (value << (32 - bits));
    }

    static std::string sha256(const std::string& input) {
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

        std::vector<uint8_t> data(input.begin(), input.end());
        uint64_t bit_len = static_cast<uint64_t>(data.size()) * 8;
        data.push_back(0x80);
        while ((data.size() % 64) != 56) {
            data.push_back(0);
        }
        for (int i = 7; i >= 0; --i) {
            data.push_back(static_cast<uint8_t>((bit_len >> (i * 8)) & 0xff));
        }

        for (size_t chunk = 0; chunk < data.size(); chunk += 64) {
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

        std::ostringstream out;
        out << std::hex << std::setfill('0');
        for (uint32_t value : h) {
            out << std::setw(8) << value;
        }
        return out.str();
    }

    std::string serializeRecord(const AuditRecord& record) const {
        std::ostringstream ss;
        ss << "timestamp_ns=" << record.timestamp_ns << '\x1f'
           << "decision_id=" << record.decision_id << '\x1f'
           << "status=" << static_cast<int>(record.status) << '\x1f'
           << "final_value=" << std::setprecision(10) << record.final_value << '\x1f'
           << "confidence=" << std::setprecision(10) << record.confidence << '\x1f'
           << "models_agreed=" << record.models_agreed << '\x1f'
           << "fallback_used=" << (record.fallback_used ? "1" : "0") << '\x1f'
           << "reasoning=" << record.reasoning << '\x1f'
           << "symbol=" << record.symbol << '\x1f'
           << "strategy_id=" << record.strategy_id << '\x1f'
           << "prev_hash=" << record.prev_hash << '\x1f'
           << "contributing_models=" << "";
        return ss.str();
    }

    std::string computeHash(const AuditRecord& record) const {
        std::ostringstream ss;
        ss << serializeRecord(record);
        for (size_t i = 0; i < record.contributing_models.size(); ++i) {
            if (i > 0) {
                ss << ",";
            }
            ss << record.contributing_models[i];
        }
        return sha256(ss.str());
    }

    std::string getTimestamp(uint64_t ns) const {
        time_t seconds = ns / 1000000000ULL;
        struct tm timeinfo;
#if defined(_WIN32)
        gmtime_s(&timeinfo, &seconds);
#else
        gmtime_r(&seconds, &timeinfo);
#endif
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return std::string(buffer);
    }

    std::string csvEscape(const std::string& value) const {
        std::string escaped = "\"";
        for (char c : value) {
            if (c == '"') {
                escaped += "\"\"";
            } else {
                escaped += c;
            }
        }
        escaped += "\"";
        return escaped;
    }
    
    std::string statusToString(DecisionStatus s) const {
        switch (s) {
            case DECISION_VALID: return "VALID";
            case REJECTED_LOW_CONFIDENCE: return "REJECTED_CONF";
            case REJECTED_NO_CONSENSUS: return "REJECTED_CONSENSUS";
            case FALLBACK_ACTIVATED: return "FALLBACK";
            default: return "ERROR";
        }
    }
    
public:
    AuditLogger(bool flush = true) : next_decision_id(1), last_hash("0000000000000000"), flush_on_write_(flush) {}
    
    explicit AuditLogger(const std::string& filename, bool flush = true)
        : next_decision_id(1), last_hash("0000000000000000"), flush_on_write_(flush) {
        open(filename);
    }

    AuditLogger(const AuditLogger&) = delete;
    AuditLogger& operator=(const AuditLogger&) = delete;
    
    ~AuditLogger() { close(); }
    
    bool open(const std::string& filename) {
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
    
    void logDecision(const Decision& d, const std::string& symbol = "",
                    const std::string& strategy = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        AuditRecord rec;
        rec.timestamp_ns = d.timestamp_ns;
        rec.decision_id = next_decision_id++;
        rec.status = d.status;
        rec.final_value = d.final_value;
        rec.confidence = d.confidence;
        rec.models_agreed = d.models_agreed;
        rec.fallback_used = d.fallback_used;
        rec.reasoning = d.reasoning;
        rec.contributing_models = d.contributing_models;
        rec.symbol = symbol;
        rec.strategy_id = strategy;
        rec.prev_hash = last_hash;
        rec.hash = computeHash(rec);
        last_hash = rec.hash;
        
        audit_trail.push_back(rec);
        
        if (log_file.is_open()) {
            std::ostringstream model_list;
            model_list << "[";
            for (size_t i = 0; i < rec.contributing_models.size(); ++i) {
                if (i > 0) {
                    model_list << ",";
                }
                model_list << rec.contributing_models[i];
            }
            model_list << "]";

            log_file << getTimestamp(rec.timestamp_ns) << "," << rec.decision_id << ","
                    << statusToString(rec.status) << "," << rec.final_value << ","
                    << rec.confidence << "," << rec.models_agreed << ","
                    << (rec.fallback_used ? "true" : "false") << ","
                    << csvEscape(rec.reasoning) << ","
                    << csvEscape(model_list.str()) << ","
                    << csvEscape(rec.symbol) << ","
                    << csvEscape(rec.strategy_id) << ","
                    << rec.hash << "," << rec.prev_hash << "\n";
            if (flush_on_write_) log_file.flush();
        }
    }
    
    bool verifyIntegrity() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (audit_trail.empty()) return true;
        std::string expected = "0000000000000000";
        for (const auto& rec : audit_trail) {
            if (rec.prev_hash != expected) return false;
            if (rec.hash != computeHash(rec)) return false;
            expected = rec.hash;
        }
        return true;
    }
};

} // namespace AILLE

#endif // AILLE_HPP

/*
 * ============================================================================
 * QUICK START EXAMPLE
 * ============================================================================
 * 
 * #include "aille.hpp"
 * 
 * int main() {
 *     // Step 1: Create engine
 *     AILLE::AILLEEngine engine;
 *     
 *     // Step 2: Get your model predictions
 *     std::vector<AILLE::ModelSignal> signals;
 *     signals.push_back(AILLE::ModelSignal(0.05f, 0.85f, 0));  // Model 0
 *     signals.push_back(AILLE::ModelSignal(0.03f, 0.72f, 1));  // Model 1
 *     signals.push_back(AILLE::ModelSignal(0.04f, 0.68f, 2));  // Model 2
 *     
 *     // Step 3: Get validated decision
 *     AILLE::Decision decision = engine.makeDecision(signals);
 *     
 *     // Step 4: Act on it
 *     if (decision.status == AILLE::DECISION_VALID) {
 *         execute_trade(decision.final_value);
 *     }
 *     
 *     return 0;
 * }
 * 
 * ============================================================================
 * COMPILE & RUN
 * ============================================================================
 * 
 * g++ -std=c++17 -O3 your_trading_system.cpp -o trader
 * ./trader
 * 
 * That's it. No linking. No external libraries. Just works.
 */
