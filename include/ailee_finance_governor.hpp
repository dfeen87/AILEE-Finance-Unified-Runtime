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

struct SellGovernanceDecisionCpp {
    int level{3};
    double allowed_sell_amount{0.0};
    double trust_score{0.0};
    double manipulation_score{1.0};
    double consensus_score{0.0};
    std::string reason;
};

class AileeFinanceGovernor {
public:
    AileeFinanceGovernor();
    ~AileeFinanceGovernor();

    SellGovernanceDecisionCpp evaluateSell(const RawSellSignals& signals);
};

} // namespace ailee

#endif // AILEE_FINANCE_GOVERNOR_HPP
