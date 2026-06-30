/*
 * AILLE Plugin Interface — IExecutionProvider
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 *
 * Stable interface for execution plugins. Implementors consume validated
 * AILLE Decision objects and route them to an order management system,
 * paper-trading simulator, or downstream risk layer.
 *
 * Decision Routing pattern (implementors must follow):
 *   DECISION_VALID          → submitOrder() at full confidence
 *   FALLBACK_ACTIVATED      → submitOrder() at reduced confidence (× 0.5)
 *   REJECTED_LOW_CONFIDENCE → cancelOrder() / do not trade
 *   REJECTED_NO_CONSENSUS   → cancelOrder() / do not trade
 *
 * `decision.final_value` is a tanh-normalised direction, not a monetary
 * amount. Scaling to actual position size is the plugin's responsibility.
 *
 * Implementations must not modify the Decision object.
 */

#ifndef AILEE_PLUGINS_IEXECUTION_PROVIDER_HPP
#define AILEE_PLUGINS_IEXECUTION_PROVIDER_HPP

#include <string>
#include <cstdint>

#include "../aille.hpp"

namespace AILLE {
namespace Plugins {

// ============================================================================
// ORDER REQUEST VALUE OBJECT
// ============================================================================

/// Direction of the order submitted to the broker.
enum class OrderSide {
    BUY,
    SELL,
    FLAT  ///< Close / reduce position; no new directional trade
};

/// A fully-specified order ready for submission.
struct OrderRequest {
    std::string  symbol;        ///< Ticker to trade
    OrderSide    side;          ///< BUY, SELL, or FLAT
    float        quantity;      ///< Quantity in base units (shares, contracts, …)
    float        limit_price;   ///< 0.0 = market order; > 0 = limit order
    std::string  client_ref;    ///< Caller-assigned idempotency key
    uint64_t     timestamp_ns;  ///< Time of order construction
};

// ============================================================================
// IEXECUTION PROVIDER INTERFACE
// ============================================================================

/// Stable interface for all execution plugins.
///
/// Implementations are responsible for:
///   1. Translating a Decision into an OrderRequest (scaling, side logic).
///   2. Submitting or cancelling the order via the broker API.
///   3. Returning a unique order identifier string, or empty string on error.
///
/// Thread safety: AILLE does not call execution providers concurrently on
/// the same instance. Implementors need not synchronise submitOrder /
/// cancelOrder internally unless they share state across instances.
class IExecutionProvider {
public:
    virtual ~IExecutionProvider() = default;

    /// Return the plugin's human-readable identifier (e.g. "alpaca", "ib").
    virtual std::string name() const = 0;

    /// Submit an order derived from an AILLE Decision.
    ///
    /// Implementors should apply the Decision Routing pattern:
    ///   - DECISION_VALID:        submit at full size
    ///   - FALLBACK_ACTIVATED:    submit at reduced size (confidence × 0.5)
    ///   - REJECTED_*:            do not call submitOrder; call cancelOrder
    ///
    /// @param request  Fully-specified order (see OrderRequest).
    /// @return         Broker-assigned order ID, or empty string on failure.
    virtual std::string submitOrder(const OrderRequest& request) = 0;

    /// Cancel a previously submitted order by its broker-assigned ID.
    ///
    /// @param order_id  The ID returned by a prior submitOrder() call.
    /// @return          true if the cancellation was accepted.
    virtual bool cancelOrder(const std::string& order_id) = 0;

    /// Convenience helper: route a Decision through the standard pattern.
    ///
    /// This default implementation encodes the Decision Routing pattern so
    /// that simple plugins can call routeDecision() without reimplementing
    /// the switch logic. Override if the broker requires custom routing.
    ///
    /// @param decision  Validated AILLE Decision.
    /// @param symbol    Ticker to trade.
    /// @param base_qty  Maximum position quantity (scaled by confidence).
    /// @return          Broker order ID, or empty string if no trade placed.
    virtual std::string routeDecision(const Decision& decision,
                                      const std::string& symbol,
                                      float base_qty) {
        switch (decision.status) {
            case DECISION_VALID: {
                OrderRequest req = buildRequest(decision, symbol,
                                                base_qty, /*scale=*/1.0f);
                return submitOrder(req);
            }
            case FALLBACK_ACTIVATED: {
                OrderRequest req = buildRequest(decision, symbol,
                                                base_qty, /*scale=*/0.5f);
                return submitOrder(req);
            }
            case REJECTED_LOW_CONFIDENCE:
            case REJECTED_NO_CONSENSUS:
            case ERROR_NO_MODELS:
            default:
                return {};   // do not trade
        }
    }

protected:
    /// Build an OrderRequest from a Decision, applying a confidence scale.
    static OrderRequest buildRequest(const Decision& decision,
                                     const std::string& symbol,
                                     float base_qty,
                                     float confidence_scale) {
        using namespace std::chrono;
        OrderRequest req;
        req.symbol       = symbol;
        req.quantity     = base_qty * decision.confidence * confidence_scale;
        req.limit_price  = 0.0f;  // market order by default
        req.timestamp_ns = static_cast<uint64_t>(
            duration_cast<nanoseconds>(
                high_resolution_clock::now().time_since_epoch()
            ).count()
        );

        if (decision.final_value > 0.0f)
            req.side = OrderSide::BUY;
        else if (decision.final_value < 0.0f)
            req.side = OrderSide::SELL;
        else
            req.side = OrderSide::FLAT;

        return req;
    }
};

} // namespace Plugins
} // namespace AILLE

#endif // AILEE_PLUGINS_IEXECUTION_PROVIDER_HPP
