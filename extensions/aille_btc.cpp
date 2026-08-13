/*
 * AILLE Framework - BTC Risk & Growth Advisory Module Implementation
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#include "../aille.hpp"
#include "aille_btc.hpp"
#include "aille_math.hpp"

namespace AILLE {

void AILLEEngine::evaluate_btc_advisory() {
    if (btc_state_ != nullptr && btc_advisory_ != nullptr) {
        *btc_advisory_ = evaluate_btc_state(*btc_state_, safety_state_);
    }
}

} // namespace AILLE