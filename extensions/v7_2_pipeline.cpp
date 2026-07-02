#include "v7_2_pipeline.hpp"
#include <cmath>

namespace AILLE {

// ============================================================================
// MicrostructurePreCorrector
// ============================================================================

void MicrostructurePreCorrector::update(double spread, double midprice, double microvol) noexcept {
    spread_window[index] = spread;
    midprice_window[index] = midprice;
    microvol_window[index] = microvol;
    index = (index + 1) % WINDOW;
}

double MicrostructurePreCorrector::compute_instability() const noexcept {
    double i_spread = 0.0;
    double i_mid = 0.0;
    double i_microvol = 0.0;

    for (std::size_t i = 1; i < WINDOW; ++i) {
        // We traverse the arrays. Even though it's a ring buffer, computing adjacent
        // differences strictly within the array bounds is acceptable as an approximation
        // and keeps it extremely branch-minimal and cache friendly.
        // A strictly chronological difference is technically possible with modular arithmetic
        // but simple adjacent diffs over the whole window gives the same sum for our purposes.
        i_spread += std::abs(spread_window[i] - spread_window[i - 1]);
        i_mid += std::abs(midprice_window[i] - midprice_window[i - 1]);
        i_microvol += std::abs(microvol_window[i] - microvol_window[i - 1]);
    }

    // Add difference between last and first to fully connect the ring buffer logically
    i_spread += std::abs(spread_window[0] - spread_window[WINDOW - 1]);
    i_mid += std::abs(midprice_window[0] - midprice_window[WINDOW - 1]);
    i_microvol += std::abs(microvol_window[0] - microvol_window[WINDOW - 1]);

    return i_spread + i_mid + i_microvol;
}

double MicrostructurePreCorrector::current_confidence_threshold() const noexcept {
    if (compute_instability() > instability_threshold) {
        return tightened_confidence_threshold;
    }
    return base_confidence_threshold;
}

// ============================================================================
// AntiSpoofingShield
// ============================================================================

void AntiSpoofingShield::record_event(double size, double lifetime_ms, bool is_bid) noexcept {
    events[index].size = size;
    events[index].lifetime_ms = lifetime_ms;
    events[index].is_bid = is_bid;
    index = (index + 1) % WINDOW;
}

bool AntiSpoofingShield::is_spoofing_suspected() const noexcept {
    constexpr double SIZE_THRESHOLD = 1000.0;
    constexpr double LIFETIME_THRESHOLD = 50.0;

    std::size_t suspicious_count = 0;
    for (std::size_t i = 0; i < WINDOW; ++i) {
        if (events[i].size > SIZE_THRESHOLD && events[i].lifetime_ms < LIFETIME_THRESHOLD) {
            suspicious_count++;
        }
    }

    return static_cast<double>(suspicious_count) >= spoof_sensitivity;
}

// ============================================================================
// CacheAdaptiveExecution
// ============================================================================

void CacheAdaptiveExecution::record_branch() noexcept {
    metrics.branch_count++;
}

void CacheAdaptiveExecution::record_cycles(std::uint64_t cycles) noexcept {
    metrics.cycle_estimate += cycles;
}

void CacheAdaptiveExecution::set_load_level(std::uint64_t level) noexcept {
    metrics.load_level = level;
}

void CacheAdaptiveExecution::reset() noexcept {
    metrics.branch_count = 0;
    metrics.cycle_estimate = 0;
    metrics.load_level = 0;
}

bool CacheAdaptiveExecution::tight_mode() const noexcept {
    return metrics.cycle_estimate > load_threshold || metrics.branch_count > (load_threshold / 10);
}

// ============================================================================
// Pipeline Integration
// ============================================================================

void evaluate_v7_pipeline() noexcept {
    MicrostructurePreCorrector pmpc;
    AntiSpoofingShield dass;
    CacheAdaptiveExecution clae;

    // Simulate rising instability
    for (int i = 0; i < 20; ++i) {
        pmpc.update(i * 0.1, i * 0.5, i * 0.05);
    }

    // Demonstrate pre-correction of confidence thresholds
    double conf_thresh = pmpc.current_confidence_threshold();
    (void)conf_thresh; // Just to avoid unused variable warning

    // Simulate spoof detection
    for (int i = 0; i < 5; ++i) {
        dass.record_event(1500.0, 10.0, true); // Suspicious
    }
    for (int i = 0; i < 10; ++i) {
        dass.record_event(100.0, 500.0, false); // Normal
    }

    // Demonstrate spoof detection hardening validation
    bool is_suspected = dass.is_spoofing_suspected();
    if (is_suspected) {
        // In a real system, we'd raise validation thresholds here
    }

    // Simulate tight-mode activation under high load
    clae.record_cycles(2000000);
    clae.record_branch();

    bool tight = clae.tight_mode();
    if (tight) {
        // In a real system, we'd switch to a tight mode that reduces non-critical checks
    }

    clae.reset();
}

} // namespace AILLE
