#ifndef AILLE_GOVERNOR_RECONCILIATION_HPP
#define AILLE_GOVERNOR_RECONCILIATION_HPP

#include <cstdint>
#include <cstddef>
#include <vector>
#include "aille_arbitration.hpp" // For AssetId

namespace AILLE {

// ============================================================================
// VERSIONING & CONSTANTS
// ============================================================================

constexpr const char* GOVERNOR_LADDER_V1 = "GOVERNOR_LADDER_V1";
constexpr const char* OVERRIDE_MATRIX_V1  = "OVERRIDE_MATRIX_V1";

// Special AssetId sentinel for global scalar proposals
constexpr AssetId GLOBAL_ASSET_ID = static_cast<AssetId>(0xFFFF);

// ============================================================================
// ENUMS
// ============================================================================

enum class GovernorType : uint8_t {
    COMPLIANCE = 0,
    RISK       = 1,
    LIQUIDITY  = 2,
    RETURN     = 3,
    STRATEGY   = 4
};

enum class ReconciliationFlags : uint8_t {
    NONE       = 0,
    HARD_BLOCK = 1,
    RISK_LIMIT = 2,
    VOL_CLAMP  = 4
};

// ============================================================================
// CORE STRUCTS (64-byte Cache-Aligned)
// ============================================================================

struct alignas(64) GovernorProposal {
    AssetId asset_id;        // 2 bytes
    uint8_t governor_type;   // 1 byte (GovernorType enum)
    uint8_t flags;           // 1 byte (ReconciliationFlags bitmask)
    float risk_score;        // 4 bytes (e.g. for RISK threshold overrides)
    double proposed_value;   // 8 bytes (e.g. target allocation or magnitude)
    uint8_t reserved[48];    // 48 bytes padding -> 64 bytes total
};
static_assert(sizeof(GovernorProposal) == 64, "GovernorProposal must be exactly 64 bytes");

struct alignas(64) GovernorDecision {
    AssetId asset_id;        // 2 bytes
    uint8_t flags_applied;   // 1 byte
    uint8_t resolved_type;   // 1 byte (GovernorType of the primary driving governor)
    uint8_t _padding1[4];    // 4 bytes alignment
    double final_value;      // 8 bytes (reconciled outcome value)
    uint8_t reserved[48];    // 48 bytes padding -> 64 bytes total
};
static_assert(sizeof(GovernorDecision) == 64, "GovernorDecision must be exactly 64 bytes");

struct alignas(64) ReconciliationTraceStep {
    AssetId asset_id;        // 2 bytes
    uint8_t governor_type;   // 1 byte
    uint8_t action_taken;    // 1 byte (0 = Accepted, 1 = Overridden, 2 = Clamped, 3 = Blocked, 4 = Damped)
    uint8_t _padding1[4];    // 4 bytes alignment
    double proposed_value;   // 8 bytes
    double interim_value;    // 8 bytes
    char log[40];            // 40 bytes compact descriptive string
};
static_assert(sizeof(ReconciliationTraceStep) == 64, "ReconciliationTraceStep must be exactly 64 bytes");

struct alignas(64) ReconciliationResidual {
    AssetId asset_id;        // 2 bytes
    uint8_t _padding1[6];    // 6 bytes alignment
    double residual_value;   // 8 bytes (sum(w_i * |p_i - d|))
    uint8_t reserved[48];    // 48 bytes padding -> 64 bytes total
};
static_assert(sizeof(ReconciliationResidual) == 64, "ReconciliationResidual must be exactly 64 bytes");

// ============================================================================
// TRACE CONTAINER
// ============================================================================

struct ReconciliationTrace {
    static constexpr size_t MAX_STEPS = 64;
    ReconciliationTraceStep steps[MAX_STEPS];
    size_t step_count{0};
};

struct alignas(64) ReconciledResultSummary {
    double total_residual;   // 8 bytes
    uint32_t trace_count;    // 4 bytes
    uint8_t _pad1[4];        // 4 bytes alignment
    double lyapunov_energy;  // 8 bytes
    double tension;          // 8 bytes
    uint8_t reserved[32];    // 32 bytes padding -> 64 bytes total
};
static_assert(sizeof(ReconciledResultSummary) == 64, "ReconciledResultSummary must be exactly 64 bytes");

struct ReconciledResult {
    GovernorDecision decision;
    ReconciliationResidual residual;
    ReconciledResultSummary summary;
    ReconciliationTrace trace;
};

// ============================================================================
// RECONCILIATION INTERFACE
// ============================================================================

ReconciledResult reconcile_governors(
    const std::vector<GovernorProposal>& proposals,
    AssetId asset_id
);

} // namespace AILLE

#endif // AILLE_GOVERNOR_RECONCILIATION_HPP
