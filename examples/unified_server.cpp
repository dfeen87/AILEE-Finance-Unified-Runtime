/*
 * AILLE Unified Server
 * REST + WebSocket + Metrics + Telemetry + Advisory + Hotpath
 *
 * Compatible with AILLE 3.3.1 (header-only engine)
 */

#include "aille.hpp"
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>
#include <csignal>
#include <random>
#include <mutex>
#include <vector>
#include <string>

// httplib (expected in external/)
#include "httplib.h"

// If you have a MetricsCollector header, include it here.
// Otherwise, this is a simple stub you can replace.
namespace AILLE {
class MetricsCollector {
public:
    void recordDecision(const Decision& d) {
        // Extend with real metrics logic
        (void)d;
    }
};
}

// Simple WebSocket broadcast wrapper
class WebSocketHub {
public:
    void addClient(httplib::WebSocket* ws) {
        std::lock_guard<std::mutex> lock(mtx_);
        clients_.push_back(ws);
    }

    void removeClient(httplib::WebSocket* ws) {
        std::lock_guard<std::mutex> lock(mtx_);
        clients_.erase(
            std::remove(clients_.begin(), clients_.end(), ws),
            clients_.end()
        );
    }

    void broadcast(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mtx_);
        for (auto* ws : clients_) {
            if (ws) {
                ws->send(msg);
            }
        }
    }

private:
    std::mutex mtx_;
    std::vector<httplib::WebSocket*> clients_;
};

// Global control flags
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
        std::cout << "[AILLE Unified Server] Heartbeat at "
                  << std::ctime(&t);
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

// Serialize a Decision to JSON-ish string (simple)
std::string decision_to_json(const AILLE::Decision& d) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"status\":" << d.status << ",";
    oss << "\"final_value\":" << d.final_value << ",";
    oss << "\"confidence\":" << d.confidence << ",";
    oss << "\"fallback_used\":" << (d.fallback_used ? "true" : "false") << ",";
    oss << "\"num_contributing_models\":" << d.num_contributing_models << ",";
    oss << "\"models_agreed\":" << d.models_agreed << ",";
    oss << "\"reasoning\":\"" << d.reasoning << "\"";
    oss << "}";
    return oss.str();
}

// Simple helper: build a dummy ModelSignal vector from query params
std::vector<AILLE::ModelSignal> build_signals_from_request(const httplib::Request& req) {
    std::vector<AILLE::ModelSignal> signals;

    // Example: ?value=0.1&confidence=0.9&model_id=1
    float value = 0.0f;
    float conf = 0.5f;
    int model_id = 0;

    if (req.has_param("value")) {
        value = std::stof(req.get_param_value("value"));
    }
    if (req.has_param("confidence")) {
        conf = std::stof(req.get_param_value("confidence"));
    }
    if (req.has_param("model_id")) {
        model_id = std::stoi(req.get_param_value("model_id"));
    }

    signals.emplace_back(value, conf, model_id);
    return signals;
}

// Hotpath demo: generate synthetic signals and run through engine
AILLE::Decision run_hotpath_demo(AILLE::AILLEEngine& engine) {
    std::mt19937 rng(7);
    std::normal_distribution<float> dist(0.0f, 0.02f);

    std::vector<AILLE::ModelSignal> signals;
    for (int i = 0; i < 4; ++i) {
        float v = dist(rng);
        float c = 0.8f + 0.1f * dist(rng);
        if (c < 0.0f) c = 0.0f;
        if (c > 1.0f) c = 1.0f;
        signals.emplace_back(v, c, i);
    }

    return engine.makeDecision(signals.data(), signals.size());
}

