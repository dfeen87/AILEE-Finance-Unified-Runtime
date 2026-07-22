#ifndef AILLE_ROUTING_HPP
#define AILLE_ROUTING_HPP

#include <cstdint>
#include <cstddef>
#include <vector>
#include "aille_arbitration.hpp" // For AssetId

namespace AILLE {

// ============================================================================
// VERSIONING & REGISTRY TAGS
// ============================================================================

constexpr const char* LIQUIDITY_ROUTING_V1  = "LIQUIDITY_ROUTING_V1";
constexpr const char* LIQUIDITY_CAPS_V1     = "LIQUIDITY_CAPS_V1";
constexpr const char* ROUTING_TABLE_V1      = "ROUTING_TABLE_V1";
constexpr const char* SHOCK_BOUNDS_V1       = "SHOCK_BOUNDS_V1";

// ============================================================================
// ENUMS
// ============================================================================

enum class StressLevel : uint8_t {
    NORMAL   = 0,
    ELEVATED = 1,
    CRISIS   = 2
};

// ============================================================================
// CORE STRUCTS (64-byte Cache-Aligned)
// ============================================================================

struct alignas(64) LiquidityCap {
    AssetId asset_id;                 // 2 bytes
    uint8_t stress_level;             // 1 byte (StressLevel enum)
    uint8_t _padding1[5];             // 5 bytes alignment
    double max_outflow_ratio;         // 8 bytes
    double max_inflow_ratio;          // 8 bytes
    uint8_t reserved[40];             // 40 bytes padding -> 64 bytes total
};
static_assert(sizeof(LiquidityCap) == 64, "LiquidityCap must be exactly 64 bytes");

struct alignas(64) RoutingRule {
    AssetId source;                   // 2 bytes
    AssetId primary_target;           // 2 bytes
    AssetId fallback_target;          // 2 bytes
    uint8_t stress_level;             // 1 byte (StressLevel enum)
    uint8_t _padding1[1];             // 1 byte alignment
    double preferred_flow_ratio;      // 8 bytes
    uint8_t reserved[48];             // 48 bytes padding -> 64 bytes total
};
static_assert(sizeof(RoutingRule) == 64, "RoutingRule must be exactly 64 bytes");

struct alignas(64) ShockBounds {
    double max_portfolio_liquidity_shift_per_step; // 8 bytes (fraction of total portfolio)
    double max_asset_liquidity_shift_per_step;     // 8 bytes (fraction of asset liquidity)
    uint8_t reserved[48];                          // 48 bytes padding -> 64 bytes total
};
static_assert(sizeof(ShockBounds) == 64, "ShockBounds must be exactly 64 bytes");

struct alignas(64) CrossAssetDecision {
    AssetId asset_id;                 // 2 bytes
    uint8_t flags;                    // 1 byte
    uint8_t _padding1[5];             // 5 bytes alignment
    double target_allocation_ratio;   // 8 bytes
    double current_allocation_ratio;  // 8 bytes
    uint8_t reserved[40];             // 40 bytes padding -> 64 bytes total
};
static_assert(sizeof(CrossAssetDecision) == 64, "CrossAssetDecision must be exactly 64 bytes");

struct alignas(64) StressProfile {
    double volatility_index;          // 8 bytes
    double drawdown_index;            // 8 bytes
    double correlation_index;         // 8 bytes
    uint8_t stress_level;             // 1 byte (StressLevel enum)
    uint8_t reserved[39];             // 39 bytes padding -> 64 bytes total
};
static_assert(sizeof(StressProfile) == 64, "StressProfile must be exactly 64 bytes");

struct alignas(64) LiquidityState {
    AssetId asset_id;                 // 2 bytes
    uint8_t flags;                    // 1 byte (e.g. frozen/restricted)
    uint8_t _padding1[5];             // 5 bytes alignment
    double current_liquidity_value;   // 8 bytes
    double current_allocation_ratio;  // 8 bytes
    uint8_t reserved[40];             // 40 bytes padding -> 64 bytes total
};
static_assert(sizeof(LiquidityState) == 64, "LiquidityState must be exactly 64 bytes");

struct alignas(64) LiquidityFlow {
    AssetId source;                   // 2 bytes
    AssetId target;                   // 2 bytes
    uint8_t _padding1[4];             // 4 bytes alignment
    double amount;                    // 8 bytes
    uint8_t reserved[48];             // 48 bytes padding -> 64 bytes total
};
static_assert(sizeof(LiquidityFlow) == 64, "LiquidityFlow must be exactly 64 bytes");

struct alignas(64) RoutingTraceStep {
    AssetId source;                   // 2 bytes
    AssetId target;                   // 2 bytes
    uint8_t stress_level;             // 1 byte
    uint8_t reserved_flags;           // 1 byte
    uint8_t _padding1[2];             // 2 bytes alignment
    double proposed_flow;             // 8 bytes
    double actual_flow;               // 8 bytes
    char log[40];                     // 40 bytes compact descriptive string
};
static_assert(sizeof(RoutingTraceStep) == 64, "RoutingTraceStep must be exactly 64 bytes");

struct RoutingTrace {
    static constexpr size_t MAX_STEPS = 64;
    RoutingTraceStep steps[MAX_STEPS];
    size_t step_count{0};
};

struct alignas(64) RoutingResult {
    double total_shift_value;         // 8 bytes
    uint32_t flow_count;              // 4 bytes
    uint8_t reserved[52];             // 52 bytes padding -> 64 bytes total
};
static_assert(sizeof(RoutingResult) == 64, "RoutingResult must be exactly 64 bytes");

// ============================================================================
// CONTAINERS
// ============================================================================

struct LiquidityCaps {
    static constexpr size_t MAX_CAPS = 16;
    LiquidityCap caps[MAX_CAPS];
    size_t count{0};
};

struct RoutingTable {
    static constexpr size_t MAX_RULES = 32;
    RoutingRule rules[MAX_RULES];
    size_t count{0};
};

struct LiquidityStateSet {
    static constexpr size_t MAX_ASSETS = 16;
    LiquidityState states[MAX_ASSETS];
    size_t count{0};
};

struct CrossAssetDecisions {
    static constexpr size_t MAX_DECISIONS = 16;
    CrossAssetDecision decisions[MAX_DECISIONS];
    size_t count{0};
};

struct DetailedRoutingResult {
    RoutingResult summary;
    static constexpr size_t MAX_FLOWS = 32;
    LiquidityFlow flows[MAX_FLOWS];
    size_t flow_count{0};
    RoutingTrace trace;
};

// ============================================================================
// CORE ROUTING FUNCTION
// ============================================================================

DetailedRoutingResult route_liquidity(
    const CrossAssetDecisions& decisions,
    const StressProfile& stress,
    const LiquidityStateSet& states,
    const LiquidityCaps& caps,
    const RoutingTable& table,
    const ShockBounds& bounds
);

} // namespace AILLE

#endif // AILLE_ROUTING_HPP
