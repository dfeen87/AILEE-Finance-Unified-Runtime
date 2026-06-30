/*
 * AILLE Metrics Extension – Minimal Integration Example
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 *
 * Demonstrates how to observe AILLE decisions in real time
 * without modifying core decision logic.
 *
 * This example is intentionally simple and suitable for:
 * - trading engines
 * - simulation loops
 * - risk evaluation pipelines
 */

#include <iostream>
#include <vector>

#include "aille.hpp"
#include "extensions/aille_metrics.hpp"

int main() {
    // ------------------------------------------------------------------------
    // Initialize AILLE engine and metrics collector
    // ------------------------------------------------------------------------
    AILLE::AILLEEngine engine;
    AILLE::MetricsCollector metrics;

    // ------------------------------------------------------------------------
    // Simulated model signals (replace with real models)
    // ------------------------------------------------------------------------
    std::vector<AILLE::ModelSignal> signals;
    signals.emplace_back(0.04f, 0.85f, 0);  // Model 0
    signals.emplace_back(0.03f, 0.72f, 1);  // Model 1
    signals.emplace_back(0.02f, 0.68f, 2);  // Model 2

    // ------------------------------------------------------------------------
    // Run several decisions to populate metrics
    // ------------------------------------------------------------------------
    for (int i = 0; i < 10; ++i) {
        AILLE::Decision decision = engine.makeDecision(signals);

        // Observe decision (read-only)
        metrics.observeDecision(decision);

        // Execute based on decision status
        if (decision.status == AILLE::DECISION_VALID) {
            std::cout << "[EXECUTE] Position: "
                      << decision.final_value << "\n";
        } else if (decision.fallback_used) {
            std::cout << "[FALLBACK] Conservative position: "
                      << decision.final_value << "\n";
        } else {
            std::cout << "[SKIP] Decision rejected\n";
        }
    }

    // ------------------------------------------------------------------------
    // Retrieve and display metrics snapshot
    // ------------------------------------------------------------------------
    AILLE::MetricsSnapshot snapshot = metrics.getSnapshot();

    std::cout << "\n=== AILLE Metrics Snapshot ===\n";
    std::cout << AILLE::formatMetrics(snapshot) << "\n";

    // ------------------------------------------------------------------------
    // Example health check
    // ------------------------------------------------------------------------
    if (!metrics.isHealthy(0.10f)) {
        std::cout << "WARNING: Fallback rate exceeds threshold\n";
    } else {
        std::cout << "System health: OK\n";
    }

    return 0;
}