int main(int argc, char* argv[]) {
    int rest_port = 8080;
    int ws_port   = 8081;
    std::string host = "127.0.0.1";

    if (argc > 1) {
        rest_port = std::stoi(argv[1]);
    }
    if (argc > 2) {
        ws_port = std::stoi(argv[2]);
    }
    if (argc > 3) {
        host = argv[3];
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "=== AILLE Unified Server ===\n\n";

    // Engine config
    AILLE::AILLEConfig config;
    config.min_confidence_threshold = 0.40f;
    config.min_models_required = 2;
    config.fallback_window_size = 100;

    std::cout << "AILLE Configuration:\n";
    std::cout << "  Min Confidence Threshold: " << config.min_confidence_threshold << "\n";
    std::cout << "  Min Models Required: " << config.min_models_required << "\n";
    std::cout << "  Fallback Window Size: " << config.fallback_window_size << "\n\n";

    // Core engine + metrics + audit
    AILLE::AILLEEngine engine(config);
    AILLE::MetricsCollector metrics;
    AILLE::AuditLogger logger("unified_audit.csv");

    WebSocketHub ws_hub;

    // REST server
    httplib::Server rest;

    // Decision endpoint
    rest.Post("/decision", [&](const httplib::Request& req, httplib::Response& res) {
        auto signals = build_signals_from_request(req);
        auto decision = engine.makeDecision(signals.data(), signals.size());

        // Metrics + audit + telemetry
        metrics.recordDecision(decision);
        logger.log(decision);

        auto payload = decision_to_json(decision);
        ws_hub.broadcast(payload); // WebSocket streaming

        res.set_content(payload, "application/json");
    });

    // Advisory endpoints (BTC/ETH/OIL/GOLD/etc.) — stubbed as simple echoes
    rest.Get("/advisory/btc", [&](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        res.set_content("{\"symbol\":\"BTC\",\"status\":\"advisory_only\"}", "application/json");
    });

    rest.Get("/advisory/eth", [&](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        res.set_content("{\"symbol\":\"ETH\",\"status\":\"advisory_only\"}", "application/json");
    });

    rest.Get("/advisory/oil", [&](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        res.set_content("{\"symbol\":\"OIL\",\"status\":\"advisory_only\"}", "application/json");
    });

    rest.Get("/advisory/gold", [&](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        res.set_content("{\"symbol\":\"GOLD\",\"status\":\"advisory_only\"}", "application/json");
    });

    // Hotpath demo endpoint
    rest.Get("/hotpath", [&](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        auto decision = run_hotpath_demo(engine);
        metrics.recordDecision(decision);
        logger.log(decision);
        auto payload = decision_to_json(decision);
        ws_hub.broadcast(payload);
        res.set_content(payload, "application/json");
    });

    // Simple dashboard endpoint
    rest.Get("/dashboard", [&](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        res.set_content(
            "{ \"message\": \"AILLE Unified Dashboard - REST + WS + Metrics + Hotpath\" }",
            "application/json"
        );
    });

    // WebSocket server (using httplib's built-in WS)
    httplib::Server ws_server;

    ws_server.set_ws_endpoint("/stream", [&](const httplib::Request& req,
                                             httplib::Response& res,
                                             httplib::WebSocket& ws) {
        (void)req;
        (void)res;

        ws_hub.addClient(&ws);

        ws.on_message = [&](const std::string& msg) {
            // Echo or handle control messages if desired
            (void)msg;
        };

        ws.on_close = [&]() {
            ws_hub.removeClient(&ws);
        };
    });

    std::cout << "REST listening on " << host << ":" << rest_port << "\n";
    std::cout << "WebSocket streaming on " << host << ":" << ws_port << " (path: /stream)\n";
    std::cout << "Audit file: unified_audit.csv\n";
    std::cout << "\nPress Ctrl+C to stop the server\n";
    std::cout << "=====================================\n\n";

    // Heartbeat thread
    std::thread heartbeat(heartbeat_loop);

    // REST + WS threads
    std::thread rest_thread([&]() {
        rest.listen(host.c_str(), rest_port);
    });

    std::thread ws_thread([&]() {
        ws_server.listen(host.c_str(), ws_port);
    });

    // Wait for shutdown
    while (keep_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Stop servers
    rest.stop();
    ws_server.stop();

    if (rest_thread.joinable()) rest_thread.join();
    if (ws_thread.joinable()) ws_thread.join();

    notifier_running.store(false);
    if (heartbeat.joinable()) heartbeat.join();

    std::cout << "Unified server stopped. Goodbye.\n";
    return 0;
}
