"""
AILEE Framework - Sync Adapter Operator
AI-Load Integrity and Layered Evaluation

Deterministic temporal clock synchronization adapter binding AILEE Finance runtime
to AILEE Runtime Protocol authoritative clock.

Version Tag: SYNC_ADAPTER_V1
Copyright (c) Don Michael Feeney Jr.
Licensed under the MIT License.
"""

from dataclasses import dataclass
import time

SYNC_ADAPTER_VERSION = "SYNC_ADAPTER_V1"

SYNC_FLAG_ALIGNED = 0x01
SYNC_FLAG_EXTERNAL = 0x02
SYNC_FLAG_FALLBACK = 0x04
SYNC_FLAG_DRIFT_WARN = 0x08
SYNC_FLAG_GAP_DETECTED = 0x10


@dataclass
class SyncTick:
    tick_index: int = 0
    timestamp_ns: int = 0
    drift_ns: int = 0
    tick_cadence_ns: int = 10000000
    wave_phase: float = 0.0
    confidence: float = 1.0
    alignment_flags: int = SYNC_FLAG_ALIGNED
    degraded: int = 0
    escalate_stress: int = 0
    escalate_meta_lock: int = 0


@dataclass
class SyncAdapterState:
    current_tick_index: int = 0
    last_timestamp_ns: int = 0
    expected_cadence_ns: int = 10000000
    max_observed_drift_ns: int = 0
    current_wave_phase: float = 0.0
    clock_confidence: float = 1.0
    external_clock_active: int = 0
    sync_degraded: int = 0


@dataclass
class SyncAdapterConfig:
    target_cadence_ns: int = 10000000
    max_drift_threshold_ns: int = 5000000
    min_confidence_threshold: float = 0.80
    auto_escalate_stress: int = 1
    auto_escalate_meta_lock: int = 1


@dataclass
class SyncAdapterObservabilityMetrics:
    total_ticks_processed: int = 0
    external_sync_ticks: int = 0
    fallback_ticks: int = 0
    last_drift_ns: int = 0
    current_confidence: float = 1.0
    external_clock_active: int = 0
    sync_degraded: int = 0
    stress_escalations: int = 0
    meta_lock_escalations: int = 0


class SyncAdapter:
    def __init__(self, config: SyncAdapterConfig = None):
        self.config = config if config is not None else SyncAdapterConfig()
        self.state = SyncAdapterState(expected_cadence_ns=self.config.target_cadence_ns)
        self.metrics = SyncAdapterObservabilityMetrics()
        self.current_tick = SyncTick()

    def reset(self):
        self.state = SyncAdapterState(expected_cadence_ns=self.config.target_cadence_ns)
        self.metrics = SyncAdapterObservabilityMetrics()
        self.current_tick = SyncTick()

    def ingest_protocol_clock(
        self,
        tick_index: int,
        timestamp_ns: int,
        wave_phase: float,
        confidence: float = 1.0
    ) -> SyncTick:
        tick = SyncTick()
        tick.tick_index = tick_index
        tick.timestamp_ns = timestamp_ns
        tick.tick_cadence_ns = self.config.target_cadence_ns
        tick.wave_phase = wave_phase
        tick.confidence = max(0.0, min(1.0, float(confidence)))
        tick.alignment_flags = SYNC_FLAG_ALIGNED | SYNC_FLAG_EXTERNAL

        if self.state.last_timestamp_ns > 0 and timestamp_ns > self.state.last_timestamp_ns:
            elapsed_ns = timestamp_ns - self.state.last_timestamp_ns
            tick.drift_ns = elapsed_ns - self.config.target_cadence_ns
        else:
            tick.drift_ns = 0

        abs_drift = abs(tick.drift_ns)
        if abs_drift > self.state.max_observed_drift_ns:
            self.state.max_observed_drift_ns = abs_drift

        if self.state.current_tick_index > 0 and tick_index > (self.state.current_tick_index + 1):
            tick.alignment_flags |= SYNC_FLAG_GAP_DETECTED
            tick.degraded = 1
            tick.confidence *= 0.8

        if abs_drift > self.config.max_drift_threshold_ns:
            tick.alignment_flags |= SYNC_FLAG_DRIFT_WARN
            tick.degraded = 1
            tick.confidence *= 0.85

        if tick.confidence < self.config.min_confidence_threshold:
            tick.degraded = 1

        if tick.degraded == 1:
            if self.config.auto_escalate_stress and (
                tick.confidence < self.config.min_confidence_threshold or abs_drift > self.config.max_drift_threshold_ns
            ):
                tick.escalate_stress = 1
                self.metrics.stress_escalations += 1

            if self.config.auto_escalate_meta_lock and (
                tick.confidence < 0.50 or abs_drift > (self.config.max_drift_threshold_ns * 2)
            ):
                tick.escalate_meta_lock = 1
                self.metrics.meta_lock_escalations += 1

        self.state.current_tick_index = tick_index
        self.state.last_timestamp_ns = timestamp_ns
        self.state.current_wave_phase = wave_phase
        self.state.clock_confidence = tick.confidence
        self.state.external_clock_active = 1
        self.state.sync_degraded = tick.degraded

        self.metrics.total_ticks_processed += 1
        self.metrics.external_sync_ticks += 1
        self.metrics.last_drift_ns = tick.drift_ns
        self.metrics.current_confidence = tick.confidence
        self.metrics.external_clock_active = 1
        self.metrics.sync_degraded = tick.degraded

        self.current_tick = tick
        return self.current_tick

    def advance_tick(self) -> SyncTick:
        tick = SyncTick()
        tick.tick_index = self.state.current_tick_index + 1

        if self.state.last_timestamp_ns > 0:
            tick.timestamp_ns = self.state.last_timestamp_ns + self.config.target_cadence_ns
        else:
            tick.timestamp_ns = int(time.time_ns())

        tick.drift_ns = 0
        tick.tick_cadence_ns = self.config.target_cadence_ns

        TWO_PI = 6.283185307179586
        next_phase = self.state.current_wave_phase + 0.10
        if next_phase >= TWO_PI:
            next_phase -= TWO_PI
        tick.wave_phase = next_phase
        tick.confidence = 1.0
        tick.alignment_flags = SYNC_FLAG_ALIGNED | SYNC_FLAG_FALLBACK
        tick.degraded = 0
        tick.escalate_stress = 0
        tick.escalate_meta_lock = 0

        self.state.current_tick_index = tick.tick_index
        self.state.last_timestamp_ns = tick.timestamp_ns
        self.state.current_wave_phase = tick.wave_phase
        self.state.clock_confidence = 1.0
        self.state.external_clock_active = 0
        self.state.sync_degraded = 0

        self.metrics.total_ticks_processed += 1
        self.metrics.fallback_ticks += 1
        self.metrics.last_drift_ns = 0
        self.metrics.current_confidence = 1.0
        self.metrics.external_clock_active = 0
        self.metrics.sync_degraded = 0

        self.current_tick = tick
        return self.current_tick

    def get_sync_tick(self) -> SyncTick:
        return self.current_tick

    def get_state(self) -> SyncAdapterState:
        return self.state

    def get_metrics(self) -> SyncAdapterObservabilityMetrics:
        return self.metrics
