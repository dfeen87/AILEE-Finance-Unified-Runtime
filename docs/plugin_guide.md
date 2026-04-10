# AILLE Plugin Guide

**Extending AILLE Without Modifying the Core**

---

## Overview

AILLE is designed to be extended through well-defined interfaces without touching core decision logic. There are three categories of plugin:

| Plugin Type     | Purpose                                 | Hook Point                      |
|-----------------|-----------------------------------------|---------------------------------|
| **Market-Data** | Supply model signals to the engine      | Before `makeDecision()`         |
| **Execution**   | Consume validated decisions             | After `makeDecision()`          |
| **Analytics**   | Observe decisions for metrics/alerting  | After `makeDecision()` (passive)|

The AILLE core (`aille.hpp`) exposes three stable data structures that form the plugin contract: `ModelSignal`, `Decision`, and `AILLEConfig`. Plugins depend only on these types — not on internal engine methods.

---

## Stable Plugin Contract

The following types are stable across patch releases. Plugins may safely depend on them.

```cpp
// aille.hpp — plugin contract types

namespace AILLE {

struct ModelSignal {
    float value;           // Prediction value (e.g., expected return direction)
    float confidence;      // Confidence score, clamped to [0.0, 1.0]
    uint64_t timestamp_ns; // Signal generation time (nanoseconds since epoch)
    int model_id;          // Caller-assigned model identifier
};

struct Decision {
    float final_value;                   // Validated output (position direction/size)
    DecisionStatus status;               // DECISION_VALID, FALLBACK_ACTIVATED, etc.
    float confidence;                    // Aggregate confidence across agreeing models
    int models_agreed;                   // Count of directionally agreeing models
    bool fallback_used;                  // True if the fallback mechanism triggered
    uint64_t timestamp_ns;              // Decision timestamp (nanoseconds since epoch)
    std::vector<int> contributing_models; // IDs of models that contributed
    std::string reasoning;               // Human-readable explanation
};

enum DecisionStatus {
    DECISION_VALID,           // All layers passed
    REJECTED_LOW_CONFIDENCE,  // Safety layer rejected all signals
    REJECTED_NO_CONSENSUS,    // Consensus layer found no agreement
    FALLBACK_ACTIVATED,       // Fallback mechanism engaged
    ERROR_NO_MODELS           // No signals provided
};

} // namespace AILLE
```

---

## Market-Data Plugins

A market-data plugin supplies model signals to the engine. Its only requirement is to return a `std::vector<AILLE::ModelSignal>`.

### Minimal Interface

There is no base class to inherit. Any callable that produces signals works:

```cpp
// Function-style
std::vector<AILLE::ModelSignal> mySignalProvider();

// Class-style (preferred for stateful models)
class MyModel {
public:
    std::vector<AILLE::ModelSignal> getSignals();
};
```

### Signal Construction

```cpp
// value    — directional prediction (positive = bullish, negative = bearish)
// confidence — [0.0, 1.0]; signals below min_confidence_threshold are filtered out
// model_id — unique integer for this model; used in audit trail attribution
AILLE::ModelSignal signal(value, confidence, model_id);
```

### Constraints

- `confidence` must be in `[0.0, 1.0]`. Out-of-range values cause the signal to be rejected.
- `value` must be finite and within `±1e6`. NaN/Inf values activate the fallback mechanism.
- `model_id` values are advisory. Assign consistently for meaningful audit attribution.
- Signals below `AILLEConfig::grace_confidence_threshold` are discarded entirely.
- Signals in `[grace_confidence_threshold, min_confidence_threshold)` are admitted with a penalty (confidence × 0.8).

### Example

