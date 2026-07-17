/*
 * AILLE Plugin Interface — IBreakingNewsProvider
 * AI-Load Integrity and Layered Evaluation
 *
 * MIT License
 *
 * Optional passive news-enrichment interface for trading alerts. Providers may
 * integrate licensed feeds, RSS bridges, or webhook-fed caches from major news
 * outlets. Providers must not influence core AILLE decisions or execute trades.
 */

#ifndef AILEE_PLUGINS_IBREAKING_NEWS_PROVIDER_HPP
#define AILEE_PLUGINS_IBREAKING_NEWS_PROVIDER_HPP

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ITradingAlertAdapter.hpp"
#include "../src/telemetry/TelemetryBridge.hpp"

namespace AILLE {
namespace Plugins {

struct BreakingNewsItem {
    std::string outlet;        ///< e.g., Wall Street Journal, Reuters, Bloomberg.
    std::string headline;      ///< Short headline or alert summary.
    std::string url;           ///< Optional canonical/source URL.
    std::string sentiment;     ///< bullish, bearish, neutral, or unknown.
    uint64_t    published_ns;  ///< Publication timestamp, nanoseconds since epoch.

    BreakingNewsItem()
        : published_ns(0) {}

    BreakingNewsItem(std::string outlet_name,
                     std::string headline_text,
                     std::string source_url,
                     std::string sentiment_label,
                     uint64_t published_timestamp_ns)
        : outlet(std::move(outlet_name)),
          headline(std::move(headline_text)),
          url(std::move(source_url)),
          sentiment(std::move(sentiment_label)),
          published_ns(published_timestamp_ns) {}
};

class IBreakingNewsProvider {
public:
    virtual ~IBreakingNewsProvider() = default;
    virtual std::string name() const = 0;
    virtual std::vector<BreakingNewsItem> getBreakingNews(
        const std::string& symbol,
        AlertSide side,
        uint64_t as_of_timestamp_ns) = 0;
};

class TelemetryBreakingNewsProvider : public AILLE::Plugins::IBreakingNewsProvider {
public:
    std::string name() const override {
        return "TelemetryBreakingNewsProvider";
    }

    std::vector<AILLE::Plugins::BreakingNewsItem> getBreakingNews(
        const std::string& symbol,
        AlertSide side,
        uint64_t as_of_timestamp_ns) override
    {
        telemetry_on_breaking_news();

        // Return an empty vector (passive provider)
        return {};
    }
};

} // namespace Plugins
} // namespace AILLE

#endif // AILEE_PLUGINS_IBREAKING_NEWS_PROVIDER_HPP
