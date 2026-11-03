/*
 * AILLE Plugin — Alpaca Execution Header
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#ifndef AILEE_PLUGINS_ALPACA_EXECUTION_HPP
#define AILEE_PLUGINS_ALPACA_EXECUTION_HPP

#include "../../../IExecutionProvider.hpp"
#include <string>
#include <atomic>
#include <memory>

namespace AILLE {
namespace Plugins {
namespace Alpaca {

class IntentJournal;


struct AlpacaConfig {
    std::string api_key_id;
    std::string secret_key;
    std::string base_url;      ///< Broker origin: official HTTPS or explicit loopback HTTP/HTTPS.
    bool is_live;              ///< Default: false (paper mode)
    bool mock_mode;            ///< If true, simulates network calls (for testing/offline)
    float max_position_usd;    ///< BUY limit notional cap; reserved symbol blocks further orders until reconciliation
    float max_daily_drawdown;  ///< Max daily drawdown fraction before locking execution
    std::string intent_journal_path; ///< Required durable reservation store for non-mock directional submission.
    bool live_authorized{false}; ///< Explicit operator consent; never inferred from environment/URL.

    AlpacaConfig()
        : base_url("https://paper-api.alpaca.markets"),
          is_live(false),
          mock_mode(false),
          max_position_usd(50000.0f),
          max_daily_drawdown(0.05f) {}
};

class AlpacaExecution : public IExecutionProvider {
public:
    explicit AlpacaExecution(const AlpacaConfig& config = AlpacaConfig());
    ~AlpacaExecution() override;

    std::string name() const override { return "alpaca"; }

    /// Read credentials from environment variables ALPACA_API_KEY_ID, ALPACA_SECRET_KEY, ALPACA_BASE_URL.
    /// Does not grant live mode or consent. Construction validates the destination
    /// and disables non-mock instances with missing keys or incompatible authorization.
    static AlpacaConfig loadConfigFromEnv(bool fail_closed = true);

    /// Non-mock: explicit bounded LIMIT + client_ref + private durable journal required.
    /// Returns an acknowledged ID, never proof of a fill.
    std::string submitOrder(const OrderRequest& request) override;

    /// Cancel an order by ID via Alpaca REST API (/v2/orders/{id}).
    bool cancelOrder(const std::string& order_id) override;

    /// Request a close for a symbol (/v2/positions/{symbol}). True means accepted, not confirmed FLAT.
    bool flattenPosition(const std::string& symbol);

    /// Query account buying power / equity (/v2/account). Returns equity in USD, or -1.0f on error.
    float getAccountEquity();

    /// Check whether configuration permits operations. Lockout is reported separately.
    bool isEnabled() const { return is_enabled_; }
    bool canSubmit() const { return is_enabled_ && !locked_out_ && (config_.mock_mode || bool(journal_)); }

    /// Force lockout / disable execution (e.g., kill switch or drawdown breach).
    void triggerLockout(const std::string& reason);

    /// Check if currently in lockout state.
    bool isLockedOut() const { return locked_out_; }

    /// Return the lockout reason if locked out.
    std::string lockoutReason() const { return lockout_reason_; }

    const AlpacaConfig& config() const { return config_; }

private:
    AlpacaConfig config_;
    bool is_enabled_{false};
    bool locked_out_{false};
    std::string lockout_reason_;
    std::atomic<uint64_t> order_counter_{1};
    std::unique_ptr<IntentJournal> journal_;
    std::string broker_origin_; // Validated once before any network client is constructed.

    bool validateDestination();
    std::string sideToString(OrderSide side) const;
};

} // namespace Alpaca
} // namespace Plugins
} // namespace AILLE

#endif // AILEE_PLUGINS_ALPACA_EXECUTION_HPP
