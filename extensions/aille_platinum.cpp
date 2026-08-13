/*
 * AILLE Framework - Commodity Risk & Growth Advisory Module (Indexed)
 * PLATINUM Implementation
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#include "../aille.hpp"
#include "aille_platinum.hpp"

namespace AILLE {

void AILLEEngine::evaluate_platinum_advisory() {
    if (platinum_state_ != nullptr && platinum_advisory_ != nullptr) {
        *platinum_advisory_ = evaluate_platinum_state(*platinum_state_, safety_state_);
    }
}

} // namespace AILLE
