/*
 * AILLE Framework - Commodity Risk & Growth Advisory Module (Indexed)
 * OIL Implementation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#include "../aille.hpp"
#include "aille_oil.hpp"

namespace AILLE {

void AILLEEngine::evaluate_oil_advisory() {
    if (oil_state_ != nullptr && oil_advisory_ != nullptr) {
        *oil_advisory_ = evaluate_oil_state(*oil_state_, safety_state_);
    }
}

} // namespace AILLE
