#ifndef AILLE_PORTFOLIO_CONSTRAINTS_HPP
#define AILLE_PORTFOLIO_CONSTRAINTS_HPP

#include <cstdint>
#include <cstddef>
#include <vector>
#include "aille_arbitration.hpp" // For AssetId

namespace AILLE {

// ============================================================================
// VERSIONING & REGISTRY TAGS
// ============================================================================

constexpr const char* PORTFOLIO_CONSTRAINTS_V1  = "PORTFOLIO_CONSTRAINTS_V1";
constexpr const char* SECTOR_CAPS_V1           = "SECTOR_CAPS_V1";
constexpr const char* CORRELATION_DAMPENING_V1  = "CORRELATION_DAMPENING_V1";
constexpr const char* RISK_BUDGET_V1           = "RISK_BUDGET_V1";

// ============================================================================
// ENUMS
// ============================================================================

enum class SectorId : uint8_t {
    FIAT             = 0,
    CRYPTO           = 1,
    PRECIOUS_METALS  = 2,
    ENERGY           = 3,
    OTHER            = 4
};

enum class ConstraintStage : uint8_t {
    EXPOSURE_CLAMP        = 1,
    SECTOR_CAP_ENFORCE    = 2,
    CORRELATION_DAMPEN    = 3,
    RISK_BUDGET_ENFORCE   = 4
};

enum class ConstraintAction : uint8_t {
    NO_CHANGE             = 0,
    CLAMPED               = 1,
    DAMPENED              = 2,
    SCALED                = 3
};

// ============================================================================
// CORE STRUCTS (64-byte Cache-Aligned)
// ============================================================================

struct alignas(64) ConstraintRule {
    AssetId asset_id;                 // 2 bytes
    uint8_t is_active;                // 1 byte
    uint8_t _padding1[5];             // 5 bytes alignment
    double max_long_exposure;         // 8 bytes (e.g. 0.40)
    double max_short_exposure;        // 8 bytes (e.g. -0.20 or absolute)
    uint8_t reserved[40];             // 40 bytes padding -> 64 bytes total
};
static_assert(sizeof(ConstraintRule) == 64, "ConstraintRule must be exactly 64 bytes");

struct alignas(64) SectorDefinition {
    uint8_t sector_id;                // 1 byte (SectorId enum)
    uint8_t is_active;                // 1 byte
    uint8_t _padding1[6];             // 6 bytes alignment
    double max_sector_exposure;       // 8 bytes (e.g. 0.30)
    char sector_name[16];             // 16 bytes (e.g. "CRYPTO")
    uint8_t reserved[32];             // 32 bytes padding -> 64 bytes total
};
static_assert(sizeof(SectorDefinition) == 64, "SectorDefinition must be exactly 64 bytes");

struct alignas(64) CorrelationProfile {
    AssetId asset_a;                  // 2 bytes
    AssetId asset_b;                  // 2 bytes
    uint8_t is_active;                // 1 byte
    uint8_t _padding1[3];             // 3 bytes alignment
    double correlation_score;         // 8 bytes (rho_ij in [0, 1])
    uint8_t reserved[48];             // 48 bytes padding -> 64 bytes total
};
static_assert(sizeof(CorrelationProfile) == 64, "CorrelationProfile must be exactly 64 bytes");

struct alignas(64) RiskBudget {
    double max_portfolio_risk;        // 8 bytes (e.g. 50.0)
    uint8_t is_active;                // 1 byte
    uint8_t _padding1[7];             // 7 bytes alignment
    uint8_t reserved[48];             // 48 bytes padding -> 64 bytes total
};
static_assert(sizeof(RiskBudget) == 64, "RiskBudget must be exactly 64 bytes");

struct alignas(64) ConstraintTraceStep {
    AssetId asset_id;                 // 2 bytes
    uint8_t stage;                    // 1 byte (ConstraintStage enum)
    uint8_t action_taken;             // 1 byte (ConstraintAction enum)
    uint8_t _padding1[4];             // 4 bytes alignment
    double before_value;              // 8 bytes
    double after_value;               // 8 bytes
    char log[40];                     // 40 bytes compact descriptive string
};
static_assert(sizeof(ConstraintTraceStep) == 64, "ConstraintTraceStep must be exactly 64 bytes");

struct alignas(64) AssetAllocation {
    AssetId asset_id;                 // 2 bytes
    uint8_t _padding1[6];             // 6 bytes alignment
    double allocation;                // 8 bytes (fraction of portfolio, w_i)
    double risk_score;                // 8 bytes (raw risk score per asset)
    uint8_t reserved[40];             // 40 bytes padding -> 64 bytes total
};
static_assert(sizeof(AssetAllocation) == 64, "AssetAllocation must be exactly 64 bytes");

struct alignas(64) ConstraintResultSummary {
    double initial_portfolio_risk;    // 8 bytes
    double final_portfolio_risk;      // 8 bytes
    uint32_t trace_count;             // 4 bytes
    uint32_t remaining_violations;    // 4 bytes
    double max_risk_budget;           // 8 bytes
    uint8_t reserved[32];             // 32 bytes padding -> 64 bytes total
};
static_assert(sizeof(ConstraintResultSummary) == 64, "ConstraintResultSummary must be exactly 64 bytes");

// ============================================================================
// CONTAINERS
// ============================================================================

struct ConstraintRules {
    static constexpr size_t MAX_RULES = 32;
    ConstraintRule rules[MAX_RULES];
    size_t count{0};
};

struct SectorDefinitions {
    static constexpr size_t MAX_SECTORS = 8;
    SectorDefinition sectors[MAX_SECTORS];
    size_t count{0};
};

struct CorrelationProfiles {
    static constexpr size_t MAX_PROFILES = 32;
    CorrelationProfile profiles[MAX_PROFILES];
    size_t count{0};
};

struct ConstraintTrace {
    static constexpr size_t MAX_STEPS = 64;
    ConstraintTraceStep steps[MAX_STEPS];
    size_t step_count{0};
};

struct AssetAllocations {
    static constexpr size_t MAX_ASSETS = 16;
    AssetAllocation allocations[MAX_ASSETS];
    size_t count{0};
};

struct PortfolioConstraintResult {
    AssetAllocations allocations;
    ConstraintResultSummary summary;
    ConstraintTrace trace;
};

// ============================================================================
// CORE ALGORITHM INTERFACE
// ============================================================================

PortfolioConstraintResult apply_portfolio_constraints(
    const AssetAllocations& proposed,
    const ConstraintRules& rules,
    const SectorDefinitions& sectors,
    const CorrelationProfiles& correlations,
    const RiskBudget& budget
);

// Helper function to map AssetId to SectorId deterministically
SectorId map_asset_to_sector(AssetId asset_id);

} // namespace AILLE

#endif // AILLE_PORTFOLIO_CONSTRAINTS_HPP
