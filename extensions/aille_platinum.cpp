/*
 * AILLE Framework - Commodity Risk & Growth Advisory Module (Indexed)
 * PLATINUM Implementation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * Licensed under the PolyForm Noncommercial License 1.0.0
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
