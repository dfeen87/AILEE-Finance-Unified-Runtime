/*
 * AILLE Framework - Benchmark Harness
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 *
 * Measures decision throughput for the core engine using
 * deterministic synthetic signals.
 */

#include "aille.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <vector>
#include <algorithm>

#ifdef __x86_64__
#include <x86intrin.h>
#endif

// Helper to get raw clock cycles for microbenchmarking fast paths
inline uint64_t get_cycles() {
#ifdef __x86_64__
    unsigned int dummy;
    return __rdtscp(&dummy);
#else
    // Fallback if not x86
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
#endif
}

int main(int argc, char** argv) {
    std::size_t iterations = 200000;
    if (argc > 1) {
        try {
            iterations = static_cast<std::size_t>(std::stoull(argv[1]));
        } catch (...) {
            std::cerr << "Invalid iteration count: " << argv[1] << "\n";
            return 1;
        }
    }

    AILLE::AILLEConfig config;
    config.min_confidence_threshold = 0.35f;
    config.min_models_required = 2;

    AILLE::AILLEEngine engine(config);

    std::mt19937 rng(7);
    std::normal_distribution<float> dist(0.0f, 0.02f);

    std::vector<AILLE::ModelSignal> signals;
    signals.reserve(3);

    // Determine clock frequency via steady_clock vs __rdtscp
    std::cout << "Calibrating cycle counter..." << std::endl;
    auto calib_start = std::chrono::steady_clock::now();
    uint64_t cycles_start = get_cycles();
    while (std::chrono::steady_clock::now() - calib_start < std::chrono::milliseconds(10)) {}
    auto calib_end = std::chrono::steady_clock::now();
    uint64_t cycles_end = get_cycles();

    double elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(calib_end - calib_start).count();
    double cycles_per_ns = static_cast<double>(cycles_end - cycles_start) / elapsed_ns;

    std::vector<double> latencies_ns;
    latencies_ns.reserve(iterations);

    const auto start = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        signals.clear();
        signals.emplace_back(0.03f + dist(rng), 0.85f, 0);
        signals.emplace_back(0.02f + dist(rng), 0.70f, 1);
        signals.emplace_back(0.01f + dist(rng), 0.65f, 2);

        uint64_t t0 = get_cycles();
        (void)engine.makeDecision(signals);
        uint64_t t1 = get_cycles();

        double latency = static_cast<double>(t1 - t0) / cycles_per_ns;
        latencies_ns.push_back(latency);
    }
    const auto end = std::chrono::high_resolution_clock::now();

    const std::chrono::duration<double> elapsed = end - start;
    const double throughput = static_cast<double>(iterations) / elapsed.count();

    std::sort(latencies_ns.begin(), latencies_ns.end());

    double p50 = latencies_ns[iterations * 0.5];
    double p99 = latencies_ns[iterations * 0.99];
    double p99_9 = latencies_ns[iterations * 0.999];

    std::cout << "AILLE Benchmark\n";
    std::cout << "Iterations: " << iterations << "\n";
    std::cout << "Elapsed (s): " << elapsed.count() << "\n";
    std::cout << "Decisions/sec: " << throughput << "\n\n";

    std::cout << "Latency Percentiles (ns):\n";
    std::cout << "  p50:   " << p50 << " ns\n";
    std::cout << "  p99:   " << p99 << " ns\n";
    std::cout << "  p99.9: " << p99_9 << " ns\n";

    if (p99_9 > 5000.0) {
        std::cerr << "FAIL: Regression detected. p99.9 latency exceeds 5000ns.\n";
        return 1;
    }

    return 0;
}
