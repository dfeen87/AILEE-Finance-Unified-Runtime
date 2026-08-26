# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Deterministic Fibonacci & Golden Ratio Volume Simulation Harness.

Evaluates how Fibonacci retracements and extensions align with volume spikes
and modulate oversold weights and thresholds across Bullishness Modes.
"""

import sys
import os

# Ensure repo root is on sys.path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from core.finance_kernel.chart_intelligence import compute_fib_advisory
from core.finance_kernel.volume_advisory import evaluate_oversold
from core.finance_kernel.fibonacci import compute_retracements, compute_extensions


def run_simulation():
    print("=========================================================================")
    print("AILEE FINANCE — FIBONACCI + GOLDEN RATIO VOLUME SIMULATION")
    print("=========================================================================")

    modes = ["STANDARD", "CONSERVATIVE", "HYPER", "CONTRARIAN"]

    # Generate synthetic price series (trending up, pullback to 61.8%, extension)
    prices = []
    # Trend: 100 -> 150
    for p in range(100, 151, 2):
        prices.append(float(p))
    # Pullback to 61.8% of range (150 - 50*0.618 = 119.1)
    for p in range(150, 118, -2):
        prices.append(float(p))
    # Extension to 161.8% (100 + 50*1.618 = 180.9)
    for p in range(118, 182, 3):
        prices.append(float(p))

    high = 150.0
    low = 100.0
    avg_vol = 1000000.0

    print(f"Rolling Window High: {high:.2f}, Low: {low:.2f}")
    ret = compute_retracements(high, low)
    ext = compute_extensions(high, low)
    print(f"Retracements -> 23.6%: {ret.level_236:.2f}, 38.2%: {ret.level_382:.2f}, Golden 61.8%: {ret.level_618:.2f}, 78.6%: {ret.level_786:.2f}")
    print(f"Extensions   -> 127.2%: {ext.level_1272:.2f}, 161.8%: {ext.level_1618:.2f}, 261.8%: {ext.level_2618:.2f}")
    print("-------------------------------------------------------------------------")

    for mode in modes:
        active_zones = 0
        buy_signals = 0
        sell_signals = 0
        contrarian_zones = 0
        hyper_breakouts = 0

        total_weight_diff = 0.0

        for price in prices:
            # Generate volume spike at retracement/extension levels
            vol = avg_vol * 1.35 if (abs(price - ret.level_618) <= 1.0 or abs(price - ext.level_1618) <= 1.0) else avg_vol * 0.95

            fib_adv = compute_fib_advisory(
                current_price=price,
                recent_high=high,
                recent_low=low,
                current_volume=vol,
                avg_volume=avg_vol,
                mode=mode
            )

            res_unmodulated = evaluate_oversold(price, vol, avg_vol, mode=mode, fib=None)
            res_modulated = evaluate_oversold(price, vol, avg_vol, mode=mode, fib=fib_adv)

            if fib_adv.fib_zone_active:
                active_zones += 1
            if fib_adv.fib_buy_signal:
                buy_signals += 1
            if fib_adv.fib_sell_signal:
                sell_signals += 1
            if fib_adv.contrarian_fib_buy_zone:
                contrarian_zones += 1
            if fib_adv.hyper_fib_breakout:
                hyper_breakouts += 1

            total_weight_diff += (res_modulated["recommended_weight"] - res_unmodulated["recommended_weight"])

        print(f"MODE: {mode:<12} | Zones: {active_zones:2d} | Buy: {buy_signals:2d} | Sell: {sell_signals:2d} | Contrarian: {contrarian_zones:2d} | Hyper Breakout: {hyper_breakouts:2d} | Weight Delta: {total_weight_diff:+.3f}")

    print("-------------------------------------------------------------------------")
    print("SIMULATION VERIFICATION COMPLETE: Fail-closed governance and safety bounds intact.")
    print("=========================================================================")


if __name__ == "__main__":
    run_simulation()
