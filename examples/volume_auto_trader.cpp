/*
 * AILEE Framework - Intraday Volume Auto-Trader Daemon
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 *
 * Standalone C++ execution daemon that monitors SPY / QQQ volume intraday,
 * evaluates Volume Advisory Module (VAM) states & AILEE Governance decisions,
 * applies risk controls (hysteresis, drawdown limit, max position USD),
 * and executes trades via the Alpaca Execution Provider plugin.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>

#include "../aille.hpp"
#include "../extensions/aille_volume_advisory.hpp"
#include "../ailee_plugins/plugins/execution/alpaca/AlpacaExecution.hpp"

using namespace AILLE;
using namespace AILLE::Plugins;
using namespace AILLE::Plugins::Alpaca;

struct DaemonConfig {
    bool enable_auto_execute{false};
    bool is_live{false};
    bool mock_mode{true};
    bool enable_hft{false};
    int hft_frequency_hz{1000};
    float max_position_usd{10000.0f};
    float max_daily_drawdown_pct{0.05f};
    float risk_reduce_factor{0.5f};
    int hysteresis_bars{2};
    std::string symbol{"SPY"};
    std::string audit_log_file{"volume_trader_audit.log"};
    ailee::HFTBiasConfig hft_bias_cfg;
    bool live_authorized{false};
};

static bool validDaemonConfig(const DaemonConfig& cfg) {
    auto bias=cfg.hft_bias_cfg; bias.enabled=true;
    const bool valid_bias=ailee::is_bullish_mode_allowed(1.0f,0.0f,false,bias) &&
        (bias.bullishness_mode=="STANDARD" || bias.bullishness_mode=="CONSERVATIVE" || bias.bullishness_mode=="HYPER" || bias.bullishness_mode=="CONTRARIAN") &&
        std::isfinite(bias.contrarian_oversold_weight_mult) && bias.contrarian_oversold_weight_mult>=0 &&
        std::isfinite(bias.contrarian_oversold_threshold) && bias.contrarian_oversold_threshold>=0 && bias.contrarian_oversold_threshold<=1 &&
        std::isfinite(bias.contrarian_hf_impulse_scale) && bias.contrarian_hf_impulse_scale>=0 &&
        std::isfinite(bias.contrarian_sell_ceiling_factor) && bias.contrarian_sell_ceiling_factor>=0 && bias.contrarian_sell_ceiling_factor<=1;
    return valid_bias && std::isfinite(cfg.max_position_usd) && cfg.max_position_usd>0 &&
        std::isfinite(cfg.max_daily_drawdown_pct) && cfg.max_daily_drawdown_pct>0 && cfg.max_daily_drawdown_pct<=1 &&
        std::isfinite(cfg.risk_reduce_factor) && cfg.risk_reduce_factor>=0 && cfg.risk_reduce_factor<=1 &&
        cfg.hysteresis_bars>0 && cfg.hft_frequency_hz>=1 && cfg.hft_frequency_hz<=1000 &&
        !cfg.symbol.empty() && cfg.symbol.size()<=32 && std::all_of(cfg.symbol.begin(),cfg.symbol.end(),[](unsigned char c){ return (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='.' || c=='-' || c=='_'; }) &&
        cfg.symbol.find("..") == std::string::npos;
}

class VolumeTraderDaemon {
public:
    explicit VolumeTraderDaemon(const DaemonConfig& cfg)
        : config_(cfg) {
        if(!validDaemonConfig(config_)) throw std::invalid_argument("Invalid daemon safety configuration");
        AlpacaConfig alpaca_cfg = AlpacaExecution::loadConfigFromEnv(/*fail_closed=*/false);
        alpaca_cfg.mock_mode = config_.mock_mode;
        alpaca_cfg.is_live = config_.is_live;
        alpaca_cfg.live_authorized = config_.live_authorized;
        alpaca_cfg.max_position_usd = config_.max_position_usd;

        executor_ = std::make_unique<AlpacaExecution>(alpaca_cfg);

        peak_equity_ = executor_->getAccountEquity();
        if (!std::isfinite(peak_equity_) || peak_equity_ <= 0.0f) { peak_equity_=0; executor_->triggerLockout("Initial account equity unavailable; reconciliation required"); }
        current_equity_ = peak_equity_;
    }

    void logAudit(const std::string& action, const std::string& details, const VolumeAdvisory& adv, float price,
                  float trust_score = 0.85f, float manipulation_score = 0.0f, bool bullish_mode_active = false) {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto in_time_t = system_clock::to_time_t(now);

        std::ostringstream ss;
        ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
        std::string ts = ss.str();

        std::ostringstream json;
        json << "{"
             << "\"timestamp\":\"" << ts << "\","
             << "\"symbol\":\"" << config_.symbol << "\","
             << "\"action\":\"" << action << "\","
             << "\"price\":" << price << ","
             << "\"rec_weight\":" << adv.recommended_weight << ","
             << "\"execution_weight\":" << adv.recommended_weight << ","
             << "\"trust_score\":" << trust_score << ","
             << "\"manipulation_score\":" << manipulation_score << ","
             << "\"risk_score\":" << adv.risk_score << ","
             << "\"risk_elevated\":" << (adv.risk_elevated ? "true" : "false") << ","
             << "\"contrarian_buy\":" << (adv.contrarian_buy_signal ? "true" : "false") << ","
             << "\"growth_favorable\":" << (adv.growth_favorable ? "true" : "false") << ","
             << "\"hft_active\":" << (adv.hft_active ? "true" : "false") << ","
             << "\"hft_delta_v\":" << adv.hft_delta_v << ","
             << "\"bullish_mode_active\":" << (bullish_mode_active ? "true" : "false") << ","
             << "\"bullish_multiplier_price\":" << config_.hft_bias_cfg.bullish_multiplier_price << ","
             << "\"bullish_multiplier_volume\":" << config_.hft_bias_cfg.bullish_multiplier_volume << ","
             << "\"bullish_execution_scale\":" << config_.hft_bias_cfg.bullish_execution_scale << ","
             << "\"bullish_sell_ceiling_factor\":" << config_.hft_bias_cfg.bullish_sell_ceiling_factor << ","
             << "\"details\":\"" << details << "\","
             << "\"reason\":\"" << details << "\""
             << "}";

        std::string log_line = json.str();

        // Print to stdout
        std::cout << "[AUDIT] " << log_line << std::endl;

        // Append to file
        std::ofstream outfile(config_.audit_log_file, std::ios_base::app);
        if (outfile.is_open()) {
            outfile << log_line << std::endl;
        }
    }

    void processTick(const VolumeState& state, float current_price, const SafetyState* safety = nullptr) {
        const float evidence[] = {state.current_volume,state.avg_volume,state.volume_anomaly_ratio,state.price_change,
            state.vwap_deviation,state.prev_volume_anomaly_ratio,state.prev_recommended_weight,state.hft_p_input,state.hft_mass,state.hft_v0,current_price};
        if(!std::all_of(std::begin(evidence),std::end(evidence),[](float value){ return std::isfinite(value); }) ||
           current_price<=0 || state.current_volume<0 || state.avg_volume<0 || state.volume_anomaly_ratio<0 ||
           state.hft_mass<=0 || state.contrarian_override < -1 || state.contrarian_override > 1) {
            executor_->triggerLockout("Invalid market evidence; execution suspended"); return;
        }
        if(broker_pending_) {
            if(safety && (safety->kill_switch || safety->hardware_fault)) {
                executor_->triggerLockout("Safety fault with pending broker outcome");
                if(!pending_order_id_.empty()) (void)executor_->cancelOrder(pending_order_id_);
            }
            std::cerr << "[VolumeTrader] Pending broker outcome requires reconciliation; no new execution.\n";
            return;
        }
        // 1. Safety & Drawdown Check
        if (safety && (safety->kill_switch || safety->hardware_fault)) {
            executor_->triggerLockout("Hardware fault or kill switch triggered in SafetyState");
        }

        float equity = executor_->getAccountEquity();
        if(!std::isfinite(equity) || equity<=0) { executor_->triggerLockout("Account equity unavailable; execution suspended"); return; }
        if (equity > 0.0f) {
            current_equity_ = equity;
            if (current_equity_ > peak_equity_) peak_equity_ = current_equity_;
            float drawdown = (peak_equity_ - current_equity_) / peak_equity_;
            if (drawdown >= config_.max_daily_drawdown_pct) {
                std::ostringstream err;
                err << "Daily drawdown threshold breached: " << (drawdown * 100.0f) << "% >= " << (config_.max_daily_drawdown_pct * 100.0f) << "%";
                executor_->triggerLockout(err.str());
            }
        }

        // 2. Evaluate VAM
        VolumeState tick_state = state;
        tick_state.enable_hft_calc = config_.enable_hft;

        float trust_score = 0.85f;
        float manipulation_score = 0.0f;
        float drawdown = (peak_equity_ > 0.0f) ? (peak_equity_ - current_equity_) / peak_equity_ : 0.0f;
        bool drawdown_near_breach = (drawdown >= config_.max_daily_drawdown_pct * 0.8f) || executor_->isLockedOut();

        bool bullish_active = ailee::is_bullish_mode_allowed(trust_score, manipulation_score, drawdown_near_breach, config_.hft_bias_cfg);

        VolumeAdvisory adv = evaluate_volume_state(
            tick_state, safety, nullptr, true, 1.0f,
            &config_.hft_bias_cfg, trust_score, manipulation_score, drawdown_near_breach
        );

        if(!std::isfinite(adv.recommended_weight) || adv.recommended_weight<0 ||
           !std::isfinite(adv.risk_score) || !std::isfinite(adv.hft_delta_v) || !std::isfinite(adv.oversold_score)) {
            executor_->triggerLockout("Invalid advisory evidence; execution suspended"); return;
        }
        // 3. Translate signal to order intent
        OrderSide desired_side = OrderSide::FLAT;
        std::string signal_type = "NEUTRAL";

        if (executor_->isLockedOut()) {
            desired_side = OrderSide::FLAT;
            signal_type = "LOCKOUT_FLAT";
        } else if (adv.contrarian_buy_signal) {
            desired_side = OrderSide::BUY;
            signal_type = "CONTRARIAN_BUY";
        } else if (adv.growth_favorable && !adv.risk_elevated) {
            desired_side = OrderSide::BUY;
            signal_type = "GROWTH_BUY";
        } else if (adv.risk_elevated) {
            desired_side = OrderSide::FLAT;
            signal_type = "RISK_REDUCE";
        }

        // 4. Hysteresis / Debounce filter
        if (desired_side == pending_side_) {
            if(consecutive_bars_<config_.hysteresis_bars) ++consecutive_bars_;
        } else {
            pending_side_ = desired_side;
            consecutive_bars_ = 1;
        }

        if (consecutive_bars_ < config_.hysteresis_bars) {
            logAudit("DEBOUNCE_WAIT", "Signal pending confirmation: " + signal_type, adv, current_price, trust_score, manipulation_score, bullish_active);
            return;
        }

        // 5. Execution decision
        if (desired_side == current_position_side_ && desired_side != OrderSide::FLAT) {
            logAudit("HOLD", "Position already aligned with signal: " + signal_type, adv, current_price, trust_score, manipulation_score, bullish_active);
            return;
        }

        if (!config_.enable_auto_execute) {
            logAudit("DRY_RUN_SIGNAL", "Auto-execution disabled (--enable-auto-execute=false). Would execute: " + signal_type, adv, current_price, trust_score, manipulation_score, bullish_active);
            return;
        }

        // 6. Submit Order
        if (desired_side == OrderSide::FLAT) {
            if (current_position_side_ != OrderSide::FLAT) {
                bool ok = executor_->flattenPosition(config_.symbol);
                if(ok && config_.mock_mode) current_position_side_=OrderSide::FLAT;
                if(!config_.mock_mode) broker_pending_=true;
                logAudit(ok ? (config_.mock_mode ? "FLAT_POSITION" : "FLATTEN_ACCEPTED_PENDING") : "FLATTEN_FAILED",
                    ok ? (config_.mock_mode ? "Mock position flattened" : "Close request accepted; filled position remains unconfirmed") : "Flatten failed; previous position retained",
                    adv,current_price,trust_score,manipulation_score,bullish_active);
            }
        } else if (desired_side == OrderSide::BUY) {
            double alloc_usd = static_cast<double>(config_.max_position_usd) * std::min(1.0f,adv.recommended_weight);
            if (adv.risk_elevated) {
                alloc_usd *= config_.risk_reduce_factor;
            }

            float qty = static_cast<float>(std::floor(alloc_usd / current_price));
            if (qty > 0.0f) {
                OrderRequest req;
                req.symbol = config_.symbol;
                req.side = OrderSide::BUY;
                req.quantity = qty;
                req.limit_price = current_price; // Explicit bounded BUY limit, never an inferred market quote.
                req.timestamp_ns=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
                req.client_ref="ailee-"+std::to_string(req.timestamp_ns);

                std::string order_id = executor_->submitOrder(req);
                if (!order_id.empty()) {
                    if(config_.mock_mode) current_position_side_=OrderSide::BUY;
                    else { broker_pending_=true; pending_order_id_=order_id; } // An accepted ID is not evidence of a fill.
                    logAudit("ORDER_SUBMITTED", "Order ID: " + order_id + " Qty: " + std::to_string(qty), adv, current_price, trust_score, manipulation_score, bullish_active);
                } else {
                    if(!config_.mock_mode && executor_->isLockedOut()) broker_pending_=true;
                    logAudit("ORDER_FAILED", "Order unconfirmed or locally rejected; no filled position assumed", adv, current_price, trust_score, manipulation_score, bullish_active);
                }
            } else {
                logAudit("SKIPPED_QTY_ZERO", "Calculated order quantity is 0", adv, current_price, trust_score, manipulation_score, bullish_active);
            }
        }
    }

    const AlpacaExecution* executor() const { return executor_.get(); }
    bool brokerPending() const { return broker_pending_; }
    OrderSide confirmedPositionSide() const { return current_position_side_; }

