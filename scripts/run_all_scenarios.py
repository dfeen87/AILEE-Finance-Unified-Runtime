#!/usr/bin/env python3
"""
AILEE Finance Runtime - Batch Simulation Runner

Executes all selling pressure simulation scenarios defined in config/sim_config.yaml,
exports time series and agent data to data/, records metadata JSON, and generates
plots in plots/.
"""

import json
import os
import sys
from typing import Dict, Any

import matplotlib.pyplot as plt
import pandas as pd
import yaml

from src.agents import create_agent_population
from src.market_sim import MarketSimulator


def ensure_directories():
    os.makedirs("data", exist_ok=True)
    os.makedirs("plots", exist_ok=True)


def plot_scenario_results(scenario_key: str, scenario_name: str, df: pd.DataFrame):
    plt.style.use('dark_background') if 'dark_background' in plt.style.available else None

    fig, axes = plt.subplots(3, 1, figsize=(12, 10), sharex=True)

    # Plot 1: Price vs Fundamental Value
    axes[0].plot(df["step"], df["price"], label="Market Price", color="#00e676", linewidth=1.8)
    axes[0].plot(df["step"], df["fundamental_value"], label="Fundamental Value F(t)", color="#ff9100", linestyle="--", linewidth=1.5)
    axes[0].set_ylabel("Price ($)")
    axes[0].set_title(f"Scenario: {scenario_name} - Price vs Fundamentals", fontsize=12, fontweight='bold')
    axes[0].legend(loc="upper left")
    axes[0].grid(True, alpha=0.2)

    # Plot 2: Sell Volume vs Buy Volume
    axes[1].plot(df["step"], df["buy_volume"], label="Buy Order Volume", color="#00b0ff", alpha=0.7)
    axes[1].plot(df["step"], df["sell_volume"], label="Sell Order Volume", color="#ff1744", alpha=0.7)
    axes[1].set_ylabel("Order Volume")
    axes[1].set_title("Order Flow Dynamics (Buy vs Sell Volume)", fontsize=11)
    axes[1].legend(loc="upper left")
    axes[1].grid(True, alpha=0.2)

    # Plot 3: Selling Pressure Ratio & Volatility
    ax3_twin = axes[2].twinx()
    p1 = axes[2].plot(df["step"], df["sell_ratio"], label="Sell Ratio (Sell / Total Vol)", color="#e040fb", linewidth=1.2)
    p2 = ax3_twin.plot(df["step"], df["volatility"], label="Rolling Volatility", color="#ffd600", linestyle=":", alpha=0.8)
    axes[2].axhline(0.5, color="#888888", linestyle="--", alpha=0.5)
    axes[2].set_ylabel("Sell Ratio")
    ax3_twin.set_ylabel("Volatility")
    axes[2].set_xlabel("Simulation Step (t)")
    axes[2].set_title("Selling Pressure Ratio & Market Volatility", fontsize=11)

    # Combined legend for twin axes
    lines = p1 + p2
    labels = [l.get_label() for l in lines]
    axes[2].legend(lines, labels, loc="upper left")
    axes[2].grid(True, alpha=0.2)

    plt.tight_layout()
    plot_path = f"plots/scenario_{scenario_key}_overview.png"
    plt.savefig(plot_path, dpi=200, bbox_inches='tight')
    plt.close()
    print(f"  [+] Saved plot: {plot_path}")


def run_scenario(scenario_key: str, scenario_config: Dict[str, Any], default_settings: Dict[str, Any]) -> Dict[str, Any]:
    scenario_name = scenario_config.get("name", scenario_key)
    print(f"\n========================================================")
    print(f"Running Scenario: {scenario_name} ({scenario_key})")
    print(f"========================================================")

    initial_price = default_settings.get("initial_price", 100.0)
    agent_counts = scenario_config.get("agents", {})
    allocations = scenario_config.get("initial_allocations", {})

    # Create agent population
    agents = create_agent_population(agent_counts, allocations, initial_price)
    print(f"  [-] Initialized {len(agents)} agents across {len(agent_counts)} personas.")

    # Instantiate Market Simulator
    simulator = MarketSimulator(scenario_key, scenario_config, default_settings)

    # Execute simulation
    timeseries_df, agent_df, indicators = simulator.run(agents)

    # Save CSV outputs
    ts_path = f"data/scenario_{scenario_key}_timeseries.csv"
    agent_path = f"data/scenario_{scenario_key}_agents.csv"
    meta_path = f"data/scenario_{scenario_key}_metadata.json"

    timeseries_df.to_csv(ts_path, index=False)
    agent_df.to_csv(agent_path, index=False)

    metadata = {
        "scenario_key": scenario_key,
        "scenario_name": scenario_name,
        "description": scenario_config.get("description", ""),
        "seed": scenario_config.get("seed", 42),
        "T": scenario_config.get("T", 1000),
        "market_model": simulator.market_model_type,
        "indicators": indicators,
        "config": scenario_config,
    }

    with open(meta_path, "w") as f:
        json.dump(metadata, f, indent=2)

    print(f"  [+] Exported time series to: {ts_path}")
    print(f"  [+] Exported agent stats to: {agent_path}")
    print(f"  [+] Saved metadata JSON to: {meta_path}")

    # Generate plot
    plot_scenario_results(scenario_key, scenario_name, timeseries_df)

    print(f"  [*] Metrics summary:")
    print(f"      - Max Drawdown: {indicators['max_drawdown_pct']:.2f}%")
    print(f"      - Max Elevated Selling Duration: {indicators['max_elevated_sell_duration_steps']} steps")
    print(f"      - Mean Sell Ratio: {indicators['mean_sell_ratio']:.3f}")
    print(f"      - Peak Price: ${indicators['peak_price']:.2f}")
    print(f"      - Final Price: ${indicators['final_price']:.2f}")

    return metadata


def main():
    ensure_directories()
    config_file = "config/sim_config.yaml"

    if not os.path.exists(config_file):
        print(f"Error: Config file not found at {config_file}")
        sys.exit(1)

    with open(config_file, "r") as f:
        config = yaml.safe_load(f)

    default_settings = config.get("default_market_settings", {})
    scenarios = config.get("scenarios", {})

    all_metadata = {}
    for key, sc_config in scenarios.items():
        meta = run_scenario(key, sc_config, default_settings)
        all_metadata[key] = meta

    print("\n========================================================")
    print("All scenarios completed successfully!")
    print("========================================================\n")


if __name__ == "__main__":
    main()
