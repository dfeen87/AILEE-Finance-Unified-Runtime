/*
 * AILLE Plugin Interface — IMarketDataSource
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: Non-Commercial (see LICENSE)
 *
 * Stable interface for market-data plugins. Implementors supply
 * ModelSignal vectors to the AILLE engine without touching core logic.
 *
 * Plugin authors must implement:
 *   getCandles()    — historical OHLCV data as model signals
 *   getLatestPrice() — real-time price as a single model signal
 *
 * Confidence penalties are the plugin's responsibility:
 *   - Signals with confidence < AILLEConfig::grace_confidence_threshold
 *     are discarded by the engine's safety layer.
 *   - Signals in [grace, min) are admitted with a 20% confidence penalty.
 *   - Keep confidence values in [0.0, 1.0].
 */

#ifndef AILEE_PLUGINS_IMARKET_DATA_SOURCE_HPP
#define AILEE_PLUGINS_IMARKET_DATA_SOURCE_HPP

#include <string>
#include <vector>

#include "../aille.hpp"

namespace AILLE {
namespace Plugins {

// ============================================================================
// CANDLE (OHLCV) VALUE OBJECT
// ============================================================================

/// A single OHLCV bar. Plugins populate this from their upstream data source.
struct Candle {
    uint64_t timestamp_ns; ///< Bar open time (nanoseconds since epoch)
    float    open;
    float    high;
    float    low;
    float    close;
    float    volume;
};

// ============================================================================
// IMARKET DATA SOURCE INTERFACE
// ============================================================================

/// Stable interface for all market-data plugins.
///
/// Implementations must be thread-safe: the AILLE engine may call these
/// methods concurrently when multiple data sources are registered.
class IMarketDataSource {
public:
    virtual ~IMarketDataSource() = default;

    /// Return the plugin's human-readable identifier (e.g. "yahoo", "alpaca").
    virtual std::string name() const = 0;

    /// Fetch historical candles and convert them to ModelSignal objects.
    ///
    /// @param symbol    Ticker symbol (e.g. "AAPL", "BTC-USD")
    /// @param timeframe Bar width in seconds (e.g. 60 = 1-minute bars)
    /// @param count     Maximum number of bars to return
    /// @return          One ModelSignal per bar, ordered oldest → newest.
    ///
    /// Confidence penalty guidelines:
    ///   - Recent bars (< 60 s old): confidence unchanged
    ///   - Bars 60 s–5 min old: apply × 0.9 staleness penalty
    ///   - Bars > 5 min old: apply × 0.8 staleness penalty
    virtual std::vector<ModelSignal> getCandles(const std::string& symbol,
                                                int                timeframe,
                                                int                count) = 0;

    /// Fetch the latest price and return it as a single ModelSignal.
    ///
    /// @param symbol  Ticker symbol (e.g. "AAPL")
    /// @return        A single ModelSignal whose `value` is the mid-price
    ///                direction relative to the session open, and whose
    ///                `confidence` reflects data-feed quality.
    virtual ModelSignal getLatestPrice(const std::string& symbol) = 0;

protected:
    /// Utility: apply a staleness confidence penalty based on how old
    /// the bar timestamp is relative to now.
    static float applyStalenesspenalty(float base_confidence,
                                       uint64_t bar_timestamp_ns) {
        using namespace std::chrono;
        uint64_t now_ns = static_cast<uint64_t>(
            duration_cast<nanoseconds>(
                high_resolution_clock::now().time_since_epoch()
            ).count()
        );
        uint64_t age_ns = (now_ns > bar_timestamp_ns)
                          ? (now_ns - bar_timestamp_ns) : 0;

        constexpr uint64_t kSixtySeconds = 60ULL * 1'000'000'000ULL;
        constexpr uint64_t kFiveMinutes  = 300ULL * 1'000'000'000ULL;

        if (age_ns < kSixtySeconds)  return base_confidence;
        if (age_ns < kFiveMinutes)   return base_confidence * 0.9f;
        return base_confidence * 0.8f;
    }
};

} // namespace Plugins
} // namespace AILLE

#endif // AILEE_PLUGINS_IMARKET_DATA_SOURCE_HPP
