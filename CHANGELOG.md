# CHANGELOG

All notable changes to the AILLE project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [15.0.0] - 2026-11-01
### Added
- **Version 15 Expansion**: Structural-Stress Indicators, Regime Diagnostics & StressRegimePayload.
- 5 allocator-free structural-stress indicators: `VolatilityInstability`, `LiquidityErosion`, `CorrelationBreakdown`, `BaselineDeterioration`, `StructuralFatigue`.
- Grouped stress condition state band inside `ChartConditionState` (`StateStable`, `StateUnstable`, `StateChaotic`, `StatePreserved`, `StateEroding`, `StateDepleted`, `StateWeakening`, `StateDeteriorating`, `StateLowFatigue`, `StateMediumFatigue`, `StateHighFatigue`).
- Centralized `RegimeModifier` computing Volatility, Liquidity, and Correlation regimes once per cycle and scaling indicator evaluation thresholds dynamically.
- 32-byte cache-aligned (`alignas(32)`) `StressRegimePayload` bridging stress indicator scores and regime codes with future-proof reserved fields.
- Expanded pattern diagnostic engine with `PatternHintGroup` (`ExpansionGroup`, `StressGroup`) and weakening environment hints (`BreakdownLike`, `ExhaustionLike`, `StressConsolidationLike`).
- `SharedRegimeRingBuffer` maintaining regime-adaptive ring buffer stability without dynamic memory allocations.
- Comprehensive unit test suites in C++ (`tests/unit_tests.cpp`) and Python (`tests/test_finance_kernel_chart_intelligence.py`).

## [14.0.0] - 2026-10-01
### Added
- Real-Time Chart Intelligence & Environment Diagnostics Subsystem (Layer 17) under version tag `CHART_INTELLIGENCE_V1`.
- Allocator-free, strictly 64-byte cache-aligned `BaselineState`, `ChartConditionPayload`, `PatternEnvironmentState`, and `PatternConditionPayload` structs with `static_assert` guarantees.
- Technical indicators: Volatility Expansion Bands, Liquidity Displacement Zones, Correlation Divergence Index, Baseline Strength Meter.
- Non-predictive `PatternDiagnosticEngine` providing CupHandleLike, PennantLike, and FlagLike environment resemblance scoring.
- Allocator-free `ChartIndicatorRegistry` dispatch table and JSON serialization helpers in C++ and Python.

## [13.0.0] - 2026-09-01

### Added
- **Layer 16 — Deterministic Anomaly Detection & Market Condition Subsystem**:
  - Real-time market condition monitoring evaluating volatility expansion (EWMA), order book depth thinning (L1/L2 liquidity displacement), and cross-asset pair correlation breaks.
  - Strictly non-directive, cautionary advisory messages with zero trading recommendations or accusatory language.
  - C++ low-latency deterministic implementation (`extensions/aille_anomaly.hpp/.cpp`) using strictly 64-byte, cache-aligned, allocator-free structs (`AnomalyState`, `AnomalyAdvisory`, `AnomalyObservabilityMetrics`, `AnomalyTraceStep`, `AnomalyConfig`).
  - Python Finance Runtime Kernel operator (`core/finance_kernel/anomaly_detection.py`) with complete input validation, bounds clamping, and NaN/Inf hardening.
  - Spire interface integration (`aillee_spire::get_anomaly_advisory()`) and WebSocket JSON broadcast payload formatting.
  - Threshold configuration schema exposure in `ailee_hft_config.yaml`.
  - Comprehensive unit test suites in C++ (`tests/unit_tests.cpp`) and Python (`tests/test_finance_kernel_anomaly.py`).

## [12.0.0] - 2026-08-20

### Added
- **Optional Intraday Volume Auto-Trader Execution Plugin**:
  - Alpaca REST API execution adapter (`AlpacaExecution`) supporting `/v2/orders`, `/v2/positions`, and `/v2/account`.
  - Python execution operator (`VolumeExecutionOperator`) in `core/finance_kernel/volume_execution.py`.
  - C++ standalone auto-trader daemon (`examples/volume_auto_trader.cpp`) and Python runner (`simulations/run_volume_trader.py`).
  - Strict safety posture: explicit opt-in execution flag (`--enable-auto-execute`), paper-trading mode default, two-step live confirmation flag (`--confirm-live`), signal hysteresis/debounce filtering, daily drawdown lockout controls, and append-only structured JSON audit logging.

## [11.0.0] - 2026-07-02

