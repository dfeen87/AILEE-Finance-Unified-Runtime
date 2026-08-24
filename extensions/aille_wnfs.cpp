/*
 * AILLE Framework - WaveNativeFinanceStream (WNFS) Layer 18 Implementation
 * AI-Load Integrity and Layered Evaluation
 *
 * Deterministic, allocator-free real-time streaming data ingestion and wave synchronization transport.
 *
 * Version Tag: WAVE_NATIVE_FINANCE_STREAM_V1
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#include "aille_wnfs.hpp"
#include "aille_stress_regime_override.hpp"

namespace AILLE {

[[nodiscard]] WNFSAdvisory evaluate_wnfs_advisory(
    const WNFSFrame& frame,
    WNFSState& state,
    const WNFSConfig& config,
    const SafetyState* safety
) noexcept {
    WNFSAdvisory advisory;
    advisory.channel_id = frame.wave_channel_id;

    // Check hardware safety or global kill-switch
    if (safety && (safety->kill_switch || safety->hardware_fault)) {
        state.channel_status = WNFS_STATUS_LOCKED;
        advisory.ingestion_confidence = 0.0f;
        advisory.wave_energy_factor = 0.0f;
        advisory.stream_degraded = 1;
        advisory.trigger_stress_escalation = 1;
        advisory.hft_freeze_required = 1;
        return advisory;
    }

    // Check multi-clone cluster consensus bitmask
    if (state.clone_status_mask != 0 || state.degraded_clone_count > 0) {
        state.channel_status = WNFS_STATUS_DEGRADED;
        advisory.stream_degraded = 1;
        advisory.hft_freeze_required = 1;
        advisory.ingestion_confidence = 0.0f;
        advisory.wave_energy_factor = 0.0f;
        advisory.tick_acceleration = 0.0f;
        advisory.trigger_stress_escalation = 1;
        return advisory;
    }

    // Monotonic sequence and out-of-order tick validation
    bool is_out_of_order = false;
    if (state.processed_frames > 0) {
        if (frame.sequence_id > state.expected_sequence) {
            std::uint64_t gaps = frame.sequence_id - state.expected_sequence;
            state.gap_count += gaps;
            state.channel_status = WNFS_STATUS_DEGRADED;
        } else if (frame.sequence_id < state.expected_sequence) {
            // Out-of-order tick rejection
            state.channel_status = WNFS_STATUS_DEGRADED;
            is_out_of_order = true;
        }
    }

    if (!is_out_of_order) {
        state.expected_sequence = frame.sequence_id + 1;
        state.processed_frames++;
    }

    // Calculate Wave Phase & Wave Amplitude (Impulse Energy)
    constexpr float TWO_PI = 6.283185307179586f;
    state.wave_phase = std::fmod(static_cast<float>(frame.sequence_id) * 0.1f, TWO_PI);

    float price_spread = std::abs(frame.ask_price - frame.bid_price);
    (void)price_spread;
    float depth_sum = frame.bid_size + frame.ask_size;
    state.wave_amplitude = (depth_sum > 0.0f) ? (frame.last_size / depth_sum) : 1.0f;

    // Evaluate degradation status, out-of-order rejections & escalation flags
    if (is_out_of_order ||
        (frame.frame_flags & (WNFS_FLAG_GAP | WNFS_FLAG_CORRUPTED | WNFS_FLAG_OUT_OF_ORDER)) ||
        state.channel_status != WNFS_STATUS_HEALTHY) {
        advisory.stream_degraded = 1;
        advisory.hft_freeze_required = 1;

        if (state.gap_count >= config.max_sequence_gaps || (frame.frame_flags & WNFS_FLAG_CORRUPTED)) {
            state.channel_status = WNFS_STATUS_CORRUPTED;
            advisory.trigger_stress_escalation = 1;
            advisory.ingestion_confidence = 0.0f;
            advisory.wave_energy_factor = 0.0f;
        } else {
            advisory.ingestion_confidence = is_out_of_order ? 0.0f : 0.5f;
            advisory.wave_energy_factor = is_out_of_order ? 0.0f : state.wave_amplitude;
        }
    } else {
        advisory.ingestion_confidence = 1.0f;
        advisory.wave_energy_factor = state.wave_amplitude;
        advisory.stream_degraded = 0;
        advisory.hft_freeze_required = 0;
        advisory.trigger_stress_escalation = 0;
    }

    advisory.tick_acceleration = frame.vwap_delta;

    return advisory;
}

void AILLEEngine::evaluate_wnfs_advisory() {
    if (!wnfs_state_ || !wnfs_advisory_) return;
    WNFSConfig default_cfg;
    const WNFSConfig& cfg = wnfs_config_ ? *wnfs_config_ : default_cfg;

    // Create dummy frame matching current state expectation for periodic polling
    WNFSFrame poll_frame;
    poll_frame.sequence_id = wnfs_state_->expected_sequence;
    poll_frame.wave_channel_id = static_cast<std::uint8_t>(wnfs_state_->symbol_id);

    WNFSState mutable_state = *wnfs_state_;
    *wnfs_advisory_ = ::AILLE::evaluate_wnfs_advisory(poll_frame, mutable_state, cfg, safety_state_);

    // Direct Governance Escalation Mapping (Layers 12, 13, 14)
    if (wnfs_advisory_->trigger_stress_escalation || wnfs_advisory_->stream_degraded) {
        set_normal_safety_failed(true);
        if (stress_state_) {
            const_cast<StressPortfolioState*>(stress_state_)->stress_level = static_cast<uint8_t>(StressMode::CRISIS);
        }
    }
}

} // namespace AILLE
