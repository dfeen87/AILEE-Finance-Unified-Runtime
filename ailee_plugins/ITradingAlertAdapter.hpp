/*
 * AILLE Plugin Interface — ITradingAlertAdapter
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: Non-Commercial (see LICENSE)
 *
 * Passive adapter interface for trading-agent alerts. Implementors transform
 * validated AILLE Decision objects into broker- or agent-specific alert
 * payloads (for example, Robinhood-compatible notification bridges) without
 * executing trades or placing/cancelling orders.
 */

#ifndef AILEE_PLUGINS_ITRADING_ALERT_ADAPTER_HPP
#define AILEE_PLUGINS_ITRADING_ALERT_ADAPTER_HPP

#include <chrono>
#include <cstdint>
#include <string>

#include "../aille.hpp"

namespace AILLE {
namespace Plugins {

// ============================================================================
// ALERT REQUEST VALUE OBJECT
// ============================================================================

/// Directional alert emitted for a trading agent. ALERT_HOLD is informational
/// only and must not be interpreted as a trade instruction.
enum class AlertSide {
    ALERT_BUY,
    ALERT_SELL,
    ALERT_HOLD
};

/// Fully-specified passive alert payload ready for delivery to a notifier.
struct TradingAlert {
    std::string symbol;        ///< Ticker associated with the decision.
    AlertSide   side;          ///< BUY/SELL/HOLD recommendation for alerting.
    float       confidence;    ///< Decision confidence after any alert scaling.
    float       signal_value;  ///< Decision final_value for downstream context.
    std::string severity;      ///< info, warning, or critical.
    std::string message;       ///< Human-readable alert text.
    std::string client_ref;    ///< Caller-assigned idempotency/correlation key.
    uint64_t    timestamp_ns;  ///< Time of alert construction.
};

// ============================================================================
// ITRADING ALERT ADAPTER INTERFACE
// ============================================================================

/// Stable interface for passive trading-agent alert adapters.
///
/// Implementations are responsible for:
///   1. Translating a Decision into a TradingAlert.
///   2. Delivering that alert to a notification sink, webhook, queue, or log.
///   3. Never placing, cancelling, or modifying broker orders.
///
/// This interface intentionally does not expose submitOrder() or cancelOrder().
class ITradingAlertAdapter {
public:
    virtual ~ITradingAlertAdapter() = default;

    /// Return the adapter's human-readable identifier (e.g. "robinhood-alerts").
    virtual std::string name() const = 0;

    /// Deliver an already-built alert.
    ///
    /// @param alert Passive alert payload.
    /// @return true if the adapter accepted the alert for delivery.
    virtual bool sendAlert(const TradingAlert& alert) = 0;

    /// Convenience helper: route a Decision through the standard alert pattern.
    ///
    /// DECISION_VALID and FALLBACK_ACTIVATED produce directional BUY/SELL/HOLD
    /// alerts. Rejected decisions produce HOLD alerts so operators can monitor
    /// risk blocks without creating executable instructions.
    virtual bool alertDecision(const Decision& decision,
                               const std::string& symbol,
                               const std::string& client_ref = {}) {
        return sendAlert(buildAlert(decision, symbol, client_ref));
    }

protected:
    static TradingAlert buildAlert(const Decision& decision,
                                   const std::string& symbol,
                                   const std::string& client_ref) {
        using namespace std::chrono;

        TradingAlert alert;
        alert.symbol       = symbol;
        alert.confidence   = decision.confidence;
        alert.signal_value = decision.final_value;
        alert.client_ref   = client_ref;
        alert.timestamp_ns = static_cast<uint64_t>(
            duration_cast<nanoseconds>(
                high_resolution_clock::now().time_since_epoch()
            ).count()
        );

        if (decision.status == DECISION_VALID || decision.status == FALLBACK_ACTIVATED) {
            if (decision.final_value > 0.0f) {
                alert.side = AlertSide::ALERT_BUY;
            } else if (decision.final_value < 0.0f) {
                alert.side = AlertSide::ALERT_SELL;
            } else {
                alert.side = AlertSide::ALERT_HOLD;
            }
            alert.severity = (decision.status == FALLBACK_ACTIVATED) ? "warning" : "info";
            alert.message = "Passive trading alert: validated AILLE decision; no order executed.";
        } else {
            alert.side = AlertSide::ALERT_HOLD;
            alert.severity = "warning";
            alert.message = "Passive trading alert: decision rejected or unavailable; no order executed.";
        }

        if (!decision.reasoning.empty()) {
            alert.message += " Reason: " + decision.reasoning;
        }

        return alert;
    }
};

} // namespace Plugins
} // namespace AILLE

#endif // AILEE_PLUGINS_ITRADING_ALERT_ADAPTER_HPP
