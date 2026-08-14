# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Deterministic Intraday Volume Advisory Module for SPY and QQQ.

Evaluates intraday volume, anomalies relative to baseline, price changes,
and VWAP deviations to output structured risk and growth scores.
"""

from core.finance_kernel.kernel_context import FinanceKernelContext
from core.finance_kernel.kernel_config import FinanceKernelConfig
from core.finance_kernel.kernel_registry import BaseOperator

class VolumeState:
    """Intraday volume metrics snapshot."""
    def __init__(self, current_volume: float = 0.0, avg_volume: float = 0.0,
                 price_change: float = 0.0, vwap_deviation: float = 0.0,
                 prev_volume_anomaly_ratio: float = 0.0, prev_recommended_weight: float = -1.0):
        self.current_volume = current_volume
        self.avg_volume = avg_volume
        self.volume_anomaly_ratio = (current_volume / avg_volume) if avg_volume > 0 else 1.0
        self.price_change = price_change
        self.vwap_deviation = vwap_deviation
        self.prev_volume_anomaly_ratio = prev_volume_anomaly_ratio
        self.prev_recommended_weight = prev_recommended_weight


class VolumeAdvisory:
    """Advisory postures for Intraday Volume analysis."""
    def __init__(self):
        self.recommended_weight = 1.0
        self.risk_score = 0.0
        self.risk_elevated = False
        self.growth_favorable = True


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

        processed = {
            "current_volume": current_vol,
            "avg_volume": avg_vol,
            "price_change": price_change,
            "vwap_deviation": vwap_deviation,
            "prev_volume_anomaly_ratio": prev_vol_anomaly,
            "prev_recommended_weight": prev_weight
        }

        if "stabilizer_factor" in input_data:
            processed["stabilizer_factor"] = float(input_data["stabilizer_factor"])
        if "stabilizer_risk_elevated" in input_data:
            processed["stabilizer_risk_elevated"] = bool(input_data["stabilizer_risk_elevated"])
        if "stabilizer_risk_score" in input_data:
            processed["stabilizer_risk_score"] = float(input_data["stabilizer_risk_score"])

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
            return {
                "recommended_weight": advisory.recommended_weight,
                "risk_score": advisory.risk_score,
                "risk_elevated": advisory.risk_elevated,
                "growth_favorable": advisory.growth_favorable
            }

        state = VolumeState(
            current_volume=input_data["current_volume"],
            avg_volume=input_data["avg_volume"],
            price_change=input_data["price_change"],
            vwap_deviation=input_data["vwap_deviation"],
            prev_volume_anomaly_ratio=input_data.get("prev_volume_anomaly_ratio", 0.0),
            prev_recommended_weight=input_data.get("prev_recommended_weight", -1.0)
        )

        # Exponential smoothing on anomaly ratio
        smoothed_ratio = state.volume_anomaly_ratio
        if state.prev_volume_anomaly_ratio > 0.0:
            smoothed_ratio = 0.2 * state.volume_anomaly_ratio + 0.8 * state.prev_volume_anomaly_ratio

        vol_risk = smoothed_ratio * 15.0
        price_risk = abs(state.price_change) * 200.0 if state.price_change < 0.0 else 0.0
        vwap_risk = abs(state.vwap_deviation) * 150.0

        raw_risk = vol_risk + price_risk + vwap_risk
        advisory.risk_score = max(0.0, min(100.0, raw_risk))

        advisory.risk_elevated = (advisory.risk_score > 60.0) or (smoothed_ratio > 4.0 and state.price_change < -0.01)
        advisory.growth_favorable = (not advisory.risk_elevated) and (smoothed_ratio > 1.2) and (state.price_change > 0.0)

        advisory.recommended_weight = max(0.0, min(1.0, 1.0 - (advisory.risk_score / 100.0)))

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
            "risk_elevated": advisory.risk_elevated,
            "growth_favorable": advisory.growth_favorable
        }

    def postprocess(self, result_data: dict) -> dict:
        """Postprocesses the execution result for output formats."""
        return result_data

    def finalize(self, result_data: dict) -> dict:
        """Finalizes trace steps or outputs."""
        return result_data
