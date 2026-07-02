#include "v7_execution_pipeline.hpp"
#include <algorithm>

namespace AILLE {
namespace V7 {

// ============================================================================
// 1. Dynamic Handle-Lock (Consensus Layer Optimization)
// ============================================================================

ConsensusLayer::ConsensusLayer(double base_threshold, double multiplier, double velocity_threshold) noexcept
    : base_confidence_threshold(base_threshold),
      min_confidence_threshold(base_threshold),
      breakout_multiplier(multiplier),
      breakout_velocity_threshold(velocity_threshold),
      window_index(0),
      window_count(0) {
    for (std::size_t i = 0; i < WINDOW; ++i) {
        volume_delta_window[i] = 0.0;
    }
}

void ConsensusLayer::update_confidence(double volume_delta) noexcept {
    // Add new sample to ring buffer
    volume_delta_window[window_index] = volume_delta;
    window_index = (window_index + 1) % WINDOW;
    if (window_count < WINDOW) {
        window_count++;
    }

    // Calculate average absolute delta
    double sum_abs_delta = 0.0;
    for (std::size_t i = 0; i < window_count; ++i) {
        sum_abs_delta += std::abs(volume_delta_window[i]);
    }

    double avg_abs_delta = (window_count > 0) ? (sum_abs_delta / window_count) : 0.0;

    // Check against threshold
    if (avg_abs_delta > breakout_velocity_threshold) {
        min_confidence_threshold = base_confidence_threshold * breakout_multiplier;
    } else {
        // Automatically revert when velocity normalizes
        min_confidence_threshold = base_confidence_threshold;
    }
}

bool ConsensusLayer::validate_signal(double consensus_score) const noexcept {
    return consensus_score >= min_confidence_threshold;
}

// ============================================================================
// 2. Golden-Ratio Volatility Governor (MSM Expansion)
// ============================================================================

MacroSignalGovernor::MacroSignalGovernor() noexcept
    : volatility_prev(0.0), volatility_curr(0.0) {}

void MacroSignalGovernor::update_volatility(double new_value) noexcept {
    volatility_prev = volatility_curr;
    volatility_curr = new_value;
}

void MacroSignalGovernor::apply_beta_scaling(std::vector<double>& weights) noexcept {
    constexpr double epsilon = 1e-6;
    double max_prev = (volatility_prev > epsilon) ? volatility_prev : epsilon;

    double roc = std::abs(volatility_curr - volatility_prev) / max_prev;

    if (roc > INV_GOLDEN_RATIO) {
        for (std::size_t i = 0; i < weights.size(); ++i) {
            weights[i] *= 0.5; // Scale down high-beta components
        }
    }
}

// ============================================================================
// 3. Zero-Allocation Telemetry Export (V6.3 Clean-Up)
// ============================================================================

TelemetryLog::TelemetryLog() noexcept {
    std::memset(buffer, 0, sizeof(buffer));
}

void TelemetryLog::write(const char* msg) noexcept {
    if (!msg) return;

    // Silently truncate to fit: Max 63 chars + '\0' in a 64-byte buffer.
    std::size_t len = 0;
    while (msg[len] != '\0' && len < sizeof(buffer) - 1) {
        buffer[len] = msg[len];
        ++len;
    }
    buffer[len] = '\0'; // Ensure null termination
}

void TelemetryLog::export_now() noexcept {
    // For now, keep it simple and demonstrative: export_now() writes to std::cout
    // Real lock-free queues would be used in a full production implementation.
    if (buffer[0] != '\0') {
        std::cout << "[TELEMETRY] " << buffer << "\n";
        // Clear buffer after export
        std::memset(buffer, 0, sizeof(buffer));
    }
}

// ============================================================================
// V7 Pipeline Demo Integration
// ============================================================================

void run_v7_pipeline_demo() noexcept {
    ConsensusLayer consensus;
    MacroSignalGovernor governor;
    TelemetryLog telemetry;

    telemetry.write("V7 Pipeline Starting");
    telemetry.export_now();

    // Setup initial weights
    std::vector<double> high_beta_weights = {1.0, 1.2, 0.9, 1.5};

    telemetry.write("Normal market conditions");
    telemetry.export_now();

    consensus.update_confidence(0.5); // Normal volume delta
    governor.update_volatility(0.1);  // Normal volatility
    governor.apply_beta_scaling(high_beta_weights);

    if (consensus.validate_signal(0.40)) { // 0.40 >= 0.35
        telemetry.write("Signal accepted (Score: 0.40)");
        telemetry.export_now();
    }

    // High volatility & breakout event
    telemetry.write("Breakout detected!");
    telemetry.export_now();

    // Volume spike > 1.5 avg
    consensus.update_confidence(5.0);

    // Volatility spike > 1/phi
    governor.update_volatility(0.3); // prev was 0.1, ROC = 0.2 / 0.1 = 2.0 > 0.618

    governor.apply_beta_scaling(high_beta_weights); // High beta weights will be halved

    if (!consensus.validate_signal(0.40)) { // 0.40 is now < 0.35 * 1.5 = 0.525
        telemetry.write("Signal rejected (Score: 0.40 < 0.525)");
        telemetry.export_now();
    }

    telemetry.write("V7 Pipeline Demo Complete");
    telemetry.export_now();
}

} // namespace V7
} // namespace AILLE
