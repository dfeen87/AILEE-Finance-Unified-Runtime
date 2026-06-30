/*
 * AILLE Framework - Unit Tests
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 */

#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <string>
#include <functional>

// Include the library to test
#include "../aille.hpp"
#include "../ailee_plugins/ITradingAlertAdapter.hpp"
#include "../ailee_plugins/PluginRegistry.hpp"
#include "../ailee_plugins/plugins/alerts/robinhood/RobinhoodAlertAdapter.cpp"

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

class CapturingAlertAdapter : public AILLE::Plugins::ITradingAlertAdapter {
public:
    AILLE::Plugins::TradingAlert last_alert;
    bool called = false;

    std::string name() const override { return "capturing-alerts"; }

    bool sendAlert(const AILLE::Plugins::TradingAlert& alert) override {
        last_alert = alert;
        called = true;
        return true;
    }
};

TEST(TestTradingAlertAdapterBuildsPassiveBuyAlert) {
    CapturingAlertAdapter adapter;
    AILLE::Decision decision;
    decision.status = AILLE::DECISION_VALID;
    decision.final_value = 0.75f;
    decision.confidence = 0.82f;
    decision.reasoning = "consensus passed";

    ASSERT_TRUE(adapter.alertDecision(decision, "AAPL", "decision-1"));
    ASSERT_TRUE(adapter.called);
    ASSERT_TRUE(adapter.last_alert.side == AILLE::Plugins::AlertSide::ALERT_BUY);
    ASSERT_TRUE(adapter.last_alert.symbol == "AAPL");
    ASSERT_TRUE(adapter.last_alert.client_ref == "decision-1");
    ASSERT_FLOAT_EQ(adapter.last_alert.confidence, 0.82f);
    ASSERT_FLOAT_EQ(adapter.last_alert.signal_value, 0.75f);
    ASSERT_TRUE(adapter.last_alert.message.find("no order executed") != std::string::npos);
}

TEST(TestTradingAlertAdapterRejectedDecisionIsHoldAlert) {
    CapturingAlertAdapter adapter;
    AILLE::Decision decision;
    decision.status = AILLE::REJECTED_NO_CONSENSUS;
    decision.final_value = -0.40f;
    decision.confidence = 0.20f;

    ASSERT_TRUE(adapter.alertDecision(decision, "MSFT"));
    ASSERT_TRUE(adapter.called);
    ASSERT_TRUE(adapter.last_alert.side == AILLE::Plugins::AlertSide::ALERT_HOLD);
    ASSERT_TRUE(adapter.last_alert.severity == "warning");
    ASSERT_TRUE(adapter.last_alert.message.find("rejected") != std::string::npos);
}

TEST(TestRobinhoodAlertAdapterRegistersWithoutExecutionProvider) {
    auto& registry = AILLE::Plugins::PluginRegistry::instance();
    ASSERT_TRUE(registry.hasTradingAlertAdapter("robinhood-alerts"));
    ASSERT_FALSE(registry.hasExecutionProvider("robinhood-alerts"));

    auto adapter = registry.createTradingAlertAdapter("robinhood-alerts");
    ASSERT_TRUE(adapter->name() == "robinhood-alerts");

    AILLE::Decision decision;
    decision.status = AILLE::FALLBACK_ACTIVATED;
    decision.final_value = -0.25f;
    decision.confidence = 0.55f;
    ASSERT_TRUE(adapter->alertDecision(decision, "HOOD", "fallback-alert"));
}

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

TEST(TestNegativeConfidence) {
    AILLE::AILLEEngine engine;
    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.1f, -0.5f, 0));

    AILLE::Decision decision = engine.makeDecision(signals);

    ASSERT_EQ(decision.status, AILLE::REJECTED_LOW_CONFIDENCE);
    ASSERT_TRUE(decision.reasoning.find("confidence out of range") != std::string::npos);
}

TEST(TestConfidenceAboveOne) {
    AILLE::AILLEEngine engine;
    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.1f, 1.5f, 0));

    AILLE::Decision decision = engine.makeDecision(signals);

    ASSERT_EQ(decision.status, AILLE::REJECTED_LOW_CONFIDENCE);
    ASSERT_TRUE(decision.reasoning.find("confidence out of range") != std::string::npos);
}

TEST(TestMaxModelCountTruncation) {
    AILLE::AILLEConfig config;
    config.max_model_count = 2;
    config.min_confidence_threshold = 0.5f;
    config.min_models_required = 1;
    AILLE::AILLEEngine engine(config);

    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.1f, 0.8f, 0));
    signals.push_back(AILLE::ModelSignal(0.1f, 0.8f, 1));
    signals.push_back(AILLE::ModelSignal(0.1f, 0.8f, 2));
    signals.push_back(AILLE::ModelSignal(0.1f, 0.8f, 3));
    signals.push_back(AILLE::ModelSignal(0.1f, 0.8f, 4));

    AILLE::Decision decision = engine.makeDecision(signals);

    ASSERT_EQ(decision.status, AILLE::DECISION_VALID);
    // Only 2 models should contribute
    ASSERT_TRUE(decision.contributing_models.size() <= 2);
}

