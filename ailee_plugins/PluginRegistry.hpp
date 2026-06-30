/*
 * AILLE Plugin Registry
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: Non-Commercial (see LICENSE)
 *
 * Thread-safe singleton registry for AILLE plugins.
 *
 * Plugins are registered by name with a factory callable that returns a
 * std::unique_ptr to the plugin instance. Registration is typically done
 * at program startup before the first call to the AILLE engine.
 *
 * Usage:
 *   // Registration (once at startup)
 *   AILLE::Plugins::PluginRegistry::instance()
 *       .registerMarketData("yahoo", []{ return std::make_unique<YahooMarketData>(); });
 *
 *   // Retrieval (any time after registration)
 *   auto md = AILLE::Plugins::PluginRegistry::instance()
 *                 .createMarketData("yahoo");
 *
 * Design notes:
 *   - No global mutable state outside the singleton instance.
 *   - All public methods are guarded by a single std::mutex.
 *   - Factories return std::unique_ptr; the registry retains no ownership
 *     of the constructed plugins.
 */

#ifndef AILEE_PLUGINS_PLUGIN_REGISTRY_HPP
#define AILEE_PLUGINS_PLUGIN_REGISTRY_HPP

#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "IMarketDataSource.hpp"
#include "IExecutionProvider.hpp"
#include "IAnalyticsObserver.hpp"
#include "ITradingAlertAdapter.hpp"

namespace AILLE {
namespace Plugins {

// ============================================================================
// FACTORY TYPE ALIASES
// ============================================================================

using MarketDataFactory    = std::function<std::unique_ptr<IMarketDataSource>()>;
using ExecutionFactory     = std::function<std::unique_ptr<IExecutionProvider>()>;
using AnalyticsFactory     = std::function<std::unique_ptr<IAnalyticsObserver>()>;
using TradingAlertFactory  = std::function<std::unique_ptr<ITradingAlertAdapter>()>;

// ============================================================================
// PLUGIN REGISTRY
// ============================================================================

/// Thread-safe singleton registry. All registered plugin names must be unique
/// within each category.
class PluginRegistry {
public:
    /// Access the singleton instance.
    static PluginRegistry& instance() {
        static PluginRegistry registry;
        return registry;
    }

    // ---- Registration -------------------------------------------------------

    /// Register a market-data plugin factory.
    ///
    /// @param name     Unique plugin name (e.g. "yahoo", "alpaca").
    /// @param factory  Callable returning std::unique_ptr<IMarketDataSource>.
    /// @throws std::invalid_argument if @p name is already registered.
    void registerMarketData(const std::string& name, MarketDataFactory factory) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (market_data_.count(name))
            throw std::invalid_argument(
                "PluginRegistry: market-data plugin already registered: " + name);
        market_data_[name] = std::move(factory);
    }

    /// Register an execution provider factory.
    ///
    /// @param name     Unique plugin name (e.g. "alpaca", "ib").
    /// @param factory  Callable returning std::unique_ptr<IExecutionProvider>.
    /// @throws std::invalid_argument if @p name is already registered.
    void registerExecutionProvider(const std::string& name, ExecutionFactory factory) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (execution_.count(name))
            throw std::invalid_argument(
                "PluginRegistry: execution plugin already registered: " + name);
        execution_[name] = std::move(factory);
    }

    /// Register an analytics observer factory.
    ///
    /// @param name     Unique plugin name (e.g. "basic-metrics", "prometheus").
    /// @param factory  Callable returning std::unique_ptr<IAnalyticsObserver>.
    /// @throws std::invalid_argument if @p name is already registered.
    void registerAnalyticsObserver(const std::string& name, AnalyticsFactory factory) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (analytics_.count(name))
            throw std::invalid_argument(
                "PluginRegistry: analytics plugin already registered: " + name);
        analytics_[name] = std::move(factory);
    }

    /// Register a passive trading-alert adapter factory.
    ///
    /// @param name     Unique adapter name (e.g. "robinhood-alerts").
    /// @param factory  Callable returning std::unique_ptr<ITradingAlertAdapter>.
    /// @throws std::invalid_argument if @p name is already registered.
    void registerTradingAlertAdapter(const std::string& name, TradingAlertFactory factory) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (trading_alerts_.count(name))
            throw std::invalid_argument(
                "PluginRegistry: trading-alert adapter already registered: " + name);
        trading_alerts_[name] = std::move(factory);
    }

    // ---- Construction -------------------------------------------------------

    /// Construct and return a market-data plugin by name.
    ///
    /// @throws std::out_of_range if no plugin with @p name is registered.
    std::unique_ptr<IMarketDataSource> createMarketData(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = market_data_.find(name);
        if (it == market_data_.end())
            throw std::out_of_range(
                "PluginRegistry: market-data plugin not found: " + name);
        return it->second();
    }

    /// Construct and return an execution provider by name.
    ///
    /// @throws std::out_of_range if no plugin with @p name is registered.
    std::unique_ptr<IExecutionProvider> createExecutionProvider(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = execution_.find(name);
        if (it == execution_.end())
            throw std::out_of_range(
                "PluginRegistry: execution plugin not found: " + name);
        return it->second();
    }

    /// Construct and return an analytics observer by name.
    ///
    /// @throws std::out_of_range if no plugin with @p name is registered.
    std::unique_ptr<IAnalyticsObserver> createAnalyticsObserver(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = analytics_.find(name);
        if (it == analytics_.end())
            throw std::out_of_range(
                "PluginRegistry: analytics plugin not found: " + name);
        return it->second();
    }

    /// Construct and return a passive trading-alert adapter by name.
    ///
    /// @throws std::out_of_range if no adapter with @p name is registered.
    std::unique_ptr<ITradingAlertAdapter> createTradingAlertAdapter(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = trading_alerts_.find(name);
        if (it == trading_alerts_.end())
            throw std::out_of_range(
                "PluginRegistry: trading-alert adapter not found: " + name);
        return it->second();
    }

    // ---- Introspection ------------------------------------------------------

    /// Return true if a market-data plugin with @p name is registered.
    bool hasMarketData(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return market_data_.count(name) > 0;
    }

    /// Return true if an execution provider with @p name is registered.
    bool hasExecutionProvider(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return execution_.count(name) > 0;
    }

    /// Return true if an analytics observer with @p name is registered.
    bool hasAnalyticsObserver(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return analytics_.count(name) > 0;
    }

    /// Return true if a passive trading-alert adapter with @p name is registered.
    bool hasTradingAlertAdapter(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return trading_alerts_.count(name) > 0;
    }

    // Singletons must not be copy- or move-constructed.
    PluginRegistry(const PluginRegistry&)            = delete;
    PluginRegistry& operator=(const PluginRegistry&) = delete;

private:
    PluginRegistry() = default;

    mutable std::mutex mutex_;

    std::unordered_map<std::string, MarketDataFactory> market_data_;
    std::unordered_map<std::string, ExecutionFactory>  execution_;
    std::unordered_map<std::string, AnalyticsFactory>  analytics_;
    std::unordered_map<std::string, TradingAlertFactory> trading_alerts_;
};

} // namespace Plugins
} // namespace AILLE

#endif // AILEE_PLUGINS_PLUGIN_REGISTRY_HPP
