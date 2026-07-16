/*
 * AILLE Framework - REST API Server Example
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#include "aille.hpp"
#include "extensions/aille_rest_api.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>

// Telemetry
#include "telemetry/TelemetryBridge.hpp"

// Interfaces
#include "interfaces/IAnalyticsObserver.hpp"
#include "interfaces/IExecutionProvider.hpp"
#include "interfaces/IMarketDataSource.hpp"
#include "interfaces/IBreakingNewsProvider.hpp"
#include "interfaces/ITradingAlertAdapter.hpp"
#include "plugins/PluginRegistry.hpp"

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

/* -------------------------------------------------------------------------
   TELEMETRY HOOK IMPLEMENTATIONS
   These connect your interfaces directly to the terminal dashboard.
   ------------------------------------------------------------------------- */

// Analytics Observer → HFT decisions + queries
class TelemetryAnalyticsObserver : public IAnalyticsObserver {
public:
    std::string name() const override { return "TelemetryAnalyticsObserver"; }

    void onSignalEvaluated(const std::vector<ModelSignal>&) override {
        telemetry_on_query();
    }

    void onDecisionRouted(const Decision&) override {
        telemetry_on_hft_decision();
    }
};

// Execution Provider → executions sent
class TelemetryExecutionProvider : public IExecutionProvider {
public:
    std::string name() const override { return "TelemetryExecutionProvider"; }

    void sendOrder(const Order&) override {
        telemetry_on_execution_sent();
    }
};

// Market Data Source → market updates
class TelemetryMarketDataSource : public IMarketDataSource {
public:
    std::string name() const override { return "TelemetryMarketDataSource"; }

    void onTick(const MarketTick&) override {
        telemetry_on_market_update();
    }
};

// Breaking News Provider → news events
class TelemetryBreakingNewsProvider : public IBreakingNewsProvider {
public:
    std::string name() const override { return "TelemetryBreakingNewsProvider"; }

    void onBreakingNews(const NewsItem&) override {
        telemetry_on_breaking_news();
    }
};

// Trading Alert Adapter → alerts triggered
class TelemetryTradingAlertAdapter : public ITradingAlertAdapter {
public:
    std::string name() const override { return "TelemetryTradingAlertAdapter"; }

    void onAlertTriggered(const Alert&) override {
        telemetry_on_alert_triggered();
    }
};

// Plugin Registry → plugin count
class TelemetryPluginRegistry : public PluginRegistry {
public:
    void registerPlugin(const std::string& name, PluginPtr plugin) override {
        telemetry_on_plugin_registered();
        PluginRegistry::registerPlugin(name, plugin);
    }
};

/* -------------------------------------------------------------------------
   MAIN
   ------------------------------------------------------------------------- */

int main(int argc, char* argv[]) {

    // -------------------------------
    // Parse command line arguments
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
    // Setup signal handler
    // -------------------------------
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "=== AILLE Framework REST API Server ===\n\n";

    // -------------------------------
    // Configure AILLE engine
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
    // Create engine
    // -------------------------------
    AILLE::AILLEEngine engine(config);

    // Attach telemetry observers
    engine.addObserver(std::make_shared<TelemetryAnalyticsObserver>());

    // Attach execution provider
    engine.setExecutionProvider(std::make_shared<TelemetryExecutionProvider>());

    // Attach market data source
    engine.setMarketDataSource(std::make_shared<TelemetryMarketDataSource>());

    // Attach breaking news provider
    engine.setBreakingNewsProvider(std::make_shared<TelemetryBreakingNewsProvider>());

    // Attach trading alert adapter
    engine.setTradingAlertAdapter(std::make_shared<TelemetryTradingAlertAdapter>());

    // Plugin registry with telemetry
    auto pluginRegistry = std::make_shared<TelemetryPluginRegistry>();
    engine.setPluginRegistry(pluginRegistry);

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
    // Start heartbeat notifier
    // -------------------------------
    std::thread notifier(notifier_loop);

    // -------------------------------
    // Start REST API server
    // -------------------------------
    server.startAsync();

    // -------------------------------
    // Wait for shutdown signal
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
    notifier.join();

    std::cout << "Server stopped. Goodbye!\n";

    return 0;
}
