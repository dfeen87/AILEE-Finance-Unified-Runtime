/*
 * AILLE Plugin Interface — IAnalyticsObserver
 * AI-Load Integrity and Layered Evaluation
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: Non-Commercial (see LICENSE)
 *
 * Stable interface for analytics / observability plugins. Implementors
 * passively observe AILLE signals and decisions for monitoring, alerting,
 * and diagnostics. The observer must never influence the decision path.
 *
 * Thread safety contract:
 *   - onSignalEvaluated() and onDecisionRouted() may be called from any
 *     thread. Implementors are responsible for internal synchronisation.
 *   - The AILLE engine guarantees that callbacks are not called reentrantly
 *     on the same observer instance.
 *
 * Constraints:
 *   - Must not call engine.reset() or engine.setConfig().
 *   - Must not block the calling thread for more than 1 ms. Offload
 *     expensive work (I/O, aggregation) to a background queue.
 *   - Treat all arguments as read-only; do not retain non-const references.
 */

#ifndef AILEE_PLUGINS_IANALYTICS_OBSERVER_HPP
#define AILEE_PLUGINS_IANALYTICS_OBSERVER_HPP

#include <string>
#include <vector>

#include "../aille.hpp"

namespace AILLE {
namespace Plugins {

// ============================================================================
// IANALYTICS OBSERVER INTERFACE
// ============================================================================

/// Stable, thread-safe, passive observer interface for analytics plugins.
///
/// The AILLE engine calls onSignalEvaluated() before invoking the safety
/// and consensus layers, then calls onDecisionRouted() after makeDecision()
/// completes. Both callbacks are fire-and-forget; return values are ignored.
class IAnalyticsObserver {
public:
    virtual ~IAnalyticsObserver() = default;

    /// Return the plugin's human-readable identifier (e.g. "basic-metrics").
    virtual std::string name() const = 0;

    /// Called once per makeDecision() invocation, before the safety layer
    /// processes the signal set.
    ///
    /// Use this callback to record raw input characteristics — signal count,
    /// confidence distribution, model-ID coverage — before any filtering.
    ///
    /// @param signals  The raw signal vector passed to makeDecision().
    ///                 The vector is const; do not retain a reference past
    ///                 the scope of this callback.
    virtual void onSignalEvaluated(const std::vector<ModelSignal>& signals) = 0;

    /// Called once per makeDecision() invocation, after the decision is
    /// fully computed and before it is returned to the caller.
    ///
    /// Use this callback to record decision outcomes, fallback activations,
    /// confidence distributions, and audit information.
    ///
    /// @param decision  The completed Decision. Treat as read-only.
    virtual void onDecisionRouted(const Decision& decision) = 0;
};

} // namespace Plugins
} // namespace AILLE

#endif // AILEE_PLUGINS_IANALYTICS_OBSERVER_HPP
