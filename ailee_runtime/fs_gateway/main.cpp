/*
 * AILEE Framework - FS-Gateway Daemon Main Entry Point
 * AILEE Finance Unified Runtime Version 18.0.0
 *
 * Exposes WebSocket endpoint: ws://<host>:9002/ailee/finance/runtime
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#include "fs_gateway.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

std::atomic<bool> g_shutdown{false};

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n[FS-Gateway] Shutdown signal received. Stopping server...\n";
        g_shutdown = true;
    }
}

int main(int argc, char* argv[]) {
    int port = AILEE::FS_GATEWAY_DEFAULT_PORT;
    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "Invalid port argument: " << argv[1] << "\n";
            return 1;
        }
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "=======================================================\n";
    std::cout << "  AILEE Finance Unified Runtime v18.0.0 - FS-Gateway\n";
    std::cout << "  Endpoint: ws://0.0.0.0:" << port << AILEE::FS_GATEWAY_DEFAULT_PATH << "\n";
    std::cout << "=======================================================\n";

    AILEE::FsGateway gateway(port, AILEE::FS_GATEWAY_DEFAULT_PATH);

    if (!gateway.startAsync()) {
        std::cerr << "[FS-Gateway] Failed to start server async.\n";
        return 1;
    }

    std::cout << "[FS-Gateway] Daemon initialized and streaming live AF-WSX frames.\n";

    while (!g_shutdown && gateway.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    gateway.stop();
    gateway.join();

    std::cout << "[FS-Gateway] Server stopped cleanly.\n";
    return 0;
}
