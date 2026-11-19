/*
 * AILLE Framework - Sync Adapter Extension Implementation
 * AI-Load Integrity and Layered Evaluation
 *
 * Deterministic, allocator-free temporal clock synchronization adapter binding
 * AILEE Finance runtime to AILEE Runtime Protocol authoritative clock.
 *
 * Version Tag: SYNC_ADAPTER_V1
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#include "aille_sync_adapter.hpp"
#include <chrono>
#include <limits>
#include <ratio>
#include <type_traits>

namespace AILLE {

namespace {

// Check the signed nanosecond domain before scaling a wall-clock count.
// Integral clock units (including ns, microseconds, and 100 ns) retain exact
// precision at the boundary without an overflowing duration_cast.
template <typename Duration>
constexpr std::uint64_t checked_epoch_ns(Duration elapsed) noexcept {
    using Scale = std::ratio_divide<typename Duration::period, std::nano>;
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto ticks = elapsed.count();
    if constexpr (std::is_integral_v<typename Duration::rep> && Scale::den == 1) {
        if (ticks <= 0 || ticks > maximum / Scale::num) {
            return 0;
        }
        return static_cast<std::uint64_t>(ticks) * static_cast<std::uint64_t>(Scale::num);
    } else if constexpr (std::is_integral_v<typename Duration::rep> && Scale::num == 1) {
        const auto ns = ticks / Scale::den;
        return ns > 0 && ns <= maximum ? static_cast<std::uint64_t>(ns) : 0;
    } else {
        // For other clock representations, a checked wide conversion avoids
        // overflowing a signed integer intermediate.
        using WideNanoseconds = std::chrono::duration<long double, std::nano>;
        constexpr long double limit = static_cast<long double>(maximum) + 1.0L;
        const auto ns = WideNanoseconds(elapsed).count();
        return ns >= 1.0L && ns < limit ? static_cast<std::uint64_t>(ns) : 0;
    }
}

static_assert(checked_epoch_ns(std::chrono::nanoseconds{-1}) == 0);
static_assert(checked_epoch_ns(std::chrono::nanoseconds{0}) == 0);
static_assert(checked_epoch_ns(std::chrono::nanoseconds{1}) == 1);
static_assert(checked_epoch_ns(std::chrono::nanoseconds{
    std::numeric_limits<std::int64_t>::max()}) ==
    static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()));
static_assert(checked_epoch_ns(std::chrono::microseconds{1}) == 1000);
static_assert(checked_epoch_ns(std::chrono::microseconds{
    std::numeric_limits<std::int64_t>::max() / 1000 + 1}) == 0);
static_assert(checked_epoch_ns(std::chrono::seconds{
    std::numeric_limits<std::int64_t>::max() / 1000000000 + 1}) == 0);
static_assert(checked_epoch_ns(std::chrono::duration<std::int64_t, std::pico>{999}) == 0);
static_assert(checked_epoch_ns(std::chrono::duration<std::int64_t, std::pico>{1000}) == 1);

} // namespace

SyncAdapter::SyncAdapter() noexcept
    : state_(), config_(), metrics_(), current_tick_() {
    reset();
}

SyncAdapter::SyncAdapter(const SyncAdapterConfig& config) noexcept
    : state_(), config_(config), metrics_(), current_tick_() {
    reset();
}

void SyncAdapter::reset() noexcept {
    state_ = SyncAdapterState();
    metrics_ = SyncAdapterObservabilityMetrics();
    current_tick_ = SyncTick();
    state_.expected_cadence_ns = config_.target_cadence_ns;
}

SyncTick SyncAdapter::ingest_protocol_clock(
    std::uint64_t tick_index,
    std::uint64_t timestamp_ns,
    float wave_phase,
    float confidence
) noexcept {
    const auto max_signed = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (config_.target_cadence_ns == 0 || config_.target_cadence_ns > max_signed / 2 ||
        config_.max_drift_threshold_ns > max_signed / 2 ||
        !std::isfinite(config_.min_confidence_threshold) || config_.min_confidence_threshold < 0 || config_.min_confidence_threshold > 1 ||
        !std::isfinite(wave_phase) || wave_phase < 0 || wave_phase > 6.283185307179586f ||
        !std::isfinite(confidence) || confidence < 0 || confidence > 1 || tick_index == 0 || timestamp_ns == 0 || timestamp_ns > max_signed ||
        (metrics_.total_ticks_processed && (tick_index <= state_.current_tick_index || timestamp_ns <= state_.last_timestamp_ns))) {
        SyncTick rejected;
        rejected.tick_index = state_.current_tick_index;
        rejected.timestamp_ns = state_.last_timestamp_ns;
        rejected.confidence = 0;
        rejected.degraded = rejected.escalate_stress = rejected.escalate_meta_lock = 1;
        state_.sync_degraded = metrics_.sync_degraded = 1;
        state_.clock_confidence = metrics_.current_confidence = 0;
        current_tick_ = rejected;
        return rejected;
    }
    SyncTick tick;
    tick.tick_index = tick_index;
    tick.timestamp_ns = timestamp_ns;
    tick.tick_cadence_ns = config_.target_cadence_ns;
    tick.wave_phase = wave_phase;
    tick.confidence = std::clamp(confidence, 0.0f, 1.0f);
    tick.alignment_flags = SYNC_FLAG_ALIGNED | SYNC_FLAG_EXTERNAL;

    // Calculate drift relative to expected cadence
    if (state_.last_timestamp_ns > 0 && timestamp_ns > state_.last_timestamp_ns) {
        const std::uint64_t elapsed_ns = timestamp_ns - state_.last_timestamp_ns;
        tick.drift_ns = static_cast<std::int64_t>(elapsed_ns) - static_cast<std::int64_t>(config_.target_cadence_ns);
    } else {
        tick.drift_ns = 0;
    }

    const std::uint64_t abs_drift = static_cast<std::uint64_t>(std::abs(tick.drift_ns));
    if (abs_drift > static_cast<std::uint64_t>(state_.max_observed_drift_ns)) {
        state_.max_observed_drift_ns = static_cast<std::int64_t>(abs_drift);
    }

    // Check gap
    if (state_.current_tick_index > 0 && tick_index > (state_.current_tick_index + 1)) {
        tick.alignment_flags |= SYNC_FLAG_GAP_DETECTED;
        tick.degraded = 1;
        tick.confidence *= 0.8f;
    }

    // Check drift threshold breach
    if (abs_drift > config_.max_drift_threshold_ns) {
        tick.alignment_flags |= SYNC_FLAG_DRIFT_WARN;
        tick.degraded = 1;
        tick.confidence *= 0.85f;
    }

    if (tick.confidence < config_.min_confidence_threshold) {
        tick.degraded = 1;
    }

    // Escalation logic
    if (tick.degraded) {
        if (config_.auto_escalate_stress && (tick.confidence < config_.min_confidence_threshold || abs_drift > config_.max_drift_threshold_ns)) {
            tick.escalate_stress = 1;
            metrics_.stress_escalations++;
        }
        if (config_.auto_escalate_meta_lock && (tick.confidence < 0.50f || abs_drift > (config_.max_drift_threshold_ns * 2))) {
            tick.escalate_meta_lock = 1;
            metrics_.meta_lock_escalations++;
        }
    }

    // Update state
    state_.current_tick_index = tick_index;
    state_.last_timestamp_ns = timestamp_ns;
    state_.current_wave_phase = wave_phase;
    state_.clock_confidence = tick.confidence;
    state_.external_clock_active = 1;
    state_.sync_degraded = tick.degraded;

    // Update metrics
    metrics_.total_ticks_processed++;
    metrics_.external_sync_ticks++;
    metrics_.last_drift_ns = tick.drift_ns;
    metrics_.current_confidence = tick.confidence;
    metrics_.external_clock_active = 1;
    metrics_.sync_degraded = tick.degraded;

    current_tick_ = tick;
    return current_tick_;
}

SyncTick SyncAdapter::advance_tick() noexcept {
    const auto reject_tick = [this]() noexcept {
        current_tick_ = SyncTick();
        current_tick_.confidence = 0;
        current_tick_.degraded = current_tick_.escalate_stress = current_tick_.escalate_meta_lock = 1;
        return current_tick_;
    };
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    if (config_.target_cadence_ns == 0 || state_.current_tick_index == maximum ||
        state_.last_timestamp_ns > maximum - config_.target_cadence_ns || !std::isfinite(state_.current_wave_phase) ||
        !std::isfinite(config_.min_confidence_threshold) || config_.min_confidence_threshold < 0 || config_.min_confidence_threshold > 1) {
        return reject_tick();
    }
    SyncTick tick;
    tick.tick_index = state_.current_tick_index + 1;

    if (state_.last_timestamp_ns > 0) {
        tick.timestamp_ns = state_.last_timestamp_ns + config_.target_cadence_ns;
    } else {
        // Seed the standalone clock in the Unix epoch, not an unspecified
        // high-resolution clock epoch. Later ticks retain deterministic cadence.
        tick.timestamp_ns = checked_epoch_ns(
            std::chrono::system_clock::now().time_since_epoch());
        if (tick.timestamp_ns == 0) {
            return reject_tick();
        }
    }

    tick.drift_ns = 0; // Standalone zero jitter cadence
    tick.tick_cadence_ns = config_.target_cadence_ns;

    // Advance deterministic wave phase (0 to 2pi)
    constexpr float TWO_PI = 6.283185307179586f;
    float next_phase = state_.current_wave_phase + 0.10f;
    if (next_phase >= TWO_PI) {
        next_phase -= TWO_PI;
    }
    tick.wave_phase = next_phase;
    tick.confidence = 1.0f;
    tick.alignment_flags = SYNC_FLAG_ALIGNED | SYNC_FLAG_FALLBACK;
    tick.degraded = 0;
    tick.escalate_stress = 0;
    tick.escalate_meta_lock = 0;

    // Update state
    state_.current_tick_index = tick.tick_index;
    state_.last_timestamp_ns = tick.timestamp_ns;
    state_.current_wave_phase = tick.wave_phase;
    state_.clock_confidence = 1.0f;
    state_.external_clock_active = 0;
    state_.sync_degraded = 0;

    // Update metrics
    metrics_.total_ticks_processed++;
    metrics_.fallback_ticks++;
    metrics_.last_drift_ns = 0;
    metrics_.current_confidence = 1.0f;
    metrics_.external_clock_active = 0;
    metrics_.sync_degraded = 0;

    current_tick_ = tick;
    return current_tick_;
}

} // namespace AILLE
