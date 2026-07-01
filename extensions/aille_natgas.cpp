/*
 * AILLE Framework - Commodity Risk & Growth Advisory Module (Indexed)
 * NATGAS Implementation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#include "../aille.hpp"
#include "aille_natgas.hpp"

namespace AILLE {

void AILLEEngine::evaluate_natgas_advisory() {
    if (natgas_state_ != nullptr && natgas_advisory_ != nullptr) {
        *natgas_advisory_ = evaluate_natgas_state(*natgas_state_, safety_state_);
    }
}

} // namespace AILLE
