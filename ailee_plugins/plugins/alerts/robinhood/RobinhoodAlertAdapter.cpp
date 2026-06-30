/*
 * AILLE Plugin — RobinhoodAlertAdapter (passive alerts only)
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 *
 * Example adapter for trading-agent alerts such as Robinhood watchlist or
 * notification bridges. This adapter never executes trades: it only emits
 * human/operator-facing alerts derived from AILLE decisions.
 */

#include "../../../ITradingAlertAdapter.hpp"
#include "../../../../ailee_plugins/PluginRegistry.hpp"

#include <iostream>
#include <string>
#include <utility>

namespace AILLE {
namespace Plugins {
namespace Robinhood {

class RobinhoodAlertAdapter : public ITradingAlertAdapter {
public:
    explicit RobinhoodAlertAdapter(std::string default_symbol = "AAPL")
        : default_symbol_(std::move(default_symbol)) {}

    std::string name() const override { return "robinhood-alerts"; }

    bool sendAlert(const TradingAlert& alert) override {
        if (alert.symbol.empty()) {
            std::cerr << "[RobinhoodAlertAdapter] alert skipped: symbol is empty\n";
            return false;
        }

        std::cout << "[RobinhoodAlertAdapter] alert"
                  << " symbol=" << alert.symbol
                  << " side=" << sideToString(alert.side)
                  << " confidence=" << alert.confidence
                  << " signal=" << alert.signal_value
                  << " severity=" << alert.severity
                  << " client_ref=" << alert.client_ref
                  << " message=\"" << alert.message << "\"\n";
        return true;
    }

    bool alertDecision(const Decision& decision,
                       const std::string& symbol,
                       const std::string& client_ref = {}) override {
        const std::string& sym = symbol.empty() ? default_symbol_ : symbol;
        return ITradingAlertAdapter::alertDecision(decision, sym, client_ref);
    }

private:
    std::string default_symbol_;

    static const char* sideToString(AlertSide side) {
        switch (side) {
            case AlertSide::ALERT_BUY:  return "buy-alert";
            case AlertSide::ALERT_SELL: return "sell-alert";
            case AlertSide::ALERT_HOLD: return "hold-alert";
            default:                    return "unknown-alert";
        }
    }
};

namespace {
    struct RobinhoodAlertRegistrar {
        RobinhoodAlertRegistrar() {
            PluginRegistry::instance().registerTradingAlertAdapter(
                "robinhood-alerts",
                []() -> std::unique_ptr<ITradingAlertAdapter> {
                    return std::make_unique<RobinhoodAlertAdapter>();
                }
            );
        }
    };
    const RobinhoodAlertRegistrar g_robinhood_alert_registrar;
} // anonymous namespace

} // namespace Robinhood
} // namespace Plugins
} // namespace AILLE
