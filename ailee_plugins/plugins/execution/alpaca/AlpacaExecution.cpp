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
#include <limits>
#include <locale>
#include "../../../detail/broker_json.hpp"
#include "../../../detail/intent_journal.hpp"

namespace AILLE {
namespace Plugins {
namespace Alpaca {
namespace {
bool token(const std::string& text, std::size_t maximum=128) {
    if(text.empty() || text.size()>maximum) return false;
    return std::all_of(text.begin(),text.end(),[](unsigned char c) {
        return (c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='-' || c=='_' || c=='.';
    }) && text.find("..") == std::string::npos;
}
std::string acknowledgedId(const std::string& body) {
    try {
        const auto fields=Detail::BrokerJson(body).parse();
        const auto id=fields.find("id");
        if(id==fields.end() || id->second.kind!=Detail::JsonScalar::STRING || !token(id->second.text)) return {};
        const auto state=fields.find("status");
        if(state!=fields.end()) {
            if(state->second.kind!=Detail::JsonScalar::STRING) return {};
            const auto& value=state->second.text;
            if(value!="accepted" && value!="new" && value!="pending_new" && value!="accepted_for_bidding" &&
               value!="partially_filled" && value!="filled") return {};
        }
        return id->second.text;
    } catch(const std::exception&) { return {}; }
}
void timeouts(httplib::Client& client) {
    client.set_connection_timeout(5,0); client.set_read_timeout(5,0); client.set_write_timeout(5,0);
    // Preserve the explicit OpenSSL CA-bundle deployment contract when the
    // pinned library uses platform trust stores. Verification stays enabled.
    const char* certificate_file=std::getenv("SSL_CERT_FILE");
    const char* certificate_directory=std::getenv("SSL_CERT_DIR");
    if ((certificate_file && *certificate_file) || (certificate_directory && *certificate_directory))
        client.set_ca_cert_path(certificate_file ? certificate_file : "", certificate_directory ? certificate_directory : "");
}
}


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