### Added
- Implemented optional Intraday Volume Advisory Contrarian Oversold Buy Signal Engine for SPY and QQQ.
- Multi-factor oversold scoring utilizing normalized price change ($\le -1.2\%$), VWAP deviation ($\le -0.8\%$), and volume anomaly ratio ($\ge 2.5x$).
- Index ETF symbol sensitivity (SPY and QQQ thresholds) vs single-name strict criteria.
- Contrarian position weight multiplier (1.15x to 1.30x) with honest baseline risk score transparency.
- Configuration option `enable_contrarian_oversold` and `contrarian_oversold_aggressiveness` across C++ (`AILLEConfig`) and Python (`FinanceKernelConfig`).
- Per-bar runtime override flag `contrarian_override` on `VolumeState` and input payloads.
- Strict safety precedence: Kill switches, hardware faults, and Market Stabilizer (MSGAM) caps unconditionally override contrarian weight adjustments.
- Comprehensive unit tests in C++ (`tests/unit_tests.cpp`) and Python (`tests/test_finance_kernel_volume.py`).

## [10.2.0] - 2026-06-25

### Added
- Implemented Intraday Volume Advisory Module (VAM) stabilization to smooth volume anomalies and prevent rapid, jagged adjustments.
- Added `prev_volume_anomaly_ratio` and `prev_recommended_weight` state elements into `VolumeState` to enable robust, history-aware evaluation.
- Coupled the Volume Advisory Module with the Market Stabilizer (MSGAM), dynamically scaling recommended weight based on systemic volatility and stress factors.
- Enforced a strict temporal weight step-clamp (maximum change of 0.15 per update) to ensure slow and stable adjustments.
- Implemented equivalent Python volume stabilization logic under `core/finance_kernel/volume_advisory.py`.
- Added extensive unit tests in both C++ and Python to verify the new volume stabilization, smoothing, MSGAM coupling, and temporal step-clamping.

## [10.1.0] - 2026-06-18

### Added
- Formally introduced Intraday Volume Advisory Module (VAM) optimized for high-volume stock index trackers (SPY and QQQ).
- Implemented C++ 64-byte aligned, allocator-free structures for VAM (`VolumeState`, `VolumeAdvisory`, `VolumeObservabilityMetrics`).
- Implemented Python equivalent operator module under `core/finance_kernel/volume_advisory.py` and sequential pipeline integration.
- Added comprehensive unit test suites in C++ and Python verifying safety properties, volume anomalies, and deterministic bounds.

## [10.0.0] - 2026-06-12

### Added
- Formally introduced Layer 15 — Deterministic Deformable Membrane & Compute-Aware Governor (UFO Integration).
- Integrated UFO-style Lyapunov reconciliation principles to Layer 10 (Multi-Governor Reconciliation Engine).
- Implemented C++ 64-byte aligned, allocator-free structures for Layer 15 (`MembraneState`, `MembraneMetrics`, `ComputeEnvelopeState`, `MembraneTraceStep`).
- Implemented Python equivalent modules and tests under `core/finance_kernel/membrane.py` and `tests/test_finance_kernel_membrane.py`.
- Exposed Layer 15 metrics (asymmetry, curvature, Lyapunov energy, membrane tension, compute envelope) via new REST API endpoint `/api/membrane` and expanded WebSocket telemetry broadcasts.

## [9.0.0] - 2026-05-20

### Added
- Formally introduced Layer 8 to Layer 14 as the Deterministic Governance Stack.
- Enhanced README documentation with comprehensive, concise, and highly technical descriptions for all deterministic governance layers.

## [8.7.0] - 2026-04-15

### Added
- Created Layer 8 Cross-Asset Deterministic Arbitration to reconcile and arbitrate heterogeneous asset advisories.
- Implemented `Advisory`, `AllocationDecision`, and `ArbitrationTraceStep` structs (strictly 64 bytes each, aligned, allocator-free) inside `extensions/aille_arbitration.hpp/.cpp`.
- Implemented Python equivalents and full arbitration functionality in `core/finance_kernel/arbitration_layer.py`.
- Formally integrated `LADDER_V1` and `SCALING_RULESET_V1` as release-wide official identifiers.
- Added extensive testing validating deterministic tie-breaking, exact struct sizing, non-allocating execution paths, and cross-language output matching.

## [8.5.0] - 2026-03-31

### Added
- Created Layer 7.9 Market Stabilization Governor Advisory Module (MSGAM) to mitigate risk under market stress.
- Implemented `MarketStabilizerState` and `MarketStabilizerAdvisory` (strictly 64 bytes each, aligned, allocator-free) inside `aille.hpp` and `extensions/aille_stabilizer.hpp/.cpp`.
- Integrated automatic recommended weight scaling based on decoupling or high volatility.
- Added 7 robust unit tests asserting the MSGAM properties and correct integration within the engine decision pipeline.
- Enhanced the WebSocket visual observer (`LiveAdvisoryObserver.hpp`/`.cpp`) to stream MSGAM state parameters to the HTML frontend dashboard.
- Integrated Stabilizer Guard glowing widget and state representation in `index.html` frontend dashboard.

## [8.0.0] - 2026-02-15

### Added
- Initial v8.0.0 Release with foundational multi-asset advisory engine.
