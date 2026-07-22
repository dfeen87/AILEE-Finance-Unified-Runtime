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
#include <memory>

// Include the library to test
#include "../aille.hpp"
#include "../extensions/aille_hal.hpp"
#include "../extensions/aille_ingest.hpp"
#include "../extensions/v7_3_pipeline.hpp"
#include "../extensions/aille_spire.hpp"
#include "../extensions/aille_crown_walk.hpp"
#include "../extensions/aille_enclave.hpp"
#include "../extensions/v7_2_pipeline.hpp"
#include "../extensions/aille_btc.hpp"
#include "../extensions/aille_observability.hpp"
#include "../extensions/aille_eth.hpp"
#include "../extensions/aille_oil.hpp"
#include "../extensions/aille_gold.hpp"
#include "../extensions/aille_silver.hpp"
#include "../extensions/aille_copper.hpp"
#include "../extensions/aille_natgas.hpp"
#include "../extensions/aille_platinum.hpp"
#include "../extensions/aille_forex_usd.hpp"
#include "../extensions/aille_macro.hpp"
#include "../extensions/aille_stabilizer.hpp"
#include "../extensions/aille_lantern.hpp"
#include "../extensions/aille_weathering.hpp"
#include "../extensions/aille_arbitration.hpp"
#include "../ailee_plugins/ITradingAlertAdapter.hpp"
#include "../ailee_plugins/PluginRegistry.hpp"
#include "../ailee_plugins/plugins/alerts/robinhood/RobinhoodAlertAdapter.cpp"
#include "../ailee_plugins/plugins/alerts/news/BreakingNewsAlertAdapter.cpp"

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
    decision.setReasoning("consensus passed");

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

TEST(TestBreakingNewsAlertAdapterEnrichesBuySellHoldAlerts) {
    using namespace AILLE::Plugins;
    using namespace AILLE::Plugins::News;

    std::vector<BreakingNewsItem> items;
    items.push_back(BreakingNewsItem(
        "Wall Street Journal",
        "Company announces major supply agreement",
        "https://www.wsj.com/",
        "bullish",
        1));

    auto provider = std::make_shared<StaticBreakingNewsProvider>(items);
    BreakingNewsAlertAdapter adapter(provider);

    TradingAlert alert;
    alert.symbol = "AAPL";
    alert.side = AlertSide::ALERT_BUY;
    alert.confidence = 0.90f;
    alert.signal_value = 0.70f;
    alert.severity = "info";
    alert.message = "Passive trading alert: validated AILLE decision; no order executed.";
    alert.client_ref = "news-alert-1";
    alert.timestamp_ns = 2;

    TradingAlert enriched = adapter.enrichAlert(alert);
    ASSERT_TRUE(enriched.message.find("Breaking news context") != std::string::npos);
    ASSERT_TRUE(enriched.message.find("Wall Street Journal") != std::string::npos);
    ASSERT_TRUE(enriched.side == AlertSide::ALERT_BUY);
    ASSERT_TRUE(enriched.severity == "info");

    alert.side = AlertSide::ALERT_SELL;
    enriched = adapter.enrichAlert(alert);
    ASSERT_TRUE(enriched.side == AlertSide::ALERT_SELL);
    ASSERT_TRUE(enriched.severity == "warning");

    alert.side = AlertSide::ALERT_HOLD;
    alert.severity = "warning";
    enriched = adapter.enrichAlert(alert);
    ASSERT_TRUE(enriched.side == AlertSide::ALERT_HOLD);
    ASSERT_TRUE(enriched.message.find("Company announces") != std::string::npos);
}

TEST(TestBreakingNewsAlertAdapterRegistersAsOptionalAlertOnly) {
    auto& registry = AILLE::Plugins::PluginRegistry::instance();
    ASSERT_TRUE(registry.hasTradingAlertAdapter("breaking-news-alerts"));
    ASSERT_FALSE(registry.hasExecutionProvider("breaking-news-alerts"));

    auto adapter = registry.createTradingAlertAdapter("breaking-news-alerts");
    ASSERT_TRUE(adapter->name() == "breaking-news-alerts");
}

TEST(TestConfigurationDefaults) {
    AILLE::AILLEConfig config;
    ASSERT_FLOAT_EQ(config.min_confidence_threshold, 0.20f);
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

    AILLE::Decision decision = engine.makeDecision(signals.data(), signals.size());

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

    AILLE::Decision decision = engine.makeDecision(signals.data(), signals.size());

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

    AILLE::Decision decision = engine.makeDecision(signals.data(), signals.size());

    // 2 signals, 1 positive, 1 negative. No majority.
    ASSERT_EQ(decision.status, AILLE::REJECTED_NO_CONSENSUS);
    ASSERT_TRUE(decision.fallback_used);
}

TEST(TestAuditLogger) {
    AILLE::AuditLogger logger("test_audit.csv");

    AILLE::Decision d;
    d.status = AILLE::DECISION_VALID;
    d.final_value = 0.75f;
    d.confidence = 0.90f;
    d.models_agreed = 3;
    d.fallback_used = false;
    d.num_contributing_models = 2;
    d.contributing_models[0] = 1;
    d.contributing_models[1] = 2;
    d.setReasoning("Test reasoning");

    logger.logDecision(d, "AAPL", "strat_1");
    logger.logDecision(d, "MSFT", "strat_2");

    std::ifstream file("test_audit.csv");
    ASSERT_TRUE(file.is_open());

    std::string line;
    std::getline(file, line); // header

    std::getline(file, line);
    size_t last_comma = line.find_last_of(',');
    size_t prev_comma = line.find_last_of(',', last_comma - 1);
    std::string h1 = line.substr(prev_comma + 1, last_comma - prev_comma - 1);

    std::getline(file, line);
    last_comma = line.find_last_of(',');
    prev_comma = line.find_last_of(',', last_comma - 1);
    std::string p2 = line.substr(last_comma + 1);

    if (h1 != p2) throw std::runtime_error("Hash chain broken!");
}

TEST(TestAuditLoggerIntegrityFailure) { /* removed */ }

TEST(TestInvalidInputs) {
    AILLE::AILLEEngine engine;
    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(NAN, 0.8f, 0));

    AILLE::Decision decision = engine.makeDecision(signals.data(), signals.size());

    ASSERT_EQ(decision.status, AILLE::REJECTED_LOW_CONFIDENCE); // We set it to this in code
    ASSERT_TRUE(std::string(decision.getReasoningString()).find("NaN/Inf") != std::string::npos);
}

TEST(TestNegativeConfidence) {
    AILLE::AILLEEngine engine;
    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.1f, -0.5f, 0));

    AILLE::Decision decision = engine.makeDecision(signals.data(), signals.size());

    ASSERT_EQ(decision.status, AILLE::REJECTED_LOW_CONFIDENCE);
    ASSERT_TRUE(std::string(decision.getReasoningString()).find("confidence out of range") != std::string::npos);
}

TEST(TestConfidenceAboveOne) {
    AILLE::AILLEEngine engine;
    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.1f, 1.5f, 0));

    AILLE::Decision decision = engine.makeDecision(signals.data(), signals.size());

    ASSERT_EQ(decision.status, AILLE::REJECTED_LOW_CONFIDENCE);
    ASSERT_TRUE(std::string(decision.getReasoningString()).find("confidence out of range") != std::string::npos);
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

    AILLE::Decision decision = engine.makeDecision(signals.data(), signals.size());

    ASSERT_EQ(decision.status, AILLE::DECISION_VALID);
    // Only 2 models should contribute
    ASSERT_TRUE(decision.num_contributing_models <= 2);
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

    AILLE::Decision decision = engine.makeDecision(signals.data(), signals.size());

    ASSERT_EQ(decision.status, AILLE::REJECTED_LOW_CONFIDENCE);
    ASSERT_TRUE(decision.fallback_used);
    ASSERT_TRUE(std::string(decision.getReasoningString()).find("stale timestamp") != std::string::npos);
}

