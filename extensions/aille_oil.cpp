/*
 * AILLE Framework - Commodity Risk & Growth Advisory Module (Indexed)
 * OIL Implementation
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
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
