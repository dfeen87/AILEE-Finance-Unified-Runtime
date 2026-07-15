/*
 * AILLE Framework - Spire WebSocket Server Example
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 *
 * This example demonstrates how to run AILLE as a WebSocket API service
 * accessible over the network for continuous Spire state broadcasting.
 *
 * Build:
 *   make websocket_server
 *
 * Run:
 *   ./websocket_server [port]
 */

#include "aille.hpp"
#include "extensions/aille_websocket.hpp"
#include <iostream>
#include <csignal>
#include <atomic>

std::atomic<bool> keep_running(true);

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived shutdown signal...\n";
        keep_running = false;
    }
}

int main(int argc, char* argv[]) {
    int port = 9002;
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Invalid port number: " << argv[1] << "\n";
            return 1;
        }
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "========================================\n";
    std::cout << " AILLEE Spire WebSocket Server (v7.8.0) \n";
    std::cout << "========================================\n\n";

    // Create and start WebSocket server
    AILLE::WebSocketServer server(port);

    std::cout << "Starting WebSocket server...\n";
    std::cout << "Port: " << port << "\n";
    std::cout << "\nClients can connect to:\n";
    std::cout << "  ws://localhost:" << port << "\n";
    std::cout << "\nPress Ctrl+C to stop the server\n";
    std::cout << "=====================================\n\n";

    if (!server.startAsync()) {
        std::cerr << "Failed to start server.\n";
        return 1;
    }

    while (keep_running && server.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nShutting down server...\n";
    server.stop();
    server.join();

    std::cout << "Server stopped. Goodbye!\n";

    return 0;
}
