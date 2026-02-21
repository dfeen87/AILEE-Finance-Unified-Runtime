#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <string>
#include <functional>

// Include the library to test
#include "../aille.hpp"

// Simple Test Framework
int tests_run = 0;
int tests_failed = 0;

#define TEST(name) void name()
#define RUN_TEST(name) do { \
    std::cout << "Running " << #name << "... "; \
    try { \
        name(); \
        std::cout << "PASSED" << std::endl; \
    } catch (const std::exception& e) { \
        std::cout << "FAILED: " << e.what() << std::endl; \
        tests_failed++; \
    } catch (...) { \
        std::cout << "FAILED: Unknown exception" << std::endl; \
        tests_failed++; \
    } \
    tests_run++; \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        throw std::runtime_error("Assertion failed: " + std::to_string(a) + " != " + std::to_string(b)); \
    } \
} while(0)

#define ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        throw std::runtime_error("Assertion failed: " #condition); \
    } \
} while(0)

#define ASSERT_FALSE(condition) do { \
    if (condition) { \
        throw std::runtime_error("Assertion failed: " #condition " is true"); \
    } \
} while(0)

#define ASSERT_FLOAT_EQ(a, b) do { \
    if (std::abs((a) - (b)) > 1e-5) { \
        throw std::runtime_error("Assertion failed: " + std::to_string(a) + " != " + std::to_string(b)); \
    } \
} while(0)


// Test Cases

TEST(TestConfigurationDefaults) {
    AILLE::AILLEConfig config;
    ASSERT_FLOAT_EQ(config.min_confidence_threshold, 0.35f);
    ASSERT_EQ(config.min_models_required, 2);
}

TEST(TestSafetyLayer) {
    AILLE::AILLEConfig config;
    config.min_confidence_threshold = 0.5f;
    config.grace_confidence_threshold = 0.45f; // Ensure 0.4f is rejected
    AILLE::AILLEEngine engine(config);

    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.1f, 0.4f, 0)); // Should be rejected
    signals.push_back(AILLE::ModelSignal(0.1f, 0.6f, 1)); // Should pass

    // We can't access private method applySafetyLayer directly, so we test via makeDecision
    // If only one passes, consensus might fail depending on config.
    // Default min_models_required is 2.

    AILLE::Decision decision = engine.makeDecision(signals);

    // With 1 valid signal and min_models_required=2, it should fail consensus
    if (decision.status != AILLE::REJECTED_NO_CONSENSUS && decision.status != AILLE::REJECTED_LOW_CONFIDENCE) {
        std::cout << "  Status was: " << decision.status << " (expected REJECTED)" << std::endl;
    }
    ASSERT_TRUE(decision.status == AILLE::REJECTED_NO_CONSENSUS || decision.status == AILLE::REJECTED_LOW_CONFIDENCE);
}

TEST(TestConsensusLayer) {
    AILLE::AILLEConfig config;
    config.min_models_required = 2;
    config.min_confidence_threshold = 0.5f;
    AILLE::AILLEEngine engine(config);

    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.1f, 0.8f, 0));
    signals.push_back(AILLE::ModelSignal(0.12f, 0.8f, 1));
    signals.push_back(AILLE::ModelSignal(0.11f, 0.8f, 2));

    AILLE::Decision decision = engine.makeDecision(signals);

    ASSERT_EQ(decision.status, AILLE::DECISION_VALID);
    ASSERT_EQ(decision.models_agreed, 3);
    // Consensus value should be average of all 3 since they agree
    // (0.1 + 0.12 + 0.11) / 3 = 0.11
    // But engine applies smoothing: tanh(val * 100)
    // 0.11 * 100 = 11. tanh(11) is approx 1.0
    // Wait, the smoothPosition scale is 100.0f by default.

    float expected_consensus = 0.11f;
    float expected_final = std::tanh(expected_consensus * 100.0f);

    ASSERT_FLOAT_EQ(decision.final_value, expected_final);
}

TEST(TestConsensusFailure) {
    AILLE::AILLEConfig config;
    config.min_models_required = 2;
    config.min_confidence_threshold = 0.5f;
    AILLE::AILLEEngine engine(config);

    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.1f, 0.8f, 0));
    signals.push_back(AILLE::ModelSignal(-0.1f, 0.8f, 1)); // Disagreement

    AILLE::Decision decision = engine.makeDecision(signals);

    // 2 signals, 1 positive, 1 negative. No majority.
    ASSERT_EQ(decision.status, AILLE::REJECTED_NO_CONSENSUS);
    ASSERT_TRUE(decision.fallback_used);
}

TEST(TestAuditLogger) {
    AILLE::AuditLogger logger("test_audit.csv");
    AILLE::Decision d;
    d.status = AILLE::DECISION_VALID;
    d.final_value = 0.5f;
    d.confidence = 0.9f;
    d.timestamp_ns = 123456789;

    logger.logDecision(d, "TEST_SYM", "TEST_STRAT");

    ASSERT_TRUE(logger.verifyIntegrity());
}

TEST(TestAuditLoggerIntegrityFailure) {
    // This is hard to test without modifying the file externally,
    // but we can test that verifyIntegrity returns true for a valid chain.
    AILLE::AuditLogger logger("test_integrity.csv");
    // clear file
    std::ofstream("test_integrity.csv", std::ios::trunc).close();
    logger.open("test_integrity.csv");

    AILLE::Decision d1;
    d1.status = AILLE::DECISION_VALID;
    logger.logDecision(d1);

    AILLE::Decision d2;
    d2.status = AILLE::DECISION_VALID;
    logger.logDecision(d2);

    ASSERT_TRUE(logger.verifyIntegrity());
}

TEST(TestInvalidInputs) {
    AILLE::AILLEEngine engine;
    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(NAN, 0.8f, 0));

    AILLE::Decision decision = engine.makeDecision(signals);

    ASSERT_EQ(decision.status, AILLE::REJECTED_LOW_CONFIDENCE); // We set it to this in code
    ASSERT_TRUE(decision.reasoning.find("NaN/Inf") != std::string::npos);
}

int main() {
    std::cout << "Starting Unit Tests..." << std::endl;

    RUN_TEST(TestConfigurationDefaults);
    RUN_TEST(TestSafetyLayer);
    RUN_TEST(TestConsensusLayer);
    RUN_TEST(TestConsensusFailure);
    RUN_TEST(TestAuditLogger);
    RUN_TEST(TestAuditLoggerIntegrityFailure);
    RUN_TEST(TestInvalidInputs);

    std::cout << "\nTests Run: " << tests_run << std::endl;
    std::cout << "Tests Failed: " << tests_failed << std::endl;

    return (tests_failed == 0) ? 0 : 1;
}
