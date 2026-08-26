# Optional Next-Generation Performance Layer

The AILLE performance layer defines optional, passive acceleration modules for deployments that need lower latency signal transport and faster consensus evaluation without changing the framework's human-controlled safety posture. The layer is explicitly advisory: it can enrich risk signals, score mitigation logic, and describe hardware compilation targets, but it must not place trades, route orders, or trigger autonomous actions.

## Safety invariants

- **Advisory only:** every IPC envelope, SIMD result, and hardware manifest carries an advisory-only contract.
- **No execution pathway:** hardware manifests expose `emits_orders = false`; generated kernels are risk-scoring and mitigation-scoring units only.
- **Human-controlled decisions:** accelerated outputs remain inputs to AILLE's existing audit, safety, consensus, fallback, and alerting layers.
- **Deterministic degradation:** deployments can disable any performance module and fall back to portable CPU behavior.

## Optional modules

### Ultra-low-latency IPC descriptors

`IPCChannelConfig` describes nanosecond-class transport designs such as shared-memory rings, memory-mapped queues, kernel-bypass descriptors, or in-process fallback channels. `PerformanceLayer::publishAdvisorySignal` wraps a `ModelSignal` in an `IPCSignalEnvelope` with sequence and publication timestamps so downstream consumers can measure freshness while preserving passive semantics.

### SIMD-vectorized decision kernels

`PerformanceLayer::evaluateConsensusVector` provides a portable vector-style consensus summary over model signals. It is intentionally written as a standard C++17 baseline that compilers can auto-vectorize and that integrators can replace with AVX2, AVX-512, NEON, SVE, or other platform kernels. The result reports valid lanes, directional votes, weighted sum, and total weight; it does not mutate engine state or execute a decision.

### FPGA and ASIC-compatible manifests

`PerformanceLayer::describeHardwareTarget` emits a `HardwareKernelManifest` for portable CPU, SIMD CPU, FPGA, and ASIC targets. FPGA and ASIC manifests advertise fixed-point and streaming IPC compatibility for synthesis-oriented deployments while keeping `advisory_only = true` and `emits_orders = false`.

## Integration guidance

1. Keep the canonical `AILLEEngine::makeDecision` path as the safety boundary.
2. Use IPC envelopes to move model signals between processes with sequence numbers and timestamp freshness checks.
3. Use SIMD summaries to precompute consensus evidence, then feed the original model signals into the safety engine for auditable final decisions.
4. Compile FPGA/ASIC kernels only as risk-scoring accelerators; route their outputs through audit logging and human-facing alert adapters.
5. Document any platform-specific kernel replacement and prove it preserves the passive advisory contract before deployment.
