# CHANGELOG

All notable changes to the AILLE project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
