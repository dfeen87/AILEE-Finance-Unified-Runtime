#ifndef AILLE_STRESS_REGIME_OVERRIDE_HPP
#define AILLE_STRESS_REGIME_OVERRIDE_HPP

#include <cstdint>
#include <cstddef>
#include "aille_arbitration.hpp" // For AssetId
#include "aille_portfolio_constraints.hpp" // For AssetAllocations, AssetAllocation

namespace AILLE {

// ============================================================================
// VERSIONING & REGISTRY TAGS
// ============================================================================

constexpr const char* STRESS_REGIME_OVERRIDE_V1 = "STRESS_REGIME_OVERRIDE_V1";

// ============================================================================
// ENUMS
// ============================================================================

enum class StressMode : uint8_t {
    NORMAL = 0,
    STRESS = 1,
    CRISIS = 2
};

// ============================================================================
// CORE STRUCTS (64-byte Cache-Aligned)
// ============================================================================

struct alignas(64) StressOverrideRules {
    double volatility_threshold;         // 8 bytes
    double drawdown_threshold;           // 8 bytes
    double correlation_threshold;        // 8 bytes
    double residual_threshold;           // 8 bytes
    double crash_dampening_factor;       // 8 bytes (s_crash)
    double fallback_lambda;              // 8 bytes (lambda)
    uint8_t mode;                        // 1 byte (NORMAL/STRESS/CRISIS)
    uint8_t reserved[15];                // 15 bytes padding -> 64 bytes total
};
static_assert(sizeof(StressOverrideRules) == 64, "StressOverrideRules must be exactly 64 bytes");

struct alignas(64) SafeBaseline {
    AssetId asset_id;                    // 2 bytes
    uint8_t flags;                       // 1 byte (bit 0: IS_SAFE_SET)
    uint8_t _padding1[5];                // 5 bytes alignment
    double baseline_allocation;          // 8 bytes (e.g., CASH=0.8, others=0.0)
    uint8_t reserved[48];                // 48 bytes padding -> 64 bytes total
};
static_assert(sizeof(SafeBaseline) == 64, "SafeBaseline must be exactly 64 bytes");

struct alignas(64) StressTraceStep {
    AssetId asset_id;                    // 2 bytes
    uint8_t flags;                       // 1 byte (bit 0: dampened, bit 1: frozen, bit 2: fallback_applied)
    uint8_t _padding1[5];                // 5 bytes alignment
    double original_allocation;          // 8 bytes (before override)
    double adjusted_allocation;          // 8 bytes (after override)
    uint8_t reserved[40];                // 40 bytes padding -> 64 bytes total
};
static_assert(sizeof(StressTraceStep) == 64, "StressTraceStep must be exactly 64 bytes");

struct alignas(64) StressPortfolioState {
    double portfolio_risk;               // 8 bytes
    double stress_index;                 // 8 bytes (composite stress metric)
    double volatility_index;             // 8 bytes
    double drawdown_index;               // 8 bytes
    double correlation_index;            // 8 bytes
    double temporal_residual_sum;        // 8 bytes
    uint8_t stress_level;                // 1 byte (NORMAL/STRESS/CRISIS)
    uint8_t reserved[15];                // 15 bytes padding -> 64 bytes total
};
static_assert(sizeof(StressPortfolioState) == 64, "StressPortfolioState must be exactly 64 bytes");

// ============================================================================
// CONTAINERS
// ============================================================================

struct SafeBaselineContainer {
    static constexpr size_t MAX_ASSETS = 16;
    SafeBaseline baselines[MAX_ASSETS];
    size_t count{0};
};

struct StressTraceSteps {
    static constexpr size_t MAX_STEPS = 64;
    StressTraceStep steps[MAX_STEPS];
    size_t count{0};
};

// ============================================================================
// CORE ALGORITHM INTERFACE
// ============================================================================

void apply_stress_regime_override(
    const StressOverrideRules& rules,
    const StressPortfolioState& state,
    const AssetAllocations& prev_allocations,
    AssetAllocations& allocations,
    const SafeBaselineContainer& baselines,
    StressTraceSteps& trace,
    bool normal_safety_failed
);

} // namespace AILLE

#endif // AILLE_STRESS_REGIME_OVERRIDE_HPP
