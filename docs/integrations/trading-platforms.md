# Trading platforms, data flow and paper trading

AILEE separates provider access from strategy and governance, then separates
order intent from the system that simulates or executes it. Signing into a
broker does not establish that an AILEE strategy is running.

## Current availability

This table distinguishes bundled code from integration paths that require
additional software and configuration. An extension interface or reference
diagram does not establish a connected account or production certification.

| Component | Repository status | What it does |
| --- | --- | --- |
| Robinhood passive alerts | Bundled C++ example | Logs BUY/SELL/HOLD notifications. It has no order submission or cancellation interface. |
| Robinhood authenticated MCP data | Requires a separate host/adapter | Can supply account/market observations through an explicitly restricted caller. Data access does not grant AILEE decision authority or enable execution. |
| AILEE-controlled local paper simulator | Requires a separate simulator/runner | Can connect the existing volume advisory and execution operators to a local virtual ledger. No standalone Robinhood paper runner is bundled here. |
| Alpaca C++ execution provider | Bundled implementation | Provides mock behavior or broker-directed requests under its endpoint, consent and durable-intent checks. |
| Python volume execution operator | Bundled simulated backend | Implements strategy execution controls with mock IDs. Its paper/live label does not create a real broker connection. |
| Yahoo market-data example | Bundled placeholder | Produces synthetic data; it is not a connected live Yahoo feed. |
| Other brokers or order-management systems | Extension interfaces | Require a separate adapter and provider-specific validation. No generic multi-broker compatibility is claimed. |

The bundled [Robinhood alert example](../../ailee_plugins/plugins/alerts/robinhood/RobinhoodAlertAdapter.cpp)
and [Alpaca implementation](../../ailee_plugins/plugins/execution/alpaca/AlpacaExecution.cpp)
show these distinct roles. The [plugin guide](../api/plugin_guide.md) describes
the extension interfaces.

## Reference flow: AILEE-controlled local paper trading

An integration using the existing SPY/QQQ volume operators can follow this
flow. The diagram describes required wiring, not a bundled, automatically
configured application.

~~~mermaid
flowchart TD
    RH["Robinhood MCP: market-data reads"] --> IO["Separate authenticated adapter"]
    IO --> V["Validate fresh quotes and completed bars"]
    V --> MAP["Explicit volume and VWAP input mapping"]
    MAP --> VAM["AILEE IntradayVolumeAdvisory"]
    VAM --> EXEC["AILEE VolumeExecutionOperator"]
    EXEC --> INTENT["BUY / HOLD / FLAT intent"]
    INTENT --> LEDGER["Local virtual fills and cash"]
    EXEC --> AUDIT["Persist decisions, source evidence and replay state"]
    LEDGER --> AUDIT
    LEDGER --> EQUITY["Virtual holdings and equity"]
    EQUITY --> EXEC
~~~

AILEE's operators determine the action. The simulator translates their intent
into virtual fills; it does not add a second strategy or turn a blocked decision
into a trade. A manual buy/sell screen alone is a different application and does
not establish AILEE control.

Define the input mapping before running the strategy: bar interval and session,
volume-baseline window, return reference and VWAP calculation. For example,
completed one-minute regular-session bars can supply a rolling volume baseline,
bar return and session typical-price VWAP. A session VWAP requires the opening
bar and a complete observed session range.

Preserve the execution operator's configured confirmation, drawdown lockout
and safety-state behavior. Its default hysteresis is two calls; the host must
ensure those calls represent distinct completed bars. Risk limits must also
respect shared virtual cash and the actual virtual holdings.

The host must reject duplicate or out-of-order bars, reset pending confirmation
when continuity is lost and preserve replay cursors across restarts. It must
record source inputs, operator output, virtual fills and state together.
Missing or stale evidence blocks a cycle; it must not become a fabricated
healthy feed or a retrospective fill at a current quote. Session calendars
and holidays require explicit handling as well as fresh data.

