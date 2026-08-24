#include "aille_spire.hpp"
#include "v7_3_pipeline.hpp"
#include "aille_pilgrimage.hpp"

namespace {
    // Static instances of the bell-tower modules, representing the deterministic state
    // Zero dynamic allocation, no exceptions
    AILLE::GlobalResonanceBeacon static_beacon;
    AILLE::HarmonicSync static_sync;
    AILLE::EchoDampener static_dampener;
    AILLE::AnomalyState static_anomaly_state;
    AILLE::AnomalyConfig static_anomaly_config;
    AILLE::WNFSFrame static_wnfs_frame;
    AILLE::WNFSState static_wnfs_state;
    AILLE::WNFSConfig static_wnfs_config;
    AILLE::UnifiedRuntimeState static_unified_state;
    AILLE::UnifiedRuntimeMetrics static_unified_metrics;
    AILLE::UnifiedRuntimeConfig static_unified_config;
}

namespace aillee_spire {

    double get_resonance_bell() noexcept {
        return static_beacon.bell();
    }

    double get_sync_tick() noexcept {
        return static_sync.tick();
    }

    double get_dampened_state() noexcept {
        return static_dampener.dampened();
    }

    AILLEE_Snapshot get_snapshot() noexcept {
        return {
            get_resonance_bell(),
            get_sync_tick(),
            get_dampened_state()
        };
    }

    Lantern get_lantern() noexcept {
        return compute_lantern();
    }

    aillee_crown_walk::CrownWalkView get_crown_walk() noexcept {
        return aillee_crown_walk::walk();
    }

    aillee_weathering::WeatheringReport get_weathering() noexcept {
        return aillee_weathering::evaluate();
    }

    aillee_pilgrimage::PilgrimageReport get_pilgrimage() noexcept {
        return aillee_pilgrimage::perform();
    }

    AILLE::AnomalyAdvisory get_anomaly_advisory() noexcept {
        return AILLE::evaluate_anomaly_advisory(static_anomaly_state, static_anomaly_config);
    }

    std::size_t get_chart_condition_payloads(
        const AILLE::AnomalyState& anomaly,
        const AILLE::VolumeState& volume,
        const AILLE::BaselineState& baseline,
        AILLE::ChartConditionPayload* outputs,
        std::size_t max_outputs
    ) noexcept {
        if (outputs == nullptr || max_outputs < 4) return 0;
        outputs[0] = AILLE::evaluate_volatility_expansion_bands(anomaly, volume, baseline);
        outputs[1] = AILLE::evaluate_liquidity_displacement_zones(anomaly, volume, baseline);
        outputs[2] = AILLE::evaluate_correlation_divergence_index(anomaly, volume, baseline);
        outputs[3] = AILLE::evaluate_baseline_strength_meter(anomaly, volume, baseline);
        return 4;
    }

    AILLE::WNFSAdvisory get_wnfs_advisory() noexcept {
        AILLE::WNFSState local_state = static_wnfs_state;
        return AILLE::evaluate_wnfs_advisory(static_wnfs_frame, local_state, static_wnfs_config);
    }

    AILLE::WNFSObservabilityMetrics get_wnfs_observability() noexcept {
        AILLE::WNFSObservabilityMetrics obs;
        obs.processed_frames = static_wnfs_state.processed_frames;
        obs.gap_count = static_wnfs_state.gap_count;
        obs.clone_status_mask = static_wnfs_state.clone_status_mask;
        obs.degraded_clone_count = static_wnfs_state.degraded_clone_count;
        obs.channel_status = static_wnfs_state.channel_status;
        obs.stream_degraded = (static_wnfs_state.channel_status != AILLE::WNFS_STATUS_HEALTHY) ? 1 : 0;
        obs.ingestion_confidence = (obs.stream_degraded != 0) ? 0.0f : 1.0f;
        obs.wave_energy_factor = static_wnfs_state.wave_amplitude;
        return obs;
    }

    AILLE::UnifiedRuntimeAdvisory get_unified_runtime_advisory() noexcept {
        AILLE::UnifiedRuntimeState state = static_unified_state;
        AILLE::UnifiedRuntimeMetrics metrics = static_unified_metrics;
        AILLE::WNFSState wnfs_state = static_wnfs_state;
        AILLE::WNFSAdvisory wnfs_adv = AILLE::evaluate_wnfs_advisory(static_wnfs_frame, wnfs_state, static_wnfs_config);
        AILLE::AnomalyAdvisory anomaly_adv = AILLE::evaluate_anomaly_advisory(static_anomaly_state, static_anomaly_config);
        return AILLE::evaluate_unified_runtime(
            state, metrics, &wnfs_adv, &anomaly_adv, nullptr, nullptr, nullptr, static_unified_config
        );
    }

    AILLE::UnifiedRuntimeMetrics get_unified_runtime_metrics() noexcept {
        return static_unified_metrics;
    }

} // namespace aillee_spire
