/*
 * AILLE Plugin — BreakingNewsAlertAdapter (passive alerts only)
 * AI-Load Integrity and Layered Evaluation
 *
 * MIT License
 *
 * Optional alert adapter/decorator that enriches BUY/SELL/HOLD alerts with
 * cached breaking-news context from licensed or caller-provided news feeds.
 * This adapter never executes trades and never fetches news synchronously from
 * the decision path unless the configured provider does so.
 */

#include "../../../IBreakingNewsProvider.hpp"
#include "../../../ITradingAlertAdapter.hpp"
#include "../../../PluginRegistry.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace AILLE {
namespace Plugins {
namespace News {

class StaticBreakingNewsProvider : public IBreakingNewsProvider {
public:
    explicit StaticBreakingNewsProvider(std::vector<BreakingNewsItem> items = {})
        : items_(std::move(items)) {}

    std::string name() const override { return "static-breaking-news"; }

    std::vector<BreakingNewsItem> getBreakingNews(const std::string& symbol,
                                                  AlertSide side,
                                                  uint64_t as_of_timestamp_ns) override {
        (void)symbol;
        (void)side;
        std::vector<BreakingNewsItem> recent;
        for (const auto& item : items_) {
            if (item.published_ns == 0 || item.published_ns <= as_of_timestamp_ns) {
                recent.push_back(item);
            }
        }
        return recent;
    }

private:
    std::vector<BreakingNewsItem> items_;
};

class BreakingNewsAlertAdapter : public ITradingAlertAdapter {
public:
    explicit BreakingNewsAlertAdapter(
        std::shared_ptr<IBreakingNewsProvider> news_provider = std::make_shared<StaticBreakingNewsProvider>(),
        std::size_t max_headlines = 3)
        : news_provider_(std::move(news_provider)),
          max_headlines_(max_headlines) {}

    std::string name() const override { return "breaking-news-alerts"; }

    bool sendAlert(const TradingAlert& alert) override {
        if (alert.symbol.empty()) {
            std::cerr << "[BreakingNewsAlertAdapter] alert skipped: symbol is empty\n";
            return false;
        }

        const TradingAlert enriched = enrichAlert(alert);
        std::cout << "[BreakingNewsAlertAdapter] alert"
                  << " symbol=" << enriched.symbol
                  << " side=" << sideToString(enriched.side)
                  << " confidence=" << enriched.confidence
                  << " signal=" << enriched.signal_value
                  << " severity=" << enriched.severity
                  << " client_ref=" << enriched.client_ref
                  << " message=\"" << enriched.message << "\"\n";
        return true;
    }

    TradingAlert enrichAlert(const TradingAlert& alert) const {
        TradingAlert enriched = alert;
        if (!news_provider_ || max_headlines_ == 0) {
            return enriched;
        }

        const auto items = news_provider_->getBreakingNews(
            alert.symbol,
            alert.side,
            alert.timestamp_ns);

        if (items.empty()) {
            return enriched;
        }

        std::ostringstream context;
        context << " Breaking news context (" << news_provider_->name() << "): ";

        const std::size_t limit = std::min(max_headlines_, items.size());
        for (std::size_t i = 0; i < limit; ++i) {
            if (i > 0) {
                context << " | ";
            }
            context << items[i].outlet << ": " << items[i].headline;
            if (!items[i].sentiment.empty()) {
                context << " [" << items[i].sentiment << "]";
            }
        }

        enriched.message += context.str();
        if (enriched.severity == "info" && hasConflictingNews(alert.side, items, limit)) {
            enriched.severity = "warning";
            enriched.message += " News sentiment conflicts with alert side; review before action.";
        }
        return enriched;
    }

private:
    std::shared_ptr<IBreakingNewsProvider> news_provider_;
    std::size_t max_headlines_;

    static bool hasConflictingNews(AlertSide side,
                                   const std::vector<BreakingNewsItem>& items,
                                   std::size_t limit) {
        for (std::size_t i = 0; i < limit; ++i) {
            if ((side == AlertSide::ALERT_BUY && items[i].sentiment == "bearish") ||
                (side == AlertSide::ALERT_SELL && items[i].sentiment == "bullish")) {
                return true;
            }
        }
        return false;
    }

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
    struct BreakingNewsAlertRegistrar {
        BreakingNewsAlertRegistrar() {
            PluginRegistry::instance().registerTradingAlertAdapter(
                "breaking-news-alerts",
                []() -> std::unique_ptr<ITradingAlertAdapter> {
                    return std::make_unique<BreakingNewsAlertAdapter>();
                }
            );
        }
    };
    const BreakingNewsAlertRegistrar g_breaking_news_alert_registrar;
} // anonymous namespace

} // namespace News
} // namespace Plugins
} // namespace AILLE