This example selects a volume-strategy path. It does not establish that every
optional governor or all 19 layers are configured. Connect additional required
governors explicitly and document which controls are active. Supply measured
trust/manipulation evidence or explicit conservative unknown handling; do not
invent healthy scores to enable bullish/HFT modulation.

The canonical [ailee-delta-v/v1 mathematical engine](../MATHEMATICAL_ENGINE.md)
retains its own input and validation contract. Do not invent physical/model
parameters from stock prices or attach fabricated canonical evidence. Mathematical output alone
cannot grant trading permission, clear a lockout or release a reservation.

## Paper trading means different things

| Mode | Funds and fills | Where activity appears |
| --- | --- | --- |
| Local simulation | Virtual cash; fills modeled on the local computer | Local ledger and audit |
| Broker-hosted paper account | Virtual funds; provider simulates fills | Provider's paper dashboard/API |
| Live broker account | Real funds and actual orders | Real account activity |

Robinhood's [documented agent tools](https://robinhood.com/us/en/support/articles/trading-with-your-agent/)
include account and market-data reads, order reviews and real-account order
placement. They do not document a paper-account endpoint. A local simulator
can use Robinhood observations, but its virtual holdings and trades do not
appear in Robinhood. A pre-trade preview is not a persistent paper portfolio.

Alpaca documents a [broker-hosted paper environment](https://docs.alpaca.markets/us/docs/paper-trading)
with separate paper credentials and endpoint. That is distinct from AILEE's
mock execution mode. In the C++ daemon, mock behavior is selected separately
from paper/live destination; the Python execution backend is simulated even
when its mode label says live. Review actual adapter configuration rather than
assuming the word paper or live proves a connection.

Simulation fill models differ. For example, a local bid/ask fill may omit
slippage, liquidity, queue priority, fees and partial fills. Paper results do
not demonstrate live execution performance.

## Execution remains a separate integration

~~~mermaid
flowchart LR
    G["Configured AILEE governance"] --> D["Validated decision"]
    D --> A["Passive alert"]
    D --> S["Explicitly configured simulator"]
    D -.-> E["Separately authorized execution provider"]
    E --> B["Broker"]
    B --> R["Order / fill / position reconciliation"]
~~~

The broker branch requires its own explicit authorization and implementation.
Keep a market-data-only paper runner free of broker order submission and
cancellation paths. A rejected or fallback decision does not authorize execution.

An acknowledged broker order is pending until independently reconciled; it
must not become an invented fill. The current Alpaca directional contract
requires bounded BUY LIMIT requests, caller identity and durable intent
reservations. A SELL price limit alone does not establish exposure reduction.
See the [execution integrity contract](../releases/execution-integrity-hardening.md).

## Adding another platform

Keep authentication, network I/O and provider-specific translations in a
separate adapter. Configure the data-to-model mapping and downstream execution
provider through the existing interfaces; do not alter core decision authority
to make a broker fit.

Before calling a platform compatible, establish:

- The permitted accounts, assets, authentication scopes and destination.
- Separate read and write capabilities, plus an enforced tool allowlist.
- Symbols, units, decimal precision, timestamps, bar completeness and freshness.
- Explicit signal policy, confidence calibration and source lineage. Multiple
  views of one feed do not manufacture independent consensus.
- The provider's actual sandbox/paper support and simulated-fill assumptions.
- Order identity, restart behavior, ambiguous outcomes and reconciliation.
- Portfolio constraints and coordination with other actors sharing the account.

OAuth consent can grant more access than an application needs. A read-only
adapter must restrict its own calls; it must not describe that filter as a
provider-enforced read-only scope. Keep credentials and private account data
out of source control, examples and public documentation.

Users who want a no-code experience need a packaged, separately configured
runner with clear start/stop controls and an explicit mode display. Cloning
this repository alone does not install a standalone Robinhood runner, authorize
a broker account or start an automatic trading service.
