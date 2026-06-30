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

    /// Return the provider's human-readable identifier.
    virtual std::string name() const = 0;

    /// Return recent breaking-news items relevant to a passive alert.
    /// Implementations should use licensed/authorized feeds for outlets such as
    /// the Wall Street Journal and return cached data quickly.
    virtual std::vector<BreakingNewsItem> getBreakingNews(
        const std::string& symbol,
        AlertSide side,
        uint64_t as_of_timestamp_ns) = 0;

protected:
    static uint64_t nowNs() {
        using namespace std::chrono;
        return static_cast<uint64_t>(
            duration_cast<nanoseconds>(
                high_resolution_clock::now().time_since_epoch()
            ).count()
        );
    }
};

} // namespace Plugins
} // namespace AILLE

#endif // AILEE_PLUGINS_IBREAKING_NEWS_PROVIDER_HPP
