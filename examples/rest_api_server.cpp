/*
 * AILLE Framework - REST API Server (AILLE 3.3.1 Compatible)
 *
 * Deterministic, plugin-free, header-only engine
 * REST API for decision evaluation + telemetry heartbeat
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT
 */

#include "aille.hpp"
#include "extensions/aille_rest_api.hpp"

#include <iostream>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

// Optional telemetry heartbeat
#include "telemetry/TelemetryBridge.hpp"

std::atomic<bool> keep_running(true);
std::atomic<bool> notifier_running(true);

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        std::cout << "\nReceived shutdown signal...\n";
        keep_running = false;
        notifier_running = false;
    }
}

void heartbeat_loop() {
    while (notifier_running.load()) {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);

        std::cout << "[AILLE REST API] Heartbeat at "
                  << std::ctime(&t);

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

int main(int argc, char* argv[]) {

    // -------------------------------
    // Parse CLI args
    // -------------------------------
    int port = 8080;
    std::string host = "127.0.0.1";

    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Invalid port number: " << argv[1] << "\n";
            return 1;
        }
    }

    if (argc > 2) {
        host = argv[2];
    }

    // -------------------------------
    // Signal handling
    // -------------------------------
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "=== AILLE Framework REST API Server ===\n\n";

    // -------------------------------
    // Engine configuration
    // -------------------------------
    AILLE::AILLEConfig config;
    config.min_confidence_threshold = 0.40f;
    config.min_models_required = 2;
    config.fallback_window_size = 100;

    std::cout << "AILLE Configuration:\n";
    std::cout << "  Min Confidence Threshold: " << config.min_confidence_threshold << "\n";
    std::cout << "  Min Models Required: " << config.min_models_required << "\n";
    std::cout << "  Fallback Window Size: " << config.fallback_window_size << "\n\n";

    // -------------------------------
    // Create engine (plugin-free)
    // -------------------------------
    AILLE::AILLEEngine engine(config);

    // -------------------------------
    // Audit logger
    // -------------------------------
    AILLE::AuditLogger logger("rest_api_audit.csv");
    std::cout << "Audit logging enabled: rest_api_audit.csv\n\n";

    // -------------------------------
    // Create REST API server
    // -------------------------------
    AILLE::RestAPIServer server(engine, port, host);

    std::cout << "Starting REST API server...\n";
    std::cout << "Host: " << host << "\n";
    std::cout << "Port: " << port << "\n";
    std::cout << "\nPress Ctrl+C to stop the server\n";
    std::cout << "=====================================\n\n";

    // -------------------------------
    // Heartbeat notifier thread
    // -------------------------------
    std::thread heartbeat(heartbeat_loop);

    // -------------------------------
    // Start REST API server
    // -------------------------------
    server.startAsync();

    // -------------------------------
    // Wait for shutdown
    // -------------------------------
    while (keep_running && server.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // -------------------------------
    // Graceful shutdown
    // -------------------------------
    std::cout << "\nShutting down server...\n";
    server.stop();
    server.join();

    notifier_running.store(false);
    heartbeat.join();

    std::cout << "Server stopped. Goodbye!\n";

    return 0;
}
