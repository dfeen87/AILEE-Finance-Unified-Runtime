#ifndef AILLE_TEMPORAL_CONSISTENCY_HPP
#define AILLE_TEMPORAL_CONSISTENCY_HPP

#include <cstdint>
#include <cstddef>
#include "aille_arbitration.hpp" // For AssetId

namespace AILLE {

// ============================================================================
// VERSIONING & REGISTRY TAGS
// ============================================================================

constexpr const char* TEMPORAL_CONSISTENCY_V1 = "TEMPORAL_CONSISTENCY_V1";

// ============================================================================
// ENUMS
// ============================================================================

enum class TemporalAction : uint8_t {
    NO_CHANGE             = 0,
    CLAMPED               = 1,
    DAMPENED              = 2,
    OSCILLATED_AND_CLAMPED = 3
};

// ============================================================================
// CORE STRUCTS (64-byte Cache-Aligned)
// ============================================================================

struct alignas(64) TemporalState {
    AssetId asset_id;                 // 2 bytes
    uint8_t flags;                    // 1 byte (bit 0: oscillation_detected)
    uint8_t _padding1[5];             // 5 bytes alignment
    double prev_allocation;           // 8 bytes (w_{i,t})
    double prev_risk_score;           // 8 bytes (risk_{i,t})
    double prev_prev_allocation;      // 8 bytes (w_{i,t-1})
    uint8_t reserved[32];             // 32 bytes padding -> 64 bytes total
};
static_assert(sizeof(TemporalState) == 64, "TemporalState must be exactly 64 bytes");

struct alignas(64) TemporalResidual {
    AssetId asset_id;                 // 2 bytes
    uint8_t _padding1[6];             // 6 bytes alignment
    double expected_allocation;       // 8 bytes (\hat{w}_{i,t+1})
    double actual_allocation;         // 8 bytes (w_{i,t+1})
    double residual;                  // 8 bytes (|w_{i,t+1} - \hat{w}_{i,t+1}|)
    uint8_t reserved[32];             // 32 bytes padding -> 64 bytes total
};
static_assert(sizeof(TemporalResidual) == 64, "TemporalResidual must be exactly 64 bytes");

struct alignas(64) TemporalTraceStep {
    AssetId asset_id;                 // 2 bytes
    uint8_t action_taken;             // 1 byte (TemporalAction enum)
    uint8_t _padding1[5];             // 5 bytes alignment
    double before_value;              // 8 bytes (original proposed w_{i,t+1})
    double after_value;               // 8 bytes (final resolved w_{i,t+1})
    double residual;                  // 8 bytes
    char log[32];                     // 32 bytes compact log string -> 64 bytes total
};
static_assert(sizeof(TemporalTraceStep) == 64, "TemporalTraceStep must be exactly 64 bytes");

struct alignas(64) TemporalPortfolioState {
    double portfolio_risk;            // 8 bytes
    double residual_sum;              // 8 bytes (Canonical Temporal Residual sum)
    uint8_t reserved[48];             // 48 bytes padding -> 64 bytes total
};
static_assert(sizeof(TemporalPortfolioState) == 64, "TemporalPortfolioState must be exactly 64 bytes");

// ============================================================================
// CONTAINERS
// ============================================================================

struct TemporalStates {
    static constexpr size_t MAX_ASSETS = 16;
    TemporalState states[MAX_ASSETS];
    size_t count{0};
};

struct TemporalResiduals {
    static constexpr size_t MAX_ASSETS = 16;
    TemporalResidual residuals[MAX_ASSETS];
    size_t count{0};
};

struct TemporalTraceSteps {
    static constexpr size_t MAX_STEPS = 64;
    TemporalTraceStep steps[MAX_STEPS];
    size_t count{0};
};

// ============================================================================
// CORE ALGORITHM INTERFACE
// ============================================================================

void enforce_temporal_consistency(
    const TemporalStates& prev_states,
    TemporalStates& curr_states,
    const TemporalPortfolioState& prev_portfolio,
    TemporalPortfolioState& curr_portfolio,
    TemporalResiduals& residuals,
    TemporalTraceSteps& trace,
    double max_drift_threshold = 0.05
);

} // namespace AILLE

#endif // AILLE_TEMPORAL_CONSISTENCY_HPP
