/*
 * AILLE Framework - Market Stabilization Governor Advisory Module Implementation (Layer 7.9)
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#include "../aille.hpp"
#include "aille_stabilizer.hpp"
#include "aille_btc.hpp"
#include "aille_eth.hpp"
#include "aille_oil.hpp"
#include "aille_gold.hpp"
#include "aille_silver.hpp"
#include "aille_copper.hpp"
#include "aille_natgas.hpp"
#include "aille_platinum.hpp"
#include "aille_forex_usd.hpp"

namespace AILLE {

void AILLEEngine::evaluate_stabilizer_advisory() noexcept {
    if (stabilizer_state_ != nullptr && stabilizer_advisory_ != nullptr) {
        *stabilizer_advisory_ = ::AILLE::evaluate_market_stabilizer_advisory(*stabilizer_state_, safety_state_);

        float stabilizer_factor = stabilizer_advisory_->stabilization_factor;
        if (btc_advisory_) btc_advisory_->recommended_weight *= stabilizer_factor;
        if (eth_advisory_) eth_advisory_->recommended_weight *= stabilizer_factor;
        if (oil_advisory_) oil_advisory_->recommended_weight *= stabilizer_factor;
        if (gold_advisory_) gold_advisory_->recommended_weight *= stabilizer_factor;
        if (silver_advisory_) silver_advisory_->recommended_weight *= stabilizer_factor;
        if (copper_advisory_) copper_advisory_->recommended_weight *= stabilizer_factor;
        if (natgas_advisory_) natgas_advisory_->recommended_weight *= stabilizer_factor;
        if (platinum_advisory_) platinum_advisory_->recommended_weight *= stabilizer_factor;
        if (forex_usd_advisory_) forex_usd_advisory_->recommended_weight *= stabilizer_factor;
    }
}

} // namespace AILLE
