/*
 * AILLE Plugin — Live Advisory WebSocket Observer (AILLEE 6.2.0)
 * AI-Load Integrity and Layered Evaluation
 *
 * Header file
 */

#ifndef AILLE_LIVE_ADVISORY_OBSERVER_HPP
#define AILLE_LIVE_ADVISORY_OBSERVER_HPP

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <set>
#include <mutex>
#include <thread>
#include <atomic>
#include "../../IAnalyticsObserver.hpp"
#include "../../../aille.hpp"

namespace AILLE {
namespace Plugins {
namespace Dashboard {

class LiveAdvisoryObserver : public IAnalyticsObserver {
public:
    typedef websocketpp::server<websocketpp::config::asio> server;

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
        const MacroSignalAdvisory* macro,
        const MarketStabilizerAdvisory* stabilizer = nullptr
    );

    ~LiveAdvisoryObserver() override;

    std::string name() const override { return "live-advisory-observer"; }

    void startServer();
    void stopServer();

    void onSignalEvaluated(const std::vector<ModelSignal>& signals) override;
    void onDecisionRouted(const Decision& decision) override;

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
    const MarketStabilizerAdvisory* stabilizer_;

    server ws_server_;
    std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> connections_;
    std::mutex connections_mutex_;
    std::thread server_thread_;
    std::atomic<bool> running_;
};

} // namespace Dashboard
} // namespace Plugins
} // namespace AILLE

#endif // AILLE_LIVE_ADVISORY_OBSERVER_HPP
