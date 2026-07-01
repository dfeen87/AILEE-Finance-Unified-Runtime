/*
 * AILLE Framework - Commodity Risk & Growth Advisory Module (Indexed)
 * GOLD Implementation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#include "../aille.hpp"
#include "aille_gold.hpp"

namespace AILLE {

void AILLEEngine::evaluate_gold_advisory() {
    if (gold_state_ != nullptr && gold_advisory_ != nullptr) {
        *gold_advisory_ = evaluate_gold_state(*gold_state_, safety_state_);
    }
}

} // namespace AILLE
