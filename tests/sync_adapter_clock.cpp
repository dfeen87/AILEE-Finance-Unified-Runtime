// Copyright (c) Don Michael Feeney Jr.
// Licensed under the MIT License.
// Standalone clock regression tests; no execution runtime or transport is used.

#include "extensions/aille_sync_adapter.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::uint64_t wall_clock_ns() {
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    require(elapsed > 0, "test requires a positive system wall clock");
    return static_cast<std::uint64_t>(elapsed);
}

void check_initial_seed(AILLE::SyncAdapter& adapter) {
    const auto before = wall_clock_ns();
    const auto tick = adapter.advance_tick();
    const auto after = wall_clock_ns();
    require(tick.tick_index == 1, "initial fallback tick index changed");
    require(tick.timestamp_ns >= std::min(before, after) &&
                tick.timestamp_ns <= std::max(before, after),
            "initial fallback timestamp is not in the system-clock epoch");
    require(tick.alignment_flags == (AILLE::SYNC_FLAG_ALIGNED | AILLE::SYNC_FLAG_FALLBACK),
            "initial fallback flags changed");
    require(tick.degraded == 0 && tick.confidence == 1.0f,
            "valid wall-clock seed unexpectedly degraded");
    require(adapter.get_state().last_timestamp_ns == tick.timestamp_ns,
            "clock seed was not retained in the adapter");
}

void test_epoch() {
    AILLE::SyncAdapter adapter;
    check_initial_seed(adapter);
    adapter.advance_tick();
    adapter.reset();
    check_initial_seed(adapter);
}

void test_cadence() {
    AILLE::SyncAdapterConfig config;
    config.target_cadence_ns = 1;
    AILLE::SyncAdapter adapter(config);
    const auto first = adapter.advance_tick();
    const auto second = adapter.advance_tick();
    const auto third = adapter.advance_tick();
    require(second.tick_index == 2 && third.tick_index == 3,
            "standalone tick ordering changed");
    require(second.timestamp_ns == first.timestamp_ns + 1 &&
                third.timestamp_ns == second.timestamp_ns + 1,
            "standalone cadence was not advanced from the retained timestamp");
    require(second.drift_ns == 0 && third.drift_ns == 0 &&
                third.tick_cadence_ns == config.target_cadence_ns,
            "standalone cadence or drift changed");
    const auto metrics = adapter.get_metrics();
    require(metrics.total_ticks_processed == 3 && metrics.fallback_ticks == 3 &&
                metrics.external_sync_ticks == 0,
            "fallback observability changed");
}

void test_external() {
    AILLE::SyncAdapterConfig config;
    config.target_cadence_ns = 17;
    AILLE::SyncAdapter adapter(config);
    constexpr std::uint64_t synthetic_timestamp = 42;
    const auto authoritative = adapter.ingest_protocol_clock(
        1, synthetic_timestamp, 0.0f);
    require(authoritative.timestamp_ns == synthetic_timestamp &&
                (authoritative.alignment_flags & AILLE::SYNC_FLAG_EXTERNAL) != 0,
            "authoritative clock input changed");
    const auto fallback = adapter.advance_tick();
    require(fallback.tick_index == 2 &&
                fallback.timestamp_ns == synthetic_timestamp + config.target_cadence_ns &&
                fallback.drift_ns == 0,
            "fallback did not continue the authoritative timestamp");
    require(fallback.alignment_flags == (AILLE::SYNC_FLAG_ALIGNED | AILLE::SYNC_FLAG_FALLBACK),
            "external-to-fallback flags changed");
    const auto metrics = adapter.get_metrics();
    require(metrics.external_sync_ticks == 1 && metrics.fallback_ticks == 1 &&
                metrics.external_clock_active == 0,
            "external-to-fallback observability changed");
}

void require_rejected(const AILLE::SyncTick& tick) {
    require(tick.timestamp_ns == 0 && tick.confidence == 0.0f && tick.degraded &&
                tick.escalate_stress && tick.escalate_meta_lock,
            "invalid fallback failed to retain its rejection contract");
}

void test_rejection() {
    AILLE::SyncAdapterConfig config;
    config.target_cadence_ns = 0;
    AILLE::SyncAdapter invalid(config);
    require_rejected(invalid.advance_tick());
    require(invalid.get_state().current_tick_index == 0 &&
                invalid.get_metrics().total_ticks_processed == 0,
            "invalid configuration advanced the clock");

    AILLE::SyncAdapter overflow;
    constexpr auto max_signed = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    overflow.ingest_protocol_clock(1, max_signed, 0.0f);
    const auto before = overflow.get_state();
    config.target_cadence_ns = std::numeric_limits<std::uint64_t>::max();
    overflow.set_config(config);
    require_rejected(overflow.advance_tick());
    require(overflow.get_state().last_timestamp_ns == before.last_timestamp_ns &&
                overflow.get_state().current_tick_index == before.current_tick_index &&
                overflow.get_metrics().total_ticks_processed == 1,
            "overflowing cadence mutated accepted clock state");
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    try {
        const std::string_view test(argv[1]);
        if (test == "epoch") {
            test_epoch();
        } else if (test == "cadence") {
            test_cadence();
        } else if (test == "external") {
            test_external();
        } else if (test == "rejection") {
            test_rejection();
        } else {
            return EXIT_FAILURE;
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
