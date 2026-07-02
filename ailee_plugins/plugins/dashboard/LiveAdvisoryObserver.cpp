/*
 * AILLE Plugin — Live Advisory WebSocket Observer (AILLEE 6.2.0)
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#include "LiveAdvisoryObserver.hpp"
#include <sstream>
#include <iostream>

namespace AILLE {
namespace Plugins {
namespace Dashboard {

LiveAdvisoryObserver::LiveAdvisoryObserver(
    uint16_t port,
    const BTCAdvisory* btc,
    const ETHAdvisory* eth,
    const OILAdvisory* oil,
    const GOLDAdvisory* gold,
    const SILVERAdvisory* silver,
    const COPPERAdvisory* copper,
    const NATGASAdvisory* natgas,
    const PLATINUMAdvisory* platinum,
    const ForexUSDAdvisory* forex,
    const MacroSignalAdvisory* macro
) : port_(port), btc_(btc), eth_(eth), oil_(oil), gold_(gold), silver_(silver),
    copper_(copper), natgas_(natgas), platinum_(platinum), forex_(forex), macro_(macro),
    running_(false) {

    ws_server_.init_asio();

    // Disable logging to avoid clutter
    ws_server_.clear_access_channels(websocketpp::log::alevel::all);
    ws_server_.clear_error_channels(websocketpp::log::elevel::all);

    ws_server_.set_open_handler(bind(&LiveAdvisoryObserver::on_open, this, std::placeholders::_1));
    ws_server_.set_close_handler(bind(&LiveAdvisoryObserver::on_close, this, std::placeholders::_1));
}

LiveAdvisoryObserver::~LiveAdvisoryObserver() {
    stopServer();
}

void LiveAdvisoryObserver::on_open(websocketpp::connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.insert(hdl);
}

void LiveAdvisoryObserver::on_close(websocketpp::connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(hdl);
}

void LiveAdvisoryObserver::startServer() {
    if (running_) return;

    ws_server_.listen(port_);
    ws_server_.start_accept();

    running_ = true;
    server_thread_ = std::thread([this]() {
        try {
            ws_server_.run();
        } catch (const std::exception& e) {
            std::cerr << "WebSocket server error: " << e.what() << std::endl;
        }
    });
}

void LiveAdvisoryObserver::stopServer() {
    if (!running_) return;

    ws_server_.stop_listening();

    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto it : connections_) {
        websocketpp::lib::error_code ec;
        ws_server_.close(it, websocketpp::close::status::going_away, "Server shutting down", ec);
    }
    connections_.clear();

    running_ = false;

    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void LiveAdvisoryObserver::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto hdl : connections_) {
        websocketpp::lib::error_code ec;
        ws_server_.send(hdl, message, websocketpp::frame::opcode::text, ec);
    }
}

void LiveAdvisoryObserver::onSignalEvaluated(const std::vector<ModelSignal>& signals) {
    // Passive observer, do nothing before safety layer
}

void LiveAdvisoryObserver::onDecisionRouted(const Decision& decision) {
    // Generate JSON payload safely
    std::string payload = buildJSONPayload(decision);

    // Broadcast via WebSocket
    broadcast(payload);
}

std::string LiveAdvisoryObserver::buildJSONPayload(const Decision& decision) const {
    std::ostringstream json;

    json << "{";
    json << "\"timestamp_ns\":" << decision.timestamp_ns << ",";
    json << "\"decision\":{";
    json << "\"final_value\":" << decision.final_value << ",";
    json << "\"confidence\":" << decision.confidence << ",";
    json << "\"models_agreed\":" << decision.models_agreed;
    json << "},";

    // Nodes
    json << "\"advisories\":{";

    json << "\"BTC\":{";
    if (btc_) json << "\"risk_score\":" << btc_->risk_score << ",\"recommended_weight\":" << btc_->recommended_weight;
    else json << "\"risk_score\":0,\"recommended_weight\":0";
    json << "},";

    json << "\"ETH\":{";
    if (eth_) json << "\"risk_score\":" << eth_->risk_score << ",\"recommended_weight\":" << eth_->recommended_weight;
    else json << "\"risk_score\":0,\"recommended_weight\":0";
    json << "},";

    // Commodities average
    json << "\"Commodities\":{";
    float comm_risk = 0.0f;
    float comm_weight = 0.0f;
    int comm_count = 0;
    if (oil_) { comm_risk += oil_->risk_score; comm_weight += oil_->recommended_weight; comm_count++; }
    if (gold_) { comm_risk += gold_->risk_score; comm_weight += gold_->recommended_weight; comm_count++; }
    if (silver_) { comm_risk += silver_->risk_score; comm_weight += silver_->recommended_weight; comm_count++; }
    if (copper_) { comm_risk += copper_->risk_score; comm_weight += copper_->recommended_weight; comm_count++; }
    if (natgas_) { comm_risk += natgas_->risk_score; comm_weight += natgas_->recommended_weight; comm_count++; }
    if (platinum_) { comm_risk += platinum_->risk_score; comm_weight += platinum_->recommended_weight; comm_count++; }

    if (comm_count > 0) {
        json << "\"risk_score\":" << (comm_risk/comm_count) << ",";
        json << "\"recommended_weight\":" << (comm_weight/comm_count);
    } else {
        json << "\"risk_score\":0,\"recommended_weight\":0";
    }
    json << "},";

    json << "\"USD_FOREX\":{";
    if (forex_) json << "\"risk_score\":" << forex_->risk_score << ",\"recommended_weight\":" << forex_->recommended_weight;
    else json << "\"risk_score\":0,\"recommended_weight\":0";
    json << "},";

    json << "\"MacroSignal\":{";
    if (macro_) json << "\"risk_score\":" << macro_->macro_risk_score << ",\"recommended_weight\":" << macro_->recommended_macro_weight;
    else json << "\"risk_score\":0,\"recommended_weight\":0";
    json << "}";

    json << "}";
    json << "}";

    return json.str();
}

} // namespace Dashboard
} // namespace Plugins
} // namespace AILLE
