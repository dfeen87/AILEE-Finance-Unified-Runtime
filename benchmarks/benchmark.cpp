/*
 * AILLE Framework - Benchmark Harness
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: Non-Commercial (see LICENSE)
 *
 * Measures decision throughput for the core engine using
 * deterministic synthetic signals.
 */

#include "aille.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <vector>

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

    const auto start = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        signals.clear();
        signals.emplace_back(0.03f + dist(rng), 0.85f, 0);
        signals.emplace_back(0.02f + dist(rng), 0.70f, 1);
        signals.emplace_back(0.01f + dist(rng), 0.65f, 2);
        (void)engine.makeDecision(signals);
    }
    const auto end = std::chrono::high_resolution_clock::now();

    const std::chrono::duration<double> elapsed = end - start;
    const double throughput = static_cast<double>(iterations) / elapsed.count();

    std::cout << "AILLE Benchmark\n";
    std::cout << "Iterations: " << iterations << "\n";
    std::cout << "Elapsed (s): " << elapsed.count() << "\n";
    std::cout << "Decisions/sec: " << throughput << "\n";

    return 0;
}
