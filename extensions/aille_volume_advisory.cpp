/*
 * AILLE Framework - Intraday Volume Advisory Module (VAM) Implementation
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#include "../aille.hpp"
#include "aille_volume_advisory.hpp"

namespace AILLE {

// We'll define the out-of-line integration hook inside the AILLEEngine class.
// This is done in aille.hpp / aille_framework.cpp, and implemented here.
// Let's implement evaluate_volume_advisory() as declared in aille.hpp.

void AILLEEngine::evaluate_volume_advisory() {
    if (volume_state_ != nullptr && volume_advisory_ != nullptr) {
        *volume_advisory_ = evaluate_volume_state(*volume_state_, safety_state_, stabilizer_advisory_, config.enable_contrarian_oversold, config.contrarian_oversold_aggressiveness);
    }
}

} // namespace AILLE
