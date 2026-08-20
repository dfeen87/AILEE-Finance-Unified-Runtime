#ifndef AILEE_FINANCE_GOVERNOR_HPP
#define AILEE_FINANCE_GOVERNOR_HPP

#include <string>
#include <vector>

namespace ailee {

struct FeedDataCpp {
    std::string feed_id;
    double price{0.0};
    double confidence{1.0};
};

struct RawSellSignals {
    double position_size{0.0};
    double volatility{0.0};
    double trust_score{0.0};

    // Market data indicators
    bool spoofed_bids{false};
    double bid_liquidity_drop{0.0};
    bool mev_detected{false};
    double spread_widening{0.0};

    // Feed data
    std::vector<FeedDataCpp> feeds;

    // Intent flags
    bool intent_flag{true};
    std::string intent_reason;
};

struct HFTBiasConfig {
    bool enabled{true};
    float bullish_multiplier_price{1.05f};
    float bullish_multiplier_volume{1.05f};
    float bullish_execution_scale{1.10f};
    float bullish_sell_ceiling_factor{0.80f};
    float trust_threshold_bullish{0.70f};
    float manipulation_threshold{0.30f};
};

inline bool is_bullish_mode_allowed(
    float trust_score,
    float manipulation_score,
    bool drawdown_near_breach,
    const HFTBiasConfig& cfg = HFTBiasConfig{}
) noexcept {
    if (!cfg.enabled) return false;
    if (trust_score < cfg.trust_threshold_bullish) return false;
    if (manipulation_score > cfg.manipulation_threshold) return false;
    if (drawdown_near_breach) return false;
    return true;
}

struct SellGovernanceDecisionCpp {
    int level{3};
    double allowed_sell_amount{0.0};
    double trust_score{0.0};
    double manipulation_score{1.0};
    double consensus_score{0.0};
    std::string reason;
    bool bullish_mode_active{false};
    double bullish_multiplier_price{1.05};
    double bullish_multiplier_volume{1.05};
    double bullish_execution_scale{1.10};
    double bullish_sell_ceiling_factor{0.80};
};

class AileeFinanceGovernor {
public:
    AileeFinanceGovernor();
    ~AileeFinanceGovernor();

    SellGovernanceDecisionCpp evaluateSell(const RawSellSignals& signals);
};

} // namespace ailee

#endif // AILEE_FINANCE_GOVERNOR_HPP