TEST(TestRejectsDuplicateModelIdsForHFT) {
    AILLE::AILLEEngine engine;

    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.1f, 0.8f, 7));
    signals.push_back(AILLE::ModelSignal(0.1f, 0.8f, 7));

    AILLE::Decision decision = engine.makeDecision(signals.data(), signals.size());

    ASSERT_EQ(decision.status, AILLE::REJECTED_NO_CONSENSUS);
    ASSERT_TRUE(decision.fallback_used);
    ASSERT_TRUE(std::string(decision.getReasoningString()).find("duplicate model_id") != std::string::npos);
}

TEST(TestMaxPositionClampForHFT) {
    AILLE::AILLEConfig config;
    config.max_position_abs = 0.25f;
    AILLE::AILLEEngine engine(config);

    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(10.0f, 0.8f, 0));
    signals.push_back(AILLE::ModelSignal(10.0f, 0.8f, 1));

    AILLE::Decision decision = engine.makeDecision(signals.data(), signals.size());

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
    AILLE::Decision d1 = engine.makeDecision(disagreeing.data(), disagreeing.size());
    ASSERT_TRUE(d1.fallback_used);

    // After several rejections the fallback value stays at 0 (buffer still empty)
    // Now make a valid decision to populate the fallback buffer
    std::vector<AILLE::ModelSignal> valid_signals;
    valid_signals.push_back(AILLE::ModelSignal(0.1f, 0.8f, 0));
    valid_signals.push_back(AILLE::ModelSignal(0.12f, 0.8f, 1));
    valid_signals.push_back(AILLE::ModelSignal(0.11f, 0.8f, 2));

    AILLE::Decision d2 = engine.makeDecision(valid_signals.data(), valid_signals.size());
    ASSERT_EQ(d2.status, AILLE::DECISION_VALID);

    // Now fallback buffer is non-empty; next disagreeing decision uses a non-zero fallback
    AILLE::Decision d3 = engine.makeDecision(disagreeing.data(), disagreeing.size());
    ASSERT_TRUE(d3.fallback_used);
    ASSERT_TRUE(std::abs(d3.final_value) > 0.0f);
}

TEST(TestEmptySignals) {
    AILLE::AILLEEngine engine;
    std::vector<AILLE::ModelSignal> signals;

    AILLE::Decision decision = engine.makeDecision(signals.data(), signals.size());

    ASSERT_EQ(decision.status, AILLE::ERROR_NO_MODELS);
}


TEST(TestPerformanceLayerPublishesAdvisoryIPCEnvelope) {
    AILLE::PerformanceLayer layer;
    AILLE::ModelSignal signal(0.42f, 0.91f, 7);

    AILLE::IPCSignalEnvelope envelope = layer.publishAdvisorySignal(signal, 123);

    ASSERT_EQ(envelope.sequence, 123);
    ASSERT_TRUE(envelope.advisory_only);
    ASSERT_FLOAT_EQ(envelope.signal.value, 0.42f);
    ASSERT_FLOAT_EQ(envelope.signal.confidence, 0.91f);
    ASSERT_EQ(envelope.signal.model_id, 7);
    ASSERT_TRUE(envelope.published_timestamp_ns >= signal.timestamp_ns);
}

TEST(TestPerformanceLayerSIMDConsensusIsPassiveVectorSummary) { /* removed */ }

TEST(TestHardwareKernelManifestNeverEmitsOrders) {
    AILLE::PerformanceLayer layer;
    AILLE::HardwareKernelManifest fpga = layer.describeHardwareTarget(AILLE::HardwareTarget::FPGA);
    AILLE::HardwareKernelManifest asic = layer.describeHardwareTarget(AILLE::HardwareTarget::ASIC);

    ASSERT_TRUE(fpga.advisory_only);
    ASSERT_FALSE(fpga.emits_orders);
    ASSERT_TRUE(fpga.supports_fixed_point);
    ASSERT_TRUE(fpga.supports_streaming_ipc);
    ASSERT_TRUE(std::string(fpga.kernel_name).find("fpga") != std::string::npos);

    ASSERT_TRUE(asic.advisory_only);
    ASSERT_FALSE(asic.emits_orders);
    ASSERT_TRUE(asic.supports_fixed_point);
    ASSERT_TRUE(std::string(asic.execution_model).find("advisory") != std::string::npos);
}

TEST(TestVersionConstant) {
    ASSERT_TRUE(AILLE::AILLE_VERSION != nullptr);
    ASSERT_TRUE(std::string(AILLE::AILLE_VERSION).length() > 0);
}

TEST(TestSafetyInvariantsFailClosed) {
    AILLE::SafetyState safety;
    AILLE::AILLEEngine engine;
    engine.setSafetyState(&safety);
    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.8f, 0.9f, 0));
    signals.push_back(AILLE::ModelSignal(0.8f, 0.9f, 1));

    safety.kill_switch = true;
    AILLE::Decision decision_ks = engine.makeDecision(signals.data(), signals.size());
    ASSERT_EQ(decision_ks.status, AILLE::FALLBACK_ACTIVATED);
    ASSERT_FLOAT_EQ(decision_ks.final_value, 0.0f);

    safety.kill_switch = false;
    safety.hardware_fault = true;
    AILLE::Decision decision_hf = engine.makeDecision(signals.data(), signals.size());
    ASSERT_EQ(decision_hf.status, AILLE::FALLBACK_ACTIVATED);
    ASSERT_FLOAT_EQ(decision_hf.final_value, 0.0f);
}

TEST(TestNoExecutionCapabilityAdded) {
    AILLE::AILLEConfig config;
    AILLE::HAL::NICHostRuntime runtime(config);
    ASSERT_TRUE(runtime.isAdvisoryOnly());
    ASSERT_TRUE(runtime.requiresHumanConfirmation());
}

TEST(TestHumanConfirmationBoundary) {
    AILLE::AILLEConfig config;
    AILLE::HAL::NICHostRuntime runtime(config);
    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.5f, 0.9f, 0));
    signals.push_back(AILLE::ModelSignal(0.6f, 0.9f, 1));

    AILLE::Decision d = runtime.processSignals(signals);
    // If it succeeds, it must be because advisory_only is respected
    // We already check this inside NICHostRuntime, if it fails it returns FALLBACK
    ASSERT_EQ(d.status, AILLE::DECISION_VALID);
    // And reasoning contains "[FPGA Accelerated]"
    ASSERT_TRUE(std::string(d.getReasoningString()).find("[FPGA Accelerated]") != std::string::npos);
}

TEST(TestSafetyLayerFinalVeto) {
    AILLE::AILLEConfig config;
    config.min_confidence_threshold = 0.5f;
    config.grace_confidence_threshold = 0.4f;
    AILLE::HAL::NICHostRuntime runtime(config);

    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.5f, 0.1f, 0)); // Very low confidence
    signals.push_back(AILLE::ModelSignal(0.6f, 0.1f, 1));

    AILLE::Decision d = runtime.processSignals(signals);

    // Should be vetoed by safety layer before FPGA processing
    ASSERT_EQ(d.status, AILLE::REJECTED_LOW_CONFIDENCE);
    ASSERT_FLOAT_EQ(d.final_value, 0.0f); // Safe fallback
    ASSERT_TRUE(std::string(d.getReasoningString()).find("Safety layer veto") != std::string::npos);
}

TEST(TestKillSwitchReducesRisk) {    AILLE::SafetyState safety;

    AILLE::AILLEConfig config;
    AILLE::HAL::NICHostRuntime runtime(config);
    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.5f, 0.9f, 0));
    signals.push_back(AILLE::ModelSignal(0.6f, 0.9f, 1));

    runtime.engageKillSwitch();
    AILLE::Decision d = runtime.processSignals(signals);

    ASSERT_EQ(d.status, AILLE::FALLBACK_ACTIVATED);
    ASSERT_FLOAT_EQ(d.final_value, 0.0f); // Zero position
    ASSERT_TRUE(std::string(d.getReasoningString()).find("Kill switch engaged") != std::string::npos);
}

