#ifndef AILLE_ARBITRATION_HPP
#define AILLE_ARBITRATION_HPP

#include <cstdint>
#include <cstddef>
#include <vector>

namespace AILLE {

enum class AssetId : uint16_t {
    CASH = 0,
    BTC = 1,
    ETH = 2,
    GOLD = 3,
    OIL = 4,
    EQUITY_HIGH_RISK = 5
};

enum class AdvisoryFlags : uint16_t {
    NONE = 0,
    HARD_BLOCK = 1,
    SOFT_BLOCK = 2,
    PREFERRED = 4
};

struct alignas(64) Advisory {
    AssetId asset_id;
    uint16_t padding_flags;
    uint32_t raw_flags;       // AdvisoryFlags bitmask
    double risk_score;        // Raw risk score [0, 100]
    double safety_level;      // Raw safety level [0.0, 1.0]
    double liquidity_level;   // Raw liquidity level [0.0, 1.0]
    double regulatory_level;  // Raw regulatory level [0.0, 1.0]
    double return_score;      // Projected return score [0.0, 1.0]
    double confidence;        // Confidence in signal [0.0, 1.0]
    uint8_t _padding[8];      // Pad to exactly 64 bytes
};
static_assert(sizeof(Advisory) == 64, "Advisory must be exactly 64 bytes");

struct alignas(64) AllocationDecision {
    AssetId asset_id;
    uint16_t padding_flags;
    uint32_t reserved_flags;
    double recommended_allocation; // Deterministic allocation fraction [0.0, 1.0]
    uint8_t _padding[48];          // Pad to exactly 64 bytes
};
static_assert(sizeof(AllocationDecision) == 64, "AllocationDecision must be exactly 64 bytes");

struct alignas(64) ArbitrationTraceStep {
    AssetId asset_id;
    uint16_t dimension;            // Dimension evaluated
    uint32_t reserved_flags;
    double input_value;            // Scale input
    double result_weight;          // Resulting weight
    char log[40];                  // Compact descriptive string
};
static_assert(sizeof(ArbitrationTraceStep) == 64, "ArbitrationTraceStep must be exactly 64 bytes");

struct ArbitrationTrace {
    static constexpr size_t MAX_STEPS = 64;
    ArbitrationTraceStep steps[MAX_STEPS];
    size_t step_count{0};
};

struct ArbitrationResult {
    static constexpr size_t MAX_ASSETS = 16;
    AllocationDecision decisions[MAX_ASSETS];
    size_t decision_count{0};
    ArbitrationTrace trace;
};

enum class LadderDimension : uint16_t {
    SAFETY = 0,
    LIQUIDITY = 1,
    REGULATORY = 2,
    RISK = 3,
    RETURN = 4
};

struct Ladder {
    const char* version = "LADDER_V1";
    LadderDimension dimensions[5] = {
        LadderDimension::SAFETY,
        LadderDimension::LIQUIDITY,
        LadderDimension::REGULATORY,
        LadderDimension::RISK,
        LadderDimension::RETURN
    };
};

struct ScalingRules {
    const char* version = "SCALING_RULESET_V1";

    static constexpr double clamp(double val, double min, double max) noexcept {
        return (val < min) ? min : ((val > max) ? max : val);
    }

    static constexpr double normalize_risk(double raw_risk, double r_min = 0.0, double r_max = 100.0) noexcept {
        return clamp((raw_risk - r_min) / (r_max - r_min), 0.0, 1.0);
    }

    static constexpr double normalize_liquidity(double level) noexcept {
        return clamp(level, 0.0, 1.0);
    }

    static constexpr double normalize_regulatory(double level) noexcept {
        return clamp(level, 0.0, 1.0);
    }
};

ArbitrationResult arbitrate(
    const std::vector<Advisory>& advisories,
    const Ladder& ladder,
    const ScalingRules& rules
);

} // namespace AILLE

#endif // AILLE_ARBITRATION_HPP
