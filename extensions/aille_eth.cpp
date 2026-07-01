/*
 * AILLE Framework - ETH Risk & Growth Advisory Module Implementation
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#include "../aille.hpp"
#include "aille_eth.hpp"

namespace AILLE {

void AILLEEngine::evaluate_eth_advisory() {
    if (eth_state_ != nullptr && eth_advisory_ != nullptr) {
        *eth_advisory_ = evaluate_eth_state(*eth_state_, safety_state_);
    }
}

} // namespace AILLE
