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

struct AlpacaConfig {
    std::string api_key_id;
    std::string secret_key;
    std::string base_url;      ///< Default: https://paper-api.alpaca.markets
    bool is_live;              ///< Default: false (paper mode)
    bool mock_mode;            ///< If true, simulates network calls (for testing/offline)
    float max_position_usd;    ///< Max dollar exposure allowed per symbol
    float max_daily_drawdown;  ///< Max daily drawdown fraction before locking execution

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
    ~AlpacaExecution() override = default;

    std::string name() const override { return "alpaca"; }

    /// Read credentials from environment variables ALPACA_API_KEY_ID, ALPACA_SECRET_KEY, ALPACA_BASE_URL.
    /// If fail_closed is true and keys are missing/empty, marks the instance as disabled.
    static AlpacaConfig loadConfigFromEnv(bool fail_closed = true);

    /// Submit an order to Alpaca REST API (/v2/orders).
    std::string submitOrder(const OrderRequest& request) override;

    /// Cancel an order by ID via Alpaca REST API (/v2/orders/{id}).
    bool cancelOrder(const std::string& order_id) override;

    /// Close / flatten position for a symbol (/v2/positions/{symbol}).
    bool flattenPosition(const std::string& symbol);

    /// Query account buying power / equity (/v2/account). Returns equity in USD, or -1.0f on error.
    float getAccountEquity();

    /// Check if execution is enabled and healthy (credentials present, not locked out).
    bool isEnabled() const { return is_enabled_; }

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

    std::string parseHost(const std::string& url) const;
    int parsePort(const std::string& url) const;
    bool isHttps(const std::string& url) const;
    std::string sideToString(OrderSide side) const;
};

} // namespace Alpaca
} // namespace Plugins
} // namespace AILLE

#endif // AILEE_PLUGINS_ALPACA_EXECUTION_HPP