```cpp
#include "aille.hpp"

class FundamentalPlugin {
    int model_id_;
public:
    explicit FundamentalPlugin(int id) : model_id_(id) {}

    std::vector<AILLE::ModelSignal> getSignals() {
        float prediction = computeFundamentalScore();   // your logic
        float confidence = estimateConfidence();        // your calibration
        return { AILLE::ModelSignal(prediction, confidence, model_id_) };
    }
};

// Integration
FundamentalPlugin fundamental(0);
TechnicalPlugin   technical(1);
SentimentPlugin   sentiment(2);

std::vector<AILLE::ModelSignal> signals;
for (auto& sig : fundamental.getSignals()) signals.push_back(sig);
for (auto& sig : technical.getSignals())   signals.push_back(sig);
for (auto& sig : sentiment.getSignals())   signals.push_back(sig);

AILLE::Decision decision = engine.makeDecision(signals);
```

---

## Execution Plugins

An execution plugin consumes a validated `AILLE::Decision` and routes it to an order management system, paper-trading simulator, or downstream risk layer.

### Minimal Interface

```cpp
class MyExecutionPlugin {
public:
    void execute(const AILLE::Decision& decision);
};
```

### Decision Routing Pattern

```cpp
void execute(const AILLE::Decision& decision) {
    switch (decision.status) {
        case AILLE::DECISION_VALID:
            // Full confidence — execute at stated position size
            sendOrder(decision.final_value, decision.confidence);
            break;

        case AILLE::FALLBACK_ACTIVATED:
            // Reduced confidence — use conservative sizing
            sendOrder(decision.final_value, decision.confidence * 0.5f);
            break;

        case AILLE::REJECTED_LOW_CONFIDENCE:
        case AILLE::REJECTED_NO_CONSENSUS:
            // Engine rejected the signal set — do not trade
            logSkip(decision.reasoning);
            break;

        default:
            logError("Unexpected decision status");
            break;
    }
}
```

### Constraints

- Execution plugins must **not** modify the `Decision` object.
- `decision.final_value` reflects the consensus-smoothed position direction after `tanh` normalisation. It is not a raw monetary amount; scaling to position size is the execution plugin's responsibility.
- Always check `decision.fallback_used` when `status == DECISION_VALID` is not set to determine whether a conservative fallback value was used.

---

## Analytics Plugins

An analytics plugin passively observes decisions for monitoring, alerting, and diagnostics. It must never influence the decision path.

The built-in `MetricsCollector` in `extensions/aille_metrics.hpp` is the reference implementation. Third-party analytics plugins follow the same pattern.

### Minimal Interface

```cpp
class MyAnalyticsPlugin {
public:
    void observe(const AILLE::Decision& decision);
};
```

### Integration Pattern

```cpp
#include "aille.hpp"
#include "extensions/aille_metrics.hpp"   // optional: built-in collector

AILLE::AILLEEngine    engine;
AILLE::MetricsCollector metrics;           // built-in analytics
MyAnalyticsPlugin     custom;              // your plugin

// Decision loop
AILLE::Decision decision = engine.makeDecision(signals);

// Analytics — always after makeDecision, never before
metrics.observe(decision);   // built-in
custom.observe(decision);    // custom plugin

// Execution — separate from analytics
execute(decision);
```

### Built-in MetricsCollector API

| Method | Description |
|--------|-------------|
| `observeDecision(decision)` | Record a decision |
| `getSnapshot()` | Return a thread-safe copy of current metrics |
| `isHealthy(max_fallback_rate)` | Simple health check (default threshold: 10%) |
| `reset()` | Clear all accumulated metrics |

See `docs/metrics_extension.md` for full documentation.

### Constraints

- Analytics plugins must be read-only with respect to AILLE state.
- Do not call `engine.reset()` or `engine.setConfig()` from an analytics plugin.
- Plugins that require low-latency paths should decouple observation from aggregation using a queue.

---

## Configuration Reference

`AILLEConfig` controls the thresholds that govern which signals pass through each layer. Plugins that supply signals should be designed to produce confidence values that interact sensibly with the active configuration.

