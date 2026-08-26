# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Deterministic Intraday Volume Advisory Module for SPY and QQQ.

Evaluates intraday volume, anomalies relative to baseline, price changes,
and VWAP deviations to output structured risk and growth scores.
"""

import math
from typing import List, Dict, Any, Optional

from core.finance_kernel.kernel_context import FinanceKernelContext
from core.finance_kernel.kernel_config import FinanceKernelConfig
from core.finance_kernel.kernel_registry import BaseOperator
from core.finance_kernel.hft_bias import is_bullish_mode_allowed


def calculate_hft_delta_v(
    isp: float,
    efficiency: float,
    alpha: float,
    v0: float,
    ticks: List[Dict[str, float]],
    min_mass_floor: float = 1e-6
) -> float:
    """
    Calculates high-frequency impulse velocity Δv across micro-tick price action & volume stream.
    Δv = Isp * η * e^(-α * v0^2) * ∫0^tf [P_input(t) * e^(-α * w(t)^2) * e^(2 * α * v0) * v(t)] / M(t) dt
    """
    if not ticks or efficiency <= 0.0:
        return 0.0

    integral_sum = 0.0
    exp_2_alpha_v0 = math.exp(2.0 * alpha * v0)
    mass_floor = max(min_mass_floor, 1e-6)

    for tick in ticks:
        p_input = tick.get("p_input", 0.0)
        w = tick.get("w", 0.0)
        v = tick.get("v", 0.0)
        M = tick.get("M", 1.0)
        dt = tick.get("dt", 0.001)

        m_safe = max(M, mass_floor)
        exp_neg_alpha_w2 = math.exp(-alpha * w * w)
        dt_val = max(dt, 0.001)

        integrand = (p_input * exp_neg_alpha_w2 * exp_2_alpha_v0 * v) / m_safe
        integral_sum += integrand * dt_val

    exp_neg_alpha_v02 = math.exp(-alpha * v0 * v0)
    return isp * efficiency * exp_neg_alpha_v02 * integral_sum


class VolumeState:
    """Intraday volume metrics snapshot."""
    def __init__(self, current_volume: float = 0.0, avg_volume: float = 0.0,
                 price_change: float = 0.0, vwap_deviation: float = 0.0,
                 prev_volume_anomaly_ratio: float = 0.0, prev_recommended_weight: float = -1.0,
                 hft_p_input: float = 0.0, hft_mass: float = 1.0, hft_v0: float = 0.0,
                 is_index_etf: bool = False, contrarian_override: int = 0,
                 enable_hft_calc: bool = False):
        self.current_volume = current_volume
        self.avg_volume = avg_volume
        self.volume_anomaly_ratio = (current_volume / avg_volume) if avg_volume > 0 else 1.0
        self.price_change = price_change
        self.vwap_deviation = vwap_deviation
        self.prev_volume_anomaly_ratio = prev_volume_anomaly_ratio
        self.prev_recommended_weight = prev_recommended_weight
        self.hft_p_input = hft_p_input
        self.hft_mass = hft_mass
        self.hft_v0 = hft_v0
        self.is_index_etf = is_index_etf
        self.contrarian_override = contrarian_override
        self.enable_hft_calc = enable_hft_calc


class VolumeAdvisory:
    """Advisory postures for Intraday Volume analysis."""
    def __init__(self):
        self.recommended_weight = 1.0
        self.risk_score = 0.0
        self.oversold_score = 0.0
        self.hft_delta_v = 0.0
        self.risk_elevated = False
        self.growth_favorable = True
        self.oversold_state = False
        self.contrarian_buy_signal = False
        self.hft_active = False


class IntradayVolumeAdvisory(BaseOperator):
    """Deterministic Operator assessing intraday volume anomalies, momentum, and VWAP deviation."""

    def validate(self, input_data: dict) -> dict:
        """Validates input parameters for stock index volume assessment."""
        required_fields = ["current_volume", "avg_volume", "price_change", "vwap_deviation"]
        for field in required_fields:
            if field not in input_data:
                raise ValueError(f"Missing required volume metric field: {field}")
            try:
                float(input_data[field])
            except (TypeError, ValueError) as e:
                raise TypeError(f"Field '{field}' must be a float-compatible numeric value: {e}")
        return input_data

    def preprocess(self, input_data: dict) -> dict:
        """Preprocesses input volume data and sets bounds."""
        current_vol = max(0.0, float(input_data["current_volume"]))
        avg_vol = max(1.0, float(input_data["avg_volume"])) # Prevent division by zero
        price_change = float(input_data["price_change"])
        vwap_deviation = float(input_data["vwap_deviation"])
        prev_vol_anomaly = float(input_data.get("prev_volume_anomaly_ratio", 0.0))
        prev_weight = float(input_data.get("prev_recommended_weight", -1.0))

        symbol = str(input_data.get("symbol", "")).upper()
        is_index_etf = bool(input_data.get("is_index_etf", symbol in ("SPY", "QQQ")))
        contrarian_override = int(input_data.get("contrarian_override", 0))

        processed = {
            "current_volume": current_vol,
            "avg_volume": avg_vol,
            "price_change": price_change,
            "vwap_deviation": vwap_deviation,
            "prev_volume_anomaly_ratio": prev_vol_anomaly,
            "prev_recommended_weight": prev_weight,
            "hft_p_input": float(input_data.get("hft_p_input", 0.0)),
            "hft_mass": float(input_data.get("hft_mass", 1.0)),
            "hft_v0": float(input_data.get("hft_v0", 0.0)),
            "is_index_etf": is_index_etf,
            "contrarian_override": contrarian_override,
            "enable_hft_calc": bool(input_data.get("enable_hft_calc", False) or input_data.get("enable_hft", False))
        }

        if "stabilizer_factor" in input_data:
            processed["stabilizer_factor"] = float(input_data["stabilizer_factor"])
        if "stabilizer_risk_elevated" in input_data:
            processed["stabilizer_risk_elevated"] = bool(input_data["stabilizer_risk_elevated"])
        if "stabilizer_risk_score" in input_data:
            processed["stabilizer_risk_score"] = float(input_data["stabilizer_risk_score"])
        if "enable_contrarian_oversold" in input_data:
            processed["enable_contrarian_oversold"] = bool(input_data["enable_contrarian_oversold"])
        if "contrarian_oversold_aggressiveness" in input_data:
            processed["contrarian_oversold_aggressiveness"] = float(input_data["contrarian_oversold_aggressiveness"])
        if "trust_score" in input_data:
            processed["trust_score"] = float(input_data["trust_score"])
        if "manipulation_score" in input_data:
            processed["manipulation_score"] = float(input_data["manipulation_score"])
        if "drawdown_state" in input_data:
            processed["drawdown_state"] = input_data["drawdown_state"]
        if "hft_bias_config" in input_data:
            processed["hft_bias_config"] = input_data["hft_bias_config"]

        return processed

    def execute(self, input_data: dict) -> dict:
        """Evaluates volume anomalies, VWAP deviations, and trends to output deterministic advice."""
        kill_switch = False
        hardware_fault = False
        if self.context:
            metadata = getattr(self.context, "metadata", {})
            kill_switch = metadata.get("kill_switch", False)
            hardware_fault = metadata.get("hardware_fault", False)

        advisory = VolumeAdvisory()

        if kill_switch or hardware_fault:
            advisory.recommended_weight = 0.0
            advisory.risk_score = 100.0
            advisory.risk_elevated = True
            advisory.growth_favorable = False
            advisory.oversold_score = 0.0
            advisory.oversold_state = False
            advisory.contrarian_buy_signal = False
            return {
                "recommended_weight": advisory.recommended_weight,
                "risk_score": advisory.risk_score,
                "oversold_score": advisory.oversold_score,
                "risk_elevated": advisory.risk_elevated,
                "growth_favorable": advisory.growth_favorable,
                "oversold_state": advisory.oversold_state,
                "contrarian_buy_signal": advisory.contrarian_buy_signal
            }

        state = VolumeState(
            current_volume=input_data["current_volume"],
            avg_volume=input_data["avg_volume"],
            price_change=input_data["price_change"],
            vwap_deviation=input_data["vwap_deviation"],
            prev_volume_anomaly_ratio=input_data.get("prev_volume_anomaly_ratio", 0.0),
            prev_recommended_weight=input_data.get("prev_recommended_weight", -1.0),
            hft_p_input=input_data.get("hft_p_input", 0.0),
            hft_mass=input_data.get("hft_mass", 1.0),
            hft_v0=input_data.get("hft_v0", 0.0),
            is_index_etf=input_data.get("is_index_etf", False),
            contrarian_override=input_data.get("contrarian_override", 0),
            enable_hft_calc=input_data.get("enable_hft_calc", False) or input_data.get("enable_hft", False)
        )

        enable_contrarian = False
        aggressiveness = 1.0
        if self.config:
            enable_contrarian = getattr(self.config, "enable_contrarian_oversold", False)
            aggressiveness = getattr(self.config, "contrarian_oversold_aggressiveness", 1.0)
        elif self.context and getattr(self.context, "config", None):
            cfg = self.context.config
            enable_contrarian = getattr(cfg, "enable_contrarian_oversold", False)
            aggressiveness = getattr(cfg, "contrarian_oversold_aggressiveness", 1.0)

        enable_contrarian = bool(input_data.get("enable_contrarian_oversold", enable_contrarian))
        aggressiveness = float(input_data.get("contrarian_oversold_aggressiveness", aggressiveness))

        # Exponential smoothing on anomaly ratio
        smoothed_ratio = state.volume_anomaly_ratio
        if state.prev_volume_anomaly_ratio > 0.0:
            smoothed_ratio = 0.2 * state.volume_anomaly_ratio + 0.8 * state.prev_volume_anomaly_ratio

        # Compute Multi-Factor Oversold Score
        norm_price = max(0.0, (-state.price_change - 0.007) / 0.015)
        norm_vwap = max(0.0, (-state.vwap_deviation - 0.005) / 0.015)
        norm_vol = max(0.0, (smoothed_ratio - 1.5) / 2.0)

        advisory.oversold_score = (0.4 * norm_price + 0.3 * norm_vwap + 0.3 * norm_vol) * aggressiveness

        # Retrieve hft_bias config early for contrarian modulation
        hft_bias_cfg = input_data.get("hft_bias_config")
        if hft_bias_cfg is None:
            if self.config and hasattr(self.config, "hft_bias"):
                hft_bias_cfg = getattr(self.config, "hft_bias")
            elif self.context and getattr(self.context, "config", None) and hasattr(self.context.config, "hft_bias"):
                hft_bias_cfg = getattr(self.context.config, "hft_bias")

        if isinstance(hft_bias_cfg, dict):
            bias_dict = hft_bias_cfg
        elif hft_bias_cfg is not None and hasattr(hft_bias_cfg, "__dict__"):
            bias_dict = vars(hft_bias_cfg)
        else:
            bias_dict = {
                "enabled": True,
                "bullishness_mode": "STANDARD",
                "bullish_multiplier_price": 1.05,
                "bullish_multiplier_volume": 1.05,
                "bullish_execution_scale": 1.10,
                "bullish_sell_ceiling_factor": 0.80,
                "trust_threshold_bullish": 0.70,
                "manipulation_threshold": 0.30,
                "contrarian_oversold_weight_mult": 1.25,
                "contrarian_oversold_threshold": 0.65,
                "contrarian_hf_impulse_scale": 1.25,
                "contrarian_sell_ceiling_factor": 0.85,
            }

        mode = str(bias_dict.get("bullishness_mode", "STANDARD")).upper()
        if mode in ("CONTRARIAN", "HYPER"):
            enable_contrarian = True

        oversold_thresh = 0.6 if state.is_index_etf else 1.0
        if mode in ("CONTRARIAN", "HYPER"):
            oversold_thresh = float(bias_dict.get("contrarian_oversold_threshold", 0.65))

        # Conditions A and B
        cond_a = (state.price_change <= -0.012) and (state.vwap_deviation <= -0.008) and (smoothed_ratio >= 2.5)
        cond_b = (state.price_change <= -0.007) and (state.vwap_deviation <= -0.005) and (smoothed_ratio >= 1.8)

        advisory.oversold_state = (advisory.oversold_score >= oversold_thresh) or cond_a or (state.is_index_etf and cond_b)

        # Determine effective contrarian active
        contrarian_active = enable_contrarian
        if state.contrarian_override == 1:
            contrarian_active = True
        elif state.contrarian_override == -1:
            contrarian_active = False

        advisory.contrarian_buy_signal = contrarian_active and advisory.oversold_state

        vol_risk = smoothed_ratio * 15.0
        price_risk = abs(state.price_change) * 200.0 if state.price_change < 0.0 else 0.0
        vwap_risk = abs(state.vwap_deviation) * 150.0

        raw_risk = vol_risk + price_risk + vwap_risk
        advisory.risk_score = max(0.0, min(100.0, raw_risk))

        advisory.risk_elevated = (advisory.risk_score > 60.0) or (smoothed_ratio > 4.0 and state.price_change < -0.01)
        advisory.growth_favorable = (not advisory.risk_elevated) and (smoothed_ratio > 1.2) and (state.price_change > 0.0)

        advisory.recommended_weight = max(0.0, min(1.0, 1.0 - (advisory.risk_score / 100.0)))

        # Apply Contrarian Weight Multiplier if active
        if advisory.contrarian_buy_signal:
            c_mult = float(bias_dict.get("contrarian_oversold_weight_mult", 1.25)) if mode in ("CONTRARIAN", "HYPER") else 1.15
            if advisory.oversold_score >= 0.9 or cond_a:
                c_mult += 0.05
            advisory.recommended_weight *= c_mult

        # Market Stabilizer (MSGAM) Coupling
        stabilizer_factor = input_data.get("stabilizer_factor", 1.0)
        stabilizer_risk_elevated = input_data.get("stabilizer_risk_elevated", False)
        stabilizer_risk_score = input_data.get("stabilizer_risk_score", 0.0)

        advisory.recommended_weight *= stabilizer_factor
        if stabilizer_risk_elevated:
            advisory.risk_elevated = True
            advisory.growth_favorable = False
            advisory.risk_score = max(advisory.risk_score, stabilizer_risk_score)

        advisory.recommended_weight = max(0.0, min(1.0, advisory.recommended_weight))

        # High-Frequency Trading (HFT) AILEE MATH Delta-V Impulse Integration
        if state.enable_hft_calc:
            advisory.hft_active = True
            p_input = state.hft_p_input if state.hft_p_input != 0.0 else state.price_change * 10.0
            mass_input = state.hft_mass

            # Retrieve hft_bias config
            hft_bias_cfg = input_data.get("hft_bias_config")
            if hft_bias_cfg is None:
                if self.config and hasattr(self.config, "hft_bias"):
                    hft_bias_cfg = getattr(self.config, "hft_bias")
                elif self.context and getattr(self.context, "config", None) and hasattr(self.context.config, "hft_bias"):
                    hft_bias_cfg = getattr(self.context.config, "hft_bias")
            if hft_bias_cfg is None:
                hft_bias_cfg = {
                    "enabled": True,
                    "bullish_multiplier_price": 1.05,
                    "bullish_multiplier_volume": 1.05,
                    "bullish_execution_scale": 1.10,
                    "bullish_sell_ceiling_factor": 0.80,
                    "trust_threshold_bullish": 0.70,
                    "manipulation_threshold": 0.30,
                }

            trust_score = float(input_data.get("trust_score", 0.85))
            manipulation_score = float(input_data.get("manipulation_score", 0.0))
            drawdown_state = input_data.get("drawdown_state", False)

            bullish_active = is_bullish_mode_allowed(
                trust_score=trust_score,
                manipulation_score=manipulation_score,
                drawdown_state=drawdown_state,
                hft_bias_config=hft_bias_cfg
            )

            if bullish_active and hft_bias_cfg:
                p_mult = float(hft_bias_cfg.get("bullish_multiplier_price", 1.05))
                v_mult = float(hft_bias_cfg.get("bullish_multiplier_volume", 1.05))
                p_input *= p_mult
                mass_input *= v_mult

            v_flow = (state.current_volume / state.avg_volume) if state.avg_volume > 0 else 1.0
            w_risk = advisory.risk_score / 100.0

            tick_dict = {
                "p_input": p_input,
                "w": w_risk,
                "v": v_flow,
                "M": mass_input,
                "dt": 0.001
            }

            advisory.hft_delta_v = calculate_hft_delta_v(
                isp=1.0,
                efficiency=0.95,
                alpha=0.1,
                v0=state.hft_v0,
                ticks=[tick_dict],
                min_mass_floor=1e-6
            )

            impulse_factor = 1.0 + max(-0.5, min(0.5, advisory.hft_delta_v))
            advisory.recommended_weight = max(0.0, min(1.0, advisory.recommended_weight * impulse_factor))

            # Post-Δv Bullish Execution Weight Scaling
            if bullish_active and hft_bias_cfg:
                exec_scale = float(hft_bias_cfg.get("bullish_execution_scale", 1.10))
                # Apply only if trend is upward (price_change > 0 or hft_delta_v > 0) and volume supports the move
                upward_trend = (state.price_change > 0.0 or advisory.hft_delta_v > 0.0)
                volume_supports = (state.volume_anomaly_ratio >= 1.0)
                if upward_trend and volume_supports:
                    advisory.recommended_weight = max(0.0, min(1.0, advisory.recommended_weight * exec_scale))

        # Temporal Step Clamping (Drift Control)
        if 0.0 <= state.prev_recommended_weight <= 1.0:
            diff = advisory.recommended_weight - state.prev_recommended_weight
            if diff > 0.15:
                advisory.recommended_weight = state.prev_recommended_weight + 0.15
            elif diff < -0.15:
                advisory.recommended_weight = state.prev_recommended_weight - 0.15

        advisory.recommended_weight = max(0.0, min(1.0, advisory.recommended_weight))

        return {
            "recommended_weight": advisory.recommended_weight,
            "risk_score": advisory.risk_score,
            "oversold_score": advisory.oversold_score,
            "hft_delta_v": advisory.hft_delta_v,
            "risk_elevated": advisory.risk_elevated,
            "growth_favorable": advisory.growth_favorable,
            "oversold_state": advisory.oversold_state,
            "contrarian_buy_signal": advisory.contrarian_buy_signal,
            "hft_active": advisory.hft_active
        }

    def postprocess(self, result_data: dict) -> dict:
        """Postprocesses the execution result for output formats."""
        return result_data

    def finalize(self, result_data: dict) -> dict:
        """Finalizes trace steps or outputs."""
        return result_data


def evaluate_oversold(
    current_price: float,
    current_volume: float,
    avg_volume: float,
    mode: str = "STANDARD",
    fib: Optional[Any] = None
) -> Dict[str, Any]:
    """Evaluates oversold conditions modulated by Fibonacci advisories."""
    vol_ratio = (current_volume / avg_volume) if avg_volume > 0.0 else 1.0
    mode_str = str(mode).upper()
    base_thresh = 0.75
    if mode_str == "CONTRARIAN":
        base_thresh = 0.65
    elif mode_str == "CONSERVATIVE":
        base_thresh = 0.80
    elif mode_str == "HYPER":
        base_thresh = 0.70

    weight = 0.5
    if vol_ratio >= 1.2:
        weight = 0.7

    fib_buy = getattr(fib, 'fib_buy_signal', False) if fib else False
    fib_active = getattr(fib, 'fib_zone_active', False) if fib else False
    contrarian_fib = getattr(fib, 'contrarian_fib_buy_zone', False) if fib else False
    fib_sell = getattr(fib, 'fib_sell_signal', False) if fib else False
    hyper_breakout = getattr(fib, 'hyper_fib_breakout', False) if fib else False

    if fib_active and fib_buy:
        weight *= 1.05
        base_thresh -= 0.02

    if contrarian_fib and mode_str == "CONTRARIAN":
        weight *= 1.10
        base_thresh -= 0.03

    if fib_sell or hyper_breakout:
        weight *= 0.95

    oversold_score = min(1.0, vol_ratio / 2.0)
    oversold_state = (oversold_score >= base_thresh) or fib_buy or contrarian_fib
    contrarian_buy_signal = contrarian_fib or (mode_str == "CONTRARIAN" and oversold_state)
    recommended_weight = max(0.0, min(1.0, weight))

    return {
        "oversold_state": oversold_state,
        "contrarian_buy_signal": contrarian_buy_signal,
        "oversold_score": oversold_score,
        "recommended_weight": recommended_weight,
        "buy_threshold": base_thresh
    }