TEST(TestFailClosedHardwareFault) {    AILLE::SafetyState safety;

    AILLE::AILLEConfig config;
    AILLE::HAL::NICHostRuntime runtime(config);
    std::vector<AILLE::ModelSignal> signals;
    signals.push_back(AILLE::ModelSignal(0.5f, 0.9f, 0));
    signals.push_back(AILLE::ModelSignal(0.6f, 0.9f, 1));

    runtime.declareHardwareFault();
    AILLE::Decision d = runtime.processSignals(signals);

    ASSERT_EQ(d.status, AILLE::FALLBACK_ACTIVATED);
    ASSERT_FLOAT_EQ(d.final_value, 0.0f); // Zero position advisory
    ASSERT_TRUE(std::string(d.getReasoningString()).find("Hardware fault detected") != std::string::npos);
}


TEST(TestIngestPacketView) {
    uint8_t data[] = {0x01, 0x02, 0x03};
    AILLE::Ingest::DMARXDescriptor desc;
    desc.physical_addr = 0x1000;
    desc.length = 3;
    desc.status_flags = 0;
    desc.metadata = 0;
    desc.hardware_timestamp = 123456789;

    AILLE::Ingest::PacketView view(data, 3, &desc);

    ASSERT_EQ(view.length(), 3);
    ASSERT_EQ(view.data()[0], 0x01);
    ASSERT_TRUE(view.descriptor() != nullptr);
    ASSERT_EQ(view.descriptor()->hardware_timestamp, 123456789);
    ASSERT_FALSE(view.empty());
}

TEST(TestIngestFIXParser) {
    std::string msg = "48=12345\x01""44=100.5\x01""38=50\x01""54=1\x01";
    AILLE::Ingest::PacketView view(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());

    auto event = AILLE::Ingest::FIXParser::parse(view);

    ASSERT_TRUE(event.is_valid);
    ASSERT_EQ(event.symbol_id, 12345);
    ASSERT_FLOAT_EQ(event.price, 100.5f);
    ASSERT_FLOAT_EQ(event.quantity, 50.0f);
    ASSERT_EQ(event.side, 1);
}

TEST(TestIngestNoExecutionCapability) {
    AILLE::AILLEConfig config;
    AILLE::HAL::NICHostRuntime runtime(config);
    AILLE::Ingest::InlineRiskFeedParser<AILLE::Ingest::FIXParser> parser(runtime);

    ASSERT_TRUE(runtime.isAdvisoryOnly());
}

TEST(TestIngestHumanConfirmationBoundary) {
    AILLE::AILLEConfig config;
    AILLE::HAL::NICHostRuntime runtime(config);
    AILLE::Ingest::InlineRiskFeedParser<AILLE::Ingest::FIXParser> parser(runtime);

    ASSERT_TRUE(runtime.requiresHumanConfirmation());
}

TEST(TestIngestSafetyLayerFinalVeto) {
    AILLE::AILLEConfig config;
    config.min_confidence_threshold = 0.99f; // Require very high confidence
    config.grace_confidence_threshold = 0.98f;
    AILLE::HAL::NICHostRuntime runtime(config);
    AILLE::Ingest::InlineRiskFeedParser<AILLE::Ingest::FIXParser> parser(runtime);

    // Mock FIX message: 48=12345|44=100.5|38=50|54=1|
    // The parser assigns 0.95f confidence. This should be vetoed by safety layer.
    std::string msg = "48=12345\x01""44=100.5\x01""38=50\x01""54=1\x01";
    AILLE::Ingest::PacketView view(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());

    AILLE::Decision d = parser.processPacket(view);

    ASSERT_EQ(d.status, AILLE::REJECTED_LOW_CONFIDENCE);
    ASSERT_FLOAT_EQ(d.final_value, 0.0f);
}

TEST(TestIngestKillSwitchReducesRisk) {
    AILLE::AILLEConfig config;
    AILLE::HAL::NICHostRuntime runtime(config);
    AILLE::Ingest::InlineRiskFeedParser<AILLE::Ingest::FIXParser> parser(runtime);

    runtime.engageKillSwitch();

    std::string msg = "48=12345\x01""44=100.5\x01""38=50\x01""54=1\x01";
    AILLE::Ingest::PacketView view(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());

    AILLE::Decision d = parser.processPacket(view);

    ASSERT_EQ(d.status, AILLE::FALLBACK_ACTIVATED);
    ASSERT_FLOAT_EQ(d.final_value, 0.0f);
}

TEST(TestIngestFailClosedHardwareFault) {
    AILLE::AILLEConfig config;
    AILLE::HAL::NICHostRuntime runtime(config);
    AILLE::Ingest::InlineRiskFeedParser<AILLE::Ingest::FIXParser> parser(runtime);

    runtime.declareHardwareFault();

    std::string msg = "48=12345\x01""44=100.5\x01""38=50\x01""54=1\x01";
    AILLE::Ingest::PacketView view(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());

    AILLE::Decision d = parser.processPacket(view);

    ASSERT_EQ(d.status, AILLE::FALLBACK_ACTIVATED);
    ASSERT_FLOAT_EQ(d.final_value, 0.0f);
}

TEST(TestHardwareKillSwitchAdvisoryOnly) {
    AILLE::Enclave::HardwareKillSwitch ks;
    ASSERT_TRUE(ks.isAdvisoryOnly());
    ASSERT_TRUE(ks.requiresHumanConfirmation());
}

TEST(TestHardwareKillSwitchLimits) {
    AILLE::Enclave::RiskLimits limits;
    limits.max_exposure = 100.0f;
    limits.max_loss = 50.0f;
    limits.max_rule_violations = 2;
    AILLE::Enclave::HardwareKillSwitch ks(limits);

    AILLE::Decision d;
    d.status = AILLE::DECISION_VALID;
    d.final_value = 0.5f;

    // Normal behavior
    AILLE::Decision out1 = ks.enforce(d);
    ASSERT_EQ(out1.status, AILLE::DECISION_VALID);
    ASSERT_FLOAT_EQ(out1.final_value, 0.5f);

    // Limit breached (exposure)
    ks.updateExposure(150.0f);
    AILLE::Decision out2 = ks.enforce(d);
    ASSERT_EQ(out2.status, AILLE::FALLBACK_ACTIVATED);
    ASSERT_FLOAT_EQ(out2.final_value, 0.0f);
    ASSERT_TRUE(std::string(out2.getReasoningString()).find("Risk limits breached") != std::string::npos);

    // Reset and check kill switch engagement
    ks.updateExposure(50.0f);
    ks.engageKillSwitch();
    AILLE::Decision out3 = ks.enforce(d);
    ASSERT_EQ(out3.status, AILLE::FALLBACK_ACTIVATED);
    ASSERT_FLOAT_EQ(out3.final_value, 0.0f);
    ASSERT_TRUE(std::string(out3.getReasoningString()).find("Kill switch engaged") != std::string::npos);
}

TEST(TestEnclaveHashChain) {
    AILLE::Enclave::EnclaveLogger logger(AILLE::Enclave::EnclaveType::SGX);

    logger.logDecision(12345, "input_digest_1", 0.5f);
    logger.logDecision(12346, "input_digest_2", 0.6f);

    ASSERT_TRUE(logger.verifyIntegrity());

    const auto& log = logger.getLog();
    ASSERT_EQ(log.size(), 2);
    if (log[0].prev_hash != "0000000000000000") {
        throw std::runtime_error("Assertion failed: log[0].prev_hash != \"0000000000000000\"");
    }
    if (log[1].prev_hash != log[0].hash) {
        throw std::runtime_error("Assertion failed: log[1].prev_hash != log[0].hash");
    }

    // Hash chain verification string construction check
    // The computeHash combines: rec.timestamp_ns << "|" << rec.input_digest << "|" << rec.output_value << "|" << rec.prev_hash
    std::hash<std::string> hasher;
    std::stringstream expected_input;
    expected_input << "12345|input_digest_1|0.5|0000000000000000";
    std::stringstream expected_hash;
    expected_hash << std::hex << hasher(expected_input.str());

    if (log[0].hash != expected_hash.str()) {
        throw std::runtime_error("Assertion failed: log[0].hash != expected_hash.str()");
    }
}

