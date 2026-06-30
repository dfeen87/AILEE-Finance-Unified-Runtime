# AILLE Plugin Registry

**How Plugins Are Discovered, Registered, and Instantiated**

---

## Overview

The `PluginRegistry` is a thread-safe singleton that maps plugin names to factory
functions. It decouples the AILLE engine from concrete plugin implementations: the
engine knows only about stable interfaces (`IMarketDataSource`, `IExecutionProvider`,
`IAnalyticsObserver`, `ITradingAlertAdapter`), while concrete classes register themselves under well-known
string names.

The registry lives in:

```
ailee_plugins/PluginRegistry.hpp
```

It depends only on the three interface headers and the C++ standard library. No
global mutable variables exist outside the singleton itself.

---

## How Discovery Works

AILLE uses a **link-time self-registration** model. Each plugin translation unit
contains a file-scope registrar object whose constructor calls `PluginRegistry::instance().register*()`.
Linking the plugin's `.cpp` file into the application is sufficient to activate it —
no explicit registration code is required in `main()`.

```
┌─────────────────────────────────────────────────────┐
│  Application startup                                │
│                                                     │
│  1. Static-init phase (before main())               │
│     YahooMarketDataRegistrar  ──► registerMarketData("yahoo", factory)   │
│     AlpacaExecutionRegistrar  ──► registerExecutionProvider("alpaca", …) │
│     BasicMetricsRegistrar     ──► registerAnalyticsObserver("basic-metrics", …) │
│                                                     │
│  2. main() / application logic                      │
│     createMarketData("yahoo")    ──► factory() ──► unique_ptr<IMarketDataSource>  │
│     createExecutionProvider("alpaca") ──► factory() ──► unique_ptr<IExecutionProvider> │
│     createAnalyticsObserver("basic-metrics") ──► factory() ──► unique_ptr<IAnalyticsObserver> │
└─────────────────────────────────────────────────────┘
```

---

## Thread Safety

All public methods of `PluginRegistry` are guarded by a single `std::mutex`.
Registration and construction are safe to call concurrently from multiple threads.

The singleton itself is initialised via a function-local `static`, which is
guaranteed by the C++11 standard to be initialised exactly once even under
concurrent access (see N3690 §6.7).

---

## API Reference

### Registration

```cpp
#include "ailee_plugins/PluginRegistry.hpp"

using namespace AILLE::Plugins;

// Market-data plugin
PluginRegistry::instance().registerMarketData(
    "my-source",
    []() -> std::unique_ptr<IMarketDataSource> {
        return std::make_unique<MyMarketDataSource>();
    }
);

// Execution provider
PluginRegistry::instance().registerExecutionProvider(
    "my-broker",
    []() -> std::unique_ptr<IExecutionProvider> {
        return std::make_unique<MyBrokerPlugin>();
    }
);

// Analytics observer
PluginRegistry::instance().registerAnalyticsObserver(
    "my-metrics",
    []() -> std::unique_ptr<IAnalyticsObserver> {
        return std::make_unique<MyMetricsObserver>();
    }
);
```

Each name must be unique within its category. A second `register*()` call with
the same name throws `std::invalid_argument`.

### Construction

```cpp
// Each call invokes the registered factory and returns a fresh unique_ptr.
auto md   = PluginRegistry::instance().createMarketData("my-source");
auto exec = PluginRegistry::instance().createExecutionProvider("my-broker");
auto obs  = PluginRegistry::instance().createAnalyticsObserver("my-metrics");
```

The registry retains no ownership of constructed instances.

### Introspection

```cpp
bool exists = PluginRegistry::instance().hasMarketData("my-source");
bool alert_exists = PluginRegistry::instance().hasTradingAlertAdapter("robinhood-alerts");
```

---

## Self-Registration Pattern

Place the following snippet at the bottom of every plugin `.cpp` file to enable
link-time auto-registration:

```cpp
namespace {
    struct MyPluginRegistrar {
        MyPluginRegistrar() {
            AILLE::Plugins::PluginRegistry::instance().registerMarketData(
                "my-source",
                []() -> std::unique_ptr<AILLE::Plugins::IMarketDataSource> {
                    return std::make_unique<MyMarketDataSource>();
                }
            );
        }
    };
    const MyPluginRegistrar g_my_plugin_registrar;
} // anonymous namespace
```

Linking `my_plugin.cpp` activates registration automatically.

---

## Bundled Plugins

| Name            | Category    | File                                                       |
|-----------------|-------------|------------------------------------------------------------|
| `yahoo`         | Market-Data | `ailee_plugins/plugins/market_data/yahoo/YahooMarketData.cpp` |
| `alpaca`        | Execution   | `ailee_plugins/plugins/execution/alpaca/AlpacaExecution.cpp`  |
| `basic-metrics` | Analytics   | `ailee_plugins/plugins/analytics/basic/BasicMetricsObserver.cpp` |
| `robinhood-alerts` | Trading Alerts | `ailee_plugins/plugins/alerts/robinhood/RobinhoodAlertAdapter.cpp` |

---

## Adding a New Plugin

1. Create a subdirectory under the appropriate category:
   ```
   ailee_plugins/plugins/<category>/<vendor>/MyPlugin.cpp
   ```
2. Implement the relevant interface (`IMarketDataSource`, `IExecutionProvider`,
   `IAnalyticsObserver`, or `ITradingAlertAdapter`).
3. Add a self-registration block at the bottom of the `.cpp` file.
4. Link the `.cpp` file into the application. No other changes are required.

---

## See Also

- [plugin_guide.md](plugin_guide.md) — Interface documentation and integration examples
- [ailee_plugins/PluginRegistry.hpp](../ailee_plugins/PluginRegistry.hpp) — Full API
- [ailee_plugins/IMarketDataSource.hpp](../ailee_plugins/IMarketDataSource.hpp)
- [ailee_plugins/IExecutionProvider.hpp](../ailee_plugins/IExecutionProvider.hpp)
- [ailee_plugins/IAnalyticsObserver.hpp](../ailee_plugins/IAnalyticsObserver.hpp)
