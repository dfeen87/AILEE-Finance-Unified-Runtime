/*
 * AILLE Framework - REST API Implementation
 * HTTP server implementation for AILLE decision-making
 * 
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#include "external/httplib.h"
#include "aille_rest_api.hpp"
#include <iostream>

namespace AILLE {

RestAPIServer::~RestAPIServer() {
    stop();
    if (server_) {
        delete server_;
        server_ = nullptr;
    }
}

void RestAPIServer::setupRoutes(httplib::Server& svr) {
    // Health check endpoint - verifies server is running and responsive
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(SimpleJSON::buildHealthResponse(true), "application/json");
        res.status = 200;
    });
    
    // API info endpoint
    svr.Get("/api/info", [](const httplib::Request&, httplib::Response& res) {
        std::ostringstream json;
        json << "{\n";
        json << "  \"name\": \"AILLE Framework REST API\",\n";
        json << "  \"version\": \"" << AILLE_VERSION << "\",\n";
        json << "  \"description\": \"AI-Load Integrity and Layered Evaluation Framework\",\n";
        json << "  \"endpoints\": {\n";
        json << "    \"GET /health\": \"Health check\",\n";
        json << "    \"GET /api/info\": \"API information\",\n";
        json << "    \"POST /api/decision\": \"Make a decision based on model signals\"\n";
        json << "  }\n";
        json << "}";
        res.set_content(json.str(), "application/json");
        res.status = 200;
    });
    
    // Decision endpoint
    svr.Post("/api/decision", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            // Parse the request body into a dynamic vector of ModelSignal
            std::vector<ModelSignal> signals;
            
            if (!SimpleJSONParser::parseModelSignals(req.body, signals)) {
                res.set_content(
                    SimpleJSON::buildErrorResponse("Invalid request format. Expected array of signals with value, confidence, and optional model_id"),
                    "application/json"
                );
                res.status = 400;
                return;
            }
            
            if (signals.empty()) {
                res.set_content(
                    SimpleJSON::buildErrorResponse("No model signals provided"),
                    "application/json"
                );
                res.status = 400;
                return;
            }
            
            // Make the decision using AILLE engine.
            // The engine expects: makeDecision(const ModelSignal* model_signals, size_t count)
            Decision decision = engine_.makeDecision(signals.data(), signals.size());
            
            // Build and send response
            res.set_content(SimpleJSON::buildDecisionResponse(decision), "application/json");
            res.status = 200;
            
        } catch (const std::exception& e) {
            res.set_content(
                SimpleJSON::buildErrorResponse(std::string("Internal error: ") + e.what()),
                "application/json"
            );
            res.status = 500;
        }
    });
    
    // Root endpoint
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        std::string html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>AILLE Framework REST API</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 800px; margin: 50px auto; padding: 20px; }
        h1 { color: #2c3e50; }
        .endpoint { background: #f8f9fa; padding: 15px; margin: 10px 0; border-radius: 5px; }
        code { background: #e9ecef; padding: 2px 5px; border-radius: 3px; }
        pre { background: #2c3e50; color: #ecf0f1; padding: 15px; border-radius: 5px; overflow-x: auto; }
    </style>
</head>
<body>
    <h1>🚀 AILLE Framework REST API</h1>
    <p>AI-Load Integrity and Layered Evaluation Framework</p>
    
    <h2>Available Endpoints</h2>
    
    <div class="endpoint">
        <h3>GET /health</h3>
        <p>Health check endpoint</p>
        <pre>curl http://localhost:8080/health</pre>
    </div>
    
    <div class="endpoint">
        <h3>GET /api/info</h3>
        <p>Get API information and available endpoints</p>
        <pre>curl http://localhost:8080/api/info</pre>
    </div>
    
    <div class="endpoint">
        <h3>POST /api/decision</h3>
        <p>Make a validated decision based on model signals</p>
        <p><strong>Request Body:</strong></p>
        <pre>[
  {"value": 0.05, "confidence": 0.85, "model_id": 0},
  {"value": 0.03, "confidence": 0.72, "model_id": 1},
  {"value": 0.02, "confidence": 0.68, "model_id": 2}
]</pre>
        <p><strong>Example:</strong></p>
        <pre>curl -X POST http://localhost:8080/api/decision \
  -H "Content-Type: application/json" \
  -d '[{"value": 0.05, "confidence": 0.85, "model_id": 0}]'</pre>
    </div>
    
    <h2>About AILLE</h2>
    <p>AILLE is a five-stage decision architecture that transforms algorithmic risk from an unpredictable liability into a managed, measurable advantage.</p>
    
    <h3>Features</h3>
    <ul>
        <li>✅ Multi-model consensus validation</li>
        <li>✅ Confidence-based filtering</li>
        <li>✅ Automatic fallback mechanisms</li>
        <li>✅ Full audit trail</li>
        <li>✅ Real-time decision-making</li>
    </ul>
</body>
</html>
        )";
        res.set_content(html, "text/html");
        res.status = 200;
    });
}

bool RestAPIServer::start() {
    if (running_) {
        std::cerr << "Server already running\n";
        return false;
    }
    
    try {
        if (server_) {
            delete server_;
            server_ = nullptr;
        }
        server_ = new httplib::Server();
        
        setupRoutes(*server_);
        
        std::cout << "=== AILLE Framework REST API Server ===\n";
        std::cout << "Starting server on " << host_ << ":" << port_ << "\n";
        std::cout << "Press Ctrl+C to stop\n\n";
        std::cout << "Endpoints:\n";
        std::cout << "  GET  http://" << host_ << ":" << port_ << "/\n";
        std::cout << "  GET  http://" << host_ << ":" << port_ << "/health\n";
        std::cout << "  GET  http://" << host_ << ":" << port_ << "/api/info\n";
        std::cout << "  POST http://" << host_ << ":" << port_ << "/api/decision\n";
        std::cout << "\n";
        
        running_ = true;
        
        // Bind to configured host (default: 127.0.0.1 for security)
        if (!server_->listen(host_.c_str(), port_)) {
            std::cerr << "Failed to start server on " << host_ << ":" << port_ << "\n";
            running_ = false;
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error starting server: " << e.what() << "\n";
        running_ = false;
        return false;
    }
}

void RestAPIServer::stop() {
    if (running_ && server_) {
        std::cout << "\nStopping server...\n";
        server_->stop();
        running_ = false;
    }
}

} // namespace AILLE