| Field | Default | Layer | Effect |
|-------|---------|-------|--------|
| `min_confidence_threshold` | 0.35 | Safety | Signals at or above this pass directly |
| `grace_confidence_threshold` | 0.25 | Safety | Signals in [grace, min) pass with a 20% confidence penalty |
| `min_models_required` | 2 | Consensus | Minimum agreeing models needed for a valid decision |
| `sign_agreement_threshold` | 0.66 | Consensus | Minimum fraction of models that must agree directionally |
| `fallback_window_size` | 50 | Fallback | Rolling window of validated decisions used to compute fallback |
| `fallback_position_scale` | 0.1 | Fallback | Scale factor applied to the fallback direction |
| `max_model_count` | 10 | Input | Signals beyond this count are silently dropped |

---

## Repository Layout for Plugin Authors

```
aille.hpp                        ← Core engine — single-header, no dependencies
extensions/
  aille_metrics.hpp              ← Reference analytics plugin (MetricsCollector)
  aille_rest_api.hpp             ← Reference execution transport (HTTP/REST)
  aille_rest_api.cpp
docs/
  plugin_guide.md                ← This document
  metrics_extension.md           ← Metrics extension reference
  REST_API.md                    ← REST API reference
examples/
  example.cpp                    ← Minimal integration example
  metrics_integration.cpp        ← Analytics plugin integration example
```

### Core Stability Guarantee

`aille.hpp` exposes a stable public API across patch releases:

- `ModelSignal`, `Decision`, `AILLEConfig`, `DecisionStatus` — **stable**
- `AILLEEngine::makeDecision()`, `reset()`, `getConfig()`, `setConfig()` — **stable**
- `AuditLogger::logDecision()`, `verifyIntegrity()`, `generateReport()` — **stable**

Internal methods (e.g., `applySafetyLayer`, `checkConsensus`) are private and subject to change. Plugins must not depend on them. The `aille_framework.cpp` reference file exposes these layers publicly for educational purposes only.

---

---

## Formal Plugin Interfaces (`ailee_plugins/`)

The `ailee_plugins/` directory provides **stable C++ base classes** for each plugin
category. Using these interfaces is optional but recommended: they encode the
Decision Routing pattern, staleness penalties, and thread-safety contract so that
plugin authors do not need to re-implement them.

### IMarketDataSource

```cpp
// ailee_plugins/IMarketDataSource.hpp

class IMarketDataSource {
public:
    virtual ~IMarketDataSource() = default;
    virtual std::string name() const = 0;

    // Returns one ModelSignal per candle, oldest → newest.
    // Confidence penalties for stale bars are applied via applyStalenessPenalty().
    virtual std::vector<ModelSignal> getCandles(const std::string& symbol,
                                                int timeframe,
                                                int count) = 0;

    // Returns a single ModelSignal representing the latest price.
    // confidence should reflect data-feed quality (e.g. 0.70 for a delayed feed).
    virtual ModelSignal getLatestPrice(const std::string& symbol) = 0;

protected:
    // Helper: reduces confidence by 10 % for bars 60 s–5 min old,
    // and by 20 % for bars older than 5 minutes.
    static float applyStalenessPenalty(float base_confidence,
                                       uint64_t bar_timestamp_ns);
};
```

See the bundled example: `ailee_plugins/plugins/market_data/yahoo/YahooMarketData.cpp`

---

### IExecutionProvider

```cpp
// ailee_plugins/IExecutionProvider.hpp

class IExecutionProvider {
public:
    virtual ~IExecutionProvider() = default;
    virtual std::string name() const = 0;

    // Submit a fully-specified order. Returns a broker order ID, or "" on failure.
    virtual std::string submitOrder(const OrderRequest& request) = 0;

    // Cancel an order by its broker-assigned ID. Returns true on success.
    virtual bool cancelOrder(const std::string& order_id) = 0;

    // Default Decision Routing implementation.
    // Override to add broker-specific logging or pre-submission checks.
    virtual std::string routeDecision(const Decision& decision,
                                      const std::string& symbol,
                                      float base_qty);
};
```

