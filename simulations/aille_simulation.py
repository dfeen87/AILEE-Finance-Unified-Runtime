#!/usr/bin/env python3
# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Reproducible AILLE simulation harness.

Generates synthetic market returns, model signals, and compares the AILLE
framework decision logic against a naive baseline.
"""

import argparse
import csv
import math
import random
import statistics
from dataclasses import dataclass
from typing import List, Optional, Tuple


@dataclass
class ModelSignal:
    value: float
    confidence: float
    model_id: int


@dataclass
class Decision:
    final_value: float
    status: str
    confidence: float
    models_agreed: int
    fallback_used: bool
    reasoning: str


@dataclass
class AILLEConfig:
    min_confidence_threshold: float = 0.35
    grace_confidence_threshold: float = 0.25
    min_models_required: int = 2
    sign_agreement_threshold: float = 0.66
    fallback_window_size: int = 50
    fallback_position_scale: float = 0.1
    max_model_count: int = 10

    # Dynamic rule options
    enable_dynamic_fallback: bool = False
    fallback_alpha: float = 0.5
    fallback_beta: float = 0.1
    config_version: str = "4.1.0"


def smooth_position(signal: float, scale: float = 100.0) -> float:
    return math.tanh(signal * scale)


def apply_safety_layer(signals: List[ModelSignal], config: AILLEConfig) -> List[ModelSignal]:
    valid: List[ModelSignal] = []
    for sig in signals:
        if sig.confidence < 0.0 or sig.confidence > 1.0:
            continue  # Signal rejected: confidence out of range [0,1]
        if sig.confidence >= config.min_confidence_threshold:
            valid.append(sig)
        elif sig.confidence >= config.grace_confidence_threshold:
            valid.append(ModelSignal(sig.value, sig.confidence * 0.8, sig.model_id))
    return valid


def check_consensus(
    valid: List[ModelSignal], config: AILLEConfig
) -> Optional[Tuple[float, int]]:
    if len(valid) < config.min_models_required:
        return None

    values = [sig.value for sig in valid]
    median = statistics.median(values)
    median_sign = 1.0 if median >= 0 else -1.0

    agree_values = [v for v in values if (1.0 if v >= 0 else -1.0) == median_sign]
    models_agreed = len(agree_values)
    ratio = models_agreed / len(values)

    if ratio >= config.sign_agreement_threshold and models_agreed >= config.min_models_required:
        consensus_value = sum(agree_values) / models_agreed
        return consensus_value, models_agreed

    return None


class AILLEEngine:
    def __init__(self, config: Optional[AILLEConfig] = None) -> None:
        self.config = config or AILLEConfig()
        self.fallback_buffer: List[float] = []
        self.confidence_buffer: List[float] = []

    def _calculate_fallback_value(self) -> float:
        if not self.fallback_buffer:
            return 0.0
        return sum(self.fallback_buffer) / len(self.fallback_buffer)

    def _update_fallback_buffer(self, value: float, confidence: float) -> None:
        self.fallback_buffer.append(value)
        self.confidence_buffer.append(confidence)
        if len(self.fallback_buffer) > self.config.fallback_window_size:
            self.fallback_buffer.pop(0)
            self.confidence_buffer.pop(0)

    def _get_fallback_value(self, current_confidence: float = 0.0) -> float:
        if not self.fallback_buffer:
            return 0.0
        fallback_mean = self._calculate_fallback_value()
        sign = 1.0 if fallback_mean >= 0 else -1.0

        if self.config.enable_dynamic_fallback:
            ma_conf = sum(self.confidence_buffer) / len(self.confidence_buffer) if self.confidence_buffer else current_confidence
            scale = self.config.fallback_alpha * ma_conf + self.config.fallback_beta
            scale = max(0.1, min(0.5, scale))
        else:
            scale = self.config.fallback_position_scale

        return sign * scale

    def make_decision(self, model_signals: List[ModelSignal]) -> Decision:
        if not model_signals:
            return Decision(0.0, "ERROR_NO_MODELS", 0.0, 0, False, "No model inputs available")

        max_models = max(self.config.max_model_count, 1)
        scoped = model_signals[:max_models]
        valid = apply_safety_layer(scoped, self.config)

        if not valid:
            return Decision(
                final_value=self._get_fallback_value(0.1),
                status="REJECTED_LOW_CONFIDENCE",
                confidence=0.1,
                models_agreed=0,
                fallback_used=True,
                reasoning="All models failed confidence threshold – fallback activated",
            )

        consensus = check_consensus(valid, self.config)
        if consensus is None:
            return Decision(
                final_value=self._get_fallback_value(0.2),
                status="REJECTED_NO_CONSENSUS",
                confidence=0.2,
                models_agreed=0,
                fallback_used=True,
                reasoning="Models failed to reach consensus – fallback activated",
            )

        consensus_value, models_agreed = consensus
        final_value = smooth_position(consensus_value)
        avg_confidence = sum(sig.confidence for sig in valid) / len(valid)
        decision = Decision(
            final_value=final_value,
            status="DECISION_VALID",
            confidence=avg_confidence,
            models_agreed=models_agreed,
            fallback_used=False,
            reasoning=f"Consensus achieved with {models_agreed} models",
        )
        self._update_fallback_buffer(final_value, avg_confidence)
        return decision


@dataclass
class SimulationConfig:
    steps: int = 2000
    seed: int = 7
    base_vol: float = 0.01
    regime_vol: float = 0.03
    crash_probability: float = 0.01
    crash_magnitude: float = -0.08
    models: int = 3
    model_noise: float = 0.015
    steps_per_year: int = 252
    catastrophic_threshold: float = -0.05


def generate_market_returns(config: SimulationConfig) -> List[float]:
    random.seed(config.seed)
    returns = []
    for step in range(config.steps):
        regime_multiplier = 1.0 + (0.5 if (step // 50) % 2 == 1 else 0.0)
        vol = config.base_vol * regime_multiplier
        if random.random() < 0.05:
            vol = config.regime_vol
        daily_return = random.gauss(0.0002, vol)
        if random.random() < config.crash_probability:
            daily_return += config.crash_magnitude
        returns.append(daily_return)
    return returns


def generate_model_signals(true_return: float, config: SimulationConfig) -> List[ModelSignal]:
    signals: List[ModelSignal] = []
    for model_id in range(config.models):
        noise = random.gauss(0.0, config.model_noise)
        prediction = true_return + noise
        confidence = max(0.0, min(1.0, 1.0 - abs(noise) / max(config.base_vol, 1e-6)))
        signals.append(ModelSignal(prediction, confidence, model_id))
    return signals


def compute_metrics(returns: List[float], steps_per_year: int, catastrophic_threshold: float, positions: Optional[List[float]] = None) -> dict:
    total_return = math.prod([1 + r for r in returns]) - 1
    annualized_return = (1 + total_return) ** (steps_per_year / len(returns)) - 1
    avg_return = statistics.mean(returns)
    return_std = statistics.pstdev(returns)
    sharpe = 0.0 if return_std == 0 else (avg_return / return_std) * math.sqrt(steps_per_year)
    volatility = return_std * math.sqrt(steps_per_year)

    equity = 1.0
    peak = 1.0
    max_drawdown = 0.0
    for r in returns:
        equity *= 1 + r
        if equity > peak:
            peak = equity
        drawdown = (peak - equity) / peak
        max_drawdown = max(max_drawdown, drawdown)

    catastrophic_trades = sum(1 for r in returns if r <= catastrophic_threshold)
    worst_daily_loss = min(returns) if returns else 0.0

    turnover = 0.0
    if positions:
        for i in range(1, len(positions)):
            turnover += abs(positions[i] - positions[i-1])

    return {
        "total_return": total_return,
        "annualized_return": annualized_return,
        "sharpe_ratio": sharpe,
        "max_drawdown": max_drawdown,
        "volatility": volatility,
        "catastrophic_trades": catastrophic_trades,
        "worst_daily_loss": worst_daily_loss,
        "turnover": turnover,
    }


def run_simulation(sim_config: SimulationConfig, aille_config: AILLEConfig) -> Tuple[dict, dict]:
    market_returns = generate_market_returns(sim_config)
    engine = AILLEEngine(aille_config)

    aille_returns: List[float] = []
    aille_positions: List[float] = []
    naive_returns: List[float] = []
    naive_positions: List[float] = []

    for market_return in market_returns:
        signals = generate_model_signals(market_return, sim_config)
        decision = engine.make_decision(signals)
        aille_returns.append(decision.final_value * market_return)
        aille_positions.append(decision.final_value)

        naive_value = smooth_position(sum(sig.value for sig in signals) / len(signals))
        naive_returns.append(naive_value * market_return)
        naive_positions.append(naive_value)

    return (
        compute_metrics(aille_returns, sim_config.steps_per_year, sim_config.catastrophic_threshold, aille_positions),
        compute_metrics(naive_returns, sim_config.steps_per_year, sim_config.catastrophic_threshold, naive_positions)
    )


def format_pct(value: float) -> str:
    return f"{value * 100:.2f}%"


def optimize_hyperparameters(sim_config_template: SimulationConfig) -> Tuple[AILLEConfig, dict]:
    print("Running AILLE Hyperparameter Optimization and Robustness Checks...")

    # 1. Establish the baseline performance on a set of representative seeds
    seeds = [7, 42, 100, 2026]
    baselines = {}
    default_config = AILLEConfig()

    for s in seeds:
        sc = SimulationConfig(steps=sim_config_template.steps, seed=s, models=sim_config_template.models)
        baselines[s], _ = run_simulation(sc, default_config)

    print("Baseline computed across representative seeds.")

    # 2. Define the search space
    # Static options
    static_scales = [0.1, 0.12, 0.15, 0.18, 0.2]
    # Dynamic options
    alpha_options = [0.05, 0.1, 0.15]
    beta_options = [0.05, 0.1, 0.15]
    # Confidence thresholds
    min_conf_options = [0.2, 0.25, 0.3, 0.35, 0.4]
    grace_conf_options = [0.1, 0.15, 0.2, 0.25, 0.3]

    best_avg_sharpe = -1.0
    winning_config = default_config
    winning_results_summary = {}

    configs_to_test = []

    # Generate static test configs
    for scale in static_scales:
        for min_c in min_conf_options:
            for grace_c in grace_conf_options:
                if grace_c > min_c:
                    continue
                configs_to_test.append(AILLEConfig(
                    min_confidence_threshold=min_c,
                    grace_confidence_threshold=grace_c,
                    fallback_position_scale=scale,
                    enable_dynamic_fallback=False
                ))

    # Generate dynamic test configs
    for alpha in alpha_options:
        for beta in beta_options:
            for min_c in min_conf_options:
                for grace_c in grace_conf_options:
                    if grace_c > min_c:
                        continue
                    configs_to_test.append(AILLEConfig(
                        min_confidence_threshold=min_c,
                        grace_confidence_threshold=grace_c,
                        enable_dynamic_fallback=True,
                        fallback_alpha=alpha,
                        fallback_beta=beta
                    ))

    print(f"Total configurations to evaluate: {len(configs_to_test)}")

    for idx, cfg in enumerate(configs_to_test):
        all_seeds_passed = True
        total_sharpe = 0.0
        seed_results = {}

        for s in seeds:
            sc = SimulationConfig(steps=sim_config_template.steps, seed=s, models=sim_config_template.models)
            res, _ = run_simulation(sc, cfg)
            base_res = baselines[s]

            # Risk-floor constraint: Max drawdown must not be worse than 1.3x baseline or 3.0% absolute (extremely safe)
            max_allowed_drawdown = max(base_res["max_drawdown"] * 1.3, 0.03)
            if res["max_drawdown"] > max_allowed_drawdown:
                all_seeds_passed = False
                break

            total_sharpe += res["sharpe_ratio"]
            seed_results[s] = res

        if all_seeds_passed:
            avg_sharpe = total_sharpe / len(seeds)
            if avg_sharpe > best_avg_sharpe:
                best_avg_sharpe = avg_sharpe
                winning_config = cfg
                winning_results_summary = seed_results

    print(f"\nOptimization Finished!")
    print(f"Winning Configuration (Version: {winning_config.config_version}):")
    print(f"  Enable Dynamic Fallback: {winning_config.enable_dynamic_fallback}")
    if winning_config.enable_dynamic_fallback:
        print(f"  Fallback Alpha: {winning_config.fallback_alpha}")
        print(f"  Fallback Beta: {winning_config.fallback_beta}")
    else:
        print(f"  Fallback Position Scale: {winning_config.fallback_position_scale}")
    print(f"  Min Confidence Threshold: {winning_config.min_confidence_threshold}")
    print(f"  Grace Confidence Threshold: {winning_config.grace_confidence_threshold}")
    print(f"  Average Sharpe Ratio: {best_avg_sharpe:.3f}")

    return winning_config, winning_results_summary


def main() -> None:
    parser = argparse.ArgumentParser(description="Run AILLE simulation benchmark")
    parser.add_argument("--steps", type=int, default=2000)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--models", type=int, default=3)
    parser.add_argument("--output", type=str, default="")
    parser.add_argument("--optimize", action="store_true", help="Perform hyperparameter optimization")
    args = parser.parse_args()

    sim_config = SimulationConfig(steps=args.steps, seed=args.seed, models=args.models)

    if args.optimize:
        best_cfg, results_by_seed = optimize_hyperparameters(sim_config)
        # Output optimized results for current seed
        current_res = results_by_seed.get(args.seed)
        if not current_res:
            current_res, _ = run_simulation(sim_config, best_cfg)

        # Compare with baseline
        baseline_cfg = AILLEConfig()
        baseline_res, naive_res = run_simulation(sim_config, baseline_cfg)

        print("\n=============================================")
        print(f"Comparison for Seed {args.seed}")
        print("=============================================")
        print(f"METRIC                | BASELINE AILLE  | OPTIMIZED AILLE (v{best_cfg.config_version}) | NAIVE")
        print(f"Total Return          | {format_pct(baseline_res['total_return']):15} | {format_pct(current_res['total_return']):23} | {format_pct(naive_res['total_return'])}")
        print(f"Annualized Return     | {format_pct(baseline_res['annualized_return']):15} | {format_pct(current_res['annualized_return']):23} | {format_pct(naive_res['annualized_return'])}")
        print(f"Sharpe Ratio          | {baseline_res['sharpe_ratio']:15.3f} | {current_res['sharpe_ratio']:23.3f} | {naive_res['sharpe_ratio']:.3f}")
        print(f"Max Drawdown          | {format_pct(baseline_res['max_drawdown']):15} | {format_pct(current_res['max_drawdown']):23} | {format_pct(naive_res['max_drawdown'])}")
        print(f"Volatility            | {format_pct(baseline_res['volatility']):15} | {format_pct(current_res['volatility']):23} | {format_pct(naive_res['volatility'])}")
        print(f"Worst Daily Loss      | {format_pct(baseline_res['worst_daily_loss']):15} | {format_pct(current_res['worst_daily_loss']):23} | {format_pct(naive_res['worst_daily_loss'])}")
        print(f"Turnover              | {baseline_res['turnover']:15.2f} | {current_res['turnover']:23.2f} | {naive_res['turnover']:.2f}")
        print(f"Catastrophic Trades   | {baseline_res['catastrophic_trades']:15} | {current_res['catastrophic_trades']:23} | {naive_res['catastrophic_trades']}")

        if args.output:
            write_results = {
                "baseline": baseline_res,
                "optimized": current_res,
                "naive": naive_res
            }
            with open(args.output, "w", newline="", encoding="utf-8") as file:
                writer = csv.writer(file)
                writer.writerow(["strategy", "metric", "value"])
                for strategy, metrics in write_results.items():
                    for metric, value in metrics.items():
                        writer.writerow([strategy, metric, value])
            print(f"\nWrote results to {args.output}")

    else:
        # Standard run
        cfg = AILLEConfig()
        aille_res, naive_res = run_simulation(sim_config, cfg)

        print("AILLE Simulation Results")
        print("=========================")
        for strategy, metrics in [("aille", aille_res), ("naive", naive_res)]:
            print(f"\n{strategy.upper()}")
            print(f"Total Return: {format_pct(metrics['total_return'])}")
            print(f"Annualized Return: {format_pct(metrics['annualized_return'])}")
            print(f"Sharpe Ratio: {metrics['sharpe_ratio']:.3f}")
            print(f"Max Drawdown: {format_pct(metrics['max_drawdown'])}")
            print(f"Volatility: {format_pct(metrics['volatility'])}")
            print(f"Catastrophic Trades: {metrics['catastrophic_trades']}")
            print(f"Worst Daily Loss: {format_pct(metrics['worst_daily_loss'])}")
            print(f"Turnover: {metrics['turnover']:.2f}")

        if args.output:
            write_results = {"aille": aille_res, "naive": naive_res}
            with open(args.output, "w", newline="", encoding="utf-8") as file:
                writer = csv.writer(file)
                writer.writerow(["strategy", "metric", "value"])
                for strategy, metrics in write_results.items():
                    for metric, value in metrics.items():
                        writer.writerow([strategy, metric, value])
            print(f"\nWrote results to {args.output}")


if __name__ == "__main__":
    main()
