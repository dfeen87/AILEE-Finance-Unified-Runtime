/*
 * AILLE Plugin — Alpaca Execution Implementation
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#include "AlpacaExecution.hpp"
#include "../../../../ailee_plugins/PluginRegistry.hpp"

// Include cpp-httplib system headers
#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT 1
#endif
#include "../../../../external/httplib.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <regex>

namespace AILLE {
namespace Plugins {
namespace Alpaca {

AlpacaConfig AlpacaExecution::loadConfigFromEnv(bool fail_closed) {
    AlpacaConfig cfg;

    const char* key_env = std::getenv("ALPACA_API_KEY_ID");
    const char* sec_env = std::getenv("ALPACA_SECRET_KEY");
    const char* url_env = std::getenv("ALPACA_BASE_URL");

    if (key_env && std::string(key_env).length() > 0) {
        cfg.api_key_id = key_env;
    }
    if (sec_env && std::string(sec_env).length() > 0) {
        cfg.secret_key = sec_env;
    }
    if (url_env && std::string(url_env).length() > 0) {
        cfg.base_url = url_env;
    } else {
        cfg.base_url = "https://paper-api.alpaca.markets";
    }

    // A destination is configuration, not operator authorization. Keep the
    // default paper mode; explicit callers must request and authorize live.

    if (fail_closed && (cfg.api_key_id.empty() || cfg.secret_key.empty())) {
        cfg.mock_mode = false; // Cannot operate live/paper without creds
    }

    return cfg;
}

AlpacaExecution::AlpacaExecution(const AlpacaConfig& config)
    : config_(config) {
    if (config_.mock_mode) {
        // Simulation never constructs a client, even with real-looking URLs.
        is_enabled_ = true;
    } else if (!validateDestination()) {
        std::cerr << "[AlpacaExecution] Disabled: incompatible destination, mode, or live authorization.\n";
    } else if (!config_.api_key_id.empty() && !config_.secret_key.empty()) {
        is_enabled_ = true;
    } else {
        is_enabled_ = false;
        std::cerr << "[AlpacaExecution] Warning: Missing API credentials (ALPACA_API_KEY_ID/ALPACA_SECRET_KEY). Fail-closed mode active.\n";
    }
}

bool AlpacaExecution::validateDestination() {
    if (config_.is_live && !config_.live_authorized) return false;

    std::string url = config_.base_url;
    std::transform(url.begin(), url.end(), url.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    // Origins only: no userinfo, suffixes, path, query, fragment, escapes, or
    // alternate numeric-host spellings. Loopback is an explicit simulation
    // exception, not a hostname substring heuristic or a custom broker policy.
    static const std::regex origin(
        R"(^(https?)://(paper-api\.alpaca\.markets|api\.alpaca\.markets|localhost|127\.0\.0\.1|\[::1\])(:([0-9]{1,5}))?/?$)");
    std::smatch match;
    if (!std::regex_match(url, match, origin)) return false;
    const std::string scheme = match[1], host = match[2];
    const int port = match[4].matched ? std::stoi(match[4]) : (scheme == "https" ? 443 : 80);
    if (port < 1 || port > 65535) return false;
    const bool loopback = host == "localhost" || host == "127.0.0.1" || host == "[::1]";
    if (!loopback) {
        if (scheme != "https" || port != 443) return false;
        if (config_.is_live != (host == "api.alpaca.markets")) return false;
    }
    broker_origin_ = scheme + "://" + host + ":" + std::to_string(port);
    return true;
}

std::string AlpacaExecution::sideToString(OrderSide side) const {
    switch (side) {
        case OrderSide::BUY:  return "buy";
        case OrderSide::SELL: return "sell";
        default:              return "flat";
    }
}

void AlpacaExecution::triggerLockout(const std::string& reason) {
    locked_out_ = true;
    lockout_reason_ = reason;
    std::cerr << "[AlpacaExecution] LOCKOUT TRIGGERED: " << reason << "\n";
}

std::string AlpacaExecution::submitOrder(const OrderRequest& request) {
    if (!is_enabled_) {
        std::cerr << "[AlpacaExecution] submitOrder rejected: Plugin disabled (configuration/fail-closed).\n";
        return {};
    }
    if (locked_out_) {
        std::cerr << "[AlpacaExecution] submitOrder rejected: Plugin is locked out (" << lockout_reason_ << ").\n";
        return {};
    }
    if (request.quantity <= 0.0f) {
        std::cerr << "[AlpacaExecution] submitOrder skipped: quantity <= 0\n";
        return {};
    }

    if (request.side == OrderSide::FLAT) {
        bool ok = flattenPosition(request.symbol);
        return ok ? "FLAT_SUCCESS" : "";
    }

    if (config_.mock_mode) {
        std::ostringstream oss;
        oss << "MOCK-ALPACA-" << order_counter_.fetch_add(1);
        std::string mock_id = oss.str();
        std::cout << "[AlpacaExecution] [MOCK SUBMIT] symbol=" << request.symbol
                  << " side=" << sideToString(request.side)
                  << " qty=" << request.quantity
                  << " order_id=" << mock_id << "\n";
        return mock_id;
    }

    // The host/port overload is HTTP-only. Preserve HTTPS so credentials and
    // order operations use TLS with httplib's default certificate verification.
    httplib::Client cli(broker_origin_);
    cli.set_connection_timeout(5, 0); // 5 sec timeout

    httplib::Headers headers = {
        {"APCA-API-KEY-ID", config_.api_key_id},
        {"APCA-API-SECRET-KEY", config_.secret_key},
        {"Content-Type", "application/json"}
    };

    std::ostringstream json_body;
    json_body << "{"
              << "\"symbol\":\"" << request.symbol << "\","
              << "\"qty\":" << static_cast<long>(request.quantity) << ","
              << "\"side\":\"" << sideToString(request.side) << "\","
              << "\"type\":\"market\","
              << "\"time_in_force\":\"day\""
              << "}";

    auto res = cli.Post("/v2/orders", headers, json_body.str(), "application/json");

    if (res && res->status == 200) {
        // Simple extraction of order id from JSON body {"id": "xxx"...}
        std::string body = res->body;
        size_t id_pos = body.find("\"id\":\"");
        if (id_pos != std::string::npos) {
            size_t start = id_pos + 6;
            size_t end = body.find("\"", start);
            if (end != std::string::npos) {
                return body.substr(start, end - start);
            }
        }
        return "SUBMITTED_SUCCESS";
    } else {
        int status = res ? res->status : -1;
        std::cerr << "[AlpacaExecution] HTTP POST /v2/orders failed with status: " << status << "\n";
        return {};
    }
}

bool AlpacaExecution::cancelOrder(const std::string& order_id) {
    if (!is_enabled_ || order_id.empty()) return false;

    if (config_.mock_mode) {
        std::cout << "[AlpacaExecution] [MOCK CANCEL] order_id=" << order_id << "\n";
        return true;
    }

    httplib::Client cli(broker_origin_);
    httplib::Headers headers = {
        {"APCA-API-KEY-ID", config_.api_key_id},
        {"APCA-API-SECRET-KEY", config_.secret_key}
    };

    std::string path = "/v2/orders/" + order_id;
    auto res = cli.Delete(path.c_str(), headers);

    return (res && (res->status == 200 || res->status == 204));
}

bool AlpacaExecution::flattenPosition(const std::string& symbol) {
    if (!is_enabled_) return false;

    if (config_.mock_mode) {
        std::cout << "[AlpacaExecution] [MOCK FLATTEN] symbol=" << symbol << "\n";
        return true;
    }

    httplib::Client cli(broker_origin_);
    httplib::Headers headers = {
        {"APCA-API-KEY-ID", config_.api_key_id},
        {"APCA-API-SECRET-KEY", config_.secret_key}
    };

    std::string path = "/v2/positions/" + symbol;
    auto res = cli.Delete(path.c_str(), headers);

    return (res && (res->status == 200 || res->status == 204));
}

float AlpacaExecution::getAccountEquity() {
    if (!is_enabled_) return -1.0f;

    if (config_.mock_mode) {
        return 100000.0f; // Mock 100k equity
    }

    httplib::Client cli(broker_origin_);
    httplib::Headers headers = {
        {"APCA-API-KEY-ID", config_.api_key_id},
        {"APCA-API-SECRET-KEY", config_.secret_key}
    };

    auto res = cli.Get("/v2/account", headers);
    if (res && res->status == 200) {
        std::string body = res->body;
        size_t eq_pos = body.find("\"equity\":\"");
        if (eq_pos != std::string::npos) {
            size_t start = eq_pos + 10;
            size_t end = body.find("\"", start);
            if (end != std::string::npos) {
                return std::strtof(body.substr(start, end - start).c_str(), nullptr);
            }
        }
    }
    return -1.0f;
}

namespace {
    struct AlpacaExecutionRegistrar {
        AlpacaExecutionRegistrar() {
            PluginRegistry::instance().registerExecutionProvider(
                "alpaca",
                []() -> std::unique_ptr<IExecutionProvider> {
                    auto cfg = AlpacaExecution::loadConfigFromEnv(/*fail_closed=*/false);
                    return std::make_unique<AlpacaExecution>(cfg);
                }
            );
        }
    };
    const AlpacaExecutionRegistrar g_alpaca_registrar;
} // anonymous namespace

} // namespace Alpaca
} // namespace Plugins
} // namespace AILLE
