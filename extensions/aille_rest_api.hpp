/*
 * AILLE Framework - REST API Extension
 * Provides HTTP/REST interface for AILLE decision-making
 * 
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 * 
 * USAGE:
 * ------
 * #include "aille.hpp"
 * #include "extensions/aille_rest_api.hpp"
 * 
 * AILLE::AILLEEngine engine;
 * AILLE::RestAPIServer server(engine, 8080);
 * server.start(); // Starts server on 127.0.0.1:8080 (localhost only; use "0.0.0.0" to bind to all interfaces)
 * 
 * NOTE: This requires cpp-httplib (single-header library)
 * Download from: https://github.com/yhirose/cpp-httplib
 * Or use the bundled version if available
 */

#ifndef AILLE_REST_API_HPP
#define AILLE_REST_API_HPP

#include "../aille.hpp"
#include <string>
#include <sstream>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <vector>

// Forward declaration - httplib will be included separately
namespace httplib {
    class Server;
}

namespace AILLE {

// Simple JSON parser for input
class SimpleJSONParser {
public:
    static bool parseModelSignals(const std::string& json, std::vector<ModelSignal>& signals) {
        // Simple parser - expects array of objects with value, confidence, model_id
        // Format: [{"value": 0.5, "confidence": 0.8, "model_id": 0}, ...]
        
        signals.clear();
        
        // Find the array brackets
        size_t start = json.find('[');
        size_t end = json.rfind(']');
        if (start == std::string::npos || end == std::string::npos) {
            return false;
        }
        
        std::string content = json.substr(start + 1, end - start - 1);
        
        // Parse each object
        size_t pos = 0;
        static constexpr size_t MAX_PARSED_SIGNALS = 100;
        while (pos < content.length()) {
            // Find next object
            size_t objStart = content.find('{', pos);
            if (objStart == std::string::npos) break;
            
            size_t objEnd = content.find('}', objStart);
            if (objEnd == std::string::npos) break;
            
            std::string obj = content.substr(objStart, objEnd - objStart + 1);
            
            // Extract value, confidence, model_id
            float value = 0.0f;
            float confidence = 0.0f;
            int model_id = 0;
            
            if (!extractFloat(obj, "value", value)) return false;
            if (!extractFloat(obj, "confidence", confidence)) return false;
            extractInt(obj, "model_id", model_id); // Optional
            
            signals.push_back(ModelSignal(value, confidence, model_id));
            
            if (signals.size() >= MAX_PARSED_SIGNALS) break;
            pos = objEnd + 1;
        }
        
        return !signals.empty();
    }

private:
    static bool extractFloat(const std::string& obj, const std::string& key, float& value) {
        std::string searchKey = "\"" + key + "\"";
        size_t keyPos = obj.find(searchKey);
        if (keyPos == std::string::npos) return false;
        
        size_t colonPos = obj.find(':', keyPos);
        if (colonPos == std::string::npos) return false;
        
        size_t valueStart = colonPos + 1;
        while (valueStart < obj.length() && std::isspace(static_cast<unsigned char>(obj[valueStart]))) valueStart++;
        
        size_t valueEnd = valueStart;
        while (valueEnd < obj.length() && 
               (std::isdigit(static_cast<unsigned char>(obj[valueEnd])) || obj[valueEnd] == '.' || 
                obj[valueEnd] == '-' || obj[valueEnd] == '+' || obj[valueEnd] == 'e' || obj[valueEnd] == 'E')) {
            valueEnd++;
        }
        
        std::string valueStr = obj.substr(valueStart, valueEnd - valueStart);
        try {
            value = std::stof(valueStr);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    static bool extractInt(const std::string& obj, const std::string& key, int& value) {
        std::string searchKey = "\"" + key + "\"";
        size_t keyPos = obj.find(searchKey);
        if (keyPos == std::string::npos) return false;
        
        size_t colonPos = obj.find(':', keyPos);
        if (colonPos == std::string::npos) return false;
        
        size_t valueStart = colonPos + 1;
        while (valueStart < obj.length() && std::isspace(static_cast<unsigned char>(obj[valueStart]))) valueStart++;
        
        size_t valueEnd = valueStart;
        while (valueEnd < obj.length() && (std::isdigit(static_cast<unsigned char>(obj[valueEnd])) || obj[valueEnd] == '-')) {
            valueEnd++;
        }
        
        std::string valueStr = obj.substr(valueStart, valueEnd - valueStart);
        try {
            value = std::stoi(valueStr);
            return true;
        } catch (...) {
            return false;
        }
    }
};

// REST API Server class
class RestAPIServer {
public:
    RestAPIServer(AILLEEngine& engine, int port = 8080, const std::string& host = "127.0.0.1")
        : engine_(engine), port_(port), host_(host), running_(false), server_(nullptr) {
    }
    
    ~RestAPIServer();  // Defined in .cpp to allow proper cleanup
    
    // Start the server (blocking call)
    bool start();
    
    // Start the server in a background thread
    void startAsync() {
        server_thread_ = std::thread([this]() {
            start();
        });
    }
    
    // Stop the server
    void stop();
    
    // Wait for server to finish (when started async)
    void join() {
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }
    
    bool isRunning() const { return running_; }

private:
    AILLEEngine& engine_;
    int port_;
    std::string host_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    httplib::Server* server_;  // Raw pointer to avoid incomplete type issues
    
    void setupRoutes(httplib::Server& svr);
};

} // namespace AILLE

// Include httplib here if available
// This is a placeholder - the actual implementation will be in a .cpp file
// or httplib.h should be included before this header

#endif // AILLE_REST_API_HPP