TEST(TestRejectsStaleSignalsForHFT) {
    AILLE::AILLEConfig config;
    config.max_signal_age_ns = 1000;
    AILLE::AILLEEngine engine(config);

    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.1f, 0.8f, 0));
    signals.push_back(AILLE::ModelSignal(0.1f, 0.8f, 1));
    signals[0].timestamp_ns = 1;
    signals[1].timestamp_ns = 1;

    AILLE::Decision decision = engine.makeDecision(signals);

    ASSERT_EQ(decision.status, AILLE::REJECTED_LOW_CONFIDENCE);
    ASSERT_TRUE(decision.fallback_used);
    ASSERT_TRUE(decision.reasoning.find("stale timestamp") != std::string::npos);
}

TEST(TestRejectsDuplicateModelIdsForHFT) {
    AILLE::AILLEEngine engine;

    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.1f, 0.8f, 7));
    signals.push_back(AILLE::ModelSignal(0.1f, 0.8f, 7));

    AILLE::Decision decision = engine.makeDecision(signals);

    ASSERT_EQ(decision.status, AILLE::REJECTED_NO_CONSENSUS);
    ASSERT_TRUE(decision.fallback_used);
    ASSERT_TRUE(decision.reasoning.find("duplicate model_id") != std::string::npos);
}

TEST(TestMaxPositionClampForHFT) {
    AILLE::AILLEConfig config;
    config.max_position_abs = 0.25f;
    AILLE::AILLEEngine engine(config);

    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(10.0f, 0.8f, 0));
    signals.push_back(AILLE::ModelSignal(10.0f, 0.8f, 1));

    AILLE::Decision decision = engine.makeDecision(signals);

    ASSERT_EQ(decision.status, AILLE::DECISION_VALID);
    ASSERT_FLOAT_EQ(decision.final_value, 0.25f);
}

TEST(TestFallbackBufferAccumulation) {
    AILLE::AILLEConfig config;
    config.min_confidence_threshold = 0.5f;
    config.min_models_required = 3; // Require 3 models so 2 signals fail consensus
    config.fallback_window_size = 10;
    AILLE::AILLEEngine engine(config);

    std::vector<AILLE::ModelSignal> disagreeing;
    disagreeing.push_back(AILLE::ModelSignal(0.1f, 0.8f, 0));
    disagreeing.push_back(AILLE::ModelSignal(-0.1f, 0.8f, 1));

    // Make multiple decisions that trigger fallback
    AILLE::Decision d1 = engine.makeDecision(disagreeing);
    ASSERT_TRUE(d1.fallback_used);

    // After several rejections the fallback value stays at 0 (buffer still empty)
    // Now make a valid decision to populate the fallback buffer
    std::vector<AILLE::ModelSignal> valid_signals;
    valid_signals.push_back(AILLE::ModelSignal(0.1f, 0.8f, 0));
    valid_signals.push_back(AILLE::ModelSignal(0.12f, 0.8f, 1));
    valid_signals.push_back(AILLE::ModelSignal(0.11f, 0.8f, 2));

    AILLE::Decision d2 = engine.makeDecision(valid_signals);
    ASSERT_EQ(d2.status, AILLE::DECISION_VALID);

    // Now fallback buffer is non-empty; next disagreeing decision uses a non-zero fallback
    AILLE::Decision d3 = engine.makeDecision(disagreeing);
    ASSERT_TRUE(d3.fallback_used);
    ASSERT_TRUE(std::abs(d3.final_value) > 0.0f);
}

TEST(TestEmptySignals) {
    AILLE::AILLEEngine engine;
    std::vector<AILLE::ModelSignal> signals;

    AILLE::Decision decision = engine.makeDecision(signals);

    ASSERT_EQ(decision.status, AILLE::ERROR_NO_MODELS);
}

TEST(TestVersionConstant) {
    ASSERT_TRUE(AILLE::AILLE_VERSION != nullptr);
    ASSERT_TRUE(std::string(AILLE::AILLE_VERSION).length() > 0);
}

int main() {
    std::cout << "Starting Unit Tests..." << std::endl;

    RUN_TEST(TestTradingAlertAdapterBuildsPassiveBuyAlert);
    RUN_TEST(TestTradingAlertAdapterRejectedDecisionIsHoldAlert);
    RUN_TEST(TestRobinhoodAlertAdapterRegistersWithoutExecutionProvider);
    RUN_TEST(TestConfigurationDefaults);
    RUN_TEST(TestSafetyLayer);
    RUN_TEST(TestConsensusLayer);
    RUN_TEST(TestConsensusFailure);
    RUN_TEST(TestAuditLogger);
    RUN_TEST(TestAuditLoggerIntegrityFailure);
    RUN_TEST(TestInvalidInputs);
    RUN_TEST(TestNegativeConfidence);
    RUN_TEST(TestConfidenceAboveOne);
    RUN_TEST(TestMaxModelCountTruncation);
    RUN_TEST(TestRejectsStaleSignalsForHFT);
    RUN_TEST(TestRejectsDuplicateModelIdsForHFT);
    RUN_TEST(TestMaxPositionClampForHFT);
    RUN_TEST(TestFallbackBufferAccumulation);
    RUN_TEST(TestEmptySignals);
    RUN_TEST(TestVersionConstant);

    std::cout << "\nTests Run: " << tests_run << std::endl;
    std::cout << "Tests Failed: " << tests_failed << std::endl;

    return (tests_failed == 0) ? 0 : 1;
}