    const char* journal=std::getenv("ALPACA_INTENT_JOURNAL");
    if(journal) cfg.intent_journal_path=journal;
    return cfg;
}

AlpacaExecution::AlpacaExecution(const AlpacaConfig& config)
    : config_(config) {
    if(!std::isfinite(config_.max_position_usd) || config_.max_position_usd<=0.0f ||
       !std::isfinite(config_.max_daily_drawdown) || config_.max_daily_drawdown<=0.0f || config_.max_daily_drawdown>1.0f) {
        std::cerr << "[AlpacaExecution] Disabled: invalid monetary/drawdown limits.\n";
        return;
    }
    if (config_.mock_mode) {
        // Simulation never constructs a client, even with real-looking URLs.
        is_enabled_ = true;
    } else if (!validateDestination()) {
        std::cerr << "[AlpacaExecution] Disabled: incompatible destination, mode, or live authorization.\n";
    } else if (!config_.api_key_id.empty() && !config_.secret_key.empty() &&
        config_.api_key_id.size()<=4096 && config_.secret_key.size()<=4096 &&
        std::all_of(config_.api_key_id.begin(),config_.api_key_id.end(),[](unsigned char c){ return c>=33 && c<=126; }) &&
        std::all_of(config_.secret_key.begin(),config_.secret_key.end(),[](unsigned char c){ return c>=33 && c<=126; })) {
        is_enabled_ = true;
        if(!config_.intent_journal_path.empty()) {
            try { journal_=std::make_unique<IntentJournal>(config_.intent_journal_path); if(journal_->hasPending()) triggerLockout("Pending durable intent requires broker reconciliation"); }
            catch(const std::exception& error) { is_enabled_=false; std::cerr << "[AlpacaExecution] " << error.what() << '\n'; }
        }
    } else {
        is_enabled_ = false;
        std::cerr << "[AlpacaExecution] Warning: Missing API credentials (ALPACA_API_KEY_ID/ALPACA_SECRET_KEY). Fail-closed mode active.\n";
    }
}

AlpacaExecution::~AlpacaExecution() = default;

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
    if(!token(request.symbol,32) || std::any_of(request.symbol.begin(),request.symbol.end(),[](unsigned char c){return c>='a'&&c<='z';}) || !std::isfinite(request.quantity) || request.quantity<=0.0f ||
       !std::isfinite(request.limit_price) || request.limit_price<0.0f ||
       (request.side!=OrderSide::BUY && request.side!=OrderSide::SELL) ||
       (!request.client_ref.empty() && !token(request.client_ref,48))) return {};
    const double notional=static_cast<double>(request.quantity)*request.limit_price;
    if(!std::isfinite(notional) || notional>config_.max_position_usd) return {};
    if(config_.mock_mode) {
        std::ostringstream out; out << "MOCK-ALPACA-" << order_counter_.fetch_add(1); return out.str();
    }
    // A reference quote cannot constrain market fills. Directional execution
    // therefore requires an explicit broker-enforced limit and durable identity.
    if(request.side!=OrderSide::BUY || request.limit_price<=0.0f || request.client_ref.empty() || !journal_) {
        std::cerr << "[AlpacaExecution] Bounded limit order, client_ref, and intent journal required.\n";
        return {};
    }
    std::ostringstream json;
    json.imbue(std::locale::classic());
    json << std::setprecision(std::numeric_limits<float>::max_digits10)
         << "{\"symbol\":\"" << request.symbol << "\",\"qty\":" << request.quantity
         << ",\"side\":\"" << sideToString(request.side) << "\",\"type\":\"limit\",\"limit_price\":" << request.limit_price
         << ",\"time_in_force\":\"day\",\"client_order_id\":\"" << request.client_ref << "\"}";
    const std::string body=json.str();
    const std::string fingerprint=intentDigest(broker_origin_+"\n"+config_.api_key_id+"\n"+body);
    try {
        std::string cached;
        if(journal_->lookup(request.client_ref,fingerprint,cached)) return cached;
        if(journal_->containsSymbol(request.symbol)) return {};
        if(journal_->reserved(request.symbol)+notional>config_.max_position_usd) return {};
        journal_->reserve(request.client_ref,fingerprint,request.symbol,notional);
    } catch(const std::exception& error) { triggerLockout(error.what()); return {}; }
    httplib::Client cli(broker_origin_); timeouts(cli);
    httplib::Headers headers={{"APCA-API-KEY-ID",config_.api_key_id},{"APCA-API-SECRET-KEY",config_.secret_key}};
    auto result=cli.Post("/v2/orders",headers,body,"application/json");
    const std::string id=result && (result->status==200 || result->status==201) ? acknowledgedId(result->body) : std::string();
    if(id.empty()) {
        triggerLockout("Order outcome unconfirmed; client_ref remains reserved pending reconciliation");
        return {};
    }
    try { journal_->acknowledge(request.client_ref,fingerprint,id); }
    catch(const std::exception& error) { triggerLockout(error.what()); return {}; }
    return id;
}

bool AlpacaExecution::cancelOrder(const std::string& order_id) {
    if (!is_enabled_ || !token(order_id)) return false;

    if (config_.mock_mode) {
        std::cout << "[AlpacaExecution] [MOCK CANCEL] order_id=" << order_id << "\n";
        return true;
    }

    httplib::Client cli(broker_origin_); timeouts(cli);
    httplib::Headers headers = {
        {"APCA-API-KEY-ID", config_.api_key_id},
        {"APCA-API-SECRET-KEY", config_.secret_key}
    };

    std::string path = "/v2/orders/" + order_id;
    auto res = cli.Delete(path.c_str(), headers);

    return (res && (res->status == 200 || res->status == 204));
}

bool AlpacaExecution::flattenPosition(const std::string& symbol) {
    if (!is_enabled_ || !token(symbol,32)) return false;

    if (config_.mock_mode) {
        std::cout << "[AlpacaExecution] [MOCK FLATTEN] symbol=" << symbol << "\n";
        return true;
    }

    httplib::Client cli(broker_origin_); timeouts(cli);
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

    httplib::Client cli(broker_origin_); timeouts(cli);
    httplib::Headers headers = {
        {"APCA-API-KEY-ID", config_.api_key_id},
        {"APCA-API-SECRET-KEY", config_.secret_key}
    };

    auto res = cli.Get("/v2/account", headers);
    if(res && res->status==200) {
        try {
            const auto fields=Detail::BrokerJson(res->body).parse();
            const auto equity=fields.find("equity");
            if(equity==fields.end() || equity->second.kind==Detail::JsonScalar::OTHER) return -1.0f;
            std::istringstream value(equity->second.text); value.imbue(std::locale::classic());
            double parsed; value >> std::noskipws >> parsed;
            if(!value || value.peek()!=std::char_traits<char>::eof() || !std::isfinite(parsed) || parsed<0.0 || parsed>std::numeric_limits<float>::max()) return -1.0f;
            return static_cast<float>(parsed);
        } catch(const std::exception&) { return -1.0f; }
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
