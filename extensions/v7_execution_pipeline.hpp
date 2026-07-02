#ifndef AILLE_V7_EXECUTION_PIPELINE_HPP
#define AILLE_V7_EXECUTION_PIPELINE_HPP

#include <vector>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <iostream>

namespace AILLE {
namespace V7 {

// ============================================================================
// 1. Dynamic Handle-Lock (Consensus Layer Optimization)
// ============================================================================

struct ConsensusLayer {
    static constexpr std::size_t WINDOW = 16;

    double base_confidence_threshold;
    double min_confidence_threshold;
    double breakout_multiplier;
    double breakout_velocity_threshold;

    double volume_delta_window[WINDOW];
    std::size_t window_index;
    std::size_t window_count;

    ConsensusLayer(double base_threshold = 0.35, double multiplier = 1.5, double velocity_threshold = 1.5) noexcept;

    void update_confidence(double volume_delta) noexcept;
    bool validate_signal(double consensus_score) const noexcept;
};

// ============================================================================
// 2. Golden-Ratio Volatility Governor (MSM Expansion)
// ============================================================================

struct MacroSignalGovernor {
    static constexpr double INV_GOLDEN_RATIO = 0.6180339887;

    double volatility_prev;
    double volatility_curr;

    MacroSignalGovernor() noexcept;

    void update_volatility(double new_value) noexcept;

    // Accepts an external std::vector<double>& but performs NO allocations internally
    void apply_beta_scaling(std::vector<double>& weights) noexcept;
};

// ============================================================================
// 3. Zero-Allocation Telemetry Export (V6.3 Clean-Up)
// ============================================================================

struct alignas(64) TelemetryLog {
    char buffer[64]; // single cache line

    TelemetryLog() noexcept;

    void write(const char* msg) noexcept;
    void export_now() noexcept;
};

// ============================================================================
// V7 Pipeline Demo Integration
// ============================================================================

void run_v7_pipeline_demo() noexcept;

} // namespace V7
} // namespace AILLE

#endif // AILLE_V7_EXECUTION_PIPELINE_HPP
