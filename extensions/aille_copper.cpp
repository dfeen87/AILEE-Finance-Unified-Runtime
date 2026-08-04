/*
 * AILLE Framework - Commodity Risk & Growth Advisory Module (Indexed)
 * COPPER Implementation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * Licensed under the PolyForm Noncommercial License 1.0.0
 */

#include "../aille.hpp"
#include "aille_copper.hpp"

namespace AILLE {

void AILLEEngine::evaluate_copper_advisory() {
    if (copper_state_ != nullptr && copper_advisory_ != nullptr) {
        *copper_advisory_ = evaluate_copper_state(*copper_state_, safety_state_);
    }
}

} // namespace AILLE
