/*
 * AILLE Framework - Commodity Risk & Growth Advisory Module (Indexed)
 * SILVER Implementation
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#include "../aille.hpp"
#include "aille_silver.hpp"

namespace AILLE {

void AILLEEngine::evaluate_silver_advisory() {
    if (silver_state_ != nullptr && silver_advisory_ != nullptr) {
        *silver_advisory_ = evaluate_silver_state(*silver_state_, safety_state_);
    }
}

} // namespace AILLE