private:
    DaemonConfig config_;
    std::unique_ptr<AlpacaExecution> executor_;
    bool broker_pending_{false};
    std::string pending_order_id_;
    OrderSide current_position_side_{OrderSide::FLAT};
    OrderSide pending_side_{OrderSide::FLAT};
    int consecutive_bars_{0};
    float peak_equity_{100000.0f};
    float current_equity_{100000.0f};
};

// ============================================================================
// CLI MAIN RUNNER
// ============================================================================

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --enable-auto-execute     Enable live/mock trade execution (default: false / dry-run)\n"
              << "  --enable-hft              Enable high-frequency price action & volume impulse analysis (default: false)\n"
              << "  --hft-frequency-hz=NUM    HFT micro-tick sampling frequency in Hz [1..1000] (default: 1000)\n"
              << "  --disable-bullish         Disable controlled bullish bias (default: enabled / ON)\n"
              << "  --enable-bullish          Enable controlled bullish bias (default: enabled / ON)\n"
              << "  --mode=paper|live          Trading mode (default: paper)\n"
              << "  --symbol=SPY|QQQ           Target ETF ticker (default: SPY)\n"
              << "  --max-position-usd=NUM     Maximum dollar allocation (default: 10000)\n"
              << "  --max-drawdown-pct=NUM     Daily max drawdown threshold [0.01..0.50] (default: 0.05)\n"
              << "  --hysteresis=NUM           Bar count confirmation hysteresis (default: 2)\n"
              << "  --mock                     Use mock order simulator (default: enabled)\n"
              << "  --no-mock                  Use the validated broker destination from ALPACA_BASE_URL\n"
              << "  --confirm-live             Required flag when using --mode=live\n"
              << "  --help                     Show this help message\n";
}

