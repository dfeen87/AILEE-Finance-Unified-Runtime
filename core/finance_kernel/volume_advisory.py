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
                 price_change: float = 0.0, vwap_deviation: float = 0.0):
        self.current_volume = current_volume
        self.avg_volume = avg_volume
        self.volume_anomaly_ratio = (current_volume / avg_volume) if avg_volume > 0 else 1.0
        self.price_change = price_change
        self.vwap_deviation = vwap_deviation


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
        # Force values to valid bounds
        current_vol = max(0.0, float(input_data["current_volume"]))
        avg_vol = max(1.0, float(input_data["avg_volume"])) # Prevent division by zero
        price_change = float(input_data["price_change"])
        vwap_deviation = float(input_data["vwap_deviation"])

        return {
            "current_volume": current_vol,
            "avg_volume": avg_vol,
            "price_change": price_change,
            "vwap_deviation": vwap_deviation
        }

    def execute(self, input_data: dict) -> dict:
        """Evaluates volume anomalies, VWAP deviations, and trends to output deterministic advice."""
        # If kill switch or hardware fault is active, enforce fallback
        # In a real pipeline, these could be set on context or config
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
            vwap_deviation=input_data["vwap_deviation"]
        )

        vol_risk = state.volume_anomaly_ratio * 15.0
        price_risk = abs(state.price_change) * 200.0 if state.price_change < 0.0 else 0.0
        vwap_risk = abs(state.vwap_deviation) * 150.0

        raw_risk = vol_risk + price_risk + vwap_risk
        advisory.risk_score = max(0.0, min(100.0, raw_risk))

        advisory.risk_elevated = (advisory.risk_score > 60.0) or (state.volume_anomaly_ratio > 4.0 and state.price_change < -0.01)
        advisory.growth_favorable = (not advisory.risk_elevated) and (state.volume_anomaly_ratio > 1.2) and (state.price_change > 0.0)

        advisory.recommended_weight = max(0.0, min(1.0, 1.0 - (advisory.risk_score / 100.0)))

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
