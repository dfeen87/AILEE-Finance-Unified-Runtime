#!/usr/bin/env python3
# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Simulation Harness for Unified Cohesive Runtime & Resiliency Engine (Layer 19).

Simulates multi-layer master execution cycles, cross-layer anomaly/stream faults,
and fail-closed escalation under system stress.
"""

import time
import argparse
import statistics
from core.finance_kernel.unified_runtime import (
    UnifiedRuntimeOperator,
    UNIFIED_STATUS_NOMINAL,
    UNIFIED_STATUS_DEGRADED,
    UNIFIED_STATUS_STRESS_OVERRIDE,
    UNIFIED_STATUS_META_LOCKED
)


def run_unified_simulation(num_cycles: int = 100000, inject_faults: bool = True):
    print("=================================================================")
    print("  AILEE Unified Finance Runtime (v17.0.0)")
    print("  Layer 19 — Unified Cohesive Runtime & Resiliency Engine Simulation")
    print("=================================================================\n")

    operator = UnifiedRuntimeOperator()
    latencies_us = []

    print(f"[1] Simulating {num_cycles} Master Execution Cycles...")

    for i in range(1, num_cycles + 1):
        # Inject fault escalation at cycle 50,000 if enabled
        stream_degraded = False
        trigger_escalation = False
        if inject_faults and i == 50000:
            stream_degraded = True
            trigger_escalation = True

        cycle_input = {
            "cycle_sequence_id": i - 1,
            "timestamp_ns": time.time_ns(),
            "stream_degraded": stream_degraded,
            "trigger_stress_escalation": trigger_escalation,
            "anomaly_active": False,
            "anomaly_severity": 0.0,
            "msgam_risk_elevated": False,
            "stress_level": 0,
            "meta_execution_ready": True
        }

        t_start = time.perf_counter_ns()
        pre = operator.preprocess(cycle_input)
        res = operator.execute(pre)
        t_end = time.perf_counter_ns()

        latencies_us.append((t_end - t_start) / 1000.0)

    latencies_us.sort()
    mean_us = statistics.mean(latencies_us)
    p50_us = latencies_us[int(num_cycles * 0.50)]
    p99_us = latencies_us[int(num_cycles * 0.99)]

    print("\n[2] Execution Performance Results:")
    print(f"    - Processed Cycles  : {num_cycles}")
    print(f"    - Mean Latency      : {mean_us:.3f} µs")
    print(f"    - p50 Latency       : {p50_us:.3f} µs")
    print(f"    - p99 Latency       : {p99_us:.3f} µs")

    print("\n[3] Master Runtime Resiliency Analysis:")
    print(f"    - System Status     : {res['system_status']} (0=NOMINAL, 1=DEGRADED, 2=STRESS_OVERRIDE, 3=META_LOCKED)")
    print(f"    - Fault Escalated   : {res['fault_escalated']}")
    print(f"    - Execution Ready   : {res['execution_ready']}")

    print("\n=================================================================")
    print("  Unified Runtime Simulation Harness Completed Successfully!")
    print("=================================================================")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Unified Runtime Simulation Harness")
    parser.add_argument("--cycles", type=int, default=100000, help="Number of cycles to simulate")
    parser.add_argument("--no-faults", action="store_true", help="Disable synthetic fault injection")
    args = parser.parse_args()

    run_unified_simulation(num_cycles=args.cycles, inject_faults=not args.no_faults)
