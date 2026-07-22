#ifndef AILLE_META_GOVERNANCE_HPP
#define AILLE_META_GOVERNANCE_HPP

#include <cstdint>
#include <cstddef>
#include "aille_arbitration.hpp" // For AssetId
#include "aille_governor_reconciliation.hpp" // For ReconciledResult
#include "aille_portfolio_constraints.hpp" // For PortfolioConstraintResult
#include "aille_stress_regime_override.hpp" // For StressPortfolioState, StressTraceSteps
#include "aille_temporal_consistency.hpp" // For TemporalPortfolioState

namespace AILLE {

// ============================================================================
// VERSIONING & CONSTANTS
// ============================================================================

constexpr const char* META_GOVERNANCE_LOCK_V1 = "META_GOVERNANCE_LOCK_V1";

constexpr double META_GOVERNANCE_RESIDUAL_THRESHOLD = 0.05;
constexpr double TEMPORAL_RESIDUAL_CRITICAL_THRESHOLD = 0.10;

// ============================================================================
// REASON CODES (Bitmask)
// ============================================================================

enum MetaGovernanceReason : uint32_t {
    GOVERNOR_CONFLICT         = 1,
    CONSTRAINT_VIOLATION      = 2,
    STRESS_OVERRIDE_MISSING   = 4,
    TEMPORAL_INCONSISTENT     = 8
};

// ============================================================================
// CORE STRUCTS (64-byte Cache-Aligned)
// ============================================================================

struct alignas(64) MetaGovernanceState {
    double final_portfolio_risk;  // 8 bytes
    double final_residual_sum;    // 8 bytes
    uint8_t final_stress_level;   // 1 byte
    uint8_t execution_ready;      // 1 byte (1 for true, 0 for false)
    uint8_t reserved[46];         // 46 bytes padding -> 64 bytes total
};
static_assert(sizeof(MetaGovernanceState) == 64, "MetaGovernanceState must be exactly 64 bytes");

struct alignas(64) MetaGovernanceTraceStep {
    uint32_t reason_code;         // 4 bytes
    char log[40];                 // 40 bytes (short message)
    uint8_t reserved[20];         // 20 bytes padding -> 64 bytes total
};
static_assert(sizeof(MetaGovernanceTraceStep) == 64, "MetaGovernanceTraceStep must be exactly 64 bytes");

// ============================================================================
// CONTAINERS
// ============================================================================

struct MetaGovernanceTraceSteps {
    static constexpr size_t MAX_STEPS = 32;
    MetaGovernanceTraceStep steps[MAX_STEPS];
    size_t count{0};
};

// ============================================================================
// ALIASES FOR COMPATIBILITY
// ============================================================================

using ReconciledDecision = ReconciledResult;
using PortfolioConstraintsState = PortfolioConstraintResult;

// ============================================================================
// CORE ALGORITHM INTERFACE
// ============================================================================

MetaGovernanceState apply_meta_governance_lock(
    const ReconciledResult& decision,
    const PortfolioConstraintResult& constraints,
    const StressPortfolioState& stress_state,
    const StressTraceSteps& stress_trace,
    const TemporalPortfolioState& temporal_state,
    MetaGovernanceTraceSteps& trace
);

} // namespace AILLE

#endif // AILLE_META_GOVERNANCE_HPP
