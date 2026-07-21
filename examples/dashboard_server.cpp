/*
 * AILLE Framework — Live Dashboard Server Example
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) 2026 DonMichael Feeney Jr
 * License: MIT (see LICENSE)
 */

#include "../aille.hpp"
#include "../extensions/aille_btc.hpp"
#include "../extensions/aille_eth.hpp"
#include "../extensions/aille_oil.hpp"
#include "../extensions/aille_gold.hpp"
#include "../extensions/aille_silver.hpp"
#include "../extensions/aille_copper.hpp"
#include "../extensions/aille_natgas.hpp"
#include "../extensions/aille_platinum.hpp"
#include "../extensions/aille_forex_usd.hpp"
#include "../extensions/aille_macro.hpp"
#include "../extensions/aille_stabilizer.hpp"
#include "../ailee_plugins/plugins/dashboard/LiveAdvisoryObserver.hpp"

#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <vector>

using namespace AILLE;
using namespace AILLE::Plugins::Dashboard;

int main() {
    std::cout << "Starting AILLE Engine & Live Dashboard Server...\n";

    SafetyState safety_state;
    BTCState btc_state;
    BTCAdvisory btc_adv;
    ETHState eth_state;
    ETHAdvisory eth_adv;
    OILState oil_state;
    OILAdvisory oil_adv;
    GOLDState gold_state;
    GOLDAdvisory gold_adv;
    SILVERState silver_state;
    SILVERAdvisory silver_adv;
    COPPERState copper_state;
    COPPERAdvisory copper_adv;
    NATGASState natgas_state;
    NATGASAdvisory natgas_adv;
    PLATINUMState platinum_state;
    PLATINUMAdvisory platinum_adv;
    ForexUSDState forex_state;
    ForexUSDAdvisory forex_adv;
    MacroSignalState macro_state;
    MacroSignalAdvisory macro_adv;
    MarketStabilizerState stabilizer_state;
    MarketStabilizerAdvisory stabilizer_adv;

    AILLEConfig config;
    AILLEEngine engine(config);

    engine.setSafetyState(&safety_state);
    engine.set_btc_state(&btc_state);
    engine.set_btc_advisory(&btc_adv);
    engine.set_eth_state(&eth_state);
    engine.set_eth_advisory(&eth_adv);
    engine.set_oil_state(&oil_state);
    engine.set_oil_advisory(&oil_adv);
    engine.set_gold_state(&gold_state);
    engine.set_gold_advisory(&gold_adv);
    engine.set_silver_state(&silver_state);
    engine.set_silver_advisory(&silver_adv);
    engine.set_copper_state(&copper_state);
    engine.set_copper_advisory(&copper_adv);
    engine.set_natgas_state(&natgas_state);
    engine.set_natgas_advisory(&natgas_adv);
    engine.set_platinum_state(&platinum_state);
    engine.set_platinum_advisory(&platinum_adv);
    engine.set_forex_usd_state(&forex_state);
    engine.set_forex_usd_advisory(&forex_adv);
    engine.set_macro_state(&macro_state);
    engine.set_macro_advisory(&macro_adv);
    engine.set_stabilizer_state(&stabilizer_state);
    engine.set_stabilizer_advisory(&stabilizer_adv);

    LiveAdvisoryObserver observer(
        9002,
        &btc_adv, &eth_adv, &oil_adv, &gold_adv, &silver_adv,
        &copper_adv, &natgas_adv, &platinum_adv, &forex_adv, &macro_adv,
        &stabilizer_adv
    );

    observer.startServer();
    std::cout << "WebSocket server running on ws://localhost:9002\n";
    std::cout << "Open ailee_plugins/plugins/dashboard/index.html to view the dashboard.\n";

    // Simulate engine loop
    uint64_t tick = 0;
    while (true) {
        // Mock state updates
        btc_state.realized_vol = 0.5f + 0.2f * std::sin(tick * 0.1f);
        eth_state.realized_vol = 0.5f + 0.3f * std::cos(tick * 0.15f);
        forex_state.usd_index = 100.0f + 5.0f * std::sin(tick * 0.05f);
        macro_state.inflation_pressure = 0.5f + 0.1f * std::cos(tick * 0.02f);
        oil_state.realized_vol = 0.3f + 0.1f * std::sin(tick * 0.08f);

        // Mock stabilizer updates
        stabilizer_state.systemic_volatility = 0.4f + 0.4f * std::sin(tick * 0.07f);
        stabilizer_state.bid_ask_spread_deviation = 0.3f + 0.3f * std::cos(tick * 0.11f);
        stabilizer_state.order_book_depth_deficit = 0.2f * std::sin(tick * 0.05f);
        stabilizer_state.consecutive_crash_count = (tick % 15 == 0) ? 3.0f : 0.0f;

        std::vector<ModelSignal> signals = {
            ModelSignal(0.8f, 0.9f, 1),
            ModelSignal(0.7f, 0.85f, 2),
            ModelSignal(0.75f, 0.88f, 3)
        };

        // Notify observer before evaluation (pass vector directly)
        observer.onSignalEvaluated(signals);

        // Make decision (this evaluates all advisories internally)
        Decision d = engine.makeDecision(signals.data(), signals.size());

        // Notify observer after decision
        observer.onDecisionRouted(d);

        tick++;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}
