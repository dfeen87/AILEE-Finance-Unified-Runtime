#!/usr/bin/env python3
# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Simulation Harness for Bitcoin Mainnet Temporal Timing Synchronization.

Simulates authoritative Bitcoin mainnet block header timestamp synchronization,
intra-block micro-tick clock advancement, temporal drift / jitter injection,
block height sequence gaps, clock confidence decay, and fail-closed escalation
into Layer 13 Stress Override and Layer 14 Meta-Governance Lock.
"""

import sys
import os
from pathlib import Path

# Add project root to Python module path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import time
import argparse
import statistics
from core.finance_kernel.sync_adapter import (
    SyncAdapter,
    SyncAdapterConfig,
    SYNC_FLAG_ALIGNED,
    SYNC_FLAG_EXTERNAL,
    SYNC_FLAG_FALLBACK,
    SYNC_FLAG_DRIFT_WARN,
    SYNC_FLAG_GAP_DETECTED
)


def run_bitcoin_sync_simulation(num_ticks: int = 100000, inject_drift: bool = True, inject_gaps: bool = True):
    print("=================================================================")
    print("  AILEE Finance Runtime (v23.0.0)")
    print("  Bitcoin Mainnet Temporal Timing Sync Simulation Harness")
    print("=================================================================\n")

    config = SyncAdapterConfig(
        target_cadence_ns=10_000_000,       # 10ms target cadence
        max_drift_threshold_ns=5_000_000,   # 5ms drift threshold
        min_confidence_threshold=0.80,       # 80% confidence floor
        auto_escalate_stress=1,
        auto_escalate_meta_lock=1
    )
    adapter = SyncAdapter(config=config)

    latencies_us = []
    base_ts_ns = time.time_ns()
    curr_ts_ns = base_ts_ns

    drift_warn_count = 0
    gap_count = 0
    stress_escalation_count = 0
    meta_lock_escalation_count = 0

    print(f"[1] Simulating {num_ticks} Bitcoin Mainnet Temporal Sync Ticks...")

    for i in range(1, num_ticks + 1):
        tick_idx = i
        ts = curr_ts_ns + 10_000_000  # Default 10ms cadence
        wave_phase = (i * 0.05) % 6.283185307179586
        confidence = 1.0

        # Scenario 1: Inject clock drift at tick 25,000 (+12ms drift)
        if inject_drift and i == 25000:
            ts += 12_000_000  # 12ms drift > 5ms threshold

        # Scenario 2: Inject block height sequence gap at tick 50,000 (+10 blocks)
        if inject_gaps and i == 50000:
            tick_idx += 10

        # Scenario 3: Inject low clock confidence at tick 75,000
        if inject_drift and i == 75000:
            confidence = 0.40  # Breaches 0.50 meta-lock threshold

        t_start = time.perf_counter_ns()
        tick = adapter.ingest_protocol_clock(
            tick_index=tick_idx,
            timestamp_ns=ts,
            wave_phase=wave_phase,
            confidence=confidence
        )
        t_end = time.perf_counter_ns()

        curr_ts_ns = ts
        latencies_us.append((t_end - t_start) / 1000.0)

        if (tick.alignment_flags & SYNC_FLAG_DRIFT_WARN) != 0:
            drift_warn_count += 1
        if (tick.alignment_flags & SYNC_FLAG_GAP_DETECTED) != 0:
            gap_count += 1
        if tick.escalate_stress == 1:
            stress_escalation_count += 1
        if tick.escalate_meta_lock == 1:
            meta_lock_escalation_count += 1

    # Measure standalone zero-jitter fallback advance performance
    t_start_fallback = time.perf_counter_ns()
    fallback_tick = adapter.advance_tick()
    t_end_fallback = time.perf_counter_ns()
    fallback_latency_us = (t_end_fallback - t_start_fallback) / 1000.0

    latencies_us.sort()
    mean_us = statistics.mean(latencies_us)
    p50_us = latencies_us[int(num_ticks * 0.50)]
    p99_us = latencies_us[int(num_ticks * 0.99)]

    metrics = adapter.get_metrics()
    state = adapter.get_state()

    print("\n[2] Execution Performance Results:")
    print(f"    - Processed Ticks            : {metrics.total_ticks_processed}")
    print(f"    - Mean Ingestion Latency      : {mean_us:.3f} µs")
    print(f"    - p50 (Median) Latency        : {p50_us:.3f} µs")
    print(f"    - p99 Latency                 : {p99_us:.3f} µs")
    print(f"    - Standalone Fallback Latency : {fallback_latency_us:.3f} µs")

    print("\n[3] Temporal Synchronization & Integrity Metrics:")
    print(f"    - External Sync Ticks         : {metrics.external_sync_ticks}")
    print(f"    - Maximum Observed Drift      : {state.max_observed_drift_ns / 1_000_000.0:.3f} ms")
    print(f"    - Clock Drift Warnings        : {drift_warn_count}")
    print(f"    - Block Sequence Gaps Caught  : {gap_count}")
    print(f"    - Degraded Sync State Count   : {metrics.sync_degraded}")
    print(f"    - Stress Escalations (L13)    : {metrics.stress_escalations}")
    print(f"    - Meta-Lock Escalations (L14) : {metrics.meta_lock_escalations}")

    if inject_gaps:
        assert gap_count > 0, "Sequence gap was not detected!"
        print("    - Gap Detection Accuracy Test : PASSED")

    if inject_drift:
        assert drift_warn_count > 0, "Drift warning was not triggered!"
        assert metrics.stress_escalations > 0, "Stress escalation was not triggered!"
        print("    - Drift Warning & Stress Gate : PASSED")

    print("\n=================================================================")
    print("  Bitcoin Mainnet Temporal Sync Simulation Completed Successfully!")
    print("=================================================================")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Bitcoin Mainnet Temporal Sync Simulation Harness")
    parser.add_argument("--ticks", type=int, default=100000, help="Number of ticks to simulate")
    parser.add_argument("--no-drift", action="store_true", help="Disable synthetic drift injection")
    parser.add_argument("--no-gaps", action="store_true", help="Disable synthetic gap injection")
    args = parser.parse_args()

    run_bitcoin_sync_simulation(
        num_ticks=args.ticks,
        inject_drift=not args.no_drift,
        inject_gaps=not args.no_gaps
    )