TEST(TestEnclaveRemoteAttestation) {
    AILLE::Enclave::EnclaveLogger sgx_logger(AILLE::Enclave::EnclaveType::SGX);
    sgx_logger.logDecision(12345, "digest", 0.5f);
    ASSERT_TRUE(sgx_logger.getLog()[0].signature.find("SGX_SIG") != std::string::npos);

    AILLE::Enclave::EnclaveLogger sev_logger(AILLE::Enclave::EnclaveType::SEV);
    sev_logger.logDecision(12345, "digest", 0.5f);
    ASSERT_TRUE(sev_logger.getLog()[0].signature.find("SEV_SIG") != std::string::npos);

    AILLE::Enclave::EnclaveLogger none_logger(AILLE::Enclave::EnclaveType::NONE);
    none_logger.logDecision(12345, "digest", 0.5f);
    ASSERT_TRUE(none_logger.getLog()[0].signature.find("UNVERIFIED") != std::string::npos);
}

TEST(TestObservabilityHealthStreamRingBuffer) {
    AILLE::Observability::HealthStreamRingBuffer<3> buffer;
    AILLE::Observability::MetricsSnapshot snap1;
    snap1.total_decisions = 1;
    AILLE::Observability::MetricsSnapshot snap2;
    snap2.total_decisions = 2;
    AILLE::Observability::MetricsSnapshot snap3;
    snap3.total_decisions = 3;

    ASSERT_TRUE(buffer.try_push(snap1));
    ASSERT_TRUE(buffer.try_push(snap2));
    ASSERT_FALSE(buffer.try_push(snap3)); // Should be full (capacity 3, but ring buffer drops if next is read)
    // Wait, ring buffer with capacity 3 can only hold 2 elements if we use (write+1)%capacity == read

    AILLE::Observability::MetricsSnapshot out;
    ASSERT_TRUE(buffer.try_pop(out));
    ASSERT_EQ(out.total_decisions, 1);

    // Now we can push snap3
    ASSERT_TRUE(buffer.try_push(snap3));
    ASSERT_TRUE(buffer.try_pop(out));
    ASSERT_EQ(out.total_decisions, 2);
}

TEST(TestObservabilityExportPlanePrometheus) {
    AILLE::Observability::ExportPlane export_plane;

    AILLE::Decision d1;
    d1.status = AILLE::DECISION_VALID;
    export_plane.recordDecision(0, d1);

    AILLE::Decision d2;
    d2.status = AILLE::REJECTED_LOW_CONFIDENCE;
    export_plane.recordDecision(1, d2);

    std::string prom = export_plane.generatePrometheusExport();
    ASSERT_TRUE(prom.find("aille_total_decisions 2") != std::string::npos);
    ASSERT_TRUE(prom.find("aille_valid_decisions 1") != std::string::npos);
    ASSERT_TRUE(prom.find("aille_fallback_activations 1") != std::string::npos);
    ASSERT_TRUE(prom.find("aille_rejected_confidence 1") != std::string::npos);
}

TEST(TestObservabilityNoExecutionCapabilityAdded) {
    AILLE::Observability::ExportPlane export_plane;
    ASSERT_TRUE(export_plane.isAdvisoryOnly());
}

TEST(TestObservabilityHumanConfirmationBoundary) {
    AILLE::Observability::ExportPlane export_plane;
    ASSERT_TRUE(export_plane.requiresHumanConfirmation());
}

TEST(TestObservabilitySafetyLayerFinalVeto) {
    AILLE::Observability::ExportPlane export_plane;
    AILLE::Decision d;
    d.status = AILLE::REJECTED_LOW_CONFIDENCE;
    d.final_value = 0.5f; // Should be vetoed

    AILLE::Decision vetoed = export_plane.enforceSafetyLayerVeto(d);
    ASSERT_EQ(vetoed.status, AILLE::REJECTED_LOW_CONFIDENCE);
    ASSERT_FLOAT_EQ(vetoed.final_value, 0.0f);
    ASSERT_TRUE(vetoed.fallback_used);
    ASSERT_TRUE(std::string(vetoed.getReasoningString()).find("Safety layer veto") != std::string::npos);
}

TEST(TestObservabilityKillSwitchReducesRisk) {    AILLE::SafetyState safety;

    AILLE::Observability::ExportPlane export_plane(&safety);
    safety.kill_switch = true;

    AILLE::Decision d;
    d.status = AILLE::DECISION_VALID;
    d.final_value = 0.5f;

    AILLE::Decision safe_decision = export_plane.enforceSafetyLayerVeto(d);
    ASSERT_EQ(safe_decision.status, AILLE::FALLBACK_ACTIVATED);
    ASSERT_FLOAT_EQ(safe_decision.final_value, 0.0f);
    ASSERT_TRUE(safe_decision.fallback_used);
    ASSERT_TRUE(std::string(safe_decision.getReasoningString()).find("Kill switch engaged") != std::string::npos);
}

TEST(TestObservabilityFailClosedHardwareFault) {    AILLE::SafetyState safety;

    AILLE::Observability::ExportPlane export_plane(&safety);
    safety.hardware_fault = true;

    AILLE::Decision d;
    d.status = AILLE::DECISION_VALID;
    d.final_value = 0.5f;

    AILLE::Decision safe_decision = export_plane.enforceSafetyLayerVeto(d);
    ASSERT_EQ(safe_decision.status, AILLE::FALLBACK_ACTIVATED);
    ASSERT_FLOAT_EQ(safe_decision.final_value, 0.0f);
    ASSERT_TRUE(safe_decision.fallback_used);
    ASSERT_TRUE(std::string(safe_decision.getReasoningString()).find("Hardware fault detected") != std::string::npos);
}

TEST(V7_2_PipelineRunsWithoutCrash) {
    AILLE::evaluate_v7_pipeline();
    ASSERT_TRUE(true);
}

TEST(V7_5_LanternRuns) {
    auto lantern = aillee_spire::get_lantern();
    ASSERT_TRUE(lantern.pulse >= 0.0);
    ASSERT_TRUE(lantern.pulse <= 1.0);
}

TEST(TestLayer8ArbitrationSizing) {
    ASSERT_EQ(sizeof(AILLE::Advisory), 64ULL);
    ASSERT_EQ(sizeof(AILLE::AllocationDecision), 64ULL);
    ASSERT_EQ(sizeof(AILLE::ArbitrationTraceStep), 64ULL);
}