static bool parseFloatOption(const std::string& text,float& output) {
    char* end=nullptr; output=std::strtof(text.c_str(),&end);
    return !text.empty() && end==text.c_str()+text.size() && std::isfinite(output);
}
static bool parseIntOption(const std::string& text,int& output) {
    char* end=nullptr; const long value=std::strtol(text.c_str(),&end,10);
    if(text.empty() || end!=text.c_str()+text.size() || value<std::numeric_limits<int>::min() || value>std::numeric_limits<int>::max()) return false;
    output=static_cast<int>(value); return true;
}
int main(int argc, char* argv[]) {
    DaemonConfig cfg;
    bool confirm_live = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--enable-auto-execute") {
            cfg.enable_auto_execute = true;
        } else if (arg == "--enable-hft") {
            cfg.enable_hft = true;
        } else if (arg.rfind("--hft-frequency-hz=", 0) == 0) {
            int freq=0; if(!parseIntOption(arg.substr(19),freq)) { std::cerr<<"Invalid HFT frequency\n"; return 1; }
            cfg.hft_frequency_hz = (freq > 1000) ? 1000 : ((freq < 1) ? 1 : freq);
        } else if (arg == "--disable-bullish" || arg == "--disable-bullish-bias") {
            cfg.hft_bias_cfg.enabled = false;
        } else if (arg == "--enable-bullish" || arg == "--enable-bullish-bias") {
            cfg.hft_bias_cfg.enabled = true;
        } else if (arg == "--mode=live") {
            cfg.is_live = true;
        } else if (arg == "--mode=paper") {
            cfg.is_live = false;
        } else if (arg == "--confirm-live") {
            confirm_live = true;
        } else if (arg == "--mock") {
            cfg.mock_mode = true;
        } else if (arg == "--no-mock") {
            cfg.mock_mode = false;
        } else if (arg.rfind("--symbol=", 0) == 0) {
            cfg.symbol = arg.substr(9);
        } else if (arg.rfind("--max-position-usd=", 0) == 0) {
            if(!parseFloatOption(arg.substr(19),cfg.max_position_usd)) { std::cerr<<"Invalid position limit\n"; return 1; }
        } else if (arg.rfind("--max-drawdown-pct=", 0) == 0) {
            if(!parseFloatOption(arg.substr(19),cfg.max_daily_drawdown_pct)) { std::cerr<<"Invalid drawdown limit\n"; return 1; }
        } else if (arg.rfind("--hysteresis=", 0) == 0) {
            if(!parseIntOption(arg.substr(13),cfg.hysteresis_bars)) { std::cerr<<"Invalid hysteresis\n"; return 1; }
        } else if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else { std::cerr<<"Unknown option: "<<arg<<'\n'; return 1; }
    }

    if (cfg.is_live && !confirm_live) {
        std::cerr << "ERROR: Live trading mode requested (--mode=live) without --confirm-live safety flag. Aborting.\n";
        return 1;
    }
    cfg.live_authorized = confirm_live;

    std::cout << "========================================================\n"
              << " AILEE Intraday Volume Auto-Trader Daemon v13.0.0\n"
              << " Target Symbol:       " << cfg.symbol << "\n"
              << " Execution Enabled:   " << (cfg.enable_auto_execute ? "YES" : "NO (Dry-Run)") << "\n"
              << " High-Frequency (HFT):" << (cfg.enable_hft ? "ENABLED (" + std::to_string(cfg.hft_frequency_hz) + " Hz)" : "DISABLED") << "\n"
              << " Controlled Bullish:  " << (cfg.hft_bias_cfg.enabled ? "ENABLED (ON)" : "DISABLED (OFF)") << "\n"
              << " Mode:                " << (cfg.is_live ? "LIVE" : "PAPER") << "\n"
              << " Mock Simulator:      " << (cfg.mock_mode ? "YES" : "NO") << "\n"
              << " Max Position (USD):  $" << cfg.max_position_usd << "\n"
              << " Max Daily Drawdown:  " << (cfg.max_daily_drawdown_pct * 100.0f) << "%\n"
              << " Hysteresis Bars:     " << cfg.hysteresis_bars << "\n"
              << "========================================================\n";

    if(!validDaemonConfig(cfg)) { std::cerr<<"ERROR: Invalid daemon risk configuration.\n"; return 1; }
    VolumeTraderDaemon daemon(cfg);
    if (!daemon.executor()->isEnabled() || (cfg.enable_auto_execute && !daemon.executor()->canSubmit())) {
        std::cerr << "ERROR: Broker execution unavailable; check credentials, destination, mode authorization, and durable journal.\n";
        return 1;
    }

    // Simulate 5 intraday bar updates
    std::vector<VolumeState> sim_ticks(5);

    // Tick 1: Normal volume
    sim_ticks[0].current_volume = 10000.0f;
    sim_ticks[0].avg_volume = 10000.0f;
    sim_ticks[0].volume_anomaly_ratio = 1.0f;
    sim_ticks[0].price_change = 0.002f;
    sim_ticks[0].is_index_etf = true;

    // Tick 2: Growth accumulation
    sim_ticks[1].current_volume = 20000.0f;
    sim_ticks[1].avg_volume = 10000.0f;
    sim_ticks[1].volume_anomaly_ratio = 2.0f;
    sim_ticks[1].price_change = 0.008f;
    sim_ticks[1].hft_p_input = 0.08f;
    sim_ticks[1].hft_mass = 1.0f;
    sim_ticks[1].is_index_etf = true;

    // Tick 3: Second growth bar (confirms hysteresis)
    sim_ticks[2] = sim_ticks[1];
    sim_ticks[2].prev_volume_anomaly_ratio = 2.0f;

    // Tick 4: Oversold spike (Contrarian Buy)
    sim_ticks[3].current_volume = 30000.0f;
    sim_ticks[3].avg_volume = 10000.0f;
    sim_ticks[3].volume_anomaly_ratio = 3.0f;
    sim_ticks[3].price_change = -0.015f;
    sim_ticks[3].vwap_deviation = -0.010f;
    sim_ticks[3].is_index_etf = true;

    // Tick 5: Second oversold bar (confirms hysteresis)
    sim_ticks[4] = sim_ticks[3];
    sim_ticks[4].prev_volume_anomaly_ratio = 3.0f;

    float price = 500.0f;
    SafetyState safety{};

    for (size_t i = 0; i < sim_ticks.size(); ++i) {
        std::cout << "\n--- Bar " << (i + 1) << " Processing ---" << std::endl;
        daemon.processTick(sim_ticks[i], price, &safety);
        price += 1.0f;
    }

    std::cout << "\n[Daemon Finished Successfully]\n";
    return 0;
}
