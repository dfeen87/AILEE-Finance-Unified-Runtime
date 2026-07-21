/*
 * AILLE Plugin — Live Advisory WebSocket Observer (AILLEE 6.2.0)
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#include "LiveAdvisoryObserver.hpp"
#include "../../../extensions/aille_btc.hpp"
#include "../../../extensions/aille_eth.hpp"
#include "../../../extensions/aille_oil.hpp"
#include "../../../extensions/aille_gold.hpp"
#include "../../../extensions/aille_silver.hpp"
#include "../../../extensions/aille_copper.hpp"
#include "../../../extensions/aille_natgas.hpp"
#include "../../../extensions/aille_platinum.hpp"
#include "../../../extensions/aille_forex_usd.hpp"
#include "../../../extensions/aille_macro.hpp"
#include "../../../extensions/aille_stabilizer.hpp"
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
    const MacroSignalAdvisory* macro,
    const MarketStabilizerAdvisory* stabilizer
) : port_(port), btc_(btc), eth_(eth), oil_(oil), gold_(gold), silver_(silver),
    copper_(copper), natgas_(natgas), platinum_(platinum), forex_(forex), macro_(macro),
    stabilizer_(stabilizer), running_(false) {

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

    json << "\"OIL\":{";
    if (oil_) json << "\"risk_score\":" << oil_->risk_score << ",\"recommended_weight\":" << oil_->recommended_weight;
    else json << "\"risk_score\":0,\"recommended_weight\":0";
    json << "},";

    json << "\"GOLD\":{";
    if (gold_) json << "\"risk_score\":" << gold_->risk_score << ",\"recommended_weight\":" << gold_->recommended_weight;
    else json << "\"risk_score\":0,\"recommended_weight\":0";
    json << "},";

    json << "\"SILVER\":{";
    if (silver_) json << "\"risk_score\":" << silver_->risk_score << ",\"recommended_weight\":" << silver_->recommended_weight;
    else json << "\"risk_score\":0,\"recommended_weight\":0";
    json << "},";

    json << "\"COPPER\":{";
    if (copper_) json << "\"risk_score\":" << copper_->risk_score << ",\"recommended_weight\":" << copper_->recommended_weight;
    else json << "\"risk_score\":0,\"recommended_weight\":0";
    json << "},";

    json << "\"NATGAS\":{";
    if (natgas_) json << "\"risk_score\":" << natgas_->risk_score << ",\"recommended_weight\":" << natgas_->recommended_weight;
    else json << "\"risk_score\":0,\"recommended_weight\":0";
    json << "},";

    json << "\"PLATINUM\":{";
    if (platinum_) json << "\"risk_score\":" << platinum_->risk_score << ",\"recommended_weight\":" << platinum_->recommended_weight;
    else json << "\"risk_score\":0,\"recommended_weight\":0";
    json << "},";

    json << "\"USD_FOREX\":{";
    if (forex_) json << "\"risk_score\":" << forex_->risk_score << ",\"recommended_weight\":" << forex_->recommended_weight;
    else json << "\"risk_score\":0,\"recommended_weight\":0";
    json << "},";

    json << "\"MacroSignal\":{";
    if (macro_) json << "\"risk_score\":" << macro_->macro_risk_score << ",\"recommended_weight\":" << macro_->recommended_macro_weight;
    else json << "\"risk_score\":0,\"recommended_weight\":0";
    json << "},";

    json << "\"MarketStabilizer\":{";
    if (stabilizer_) json << "\"risk_score\":" << stabilizer_->stabilization_risk_score << ",\"stabilization_factor\":" << stabilizer_->stabilization_factor << ",\"dynamic_clamp_limit\":" << stabilizer_->dynamic_clamp_limit;
    else json << "\"risk_score\":0,\"stabilization_factor\":1,\"dynamic_clamp_limit\":1";
    json << "}";

    json << "},";

    float total_exposure = 0.0f;
    if (btc_) total_exposure += btc_->recommended_weight;
    if (eth_) total_exposure += eth_->recommended_weight;
    if (oil_) total_exposure += oil_->recommended_weight;
    if (gold_) total_exposure += gold_->recommended_weight;
    if (silver_) total_exposure += silver_->recommended_weight;
    if (copper_) total_exposure += copper_->recommended_weight;
    if (natgas_) total_exposure += natgas_->recommended_weight;
    if (platinum_) total_exposure += platinum_->recommended_weight;
    if (forex_) total_exposure += forex_->recommended_weight;
    if (macro_) total_exposure += macro_->recommended_macro_weight;

    json << "\"exposure_balancing\":{";
    json << "\"total_exposure\":" << total_exposure;
    json << "}";

    json << "}";

    return json.str();
}

} // namespace Dashboard
} // namespace Plugins
} // namespace AILLE
