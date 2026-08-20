#!/usr/bin/env python3
"""
AILEE Framework - Intraday Volume Auto-Trader Script
AI-Load Integrity and Layered Evaluation

Copyright (c) Don Michael Feeney Jr.
Licensed under the MIT License.
"""

import argparse
import logging
import sys
from core.finance_kernel.volume_advisory import IntradayVolumeAdvisory, VolumeState
from core.finance_kernel.volume_execution import VolumeExecutionOperator

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")

def main():
    parser = argparse.ArgumentParser(description="AILEE Intraday Volume Auto-Trader Python Runner")
    parser.add_argument("--enable-auto-execute", action="store_true", help="Enable trade execution (default: dry-run)")
    parser.add_argument("--mode", choices=["paper", "live"], default="paper", help="Trading mode")
    parser.add_argument("--confirm-live", action="store_true", help="Required safety flag for live mode")
    parser.add_argument("--symbol", default="SPY", help="Target ticker symbol")
    parser.add_argument("--max-position-usd", type=float, default=10000.0, help="Max USD position size")
    parser.add_argument("--max-drawdown-pct", type=float, default=0.05, help="Max daily drawdown threshold")
    parser.add_argument("--hysteresis", type=int, default=2, help="Bar count hysteresis confirmation")
    args = parser.parse_args()

    if args.mode == "live" and not args.confirm_live:
        print("ERROR: --mode=live specified without --confirm-live flag. Aborting.", file=sys.stderr)
        sys.exit(1)

    print("========================================================")
    print(" AILEE Intraday Volume Auto-Trader Python Runner v11.0.0")
    print(f" Target Symbol:       {args.symbol}")
    print(f" Execution Enabled:   {'YES' if args.enable_auto_execute else 'NO (Dry-Run)'}")
    print(f" Mode:                {args.mode.upper()}")
    print(f" Max Position (USD):  ${args.max_position_usd}")
    print(f" Max Daily Drawdown:  {args.max_drawdown_pct * 100:.1f}%")
    print(f" Hysteresis Bars:     {args.hysteresis}")
    print("========================================================")

    vam_op = IntradayVolumeAdvisory()
    exec_op = VolumeExecutionOperator(
        enable_auto_execute=args.enable_auto_execute,
        mode=args.mode,
        max_position_usd=args.max_position_usd,
        max_daily_drawdown_pct=args.max_drawdown_pct,
        hysteresis_bars=args.hysteresis,
        symbol=args.symbol
    )

    # Simulated 5 intraday bar ticks
    ticks = [
        {"current_volume": 10000.0, "avg_volume": 10000.0, "price_change": 0.002, "vwap_deviation": 0.0, "prev_volume_anomaly_ratio": 0.0, "is_index_etf": True, "enable_contrarian_oversold": True},
        {"current_volume": 20000.0, "avg_volume": 10000.0, "price_change": 0.008, "vwap_deviation": 0.0, "prev_volume_anomaly_ratio": 0.0, "is_index_etf": True, "enable_contrarian_oversold": True},
        {"current_volume": 20000.0, "avg_volume": 10000.0, "price_change": 0.008, "vwap_deviation": 0.0, "prev_volume_anomaly_ratio": 2.0, "is_index_etf": True, "enable_contrarian_oversold": True},
        {"current_volume": 30000.0, "avg_volume": 10000.0, "price_change": -0.015, "vwap_deviation": -0.010, "prev_volume_anomaly_ratio": 0.0, "is_index_etf": True, "enable_contrarian_oversold": True},
        {"current_volume": 30000.0, "avg_volume": 10000.0, "price_change": -0.015, "vwap_deviation": -0.010, "prev_volume_anomaly_ratio": 3.0, "is_index_etf": True, "enable_contrarian_oversold": True},
    ]

    price = 500.0
    for i, tick_data in enumerate(ticks, 1):
        print(f"\n--- Bar {i} Processing ---")
        processed = vam_op.preprocess(tick_data)
        adv_dict = vam_op.execute(processed)
        exec_op.process_tick(adv_dict, price)
        price += 1.0

    print("\n[Python Runner Finished Successfully]")

if __name__ == "__main__":
    main()