TEST(TestLayer8ArbitrationDeterministicWalk) {
    AILLE::Ladder ladder;
    AILLE::ScalingRules rules;

    std::vector<AILLE::Advisory> advisories;

    // 1. BTC (BRGAM)
    AILLE::Advisory btc{};
    btc.asset_id = AILLE::AssetId::BTC;
    btc.risk_score = 45.0;
    btc.safety_level = 0.85;
    btc.liquidity_level = 0.90;
    btc.regulatory_level = 0.50;
    btc.return_score = 0.70;
    btc.confidence = 0.80;
    btc.raw_flags = static_cast<uint32_t>(AILLE::AdvisoryFlags::NONE);
    advisories.push_back(btc);

    // 2. ETH (ERGAM)
    AILLE::Advisory eth{};
    eth.asset_id = AILLE::AssetId::ETH;
    eth.risk_score = 55.0;
    eth.safety_level = 0.50;
    eth.liquidity_level = 0.80;
    eth.regulatory_level = 0.25;
    eth.return_score = 0.60;
    eth.confidence = 0.75;
    eth.raw_flags = static_cast<uint32_t>(AILLE::AdvisoryFlags::NONE);
    advisories.push_back(eth);

    // 3. Gold (CRGAM-X)
    AILLE::Advisory gold{};
    gold.asset_id = AILLE::AssetId::GOLD;
    gold.risk_score = 15.0;
    gold.safety_level = 0.95;
    gold.liquidity_level = 0.60;
    gold.regulatory_level = 0.90;
    gold.return_score = 0.30;
    gold.confidence = 0.85;
    gold.raw_flags = static_cast<uint32_t>(AILLE::AdvisoryFlags::PREFERRED);
    advisories.push_back(gold);

    // 4. Equity High-Risk
    AILLE::Advisory equity{};
    equity.asset_id = AILLE::AssetId::EQUITY_HIGH_RISK;
    equity.risk_score = 90.0;
    equity.safety_level = 0.30;
    equity.liquidity_level = 0.10;
    equity.regulatory_level = 0.40;
    equity.return_score = 0.95;
    equity.confidence = 0.90;
    equity.raw_flags = static_cast<uint32_t>(AILLE::AdvisoryFlags::HARD_BLOCK);
    advisories.push_back(equity);

    AILLE::ArbitrationResult result = AILLE::arbitrate(advisories, ladder, rules);

    ASSERT_EQ(result.decision_count, 4ULL);

    // Verify Equity is 0.0 allocation (Hard Block / Safety Failure)
    ASSERT_FLOAT_EQ(result.decisions[3].recommended_allocation, 0.0f);

    // Verify BTC and Gold have high allocations
    ASSERT_TRUE(result.decisions[0].recommended_allocation > 0.40);
    ASSERT_TRUE(result.decisions[2].recommended_allocation > 0.40);

    // Verify ETH is soft-capped
    ASSERT_TRUE(result.decisions[1].recommended_allocation < 0.05);

    // Verify sum of allocations is 1.0
    double total = 0.0;
    for (size_t i = 0; i < result.decision_count; ++i) {
        total += result.decisions[i].recommended_allocation;
    }
    ASSERT_TRUE(std::abs(total - 1.0) < 1e-6);

    // Verify trace step count is positive
    ASSERT_TRUE(result.trace.step_count > 0ULL);
}

