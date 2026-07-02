/*
 * AILLE Framework - Macro Risk & Growth Advisory Module Implementation
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#include "../aille.hpp"
#include "aille_macro.hpp"
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

void AILLEEngine::evaluate_macro_advisory() noexcept {
    if (macro_state_ != nullptr && macro_advisory_ != nullptr) {
        *macro_advisory_ = ::AILLE::evaluate_macro_advisory(*macro_state_, safety_state_);

        float macro_factor = macro_advisory_->recommended_macro_weight;
        if (btc_advisory_) btc_advisory_->recommended_weight *= macro_factor;
        if (eth_advisory_) eth_advisory_->recommended_weight *= macro_factor;
        if (oil_advisory_) oil_advisory_->recommended_weight *= macro_factor;
        if (gold_advisory_) gold_advisory_->recommended_weight *= macro_factor;
        if (silver_advisory_) silver_advisory_->recommended_weight *= macro_factor;
        if (copper_advisory_) copper_advisory_->recommended_weight *= macro_factor;
        if (natgas_advisory_) natgas_advisory_->recommended_weight *= macro_factor;
        if (platinum_advisory_) platinum_advisory_->recommended_weight *= macro_factor;
        if (forex_usd_advisory_) forex_usd_advisory_->recommended_weight *= macro_factor;
    }
}

} // namespace AILLE
