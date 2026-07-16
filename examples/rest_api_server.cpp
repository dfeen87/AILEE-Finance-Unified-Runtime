/*
 * AILLE Framework - REST API Server Example
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 * 
 * This example demonstrates how to run AILLE as a REST API service
 * accessible over the network for decision-making.
 * 
 * Build:
 *   make rest_api_server
 * 
 * Run:
 *   ./rest_api_server
 * 
 * Test:
 *   curl http://localhost:8080/health
 *   curl -X POST http://localhost:8080/api/decision \
 *     -H "Content-Type: application/json" \
 *     -d '[{"value": 0.05, "confidence": 0.85, "model_id": 0}, {"value": 0.03, "confidence": 0.72, "model_id": 1}]'
 */

#include "aille.hpp"
#include "extensions/aille_rest_api.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>

std::atomic<bool> keep_running(true);
std::atomic<bool> notifier_running(true);

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived shutdown signal...\n";
        keep_running = false;
        notifier_running = false;
    }
}

void notifier_loop() {
    while (notifier_running.load()) {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);

        std::cout << "[AILLE REST API] Heartbeat at "
                  << std::ctime(&t);

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

int main(int argc, char* argv[]) {
    // Parse command line arguments...
    // (unchanged)

    // Setup signal handler
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "=== AILLE Framework REST API Server ===\n\n";

    // Configure engine...
    // (unchanged)

    // Create engine + audit logger...
    // (unchanged)

    AILLE::RestAPIServer server(engine, port, host);

    std::cout << "Starting REST API server...\n";
    std::cout << "Host: " << host << "\n";
    std::cout << "Port: " << port << "\n";
    std::cout << "\nPress Ctrl+C to stop the server\n";
    std::cout << "=====================================\n\n";

    // 🔥 Start heartbeat notifier
    std::thread notifier(notifier_loop);

    // 🔥 Start REST API server
    server.startAsync();

    // Wait for shutdown signal
    while (keep_running && server.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 🔥 Graceful shutdown
    std::cout << "\nShutting down server...\n";
    server.stop();
    server.join();

    // 🔥 Stop notifier
    notifier_running.store(false);
    notifier.join();

    std::cout << "Server stopped. Goodbye!\n";

    return 0;
}
