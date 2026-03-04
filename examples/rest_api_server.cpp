/*
 * AILLE Framework - REST API Server Example
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

std::atomic<bool> keep_running(true);

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived shutdown signal...\n";
        keep_running = false;
    }
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    int port = 8080;
    std::string host = "127.0.0.1"; // Use "0.0.0.0" to bind to all interfaces (requires proper network security)
    
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
    
    // Setup signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    std::cout << "=== AILLE Framework REST API Server ===\n\n";
    
    // Configure AILLE engine
    AILLE::AILLEConfig config;
    config.min_confidence_threshold = 0.40f;  // Stricter safety
    config.min_models_required = 2;           // Require at least 2 models
    config.fallback_window_size = 100;        // Larger stability window
    
    std::cout << "AILLE Configuration:\n";
    std::cout << "  Min Confidence Threshold: " << config.min_confidence_threshold << "\n";
    std::cout << "  Min Models Required: " << config.min_models_required << "\n";
    std::cout << "  Fallback Window Size: " << config.fallback_window_size << "\n\n";
    
    // Create AILLE engine
    AILLE::AILLEEngine engine(config);
    
    // Optional: Create audit logger for compliance
    AILLE::AuditLogger logger("rest_api_audit.csv");
    
    std::cout << "Audit logging enabled: rest_api_audit.csv\n\n";
    
    // Create and start REST API server
    AILLE::RestAPIServer server(engine, port, host);
    
    std::cout << "Starting REST API server...\n";
    std::cout << "Host: " << host << "\n";
    std::cout << "Port: " << port << "\n";
    std::cout << "\nServer will be accessible from:\n";
    std::cout << "  - Local:    http://localhost:" << port << "\n";
    std::cout << "  - Network:  http://<your-ip>:" << port << "\n";
    std::cout << "\nPress Ctrl+C to stop the server\n";
    std::cout << "=====================================\n\n";
    
    // Start server in a separate thread so we can handle signals
    server.startAsync();
    
    // Wait for shutdown signal
    while (keep_running && server.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Graceful shutdown
    std::cout << "\nShutting down server...\n";
    server.stop();
    server.join();
    
    std::cout << "Server stopped. Goodbye!\n";
    
    return 0;
}
