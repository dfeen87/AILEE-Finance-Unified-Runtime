/*
 * AILLE Plugin — Live Advisory WebSocket Observer (AILLEE 6.2.0)
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#ifndef AILEE_PLUGINS_LIVE_ADVISORY_OBSERVER_HPP
#define AILEE_PLUGINS_LIVE_ADVISORY_OBSERVER_HPP

#include "../../IAnalyticsObserver.hpp"
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

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <set>

// Use standalone ASIO for websocketpp
#define ASIO_STANDALONE
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

namespace AILLE {
namespace Plugins {
namespace Dashboard {

typedef websocketpp::server<websocketpp::config::asio> server;

class LiveAdvisoryObserver : public IAnalyticsObserver {
public:
    LiveAdvisoryObserver(
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
    );

    ~LiveAdvisoryObserver() override;

    std::string name() const override { return "LiveAdvisoryDashboardFeed"; }

    void onSignalEvaluated(const std::vector<ModelSignal>& signals) override;
    void onDecisionRouted(const Decision& decision) override;

    void startServer();
    void stopServer();

private:
    void on_open(websocketpp::connection_hdl hdl);
    void on_close(websocketpp::connection_hdl hdl);
    void broadcast(const std::string& message);
    std::string buildJSONPayload(const Decision& decision) const;

    uint16_t port_;
    const BTCAdvisory* btc_;
    const ETHAdvisory* eth_;
    const OILAdvisory* oil_;
    const GOLDAdvisory* gold_;
    const SILVERAdvisory* silver_;
    const COPPERAdvisory* copper_;
    const NATGASAdvisory* natgas_;
    const PLATINUMAdvisory* platinum_;
    const ForexUSDAdvisory* forex_;
    const MacroSignalAdvisory* macro_;

    server ws_server_;
    std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> connections_;
    std::mutex connections_mutex_;
    std::thread server_thread_;
    std::atomic<bool> running_;
};

} // namespace Dashboard
} // namespace Plugins
} // namespace AILLE

#endif // AILEE_PLUGINS_LIVE_ADVISORY_OBSERVER_HPP