The default `routeDecision()` applies the standard pattern:

| Status                    | Action                              |
|---------------------------|-------------------------------------|
| `DECISION_VALID`          | `submitOrder()` at full confidence  |
| `FALLBACK_ACTIVATED`      | `submitOrder()` at `confidence × 0.5` |
| `REJECTED_LOW_CONFIDENCE` | No order placed                     |
| `REJECTED_NO_CONSENSUS`   | No order placed                     |

See the bundled example: `ailee_plugins/plugins/execution/alpaca/AlpacaExecution.cpp`

---

### IAnalyticsObserver

```cpp
// ailee_plugins/IAnalyticsObserver.hpp

class IAnalyticsObserver {
public:
    virtual ~IAnalyticsObserver() = default;
    virtual std::string name() const = 0;

    // Called with the raw signal vector before the safety layer filters it.
    // Must not block for more than 1 ms.
    virtual void onSignalEvaluated(const std::vector<ModelSignal>& signals) = 0;

    // Called with the completed Decision after makeDecision() returns.
    // Must not block for more than 1 ms. Treat decision as read-only.
    virtual void onDecisionRouted(const Decision& decision) = 0;
};
```

See the bundled example: `ailee_plugins/plugins/analytics/basic/BasicMetricsObserver.cpp`

---

## Plugin Registry

The `PluginRegistry` singleton maps string names to plugin factories:

```cpp
#include "ailee_plugins/PluginRegistry.hpp"

using namespace AILLE::Plugins;

// ── Registration (once at startup) ──────────────────────────────────────────
PluginRegistry::instance().registerMarketData(
    "yahoo",
    []() -> std::unique_ptr<IMarketDataSource> {
        return std::make_unique<Yahoo::YahooMarketData>("AAPL", /*model_id=*/0);
    }
);

PluginRegistry::instance().registerExecutionProvider(
    "alpaca",
    []() -> std::unique_ptr<IExecutionProvider> {
        return std::make_unique<Alpaca::AlpacaExecution>("AAPL", /*base_qty=*/100.0f);
    }
);

PluginRegistry::instance().registerAnalyticsObserver(
    "basic-metrics",
    []() -> std::unique_ptr<IAnalyticsObserver> {
        return std::make_unique<Basic::BasicMetricsObserver>();
    }
);

// ── Retrieval (any time after registration) ──────────────────────────────────
auto md   = PluginRegistry::instance().createMarketData("yahoo");
auto exec = PluginRegistry::instance().createExecutionProvider("alpaca");
auto obs  = PluginRegistry::instance().createAnalyticsObserver("basic-metrics");

// ── Decision loop ────────────────────────────────────────────────────────────
AILLE::AILLEEngine engine;

auto signals = md->getCandles("AAPL", 60, 5);
signals.push_back(md->getLatestPrice("AAPL"));

obs->onSignalEvaluated(signals);

AILLE::Decision decision = engine.makeDecision(signals);

obs->onDecisionRouted(decision);
exec->routeDecision(decision, "AAPL", 100.0f);
```

Bundled plugins support **link-time self-registration**: linking a plugin `.cpp`
file into the application automatically calls `registerMarketData()` /
`registerExecutionProvider()` / `registerAnalyticsObserver()` at static-init time,
before `main()` runs.

See [docs/plugin_registry.md](plugin_registry.md) for the full registry reference.

---

## See Also

- [README.md](../README.md) — Framework overview and architecture
- [QUICKSTART.md](../QUICKSTART.md) — Get running in 60 seconds
- [docs/metrics_extension.md](metrics_extension.md) — Analytics extension reference
- [docs/REST_API.md](REST_API.md) — REST API integration
- [docs/plugin_registry.md](plugin_registry.md) — Plugin discovery and registry reference
