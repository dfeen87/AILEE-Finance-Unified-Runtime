/*
 * AILEE Framework - Layer 19 Unified Cohesive Runtime & Resiliency Engine Benchmark & Demo
 * Master Runtime Orchestration Simulation
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
#include "../extensions/aille_unified_runtime.hpp"
#include "../extensions/aille_spire.hpp"

int main() {
    std::cout << "=================================================================\n";
    std::cout << "  AILEE Unified Finance Runtime (v17.0.0)\n";
    std::cout << "  Layer 19 — Unified Cohesive Runtime & Resiliency Engine Benchmark\n";
    std::cout << "=================================================================\n\n";

    AILLE::UnifiedRuntimeState state;
    AILLE::UnifiedRuntimeMetrics metrics;
    AILLE::UnifiedRuntimeConfig config;

    constexpr std::size_t NUM_CYCLES = 250000;
    std::vector<uint64_t> latencies_ns;
    latencies_ns.reserve(NUM_CYCLES);

    std::cout << "[1] Benchmarking Master Execution Cycle (" << NUM_CYCLES << " cycles)...\n";

    for (std::size_t i = 1; i <= NUM_CYCLES; ++i) {
        auto t_start = std::chrono::high_resolution_clock::now();

        AILLE::UnifiedRuntimeAdvisory advisory = AILLE::evaluate_unified_runtime(
            state, metrics, nullptr, nullptr, nullptr, nullptr, nullptr, config
        );
        (void)advisory;

        auto t_end = std::chrono::high_resolution_clock::now();
        uint64_t dur = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
        latencies_ns.push_back(dur);
    }

    std::sort(latencies_ns.begin(), latencies_ns.end());

    double sum = std::accumulate(latencies_ns.begin(), latencies_ns.end(), 0.0);
    double mean_ns = sum / NUM_CYCLES;
    double p50_ns = latencies_ns[NUM_CYCLES * 0.50];
    double p99_ns = latencies_ns[NUM_CYCLES * 0.99];

    std::cout << "\n[2] Master Cycle Latency Benchmarks:\n";
    std::cout << "    - Processed Cycles : " << NUM_CYCLES << "\n";
    std::cout << "    - Mean Latency     : " << std::fixed << std::setprecision(2) << mean_ns << " ns\n";
    std::cout << "    - p50 Latency      : " << p50_ns << " ns (SLA Target: < 350 ns)\n";
    std::cout << "    - p99 Latency      : " << p99_ns << " ns (SLA Target: < 900 ns)\n\n";

    std::cout << "[3] Testing Fault Escalation into Stress Override & Lock...\n";
    AILLE::WNFSAdvisory degraded_stream;
    degraded_stream.stream_degraded = 1;
    degraded_stream.trigger_stress_escalation = 1;

    AILLE::UnifiedRuntimeAdvisory fault_adv = AILLE::evaluate_unified_runtime(
        state, metrics, &degraded_stream, nullptr, nullptr, nullptr, nullptr, config
    );

    std::cout << "    - System Status              : " << static_cast<int>(state.system_status) << " (2 = STRESS_OVERRIDE)\n";
    std::cout << "    - Resiliency Mode            : " << static_cast<int>(state.resiliency_mode) << " (2 = FAIL_CLOSED)\n";
    std::cout << "    - Fault Escalated            : " << (state.fault_escalated ? "YES" : "NO") << "\n";
    std::cout << "    - Execution Scale            : " << fault_adv.recommended_execution_scale << "\n";
    std::cout << "    - HFT Freeze Active          : " << (fault_adv.hft_freeze_active ? "YES" : "NO") << "\n\n";

    std::cout << "[4] Spire Interface Master Advisory Verification:\n";
    AILLE::UnifiedRuntimeAdvisory spire_adv = aillee_spire::get_unified_runtime_advisory();
    std::cout << "    - Spire System Status        : " << static_cast<int>(spire_adv.system_status) << "\n";
    std::cout << "    - Spire Execution Permitted  : " << (spire_adv.execution_permitted ? "YES" : "NO") << "\n\n";

    std::cout << "=================================================================\n";
    std::cout << "  Layer 19 Benchmark & Demonstration Completed Successfully!\n";
    std::cout << "=================================================================\n";

    return 0;
}