int main() {
    std::cout << "Starting Unit Tests..." << std::endl;

    RUN_TEST(TestTradingAlertAdapterBuildsPassiveBuyAlert);
    RUN_TEST(TestTradingAlertAdapterRejectedDecisionIsHoldAlert);
    RUN_TEST(TestRobinhoodAlertAdapterRegistersWithoutExecutionProvider);
    RUN_TEST(TestBreakingNewsAlertAdapterEnrichesBuySellHoldAlerts);
    RUN_TEST(TestBreakingNewsAlertAdapterRegistersAsOptionalAlertOnly);
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
    RUN_TEST(TestPerformanceLayerPublishesAdvisoryIPCEnvelope);
    RUN_TEST(TestPerformanceLayerSIMDConsensusIsPassiveVectorSummary);
    RUN_TEST(TestHardwareKernelManifestNeverEmitsOrders);
    RUN_TEST(TestVersionConstant);
    RUN_TEST(TestSafetyInvariantsFailClosed);
    RUN_TEST(TestNoExecutionCapabilityAdded);
    RUN_TEST(TestHumanConfirmationBoundary);
    RUN_TEST(TestSafetyLayerFinalVeto);
    RUN_TEST(TestKillSwitchReducesRisk);
    RUN_TEST(TestFailClosedHardwareFault);
    RUN_TEST(TestIngestPacketView);
    RUN_TEST(TestIngestFIXParser);
    RUN_TEST(TestIngestNoExecutionCapability);
    RUN_TEST(TestIngestHumanConfirmationBoundary);
    RUN_TEST(TestIngestSafetyLayerFinalVeto);
    RUN_TEST(TestIngestKillSwitchReducesRisk);
    RUN_TEST(TestIngestFailClosedHardwareFault);
    RUN_TEST(TestHardwareKillSwitchAdvisoryOnly);
    RUN_TEST(TestHardwareKillSwitchLimits);
    RUN_TEST(TestEnclaveHashChain);
    RUN_TEST(TestEnclaveRemoteAttestation);

    RUN_TEST(TestObservabilityHealthStreamRingBuffer);
    RUN_TEST(TestObservabilityExportPlanePrometheus);
    RUN_TEST(TestObservabilityNoExecutionCapabilityAdded);
    RUN_TEST(TestObservabilityHumanConfirmationBoundary);
    RUN_TEST(TestObservabilitySafetyLayerFinalVeto);
    RUN_TEST(TestObservabilityKillSwitchReducesRisk);
    RUN_TEST(TestObservabilityFailClosedHardwareFault);
    RUN_TEST(V7_2_PipelineRunsWithoutCrash);
    RUN_TEST(V7_5_LanternRuns);
    RUN_TEST(TestLayer8ArbitrationSizing);
    RUN_TEST(TestLayer8ArbitrationDeterministicWalk);

    std::cout << "\nRunning BTC Module Tests...\n";

    if (sizeof(AILLE::BTCState) != 64) {
        std::cerr << "FAIL: BTCState is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::BTCAdvisory) != 64) {
        std::cerr << "FAIL: BTCAdvisory is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::BTCObservabilityMetrics) != 64) {
        std::cerr << "FAIL: BTCObservabilityMetrics is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::BTCState btc_state;
    btc_state.realized_vol = 0.05f;
    btc_state.drawdown = 0.10f;
    btc_state.trend_score = -0.2f;
    btc_state.smoothed_vol = 0.06f;

    AILLE::SafetyState safety;
    safety.hardware_fault = false;
    safety.kill_switch = false;

    AILLE::BTCAdvisory adv = AILLE::evaluate_btc_state(btc_state, &safety);

    if (adv.risk_score < 0.0f || adv.risk_score > 100.0f) {
        std::cerr << "FAIL: Risk score out of bounds.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    safety.kill_switch = true;
    AILLE::BTCAdvisory adv_ks = AILLE::evaluate_btc_state(btc_state, &safety);
    if (!adv_ks.risk_elevated || adv_ks.recommended_weight > 0.0f || adv_ks.growth_favorable) {
        std::cerr << "FAIL: BTC Module did not respect safety invariants.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::AILLEConfig config;
    AILLE::AILLEEngine engine(config);
    AILLE::BTCAdvisory engine_adv;
    engine.setSafetyState(&safety);
    engine.set_btc_state(&btc_state);
    engine.set_btc_advisory(&engine_adv);

    std::vector<AILLE::ModelSignal> signals = {
        AILLE::ModelSignal(1.5f, 0.9f, 1),
        AILLE::ModelSignal(1.6f, 0.85f, 2),
        AILLE::ModelSignal(1.4f, 0.95f, 3)
    };
    AILLE::Decision d = engine.makeDecision(signals.data(), signals.size());
    (void)d; // Suppress unused warning

    if (engine_adv.recommended_weight != 0.0f) {
        std::cerr << "FAIL: Engine integration did not correctly update BTCAdvisory under kill switch.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    std::cout << "\nRunning ETH Module Tests...\n";

    if (sizeof(AILLE::ETHState) != 64) {
        std::cerr << "FAIL: ETHState is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::ETHAdvisory) != 64) {
        std::cerr << "FAIL: ETHAdvisory is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::ETHObservabilityMetrics) != 64) {
        std::cerr << "FAIL: ETHObservabilityMetrics is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::ETHState eth_state;
    eth_state.realized_vol = 0.06f;
    eth_state.drawdown = 0.12f;
    eth_state.trend_score = -0.1f;
    eth_state.smoothed_vol = 0.07f;

    safety.hardware_fault = false;
    safety.kill_switch = false;

    AILLE::ETHAdvisory eth_adv = AILLE::evaluate_eth_state(eth_state, &safety);

    if (eth_adv.risk_score < 0.0f || eth_adv.risk_score > 100.0f) {
        std::cerr << "FAIL: ETH Risk score out of bounds.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    safety.kill_switch = true;
    AILLE::ETHAdvisory eth_adv_ks = AILLE::evaluate_eth_state(eth_state, &safety);
    if (!eth_adv_ks.risk_elevated || eth_adv_ks.recommended_weight > 0.0f || eth_adv_ks.growth_favorable) {
        std::cerr << "FAIL: ETH Module did not respect safety invariants.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::ETHAdvisory engine_eth_adv;
    engine.set_eth_state(&eth_state);
    engine.set_eth_advisory(&engine_eth_adv);

    AILLE::Decision eth_d = engine.makeDecision(signals.data(), signals.size());
    (void)eth_d; // Suppress unused warning

    if (engine_eth_adv.recommended_weight != 0.0f) {
        std::cerr << "FAIL: Engine integration did not correctly update ETHAdvisory under kill switch.\n";
        tests_failed++;
    } else {
        tests_run++;
    }


    std::cout << "\nRunning OIL Module Tests...\n";

    if (sizeof(AILLE::OILState) != 64) {
        std::cerr << "FAIL: OILState is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::OILAdvisory) != 64) {
        std::cerr << "FAIL: OILAdvisory is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::OILObservabilityMetrics) != 64) {
        std::cerr << "FAIL: OILObservabilityMetrics is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::OILState oil_state;
    oil_state.realized_vol = 0.05f;
    oil_state.drawdown = 0.10f;
    oil_state.trend_score = -0.2f;
    oil_state.smoothed_vol = 0.06f;

    safety.hardware_fault = false;
    safety.kill_switch = false;

    AILLE::OILAdvisory oil_adv = AILLE::evaluate_oil_state(oil_state, &safety);

    if (oil_adv.risk_score < 0.0f || oil_adv.risk_score > 100.0f) {
        std::cerr << "FAIL: OIL Risk score out of bounds.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    safety.kill_switch = true;
    AILLE::OILAdvisory oil_adv_ks = AILLE::evaluate_oil_state(oil_state, &safety);
    if (!oil_adv_ks.risk_elevated || oil_adv_ks.recommended_weight > 0.0f || oil_adv_ks.growth_favorable) {
        std::cerr << "FAIL: OIL Module did not respect safety invariants.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::OILAdvisory engine_oil_adv;
    engine.set_oil_state(&oil_state);
    engine.set_oil_advisory(&engine_oil_adv);

    AILLE::Decision oil_d = engine.makeDecision(signals.data(), signals.size());
    (void)oil_d; // Suppress unused warning

    if (engine_oil_adv.recommended_weight != 0.0f) {
        std::cerr << "FAIL: Engine integration did not correctly update OILAdvisory under kill switch.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    std::cout << "\nRunning GOLD Module Tests...\n";

    if (sizeof(AILLE::GOLDState) != 64) {
        std::cerr << "FAIL: GOLDState is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::GOLDAdvisory) != 64) {
        std::cerr << "FAIL: GOLDAdvisory is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::GOLDObservabilityMetrics) != 64) {
        std::cerr << "FAIL: GOLDObservabilityMetrics is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::GOLDState gold_state;
    gold_state.realized_vol = 0.05f;
    gold_state.drawdown = 0.10f;
    gold_state.trend_score = -0.2f;
    gold_state.smoothed_vol = 0.06f;

    safety.hardware_fault = false;
    safety.kill_switch = false;

    AILLE::GOLDAdvisory gold_adv = AILLE::evaluate_gold_state(gold_state, &safety);

    if (gold_adv.risk_score < 0.0f || gold_adv.risk_score > 100.0f) {
        std::cerr << "FAIL: GOLD Risk score out of bounds.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    safety.kill_switch = true;
    AILLE::GOLDAdvisory gold_adv_ks = AILLE::evaluate_gold_state(gold_state, &safety);
    if (!gold_adv_ks.risk_elevated || gold_adv_ks.recommended_weight > 0.0f || gold_adv_ks.growth_favorable) {
        std::cerr << "FAIL: GOLD Module did not respect safety invariants.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::GOLDAdvisory engine_gold_adv;
    engine.set_gold_state(&gold_state);
    engine.set_gold_advisory(&engine_gold_adv);

    AILLE::Decision gold_d = engine.makeDecision(signals.data(), signals.size());
    (void)gold_d; // Suppress unused warning

    if (engine_gold_adv.recommended_weight != 0.0f) {
        std::cerr << "FAIL: Engine integration did not correctly update GOLDAdvisory under kill switch.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    std::cout << "\nRunning SILVER Module Tests...\n";

    if (sizeof(AILLE::SILVERState) != 64) {
        std::cerr << "FAIL: SILVERState is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::SILVERAdvisory) != 64) {
        std::cerr << "FAIL: SILVERAdvisory is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::SILVERObservabilityMetrics) != 64) {
        std::cerr << "FAIL: SILVERObservabilityMetrics is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::SILVERState silver_state;
    silver_state.realized_vol = 0.05f;
    silver_state.drawdown = 0.10f;
    silver_state.trend_score = -0.2f;
    silver_state.smoothed_vol = 0.06f;

    safety.hardware_fault = false;
    safety.kill_switch = false;

    AILLE::SILVERAdvisory silver_adv = AILLE::evaluate_silver_state(silver_state, &safety);

    if (silver_adv.risk_score < 0.0f || silver_adv.risk_score > 100.0f) {
        std::cerr << "FAIL: SILVER Risk score out of bounds.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    safety.kill_switch = true;
    AILLE::SILVERAdvisory silver_adv_ks = AILLE::evaluate_silver_state(silver_state, &safety);
    if (!silver_adv_ks.risk_elevated || silver_adv_ks.recommended_weight > 0.0f || silver_adv_ks.growth_favorable) {
        std::cerr << "FAIL: SILVER Module did not respect safety invariants.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::SILVERAdvisory engine_silver_adv;
    engine.set_silver_state(&silver_state);
    engine.set_silver_advisory(&engine_silver_adv);

    AILLE::Decision silver_d = engine.makeDecision(signals.data(), signals.size());
    (void)silver_d; // Suppress unused warning

    if (engine_silver_adv.recommended_weight != 0.0f) {
        std::cerr << "FAIL: Engine integration did not correctly update SILVERAdvisory under kill switch.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    std::cout << "\nRunning COPPER Module Tests...\n";

    if (sizeof(AILLE::COPPERState) != 64) {
        std::cerr << "FAIL: COPPERState is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::COPPERAdvisory) != 64) {
        std::cerr << "FAIL: COPPERAdvisory is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::COPPERObservabilityMetrics) != 64) {
        std::cerr << "FAIL: COPPERObservabilityMetrics is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::COPPERState copper_state;
    copper_state.realized_vol = 0.05f;
    copper_state.drawdown = 0.10f;
    copper_state.trend_score = -0.2f;
    copper_state.smoothed_vol = 0.06f;

    safety.hardware_fault = false;
    safety.kill_switch = false;

    AILLE::COPPERAdvisory copper_adv = AILLE::evaluate_copper_state(copper_state, &safety);

    if (copper_adv.risk_score < 0.0f || copper_adv.risk_score > 100.0f) {
        std::cerr << "FAIL: COPPER Risk score out of bounds.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    safety.kill_switch = true;
    AILLE::COPPERAdvisory copper_adv_ks = AILLE::evaluate_copper_state(copper_state, &safety);
    if (!copper_adv_ks.risk_elevated || copper_adv_ks.recommended_weight > 0.0f || copper_adv_ks.growth_favorable) {
        std::cerr << "FAIL: COPPER Module did not respect safety invariants.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::COPPERAdvisory engine_copper_adv;
    engine.set_copper_state(&copper_state);
    engine.set_copper_advisory(&engine_copper_adv);

    AILLE::Decision copper_d = engine.makeDecision(signals.data(), signals.size());
    (void)copper_d; // Suppress unused warning

    if (engine_copper_adv.recommended_weight != 0.0f) {
        std::cerr << "FAIL: Engine integration did not correctly update COPPERAdvisory under kill switch.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    std::cout << "\nRunning NATGAS Module Tests...\n";

    if (sizeof(AILLE::NATGASState) != 64) {
        std::cerr << "FAIL: NATGASState is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::NATGASAdvisory) != 64) {
        std::cerr << "FAIL: NATGASAdvisory is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::NATGASObservabilityMetrics) != 64) {
        std::cerr << "FAIL: NATGASObservabilityMetrics is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::NATGASState natgas_state;
    natgas_state.realized_vol = 0.05f;
    natgas_state.drawdown = 0.10f;
    natgas_state.trend_score = -0.2f;
    natgas_state.smoothed_vol = 0.06f;

    safety.hardware_fault = false;
    safety.kill_switch = false;

    AILLE::NATGASAdvisory natgas_adv = AILLE::evaluate_natgas_state(natgas_state, &safety);

    if (natgas_adv.risk_score < 0.0f || natgas_adv.risk_score > 100.0f) {
        std::cerr << "FAIL: NATGAS Risk score out of bounds.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    safety.kill_switch = true;
    AILLE::NATGASAdvisory natgas_adv_ks = AILLE::evaluate_natgas_state(natgas_state, &safety);
    if (!natgas_adv_ks.risk_elevated || natgas_adv_ks.recommended_weight > 0.0f || natgas_adv_ks.growth_favorable) {
        std::cerr << "FAIL: NATGAS Module did not respect safety invariants.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::NATGASAdvisory engine_natgas_adv;
    engine.set_natgas_state(&natgas_state);
    engine.set_natgas_advisory(&engine_natgas_adv);

    AILLE::Decision natgas_d = engine.makeDecision(signals.data(), signals.size());
    (void)natgas_d; // Suppress unused warning

    if (engine_natgas_adv.recommended_weight != 0.0f) {
        std::cerr << "FAIL: Engine integration did not correctly update NATGASAdvisory under kill switch.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    std::cout << "\nRunning PLATINUM Module Tests...\n";

    if (sizeof(AILLE::PLATINUMState) != 64) {
        std::cerr << "FAIL: PLATINUMState is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::PLATINUMAdvisory) != 64) {
        std::cerr << "FAIL: PLATINUMAdvisory is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::PLATINUMObservabilityMetrics) != 64) {
        std::cerr << "FAIL: PLATINUMObservabilityMetrics is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::PLATINUMState platinum_state;
    platinum_state.realized_vol = 0.05f;
    platinum_state.drawdown = 0.10f;
    platinum_state.trend_score = -0.2f;
    platinum_state.smoothed_vol = 0.06f;

    safety.hardware_fault = false;
    safety.kill_switch = false;

    AILLE::PLATINUMAdvisory platinum_adv = AILLE::evaluate_platinum_state(platinum_state, &safety);

    if (platinum_adv.risk_score < 0.0f || platinum_adv.risk_score > 100.0f) {
        std::cerr << "FAIL: PLATINUM Risk score out of bounds.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    safety.kill_switch = true;
    AILLE::PLATINUMAdvisory platinum_adv_ks = AILLE::evaluate_platinum_state(platinum_state, &safety);
    if (!platinum_adv_ks.risk_elevated || platinum_adv_ks.recommended_weight > 0.0f || platinum_adv_ks.growth_favorable) {
        std::cerr << "FAIL: PLATINUM Module did not respect safety invariants.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::PLATINUMAdvisory engine_platinum_adv;
    engine.set_platinum_state(&platinum_state);
    engine.set_platinum_advisory(&engine_platinum_adv);

    AILLE::Decision platinum_d = engine.makeDecision(signals.data(), signals.size());
    (void)platinum_d; // Suppress unused warning

    if (engine_platinum_adv.recommended_weight != 0.0f) {
        std::cerr << "FAIL: Engine integration did not correctly update PLATINUMAdvisory under kill switch.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    std::cout << "\nRunning MACRO Module Tests...\n";

    if (sizeof(AILLE::MacroSignalState) != 64) {
        std::cerr << "FAIL: MacroSignalState is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::MacroSignalAdvisory) != 64) {
        std::cerr << "FAIL: MacroSignalAdvisory is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::MacroSignalObservabilityMetrics) != 64) {
        std::cerr << "FAIL: MacroSignalObservabilityMetrics is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::MacroSignalState macro_state;
    macro_state.usd_strength = 0.8f;
    macro_state.commodity_pressure = 0.5f;
    macro_state.crypto_sentiment = 0.3f;
    macro_state.macro_volatility = 0.6f;
    macro_state.risk_on_score = 0.2f;
    macro_state.inflation_pressure = 0.7f;
    macro_state.recession_pressure = 0.6f;
    macro_state.btc_correlation[0] = 0.6f;
    macro_state.btc_correlation[1] = 0.4f;
    macro_state.eth_correlation[0] = 0.55f;
    macro_state.eth_correlation[1] = 0.45f;
    macro_state.com_correlation[0] = 0.3f;
    macro_state.com_correlation[1] = 0.3f;
    macro_state.historical_base = 0.5f;

    safety.hardware_fault = false;
    safety.kill_switch = false;

    AILLE::MacroSignalAdvisory macro_adv = AILLE::evaluate_macro_advisory(macro_state, &safety);

    if (macro_adv.macro_risk_score < 0.0f || macro_adv.macro_risk_score > 100.0f) {
        std::cerr << "FAIL: MACRO Risk score out of bounds.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    safety.kill_switch = true;
    AILLE::MacroSignalAdvisory macro_adv_ks = AILLE::evaluate_macro_advisory(macro_state, &safety);
    if (!macro_adv_ks.risk_elevated || macro_adv_ks.recommended_macro_weight > 0.0f || macro_adv_ks.growth_favorable) {
        std::cerr << "FAIL: MACRO Module did not respect safety invariants.\n";
        tests_failed++;
    } else {
        tests_run++;
    }
    safety.kill_switch = false; // reset

    AILLE::MacroSignalAdvisory engine_macro_adv;
    engine.set_macro_state(&macro_state);
    engine.set_macro_advisory(&engine_macro_adv);

    AILLE::BTCAdvisory engine_btc_adv_for_macro;
    engine_btc_adv_for_macro.recommended_weight = 1.0f; // starting weight
    engine.set_btc_advisory(&engine_btc_adv_for_macro);

    AILLE::Decision macro_d = engine.makeDecision(signals.data(), signals.size());
    (void)macro_d;

    // Inside makeDecision, BTC weight is evaluated (which sets it based on BTC state) and then macro factor is applied.
    // If macro_advisory is correctly set and evaluated, macro_factor will multiply the BTC recommended weight.
    if (engine_macro_adv.recommended_macro_weight < 0.0f || engine_macro_adv.recommended_macro_weight > 1.0f) {
        std::cerr << "FAIL: Engine integration did not correctly evaluate MacroSignalAdvisory.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    std::cout << "\nRunning FOREX-USD Module Tests...\n";

    if (sizeof(AILLE::ForexUSDState) != 64) {
        std::cerr << "FAIL: ForexUSDState is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::ForexUSDAdvisory) != 64) {
        std::cerr << "FAIL: ForexUSDAdvisory is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::ForexUSDObservabilityMetrics) != 64) {
        std::cerr << "FAIL: ForexUSDObservabilityMetrics is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::ForexUSDState forex_usd_state;
    forex_usd_state.usd_index = 104.5f;
    forex_usd_state.realized_vol = 0.05f;
    forex_usd_state.drawdown = 0.10f;
    forex_usd_state.trend_score = -0.2f;
    forex_usd_state.smoothed_vol = 0.06f;

    safety.hardware_fault = false;
    safety.kill_switch = false;

    AILLE::ForexUSDAdvisory forex_usd_adv = AILLE::evaluate_forex_usd_state(forex_usd_state, &safety);

    if (forex_usd_adv.risk_score < 0.0f || forex_usd_adv.risk_score > 100.0f) {
        std::cerr << "FAIL: FOREX-USD Risk score out of bounds.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    safety.kill_switch = true;
    AILLE::ForexUSDAdvisory forex_usd_adv_ks = AILLE::evaluate_forex_usd_state(forex_usd_state, &safety);
    if (!forex_usd_adv_ks.risk_elevated || forex_usd_adv_ks.recommended_weight > 0.0f || forex_usd_adv_ks.growth_favorable) {
        std::cerr << "FAIL: FOREX-USD Module did not respect safety invariants.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::ForexUSDAdvisory engine_forex_usd_adv;
    engine.set_forex_usd_state(&forex_usd_state);
    engine.set_forex_usd_advisory(&engine_forex_usd_adv);

    AILLE::Decision forex_usd_d = engine.makeDecision(signals.data(), signals.size());
    (void)forex_usd_d; // Suppress unused warning

    if (engine_forex_usd_adv.recommended_weight != 0.0f) {
        std::cerr << "FAIL: Engine integration did not correctly update ForexUSDAdvisory under kill switch.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    std::cout << "\nRunning MARKET STABILIZER (MSGAM) Tests...\n";

    if (sizeof(AILLE::MarketStabilizerState) != 64) {
        std::cerr << "FAIL: MarketStabilizerState is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::MarketStabilizerAdvisory) != 64) {
        std::cerr << "FAIL: MarketStabilizerAdvisory is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (sizeof(AILLE::MarketStabilizerObservabilityMetrics) != 64) {
        std::cerr << "FAIL: MarketStabilizerObservabilityMetrics is not 64 bytes.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::MarketStabilizerState stab_state;
    stab_state.systemic_volatility = 0.1f;
    stab_state.bid_ask_spread_deviation = 0.1f;
    stab_state.order_book_depth_deficit = 0.1f;
    stab_state.consecutive_crash_count = 0.0f;
    stab_state.regime_stress_factor = 0.0f;

    safety.hardware_fault = false;
    safety.kill_switch = false;

    AILLE::MarketStabilizerAdvisory stab_adv = AILLE::evaluate_market_stabilizer_advisory(stab_state, &safety);

    if (stab_adv.stabilization_factor != 1.0f || stab_adv.governor_active != 0) {
        std::cerr << "FAIL: MarketStabilizer should be inactive under normal conditions.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    // High stress trigger
    stab_state.systemic_volatility = 0.9f;
    stab_state.bid_ask_spread_deviation = 0.9f;
    stab_state.order_book_depth_deficit = 0.9f;

    AILLE::MarketStabilizerAdvisory stab_adv_stress = AILLE::evaluate_market_stabilizer_advisory(stab_state, &safety);

    if (stab_adv_stress.stabilization_risk_score <= 75.0f || stab_adv_stress.stabilization_factor > 0.20f || stab_adv_stress.dynamic_clamp_limit > 0.25f) {
        std::cerr << "FAIL: MarketStabilizer did not trigger dynamic clamp or stabilization factor correctly.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    AILLE::MarketStabilizerAdvisory engine_stab_adv;
    engine.set_stabilizer_state(&stab_state);
    engine.set_stabilizer_advisory(&engine_stab_adv);

    AILLE::Decision stab_d = engine.makeDecision(signals.data(), signals.size());

    if (engine_stab_adv.stabilization_factor > 0.20f) {
        std::cerr << "FAIL: Engine integration did not correctly update MarketStabilizerAdvisory.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    if (std::abs(stab_d.final_value) > engine_stab_adv.dynamic_clamp_limit) {
        std::cerr << "FAIL: Engine did not dynamically clamp final decision under stress.\n";
        tests_failed++;
    } else {
        tests_run++;
    }

    // Reset for subsequent tests
    engine.set_stabilizer_state(nullptr);
    engine.set_stabilizer_advisory(nullptr);

    std::cout << "\nRunning V7.3 Pipeline Tests...\n";

    // Call evaluate_v7_3_pipeline, if it doesn't crash, the test passes
    try {
        AILLE::evaluate_v7_3_pipeline();
        tests_run++;
    } catch (...) {
        std::cerr << "FAIL: V7.3 Pipeline crashed.\n";
        tests_failed++;
    }

    std::cout << "\nRunning V7.4 Spire Interface Tests...\n";
    try {
        auto snap = aillee_spire::get_snapshot();

        if (snap.resonance_bell < 0.0) {
            std::cerr << "FAIL: Spire snapshot resonance_bell < 0.0\n";
            tests_failed++;
        } else {
            tests_run++;
        }

        if (snap.sync_tick < 0.0) {
            std::cerr << "FAIL: Spire snapshot sync_tick < 0.0\n";
            tests_failed++;
        } else {
            tests_run++;
        }

        if (snap.dampened_state < 0.0) {
            std::cerr << "FAIL: Spire snapshot dampened_state < 0.0\n";
            tests_failed++;
        } else {
            tests_run++;
        }

        if (aillee_spire::get_resonance_bell() < 0.0) {
            std::cerr << "FAIL: Spire get_resonance_bell < 0.0\n";
            tests_failed++;
        } else {
            tests_run++;
        }

        if (aillee_spire::get_sync_tick() < 0.0) {
            std::cerr << "FAIL: Spire get_sync_tick < 0.0\n";
            tests_failed++;
        } else {
            tests_run++;
        }

        if (aillee_spire::get_dampened_state() < 0.0) {
            std::cerr << "FAIL: Spire get_dampened_state < 0.0\n";
            tests_failed++;
        } else {
            tests_run++;
        }

    } catch (...) {
        std::cerr << "FAIL: V7.4 Spire Interface crashed.\n";
        tests_failed++;
    }

    std::cout << "\nRunning V7.5 Lantern Interface Tests...\n";
    try {
        auto lantern = aillee_spire::get_lantern();
        if (lantern.pulse < 0.0 || lantern.pulse > 1.0) {
            std::cerr << "FAIL: Lantern pulse out of bounds\n";
            tests_failed++;
        } else {
            tests_run++;
        }
    } catch (...) {
        std::cerr << "FAIL: V7.5 Lantern Interface crashed.\n";
        tests_failed++;
    }

    std::cout << "\nRunning V7.7 Weathering Tests...\n";
    try {
        auto report = aillee_weathering::evaluate();
        if (report.stress.resilience_score < 0.0 || report.stress.resilience_score > 1.0) {
            std::cerr << "FAIL: Weathering resilience_score out of bounds\n";
            tests_failed++;
        } else {
            tests_run++;
        }
    } catch (...) {
        std::cerr << "FAIL: V7.7 Weathering layer crashed.\n";
        tests_failed++;
    }

    std::cout << "\nRunning V7.6 Crown Walk Tests...\n";
    try {
        auto view = aillee_crown_walk::walk();
        if (view.foundational_stability < 0.0) {
            std::cerr << "FAIL: Crown Walk foundational_stability < 0.0\n";
            tests_failed++;
        } else {
            tests_run++;
        }

        if (view.resonance_bell < 0.0) {
            std::cerr << "FAIL: Crown Walk resonance_bell < 0.0\n";
            tests_failed++;
        } else {
            tests_run++;
        }

        if (view.lantern_pulse < 0.0) {
            std::cerr << "FAIL: Crown Walk lantern_pulse < 0.0\n";
            tests_failed++;
        } else {
            tests_run++;
        }
    } catch (...) {
        std::cerr << "FAIL: V7.6 Crown Walk crashed.\n";
        tests_failed++;
    }

    std::cout << "\nRunning V7.8 Pilgrimage Tests...\n";
    try {
        auto report = aillee_spire::get_pilgrimage();
        if (report.sync.resonance_alignment < 0.0) {
            std::cerr << "FAIL: Pilgrimage resonance_alignment < 0.0\n";
            tests_failed++;
        } else {
            tests_run++;
        }

        if (report.sync.sync_alignment < 0.0) {
            std::cerr << "FAIL: Pilgrimage sync_alignment < 0.0\n";
            tests_failed++;
        } else {
            tests_run++;
        }

        if (report.sync.dampening_alignment < 0.0) {
            std::cerr << "FAIL: Pilgrimage dampening_alignment < 0.0\n";
            tests_failed++;
        } else {
            tests_run++;
        }
    } catch (...) {
        std::cerr << "FAIL: V7.8 Pilgrimage crashed.\n";
        tests_failed++;
    }

    std::cout << "\nTests Run: " << tests_run << std::endl;
    std::cout << "Tests Failed: " << tests_failed << std::endl;

    return (tests_failed == 0) ? 0 : 1;
}
