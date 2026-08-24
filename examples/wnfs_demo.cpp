/*
 * AILEE Framework - WNFS Demonstration & Sub-Microsecond Benchmark
 * WaveNativeFinanceStream (WNFS) Layer 18 Simulation
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <numeric>
#include <algorithm>
#include "../extensions/aille_wnfs.hpp"
#include "../extensions/aille_spire.hpp"

int main() {
    std::cout << "=================================================================\n";
    std::cout << "  AILEE Unified Finance Runtime (V15+)\n";
    std::cout << "  Layer 18 — WaveNativeFinanceStream (WNFS) Benchmark & Demo\n";
    std::cout << "=================================================================\n\n";

    AILLE::WNFSChannel channel;
    AILLE::WNFSState state;
    AILLE::WNFSConfig config;

    constexpr std::size_t NUM_TICKS = 250000;
    std::vector<uint64_t> latencies_ns;
    latencies_ns.reserve(NUM_TICKS);

    std::cout << "[1] Warmup and Ingestion Simulation (" << NUM_TICKS << " micro-ticks)...\n";

    for (std::size_t i = 1; i <= NUM_TICKS; ++i) {
        AILLE::WNFSFrame frame;
        frame.sequence_id = i;
        frame.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        frame.bid_price = 450.00f + (i % 10) * 0.01f;
        frame.ask_price = 450.05f + (i % 10) * 0.01f;
        frame.bid_size = 100.0f;
        frame.ask_size = 120.0f;
        frame.last_price = 450.02f;
        frame.last_size = 15.0f;
        frame.vwap_delta = 0.001f;
        frame.symbol_id = 1; // SPY
        frame.wave_channel_id = 1;

        auto t_start = std::chrono::high_resolution_clock::now();

        // Push frame to lock-free wave channel
        bool pushed = channel.push_frame(frame);
        if (pushed) {
            AILLE::WNFSFrame popped_frame;
            bool popped = channel.pop_frame(popped_frame);
            if (popped) {
                AILLE::WNFSAdvisory advisory = AILLE::evaluate_wnfs_advisory(popped_frame, state, config);
                (void)advisory;
            }
        }

        auto t_end = std::chrono::high_resolution_clock::now();
        uint64_t dur = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
        latencies_ns.push_back(dur);
    }

    std::sort(latencies_ns.begin(), latencies_ns.end());

    double sum = std::accumulate(latencies_ns.begin(), latencies_ns.end(), 0.0);
    double mean_ns = sum / NUM_TICKS;
    double p50_ns = latencies_ns[NUM_TICKS * 0.50];
    double p99_ns = latencies_ns[NUM_TICKS * 0.99];
    double p99_9_ns = latencies_ns[NUM_TICKS * 0.999];

    std::cout << "\n[2] Ingestion Latency Benchmarks:\n";
    std::cout << "    - Processed Ticks : " << NUM_TICKS << "\n";
    std::cout << "    - Mean Latency    : " << std::fixed << std::setprecision(2) << mean_ns << " ns\n";
    std::cout << "    - p50 Latency     : " << p50_ns << " ns (SLA Target: < 350 ns)\n";
    std::cout << "    - p99 Latency     : " << p99_ns << " ns (SLA Target: < 900 ns)\n";
    std::cout << "    - p99.9 Tail      : " << p99_9_ns << " ns (SLA Target: < 5000 ns)\n\n";

    std::cout << "[3] Testing Sequence Gap & Fail-Closed Escalation...\n";
    AILLE::WNFSFrame gap_frame;
    gap_frame.sequence_id = state.expected_sequence + 10; // Trigger gap of 10
    gap_frame.symbol_id = 1;
    gap_frame.wave_channel_id = 1;

    AILLE::WNFSAdvisory gap_advisory = AILLE::evaluate_wnfs_advisory(gap_frame, state, config);

    std::cout << "    - Gap Count Detected         : " << state.gap_count << "\n";
    std::cout << "    - Channel Status             : " << static_cast<int>(state.channel_status) << " (1 = DEGRADED)\n";
    std::cout << "    - Stream Degraded            : " << (gap_advisory.stream_degraded ? "YES" : "NO") << "\n";
    std::cout << "    - HFT Freeze Required        : " << (gap_advisory.hft_freeze_required ? "YES" : "NO") << "\n";
    std::cout << "    - Trigger Stress Escalation  : " << (gap_advisory.trigger_stress_escalation ? "YES" : "NO") << "\n\n";

    AILLE::WNFSAdvisory spire_adv = aillee_spire::get_wnfs_advisory();
    std::cout << "[4] Spire Interface Verification:\n";
    std::cout << "    - Spire Ingestion Confidence : " << spire_adv.ingestion_confidence << "\n\n";

    std::cout << "=================================================================\n";
    std::cout << "  WNFS Benchmark & Demonstration Completed Successfully!\n";
    std::cout << "=================================================================\n";

    return 0;
}
