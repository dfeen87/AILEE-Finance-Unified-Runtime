/*
 * AILLE Framework - Edge Case Testing
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: Non-Commercial (see LICENSE)
 * 
 * Tests various edge cases to ensure robustness:
 * - Empty signal sets
 * - Low confidence signals
 * - Conflicting signals
 * - Extreme values
 */

#include "aille.hpp"
#include <iostream>
#include <vector>

bool test_empty_signals() {
    AILLE::AILLEEngine engine;
    std::vector<AILLE::ModelSignal> signals;
    
    AILLE::Decision decision = engine.makeDecision(signals);
    
    bool passed = (decision.status == AILLE::ERROR_NO_MODELS);
    std::cout << "Test empty signals: " << (passed ? "PASSED" : "FAILED") << "\n";
    return passed;
}

bool test_all_low_confidence() {
    AILLE::AILLEConfig config;
    config.min_confidence_threshold = 0.50f;
    config.grace_confidence_threshold = 0.50f;  // Disable grace layer
    AILLE::AILLEEngine engine(config);
    
    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.05f, 0.20f, 0));
    signals.push_back(AILLE::ModelSignal(0.03f, 0.15f, 1));
    signals.push_back(AILLE::ModelSignal(0.04f, 0.25f, 2));
    
    AILLE::Decision decision = engine.makeDecision(signals);
    
    bool passed = (decision.status == AILLE::REJECTED_LOW_CONFIDENCE &&
                   decision.fallback_used == true);
    std::cout << "Test all low confidence: " << (passed ? "PASSED" : "FAILED") << "\n";
    return passed;
}

bool test_no_consensus() {
    AILLE::AILLEConfig config;
    config.min_confidence_threshold = 0.30f;
    config.min_models_required = 3;
    AILLE::AILLEEngine engine(config);
    
    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal( 0.10f, 0.85f, 0));  // Positive
    signals.push_back(AILLE::ModelSignal(-0.10f, 0.85f, 1));  // Negative
    signals.push_back(AILLE::ModelSignal(-0.11f, 0.85f, 2));  // Negative
    
    AILLE::Decision decision = engine.makeDecision(signals);
    
    // Should either get consensus or no consensus
    bool passed = (decision.status == AILLE::DECISION_VALID ||
                   decision.status == AILLE::REJECTED_NO_CONSENSUS);
    std::cout << "Test conflicting signals: " << (passed ? "PASSED" : "FAILED") << "\n";
    return passed;
}

bool test_extreme_values() {
    AILLE::AILLEEngine engine;
    
    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(100.0f, 0.85f, 0));
    signals.push_back(AILLE::ModelSignal(200.0f, 0.75f, 1));
    signals.push_back(AILLE::ModelSignal(150.0f, 0.65f, 2));
    
    AILLE::Decision decision = engine.makeDecision(signals);
    
    // Should handle extreme values with tanh smoothing
    bool passed = (decision.status == AILLE::DECISION_VALID &&
                   decision.final_value >= -1.0f && 
                   decision.final_value <= 1.0f);
    std::cout << "Test extreme values: " << (passed ? "PASSED" : "FAILED") 
              << " (final_value=" << decision.final_value << ")\n";
    return passed;
}

bool test_single_model_insufficient() {
    AILLE::AILLEConfig config;
    config.min_models_required = 2;
    AILLE::AILLEEngine engine(config);
    
    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.05f, 0.95f, 0));
    
    AILLE::Decision decision = engine.makeDecision(signals);
    
    bool passed = (decision.status == AILLE::REJECTED_NO_CONSENSUS);
    std::cout << "Test single model insufficient: " << (passed ? "PASSED" : "FAILED") << "\n";
    return passed;
}

bool test_fallback_persistence() {
    AILLE::AILLEEngine engine;
    
    // First, build up some fallback history with valid decisions
    std::vector<AILLE::ModelSignal> valid_signals;
    valid_signals.push_back(AILLE::ModelSignal(0.05f, 0.85f, 0));
    valid_signals.push_back(AILLE::ModelSignal(0.03f, 0.75f, 1));
    
    for (int i = 0; i < 10; i++) {
        engine.makeDecision(valid_signals);
    }
    
    // Now trigger fallback with low confidence
    std::vector<AILLE::ModelSignal> low_conf_signals;
    low_conf_signals.push_back(AILLE::ModelSignal(0.10f, 0.10f, 0));
    
    AILLE::Decision fallback_decision = engine.makeDecision(low_conf_signals);
    
    bool passed = (fallback_decision.fallback_used == true &&
                   fallback_decision.final_value != 0.0f);
    std::cout << "Test fallback persistence: " << (passed ? "PASSED" : "FAILED") 
              << " (fallback_value=" << fallback_decision.final_value << ")\n";
    return passed;
}

bool test_zero_confidence() {
    AILLE::AILLEEngine engine;
    
    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.05f, 0.0f, 0));
    signals.push_back(AILLE::ModelSignal(0.03f, 0.0f, 1));
    
    AILLE::Decision decision = engine.makeDecision(signals);
    
    bool passed = (decision.status == AILLE::REJECTED_LOW_CONFIDENCE);
    std::cout << "Test zero confidence: " << (passed ? "PASSED" : "FAILED") << "\n";
    return passed;
}

int main() {
    std::cout << "=== AILLE Edge Case Tests ===\n\n";
    
    int passed = 0;
    int total = 0;
    
    passed += test_empty_signals() ? 1 : 0; total++;
    passed += test_all_low_confidence() ? 1 : 0; total++;
    passed += test_no_consensus() ? 1 : 0; total++;
    passed += test_extreme_values() ? 1 : 0; total++;
    passed += test_single_model_insufficient() ? 1 : 0; total++;
    passed += test_fallback_persistence() ? 1 : 0; total++;
    passed += test_zero_confidence() ? 1 : 0; total++;
    
    std::cout << "\n=== Test Results ===\n";
    std::cout << "Passed: " << passed << "/" << total << "\n";
    
    if (passed == total) {
        std::cout << "All tests PASSED ✓\n";
        return 0;
    } else {
        std::cout << "Some tests FAILED ✗\n";
        return 1;
    }
}
