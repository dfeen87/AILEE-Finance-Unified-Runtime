/*
 * AILLE Audit & Compliance Layer
 * Cryptographic logging and regulatory reporting
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 *
 * Provides immutable audit trail for all AILLE decisions
 * Compatible with SEC, EU AI Act, and MiFID II requirements
 */

#include "aille.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace AILLE {

// ============================================================================
// AUDIT RECORD METHOD IMPLEMENTATIONS (IF ANY)
// ============================================================================

// If your AuditRecord constructor isn't inline in the header, define it here:
// AuditRecord::AuditRecord() : ... {}


// ============================================================================
// AUDIT LOGGER METHOD IMPLEMENTATIONS
// ============================================================================

uint32_t AuditLogger::rotateRight(uint32_t value, uint32_t bits) {
    return (value >> bits) | (value << (32 - bits));
}

std::string AuditLogger::sha256(const std::string& input) {
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

std::string AuditLogger::serializeRecord(const AuditRecord& record) const {
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
       << "user_id=" << record.user_id << '\x1f'
       << "prev_hash=" << record.prev_hash << '\x1f'
       << "contributing_models=";
    for (size_t i = 0; i < record.contributing_models.size(); ++i) {
        if (i > 0) {
            ss << ",";
        }
        ss << record.contributing_models[i];
    }
    return ss.str();
}

std::string AuditLogger::computeHash(const AuditRecord& record) const {
    return sha256(serializeRecord(record));
}

std::string AuditLogger::getTimestamp(uint64_t ns) const {
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

std::string AuditLogger::csvEscape(const std::string& value) const {
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

std::string AuditLogger::statusToString(DecisionStatus status) const {
    switch (status) {
        case DECISION_VALID: return "VALID";
        case REJECTED_LOW_CONFIDENCE: return "REJECTED_CONFIDENCE";
        case REJECTED_NO_CONSENSUS: return "REJECTED_CONSENSUS";
        case FALLBACK_ACTIVATED: return "FALLBACK";
        case ERROR_NO_MODELS: return "ERROR_NO_MODELS";
        default: return "UNKNOWN";
    }
}

// Default Constructor
AuditLogger::AuditLogger() : next_decision_id(1), last_hash("0000000000000000") {}

// Parameterized Constructor
AuditLogger::AuditLogger(const std::string& log_filename) 
    : next_decision_id(1), last_hash("0000000000000000") {
    open(log_filename);
}

// Destructor
AuditLogger::~AuditLogger() {
    close();
}

bool AuditLogger::open(const std::string& filename) {
    log_file.open(filename, std::ios::app);
    if (!log_file.is_open()) {
        return false;
    }
    
    if (log_file.tellp() == 0) {
        log_file << "timestamp,decision_id,status,final_value,confidence,"
                << "models_agreed,fallback_used,reasoning,contributing_models,"
                << "symbol,strategy_id,user_id,hash,prev_hash\n";
    }
    
    return true;
}

void AuditLogger::close() {
    if (log_file.is_open()) {
        log_file.close();
    }
}

void AuditLogger::logDecision(const Decision& decision,
                             const std::string& symbol,
                             const std::string& strategy_id,
                             const std::string& user_id) {
    
    AuditRecord record;
    record.timestamp_ns = decision.timestamp_ns;
    record.decision_id = next_decision_id++;
    record.status = decision.status;
    record.final_value = decision.final_value;
    record.confidence = decision.confidence;
    record.models_agreed = decision.models_agreed;
    record.fallback_used = decision.fallback_used;
    record.reasoning = decision.reasoning;
    record.contributing_models = decision.contributing_models;
    record.symbol = symbol;
    record.strategy_id = strategy_id;
    record.user_id = user_id;
    record.prev_hash = last_hash;
    
    record.hash = computeHash(record);
    last_hash = record.hash;
    
    audit_trail.push_back(record);
    
    if (log_file.is_open()) {
        std::ostringstream model_list;
        model_list << "[";
        for (size_t i = 0; i < record.contributing_models.size(); i++) {
            if (i > 0) {
                model_list << ",";
            }
            model_list << record.contributing_models[i];
        }
        model_list << "]";

        log_file << getTimestamp(record.timestamp_ns) << ","
                << record.decision_id << ","
                << statusToString(record.status) << ","
                << record.final_value << ","
                << record.confidence << ","
                << record.models_agreed << ","
                << (record.fallback_used ? "true" : "false") << ","
                << csvEscape(record.reasoning) << ","
                << csvEscape(model_list.str()) << ","
                << csvEscape(record.symbol) << ","
                << csvEscape(record.strategy_id) << ","
                << csvEscape(record.user_id) << ","
                << record.hash << ","
                << record.prev_hash << "\n";
        
        log_file.flush();
    }
}

bool AuditLogger::verifyIntegrity() const {
    if (audit_trail.empty()) return true;

    std::string expected_prev_hash = "0000000000000000";

    for (const auto& record : audit_trail) {
        if (record.prev_hash != expected_prev_hash) {
            return false;
        }
        if (record.hash != computeHash(record)) {
            return false;
        }
        expected_prev_hash = record.hash;
    }

    return true;
}

void AuditLogger::generateReport(const std::string& output_file,
                                uint64_t start_ns, uint64_t end_ns) const {
    
    std::ofstream report(output_file);
    if (!report.is_open()) return;
    
    report << "AILLE Framework - Regulatory Compliance Report\n";
    report << "==============================================\n\n";
    report << "Report Period: " << getTimestamp(start_ns) 
           << " to " << getTimestamp(end_ns) << "\n\n";
    
    int total_decisions = 0;
    int valid_decisions = 0;
    int fallback_activations = 0;
    int rejected_confidence = 0;
    int rejected_consensus = 0;
    
    for (const auto& record : audit_trail) {
        if (record.timestamp_ns >= start_ns && record.timestamp_ns <= end_ns) {
            total_decisions++;
            
            if (record.status == DECISION_VALID) valid_decisions++;
            if (record.fallback_used) fallback_activations++;
            if (record.status == REJECTED_LOW_CONFIDENCE) rejected_confidence++;
            if (record.status == REJECTED_NO_CONSENSUS) rejected_consensus++;
        }
    }
    
    report << "Total Decisions: " << total_decisions << "\n";
    report << "Valid Decisions: " << valid_decisions << " ("
           << (total_decisions > 0 ? (100.0f * valid_decisions / total_decisions) : 0.0f)
           << "%)\n";
    report << "Fallback Activations: " << fallback_activations << " ("
           << (total_decisions > 0 ? (100.0f * fallback_activations / total_decisions) : 0.0f)
           << "%)\n";
    report << "Rejected (Confidence): " << rejected_confidence << "\n";
    report << "Rejected (Consensus): " << rejected_consensus << "\n\n";
    
    report << "Audit Trail Integrity: " 
           << (verifyIntegrity() ? "VERIFIED" : "COMPROMISED") << "\n\n";
    
    report << "Detailed Log:\n";
    report << "-------------\n";
    for (const auto& record : audit_trail) {
        if (record.timestamp_ns >= start_ns && record.timestamp_ns <= end_ns) {
            report << getTimestamp(record.timestamp_ns) << " | "
                   << "ID:" << record.decision_id << " | "
                   << statusToString(record.status) << " | "
                   << "Value:" << record.final_value << " | "
                   << "Conf:" << record.confidence << " | "
                   << record.reasoning << "\n";
        }
    }
    
    report.close();
}

size_t AuditLogger::getAuditTrailSize() const {
    return audit_trail.size();
}

const std::vector<AuditRecord>& AuditLogger::getAuditTrail() const {
    return audit_trail;
}

} // namespace AILLE
