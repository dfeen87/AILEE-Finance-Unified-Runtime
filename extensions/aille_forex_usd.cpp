/*
 * AILLE Framework - FOREX-USD Advisory Module (FRGAM)
 * AI-Load Integrity and Layered Evaluation
 *
 * Advisory-only deterministic risk evaluation for USD FX behavior.
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * Licensed under the PolyForm Noncommercial License 1.0.0
 */

#include "../aille.hpp"
#include "aille_forex_usd.hpp"

namespace AILLE {

void AILLEEngine::evaluate_forex_usd_advisory() {
    if (forex_usd_state_ && forex_usd_advisory_) {
        *forex_usd_advisory_ = evaluate_forex_usd_state(*forex_usd_state_, safety_state_);
    }
}

} // namespace AILLE
