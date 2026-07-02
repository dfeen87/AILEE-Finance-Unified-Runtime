#include "v7_3_pipeline.hpp"
#include "v7_2_pipeline.hpp"
#include <cmath>
#include <algorithm>

namespace AILLE {

// ============================================================================
// Global Resonance Beacon (GRB)
// ============================================================================

void GlobalResonanceBeacon::update(double c, double v, double m, double s, double cache) noexcept {
    state.consensus_stability = c;
    state.volatility_stability = v;
    state.microstructure_stability = m;
    state.spoof_integrity = s;
    state.cache_mode_factor = cache;
}

double GlobalResonanceBeacon::compute_resonance() noexcept {
    // Weighted average with equal weights as specified
    state.resonance_index = 0.2 * (state.consensus_stability +
                                   state.volatility_stability +
                                   state.microstructure_stability +
                                   state.spoof_integrity +
                                   state.cache_mode_factor);
    return state.resonance_index;
}

double GlobalResonanceBeacon::bell() const noexcept {
    return state.resonance_index;
}

// ============================================================================
// Market-Wide Harmonic Sync (MWHS)
// ============================================================================

void HarmonicSync::update(double timing_delta) noexcept {
    timing_window[index] = std::abs(timing_delta);
    index = (index + 1) % WINDOW;
}

double HarmonicSync::compute_sync() noexcept {
    double sum = 0.0;
    for (std::size_t i = 0; i < WINDOW; ++i) {
        sum += timing_window[i];
    }
    sync_factor = sum / static_cast<double>(WINDOW);
    sync_factor = std::min(1.0, sync_factor);
    return sync_factor;
}

double HarmonicSync::tick() const noexcept {
    return sync_factor;
}

// ============================================================================
// Deterministic Echo Dampener (DED)
// ============================================================================

void EchoDampener::update(double vol, double spread, double liq) noexcept {
    constexpr double VOL_THRESHOLD = 0.5;
    constexpr double SPREAD_THRESHOLD = 0.2;
    constexpr double LIQ_THRESHOLD = 0.3;

    double diff_vol = std::abs(vol - last_vol);
    double diff_spread = std::abs(spread - last_spread);
    double diff_liq = std::abs(liq - last_liquidity);

    if (diff_vol > VOL_THRESHOLD || diff_spread > SPREAD_THRESHOLD || diff_liq > LIQ_THRESHOLD) {
        dampening_factor = 0.7;
    } else {
        dampening_factor = 1.0;
    }

    last_vol = vol;
    last_spread = spread;
    last_liquidity = liq;
}

double EchoDampener::dampened() const noexcept {
    return dampening_factor * ((last_vol + last_spread + last_liquidity) / 3.0);
}

// ============================================================================
// Pipeline Integration
// ============================================================================

void evaluate_v7_3_pipeline() noexcept {
    // V7.2 components
    MicrostructurePreCorrector pmpc;
    AntiSpoofingShield dass;
    CacheAdaptiveExecution clae;

    // V7.3 components
    GlobalResonanceBeacon beacon;
    HarmonicSync sync;
    EchoDampener dampener;

    // 1. Simulate deterministic mock data for V7.2 components
    for (int i = 0; i < 20; ++i) {
        pmpc.update(i * 0.01, i * 0.05, i * 0.005);
    }
    for (int i = 0; i < 5; ++i) {
        dass.record_event(1500.0, 10.0, true); // Suspicious
    }
    clae.record_cycles(500000);
    clae.record_branch();

    // Collect stability metrics from V7.2 modules
    double micro_stability = 1.0 - std::min(1.0, pmpc.compute_instability());
    double spoof_status = dass.is_spoofing_suspected() ? 0.0 : 1.0;
    double cache_status = clae.tight_mode() ? 0.0 : 1.0;

    // Dummy values for the remaining metrics
    double consensus_stab = 0.8;
    double vol_stab = 0.9;

    // 2. Feed metrics into V7.3 modules

    // GRB
    beacon.update(consensus_stab, vol_stab, micro_stability, spoof_status, cache_status);
    beacon.compute_resonance();

    // MWHS
    for (int i = 0; i < 16; ++i) {
        sync.update(0.1 + (i * 0.05)); // mock timing deltas
    }
    sync.compute_sync();

    // DED
    dampener.update(0.6, 0.3, 0.4); // This might trigger an echo event since default lasts are 0.0

    // 3. Produce AILLEE's first external output
    double resonance_bell = beacon.bell();
    double harmonic_tick = sync.tick();
    double dampened_val = dampener.dampened();

    // Prevent unused variable warnings
    (void)resonance_bell;
    (void)harmonic_tick;
    (void)dampened_val;
}

} // namespace AILLE
