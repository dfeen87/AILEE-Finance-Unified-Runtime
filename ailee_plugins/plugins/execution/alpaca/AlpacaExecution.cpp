/*
 * AILLE Plugin — AlpacaExecution (example implementation)
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: Non-Commercial (see LICENSE)
 *
 * Minimal, dependency-free example showing how an execution plugin
 * implements IExecutionProvider and consumes AILLE Decision objects.
 *
 * NOTE: This file uses mock/placeholder data. No real API calls are made
 *       and no API keys are stored here. Replace the stub implementations
 *       with live HTTP requests before using in a production environment.
 *
 * Decision Routing pattern applied here:
 *   DECISION_VALID        → submitOrder() at full confidence scaling
 *   FALLBACK_ACTIVATED    → submitOrder() at 50% confidence scaling
 *   REJECTED_*            → no order placed; decision is logged
 *
 * Integration:
 *   #include "ailee_plugins/plugins/execution/alpaca/AlpacaExecution.cpp"
 *
 *   // Register once at startup
 *   AILLE::Plugins::PluginRegistry::instance().registerExecutionProvider(
 *       "alpaca",
 *       []{ return std::make_unique<AILLE::Plugins::Alpaca::AlpacaExecution>("AAPL", 100.0f); }
 *   );
 *
 *   // Route a decision
 *   auto exec = AILLE::Plugins::PluginRegistry::instance().createExecutionProvider("alpaca");
 *   exec->routeDecision(decision, "AAPL", 100.0f);
 */

#include "../../../IExecutionProvider.hpp"
#include "../../../../ailee_plugins/PluginRegistry.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <sstream>

namespace AILLE {
namespace Plugins {
namespace Alpaca {

// ============================================================================
// ALPACA EXECUTION PLUGIN
// ============================================================================

class AlpacaExecution : public IExecutionProvider {
public:
    /// @param default_symbol  Ticker used when no symbol is passed to routeDecision.
    /// @param base_qty        Maximum order quantity (shares) before confidence scaling.
    explicit AlpacaExecution(const std::string& default_symbol = "AAPL",
                             float base_qty = 100.0f)
        : default_symbol_(default_symbol), base_qty_(base_qty) {}

    std::string name() const override { return "alpaca"; }

    /// Submit an order to Alpaca (placeholder — returns a synthetic order ID).
    ///
    /// In a real implementation, replace sendOrderRequest() with an HTTPS
    /// POST to https://api.alpaca.markets/v2/orders using the bearer token
    /// retrieved from a secure credential store (never hardcoded).
    std::string submitOrder(const OrderRequest& request) override {
        if (request.quantity <= 0.0f) {
            std::cerr << "[AlpacaExecution] submitOrder skipped: quantity <= 0\n";
            return {};
        }

        std::string side_str = sideToString(request.side);
        std::string order_id = generateOrderId();

        // Stub: log what would be sent to the Alpaca REST API
        std::cout << "[AlpacaExecution] submitOrder"
                  << " symbol="    << request.symbol
                  << " side="      << side_str
                  << " qty="       << request.quantity
                  << " limit="     << request.limit_price
                  << " order_id="  << order_id
                  << "\n";

        // Simulate a successful broker acknowledgement
        sendOrderRequest(request, order_id);

        return order_id;
    }

    /// Cancel an order by its broker-assigned ID.
    ///
    /// In a real implementation, issue a DELETE request to
    /// https://api.alpaca.markets/v2/orders/{order_id}.
    bool cancelOrder(const std::string& order_id) override {
        if (order_id.empty()) return false;

        std::cout << "[AlpacaExecution] cancelOrder order_id=" << order_id << "\n";
        // Stub: simulate successful cancellation
        return true;
    }

    /// Override routeDecision to log rejected decisions before delegating
    /// to the base-class Decision Routing pattern.
    std::string routeDecision(const Decision& decision,
                              const std::string& symbol,
                              float base_qty) override {
        const std::string& sym = symbol.empty() ? default_symbol_ : symbol;
        float qty = (base_qty > 0.0f) ? base_qty : base_qty_;

        if (decision.status == REJECTED_LOW_CONFIDENCE ||
            decision.status == REJECTED_NO_CONSENSUS   ||
            decision.status == ERROR_NO_MODELS) {
            std::cout << "[AlpacaExecution] decision rejected — no order placed."
                      << " reason=" << decision.reasoning << "\n";
            return {};
        }

        // Delegate to base-class routing (applies confidence scaling)
        return IExecutionProvider::routeDecision(decision, sym, qty);
    }

private:
    std::string default_symbol_;
    float       base_qty_;

    // Monotonically increasing order counter (thread-safe)
    std::atomic<uint64_t> order_counter_{1};

    // ---- Stub helpers -------------------------------------------------------

    std::string generateOrderId() {
        std::ostringstream oss;
        oss << "ALPACA-" << order_counter_.fetch_add(1);
        return oss.str();
    }

    static std::string sideToString(OrderSide side) {
        switch (side) {
            case OrderSide::BUY:  return "buy";
            case OrderSide::SELL: return "sell";
            default:              return "flat";
        }
    }

    /// Placeholder for a real Alpaca REST API call.
    static void sendOrderRequest(const OrderRequest& /*request*/,
                                 const std::string&  /*order_id*/) {
        // TODO: implement HTTP POST to Alpaca /v2/orders
        // Use a secure credential store — never hardcode API keys.
    }
};

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

/// Registers this plugin with the global PluginRegistry at static-init time.
/// Link this translation unit to activate auto-registration.
namespace {
    struct AlpacaExecutionRegistrar {
        AlpacaExecutionRegistrar() {
            PluginRegistry::instance().registerExecutionProvider(
                "alpaca",
                []() -> std::unique_ptr<IExecutionProvider> {
                    return std::make_unique<AlpacaExecution>();
                }
            );
        }
    };
    // NOLINTNEXTLINE(cert-err58-cpp) — intentional static initialisation
    const AlpacaExecutionRegistrar g_alpaca_registrar;
} // anonymous namespace

} // namespace Alpaca
} // namespace Plugins
} // namespace AILLE
