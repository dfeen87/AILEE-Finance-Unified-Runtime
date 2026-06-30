/*
 * AILLE Plugin — BasicMetricsObserver (example implementation)
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
 *
 * Minimal, dependency-free example showing how an analytics plugin
 * implements IAnalyticsObserver to observe AILLE signals and decisions.
 *
 * This observer accumulates running counters and prints a summary to stdout.
 * It is intentionally lightweight — no external dependencies, no I/O on the
 * hot path. All shared state is protected by a std::mutex per the interface
 * thread-safety contract.
 *
 * Integration:
 *   #include "ailee_plugins/plugins/analytics/basic/BasicMetricsObserver.cpp"
 *
 *   // Register once at startup
 *   AILLE::Plugins::PluginRegistry::instance().registerAnalyticsObserver(
 *       "basic-metrics",
 *       []{ return std::make_unique<AILLE::Plugins::Basic::BasicMetricsObserver>(); }
 *   );
 *
 *   // Attach to the decision loop
 *   auto obs = AILLE::Plugins::PluginRegistry::instance()
 *                  .createAnalyticsObserver("basic-metrics");
 *
 *   AILLE::Decision decision = engine.makeDecision(signals);
 *   obs->onSignalEvaluated(signals);   // before or after makeDecision
 *   obs->onDecisionRouted(decision);   // always after makeDecision
 */

#include "../../../IAnalyticsObserver.hpp"
#include "../../../../ailee_plugins/PluginRegistry.hpp"

#include <atomic>
#include <iostream>
#include <mutex>
#include <numeric>
#include <vector>

namespace AILLE {
namespace Plugins {
namespace Basic {

// ============================================================================
// BASIC METRICS OBSERVER
// ============================================================================

class BasicMetricsObserver : public IAnalyticsObserver {
public:
    BasicMetricsObserver() = default;

    std::string name() const override { return "basic-metrics"; }

    /// Record raw signal characteristics before the safety layer filters them.
    void onSignalEvaluated(const std::vector<ModelSignal>& signals) override {
        std::lock_guard<std::mutex> lock(mutex_);

        total_signal_batches_++;
        total_signals_seen_ += static_cast<uint64_t>(signals.size());

        // Accumulate confidence values for running average
        for (const auto& sig : signals) {
            confidence_sum_ += static_cast<double>(sig.confidence);
        }
    }

    /// Record decision outcomes for monitoring and alerting.
    void onDecisionRouted(const Decision& decision) override {
        std::lock_guard<std::mutex> lock(mutex_);

        total_decisions_++;

        switch (decision.status) {
            case DECISION_VALID:
                valid_decisions_++;
                break;
            case FALLBACK_ACTIVATED:
                fallback_activations_++;
                break;
            case REJECTED_LOW_CONFIDENCE:
                rejected_confidence_++;
                break;
            case REJECTED_NO_CONSENSUS:
                rejected_consensus_++;
                break;
            default:
                break;
        }

        // Running confidence sum for post-filter average
        post_filter_confidence_sum_  += static_cast<double>(decision.confidence);
        post_filter_models_agreed_   += static_cast<uint64_t>(decision.models_agreed);
    }

    // ---- Reporting ----------------------------------------------------------

    /// Print a formatted summary of accumulated metrics to stdout.
    void printSummary() const {
        std::lock_guard<std::mutex> lock(mutex_);

        double avg_pre  = (total_signals_seen_ > 0)
                          ? confidence_sum_ / static_cast<double>(total_signals_seen_)
                          : 0.0;
        double avg_post = (total_decisions_ > 0)
                          ? post_filter_confidence_sum_ / static_cast<double>(total_decisions_)
                          : 0.0;
        double fallback_rate = (total_decisions_ > 0)
                               ? 100.0 * static_cast<double>(fallback_activations_)
                                       / static_cast<double>(total_decisions_)
                               : 0.0;

        std::cout << "\n[BasicMetricsObserver] ===== Summary =====\n"
                  << "  Signal batches seen    : " << total_signal_batches_ << "\n"
                  << "  Total raw signals      : " << total_signals_seen_   << "\n"
                  << "  Avg pre-filter conf    : " << avg_pre               << "\n"
                  << "  Total decisions        : " << total_decisions_       << "\n"
                  << "  Valid                  : " << valid_decisions_       << "\n"
                  << "  Fallback activations   : " << fallback_activations_  << "\n"
                  << "  Rejected (confidence)  : " << rejected_confidence_   << "\n"
                  << "  Rejected (consensus)   : " << rejected_consensus_    << "\n"
                  << "  Avg post-filter conf   : " << avg_post               << "\n"
                  << "  Fallback rate          : " << fallback_rate << "%\n"
                  << "=========================================\n\n";
    }

    /// Reset all accumulated counters.
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        total_signal_batches_      = 0;
        total_signals_seen_        = 0;
        confidence_sum_            = 0.0;
        total_decisions_           = 0;
        valid_decisions_           = 0;
        fallback_activations_      = 0;
        rejected_confidence_       = 0;
        rejected_consensus_        = 0;
        post_filter_confidence_sum_ = 0.0;
        post_filter_models_agreed_ = 0;
    }

private:
    mutable std::mutex mutex_;

    // Pre-filter signal metrics
    uint64_t total_signal_batches_ = 0;
    uint64_t total_signals_seen_   = 0;
    double   confidence_sum_       = 0.0;

    // Post-filter decision metrics
    uint64_t total_decisions_            = 0;
    uint64_t valid_decisions_            = 0;
    uint64_t fallback_activations_       = 0;
    uint64_t rejected_confidence_        = 0;
    uint64_t rejected_consensus_         = 0;
    double   post_filter_confidence_sum_ = 0.0;
    uint64_t post_filter_models_agreed_  = 0;
};

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

/// Registers this plugin with the global PluginRegistry at static-init time.
/// Link this translation unit to activate auto-registration.
namespace {
    struct BasicMetricsRegistrar {
        BasicMetricsRegistrar() {
            PluginRegistry::instance().registerAnalyticsObserver(
                "basic-metrics",
                []() -> std::unique_ptr<IAnalyticsObserver> {
                    return std::make_unique<BasicMetricsObserver>();
                }
            );
        }
    };
    // NOLINTNEXTLINE(cert-err58-cpp) — intentional static initialisation
    const BasicMetricsRegistrar g_basic_metrics_registrar;
} // anonymous namespace

} // namespace Basic
} // namespace Plugins
} // namespace AILLE
