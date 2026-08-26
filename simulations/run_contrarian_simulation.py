#!/usr/bin/env python3
# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Simulation Harness for Contrarian Bull Switch Performance Analytics.

Simulates and evaluates system performance across all 4 global bullishness modes
(STANDARD, CONSERVATIVE, HYPER, CONTRARIAN) across multi-regime market scenarios:
1. Nominal Growth Regime
2. Volatility Spike & Oversold Dip Regime
3. Structural Crash & Stress Override Regime (Fail-Closed Escalation)
4. HFT Delta-V Impulse Acceleration Regime
"""

import sys
import os
import time
import argparse
import statistics

# Ensure repo root is in python path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from core.finance_kernel.volume_advisory import IntradayVolumeAdvisory
from core.finance_kernel.chart_intelligence import ChartIntelligenceOperator
from core.finance_kernel.unified_runtime import UnifiedRuntimeOperator
from ailee_finance.domains.finance.sell_governance import compute_sell_ceiling


def run_contrarian_simulation(num_cycles: int = 5000):
    print("=================================================================")
    print("  AILEE Finance Unified Runtime (v22.0.0)")
    print("  Contrarian Bull Switch Performance Analytics & Multi-Mode Benchmark")
    print("=================================================================\n")

    modes = ["STANDARD", "CONSERVATIVE", "HYPER", "CONTRARIAN"]
    regimes = [
        ("Nominal Growth", {"price_chg": 0.005, "vwap_dev": 0.002, "vol_ratio": 1.1, "volatility": 12.0, "trust": 0.90, "manip": 0.05, "stress": False}),
        ("Oversold Dip", {"price_chg": -0.018, "vwap_dev": -0.012, "vol_ratio": 3.2, "volatility": 28.0, "trust": 0.85, "manip": 0.10, "stress": False}),
        ("Structural Crash", {"price_chg": -0.045, "vwap_dev": -0.035, "vol_ratio": 5.5, "volatility": 48.0, "trust": 0.45, "manip": 0.55, "stress": True}),
        ("HFT Delta-V Impulse", {"price_chg": -0.012, "vwap_dev": -0.008, "vol_ratio": 2.8, "volatility": 22.0, "trust": 0.80, "manip": 0.15, "stress": False}),
    ]

    vam_op = IntradayVolumeAdvisory()
    chart_op = ChartIntelligenceOperator()

    results_by_mode = {}

    for mode in modes:
        print(f"-----------------------------------------------------------------")
        print(f" Evaluating Mode: [{mode}] over {num_cycles} cycles per regime...")
        print(f"-----------------------------------------------------------------")

        mode_stats = {
            "total_cycles": 0,
            "latencies_us": [],
            "contrarian_signals": 0,
            "oversold_activations": 0,
            "total_weight": 0.0,
            "sell_ceiling_accum": 0.0,
            "contrarian_buy_zones": 0,
            "safety_kill_switch_activations": 0
        }

        t_mode_start = time.perf_counter()

        for r_name, r_params in regimes:
            for i in range(1, num_cycles + 1):
                # Build VAM input
                vam_inp = {
                    "current_volume": 1000.0 * r_params["vol_ratio"],
                    "avg_volume": 1000.0,
                    "price_change": r_params["price_chg"],
                    "vwap_deviation": r_params["vwap_dev"],
                    "symbol": "SPY",
                    "trust_score": r_params["trust"],
                    "manipulation_score": r_params["manip"],
                    "hft_bias_config": {
                        "enabled": True,
                        "bullishness_mode": mode,
                        "trust_threshold_bullish": 0.70,
                        "manipulation_threshold": 0.30,
                        "contrarian_oversold_weight_mult": 1.25,
                        "contrarian_oversold_threshold": 0.65,
                        "contrarian_hf_impulse_scale": 1.25,
                        "contrarian_sell_ceiling_factor": 0.85 if mode in ("CONTRARIAN", "HYPER") else 0.80
                    }
                }

                # Build Chart input
                chart_inp = {
                    "last_price": 100.0,
                    "ewma_volatility": r_params["volatility"],
                    "baseline_volatility": 10.0,
                    "bid_size": 10.0,
                    "ask_size": 10.0,
                    "baseline_depth": 50.0,
                    "rolling_correlation": 0.1 if r_params["stress"] else 0.8,
                    "expected_correlation": 0.8,
                    "volume_anomaly_ratio": r_params["vol_ratio"],
                    "trust_score": r_params["trust"],
                    "manipulation_score": r_params["manip"],
                    "layer_locks_engaged": r_params["stress"],
                    "hft_bias_config": {
                        "enabled": True,
                        "bullishness_mode": mode
                    }
                }

                # Benchmark tick
                t_start = time.perf_counter_ns()

                # VAM
                v_val = vam_op.validate(vam_inp)
                v_pre = vam_op.preprocess(v_val)
                v_res = vam_op.execute(v_pre)

                # Chart
                c_val = chart_op.validate(chart_inp)
                c_pre = chart_op.preprocess(c_val)
                c_res = chart_op.execute(c_pre)

                # SELL Governance ceiling calculation (Level 1, position 100.0)
                is_bullish_allowed = r_params["trust"] >= 0.70 and r_params["manip"] <= 0.30 and not r_params["stress"]
                sell_ceiling_factor = 0.85 if mode in ("CONTRARIAN", "HYPER") else 0.80
                allowed_sell = compute_sell_ceiling(
                    1,
                    100.0,
                    bullish_active=is_bullish_allowed,
                    bullish_sell_ceiling_factor=sell_ceiling_factor
                )

                t_end = time.perf_counter_ns()
                mode_stats["latencies_us"].append((t_end - t_start) / 1000.0)

                # Collect metrics
                mode_stats["total_cycles"] += 1
                if v_res.get("contrarian_buy_signal"):
                    mode_stats["contrarian_signals"] += 1
                if v_res.get("oversold_state"):
                    mode_stats["oversold_activations"] += 1
                mode_stats["total_weight"] += v_res.get("recommended_weight", 0.0)
                mode_stats["sell_ceiling_accum"] += allowed_sell

                if c_res.get("pattern_diagnostics", {}).get("contrarian_buy_zone"):
                    mode_stats["contrarian_buy_zones"] += 1

                if r_params["stress"] or not is_bullish_allowed:
                    mode_stats["safety_kill_switch_activations"] += 1

        t_mode_end = time.perf_counter()

        mode_stats["latencies_us"].sort()
        mean_us = statistics.mean(mode_stats["latencies_us"])
        p50_us = mode_stats["latencies_us"][int(len(mode_stats["latencies_us"]) * 0.50)]
        p99_us = mode_stats["latencies_us"][int(len(mode_stats["latencies_us"]) * 0.99)]
        avg_weight = mode_stats["total_weight"] / mode_stats["total_cycles"]
        avg_sell_ceiling = mode_stats["sell_ceiling_accum"] / mode_stats["total_cycles"]

        print(f"  Processed Total Cycles        : {mode_stats['total_cycles']}")
        print(f"  Wall Time                     : {t_mode_end - t_mode_start:.2f}s")
        print(f"  Mean Cycle Latency            : {mean_us:.3f} µs")
        print(f"  p50 Latency                   : {p50_us:.3f} µs")
        print(f"  p99 Latency                   : {p99_us:.3f} µs")
        print(f"  Oversold Activations          : {mode_stats['oversold_activations']}")
        print(f"  Contrarian Buy Signals        : {mode_stats['contrarian_signals']}")
        print(f"  Contrarian Buy Zones (Chart)  : {mode_stats['contrarian_buy_zones']}")
        print(f"  Avg Recommended Weight        : {avg_weight:.4f}")
        print(f"  Avg Dynamic Sell Ceiling      : {avg_sell_ceiling:.2f}")
        print(f"  Safety Kill Switch Triggers   : {mode_stats['safety_kill_switch_activations']}\n")

        results_by_mode[mode] = {
            "mean_us": mean_us,
            "p50_us": p50_us,
            "p99_us": p99_us,
            "oversold_activations": mode_stats["oversold_activations"],
            "contrarian_signals": mode_stats["contrarian_signals"],
            "contrarian_buy_zones": mode_stats["contrarian_buy_zones"],
            "avg_weight": avg_weight,
            "avg_sell_ceiling": avg_sell_ceiling,
            "kill_switch_triggers": mode_stats["safety_kill_switch_activations"]
        }

    print("=================================================================")
    print("  SUMMARY BENCHMARK COMPARISON MATRIX")
    print("=================================================================")
    print(f"{'Mode':<15} | {'p50 (µs)':<10} | {'p99 (µs)':<10} | {'Contrarian Signals':<20} | {'Avg Weight':<12} | {'Sell Ceiling':<12}")
    print("-" * 90)
    for m, s in results_by_mode.items():
        print(f"{m:<15} | {s['p50_us']:<10.3f} | {s['p99_us']:<10.3f} | {s['contrarian_signals']:<20} | {s['avg_weight']:<12.4f} | {s['avg_sell_ceiling']:<12.2f}")
    print("=================================================================\n")

    print("Contrarian Bull Switch Performance Analytics Harness Completed Successfully!")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Contrarian Bull Switch Simulation Harness")
    parser.add_argument("--cycles", type=int, default=5000, help="Cycles per regime per mode")
    args = parser.parse_args()

    run_contrarian_simulation(num_cycles=args.cycles)
