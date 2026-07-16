/*
 * AILLE Plugin — Live Advisory WebSocket Observer (AILLEE 6.2.0)
 * AI-Load Integrity and Layered Evaluation
 *
 * Implementation file
 */

#include "LiveAdvisoryObserver.hpp"

#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/server.hpp>

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
)
    : port_(port),
      btc_(btc),
      eth_(eth),
      oil_(oil),
      gold_(gold),
      silver_(silver),
      copper_(copper),
      natgas_(natgas),
      platinum_(platinum),
      forex_(forex),
      macro_(macro),
      running_(false)
{
    ws_server_.init_asio();

    ws_server_.set_open_handler(
        websocketpp::lib::bind(
            &LiveAdvisoryObserver::on_open,
            this,
            websocketpp::lib::placeholders::_1
        )
    );

    ws_server_.set_close_handler(
        websocketpp::lib::bind(
            &LiveAdvisoryObserver::on_close,
            this,
            websocketpp::lib::placeholders::_1
        )
    );

    ws_server_.set_reuse_addr(true);
}

LiveAdvisoryObserver::~LiveAdvisoryObserver() {
    stopServer();
}

void LiveAdvisoryObserver::startServer() {
    if (running_.load()) {
        return;
    }

    try {
        ws_server_.listen(port_);
        ws_server_.start_accept();

        running_.store(true);

        server_thread_ = std::thread([this]() {
            try {
                ws_server_.run();
            } catch (const std::exception& e) {
                std::cerr << "[LiveAdvisoryObserver] WebSocket server error: "
                          << e.what() << "\n";
            }
        });

        std::cout << "[LiveAdvisoryObserver] WebSocket server started on port "
                  << port_ << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[LiveAdvisoryObserver] Failed to start WebSocket server: "
                  << e.what() << "\n";
    }
}

void LiveAdvisoryObserver::stopServer() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    try {
        ws_server_.stop_listening();
        ws_server_.stop();
    } catch (const std::exception& e) {
        std::cerr << "[LiveAdvisoryObserver] Error stopping WebSocket server: "
                  << e.what() << "\n";
    }

    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_.clear();
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    std::cout << "[LiveAdvisoryObserver] WebSocket server stopped\n";
}

void LiveAdvisoryObserver::on_open(websocketpp::connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.insert(hdl);
    std::cout << "[LiveAdvisoryObserver] Client connected\n";
}

void LiveAdvisoryObserver::on_close(websocketpp::connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(hdl);
    std::cout << "[LiveAdvisoryObserver] Client disconnected\n";
}

void LiveAdvisoryObserver::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto const& hdl : connections_) {
        try {
            ws_server_.send(hdl, message, websocketpp::frame::opcode::text);
        } catch (const std::exception& e) {
            std::cerr << "[LiveAdvisoryObserver] Broadcast error: "
                      << e.what() << "\n";
        }
    }
}

void LiveAdvisoryObserver::onSignalEvaluated(const std::vector<ModelSignal>& signals) {
    // Optional: you can stream raw signal evaluations here if desired.
    // For now, we focus on decisions in onDecisionRouted().
    (void)signals;
}

void LiveAdvisoryObserver::onDecisionRouted(const Decision& decision) {
    if (!running_.load()) {
        return;
    }

    std::string payload = buildJSONPayload(decision);
    broadcast(payload);
}

std::string LiveAdvisoryObserver::buildJSONPayload(const Decision& decision) const {
    std::ostringstream oss;

    // Basic decision payload
    oss << "{";
    oss << "\"type\":\"decision\",";
    oss << "\"final_value\":" << decision.final_value << ",";
    oss << "\"confidence\":" << decision.confidence << ",";
    oss << "\"timestamp\":" << decision.timestamp << ",";

    // Advisory snapshot (simplified example)
    oss << "\"advisories\":{";

    if (btc_) {
        oss << "\"btc\":{"
            << "\"signal\":" << btc_->currentSignal()
            << "},";
    }
    if (eth_) {
        oss << "\"eth\":{"
            << "\"signal\":" << eth_->currentSignal()
            << "},";
    }
    if (oil_) {
        oss << "\"oil\":{"
            << "\"signal\":" << oil_->currentSignal()
            << "},";
    }
    if (gold_) {
        oss << "\"gold\":{"
            << "\"signal\":" << gold_->currentSignal()
            << "},";
    }
    if (silver_) {
        oss << "\"silver\":{"
            << "\"signal\":" << silver_->currentSignal()
            << "},";
    }
    if (copper_) {
        oss << "\"copper\":{"
            << "\"signal\":" << copper_->currentSignal()
            << "},";
    }
    if (natgas_) {
        oss << "\"natgas\":{"
            << "\"signal\":" << natgas_->currentSignal()
            << "},";
    }
    if (platinum_) {
        oss << "\"platinum\":{"
            << "\"signal\":" << platinum_->currentSignal()
            << "},";
    }
    if (forex_) {
        oss << "\"forex_usd\":{"
            << "\"signal\":" << forex_->currentSignal()
            << "},";
    }
    if (macro_) {
        oss << "\"macro\":{"
            << "\"signal\":" << macro_->currentSignal()
            << "}";
    }

    // Remove trailing comma if needed (simple approach: ensure last entry doesn't add one)
    oss << "},";

    // You can add more fields here: routed_model_ids, fallback_used, etc.
    oss << "\"metadata\":{";
    oss << "\"routed_model_count\":" << decision.routed_models.size();
    oss << "}";
    oss << "}";

    return oss.str();
}

} // namespace Dashboard
} // namespace Plugins
} // namespace AILLE
