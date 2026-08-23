# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Deterministic Real-Time Anomaly Detection & Market Condition Operator (Layer 16).

Monitors live market conditions for volatility expansion, liquidity displacement,
and correlation breaks. Emits non-directive, cautionary advisories strictly free
from trading advice or allegations of wrongdoing.
"""

import math
from typing import List, Dict, Any, Optional, Tuple

from core.finance_kernel.kernel_context import FinanceKernelContext
from core.finance_kernel.kernel_config import FinanceKernelConfig
from core.finance_kernel.kernel_registry import BaseOperator


DEFAULT_UNIVERSE = ["SPY", "QQQ", "DIA", "XLK", "XLF", "XLE", "AAPL", "MSFT", "GOOGL", "AMZN"]

DEFAULT_CORRELATION_PAIRS = {
    "SPY": "QQQ",
    "QQQ": "SPY",
    "DIA": "SPY",
    "XLK": "SPY",
    "XLF": "SPY",
    "XLE": "SPY",
    "AAPL": "XLK",
    "MSFT": "XLK",
    "GOOGL": "XLK",
    "AMZN": "XLK"
}


class AnomalyState:
    """Snapshot of market condition and rolling metrics."""
    def __init__(self, last_price: float = 0.0, volume: float = 0.0,
                 bid_size: float = 0.0, ask_size: float = 0.0,
                 ewma_volatility: float = 0.0, baseline_volatility: float = 0.0,
                 baseline_depth: float = 0.0, pair_last_price: float = 0.0,
                 rolling_correlation: float = 1.0, expected_correlation: float = 1.0,
                 vol_debounce_count: int = 0, liq_debounce_count: int = 0,
                 corr_debounce_count: int = 0, symbol: str = "SPY",
                 pair_symbol: str = "QQQ"):
        self.last_price = last_price
        self.volume = volume
        self.bid_size = bid_size
        self.ask_size = ask_size
        self.ewma_volatility = ewma_volatility
        self.baseline_volatility = baseline_volatility
        self.baseline_depth = baseline_depth
        self.pair_last_price = pair_last_price
        self.rolling_correlation = rolling_correlation
        self.expected_correlation = expected_correlation
        self.vol_debounce_count = vol_debounce_count
        self.liq_debounce_count = liq_debounce_count
        self.corr_debounce_count = corr_debounce_count
        self.symbol = symbol.upper()
        self.pair_symbol = pair_symbol.upper()


class AnomalyAdvisory:
    """Evaluated non-directive advisory state and human-readable messages."""
    def __init__(self):
        self.volatility_expansion_ratio = 1.0
        self.depth_thinning_pct = 0.0
        self.rolling_correlation = 1.0
        self.anomaly_severity = 0.0
        self.volatility_anomaly = False
        self.liquidity_anomaly = False
        self.correlation_break = False
        self.advisory_active = False
        self.messages: List[str] = []


class AnomalyDetectionOperator(BaseOperator):
    """Deterministic Operator assessing real-time market anomalies and generating caution advisories."""

    def validate(self, input_data: dict) -> dict:
        """Validates input market condition metrics."""
        required_fields = ["last_price", "ewma_volatility", "baseline_volatility"]
        for field in required_fields:
            if field not in input_data:
                raise ValueError(f"Missing required anomaly metric field: {field}")
            try:
                float(input_data[field])
            except (TypeError, ValueError) as e:
                raise TypeError(f"Field '{field}' must be a float-compatible numeric value: {e}")
        return input_data

    def preprocess(self, input_data: dict) -> dict:
        """Preprocesses inputs, sets defaults, and applies safety bounds."""
        symbol = str(input_data.get("symbol", "SPY")).upper()
        pair_symbol = str(input_data.get("pair_symbol", DEFAULT_CORRELATION_PAIRS.get(symbol, "QQQ"))).upper()

        processed = {
            "symbol": symbol,
            "pair_symbol": pair_symbol,
            "last_price": max(0.0001, float(input_data["last_price"])),
            "volume": max(0.0, float(input_data.get("volume", 0.0))),
            "bid_size": max(0.0, float(input_data.get("bid_size", 0.0))),
            "ask_size": max(0.0, float(input_data.get("ask_size", 0.0))),
            "ewma_volatility": max(0.0, float(input_data["ewma_volatility"])),
            "baseline_volatility": max(1e-6, float(input_data["baseline_volatility"])),
            "baseline_depth": max(0.0, float(input_data.get("baseline_depth", 0.0))),
            "pair_last_price": max(0.0, float(input_data.get("pair_last_price", 0.0))),
            "rolling_correlation": float(input_data.get("rolling_correlation", 1.0)),
            "expected_correlation": float(input_data.get("expected_correlation", 1.0)),
            "vol_debounce_count": int(input_data.get("vol_debounce_count", 0)),
            "liq_debounce_count": int(input_data.get("liq_debounce_count", 0)),
            "corr_debounce_count": int(input_data.get("corr_debounce_count", 0)),
            "volatility_threshold": float(input_data.get("volatility_threshold", 2.5)),
            "depth_thinning_threshold": float(input_data.get("depth_thinning_threshold", 0.50)),
            "min_expected_correlation": float(input_data.get("min_expected_correlation", 0.40)),
            "vol_debounce_target": int(input_data.get("vol_debounce_target", 3)),
            "liq_debounce_target": int(input_data.get("liq_debounce_target", 3)),
            "corr_debounce_target": int(input_data.get("corr_debounce_target", 5)),
        }
        return processed

    def execute(self, input_data: dict) -> dict:
        """Evaluates volatility expansion, liquidity displacement, and correlation breaks."""
        kill_switch = False
        hardware_fault = False
        if self.context:
            metadata = getattr(self.context, "metadata", {})
            kill_switch = metadata.get("kill_switch", False)
            hardware_fault = metadata.get("hardware_fault", False)

        advisory = AnomalyAdvisory()

        if kill_switch or hardware_fault:
            advisory.anomaly_severity = 100.0
            advisory.advisory_active = False
            return {
                "advisory_active": False,
                "anomaly_severity": 100.0,
                "volatility_anomaly": False,
                "liquidity_anomaly": False,
                "correlation_break": False,
                "volatility_expansion_ratio": 0.0,
                "depth_thinning_pct": 0.0,
                "rolling_correlation": 1.0,
                "messages": ["System halted: Hardware fault or kill switch active."]
            }

        state = AnomalyState(
            last_price=input_data["last_price"],
            volume=input_data["volume"],
            bid_size=input_data["bid_size"],
            ask_size=input_data["ask_size"],
            ewma_volatility=input_data["ewma_volatility"],
            baseline_volatility=input_data["baseline_volatility"],
            baseline_depth=input_data["baseline_depth"],
            pair_last_price=input_data["pair_last_price"],
            rolling_correlation=input_data["rolling_correlation"],
            expected_correlation=input_data["expected_correlation"],
            vol_debounce_count=input_data["vol_debounce_count"],
            liq_debounce_count=input_data["liq_debounce_count"],
            corr_debounce_count=input_data["corr_debounce_count"],
            symbol=input_data["symbol"],
            pair_symbol=input_data["pair_symbol"]
        )

        vol_thresh = input_data["volatility_threshold"]
        depth_thresh = input_data["depth_thinning_threshold"]
        corr_thresh = input_data["min_expected_correlation"]
        vol_target = input_data["vol_debounce_target"]
        liq_target = input_data["liq_debounce_target"]
        corr_target = input_data["corr_debounce_target"]

        # 1. Volatility Expansion Calculation
        advisory.volatility_expansion_ratio = state.ewma_volatility / state.baseline_volatility
        raw_vol_anomaly = (advisory.volatility_expansion_ratio >= vol_thresh)
        if state.vol_debounce_count >= vol_target and raw_vol_anomaly:
            advisory.volatility_anomaly = True
            msg = f"Caution: abnormal volatility detected in {state.symbol} ({advisory.volatility_expansion_ratio:.1f}x baseline)."
            advisory.messages.append(msg)

        # 2. Liquidity Displacement / Order Book Thinning Calculation
        current_depth = state.bid_size + state.ask_size
        if state.baseline_depth > 1e-6:
            depth_ratio = current_depth / state.baseline_depth
            advisory.depth_thinning_pct = max(0.0, 1.0 - depth_ratio) if depth_ratio < 1.0 else 0.0
        else:
            advisory.depth_thinning_pct = 0.0

        raw_liq_anomaly = (advisory.depth_thinning_pct >= depth_thresh)
        if state.liq_debounce_count >= liq_target and raw_liq_anomaly:
            advisory.liquidity_anomaly = True
            msg = f"Caution: liquidity displacement detected in {state.symbol} ({advisory.depth_thinning_pct*100.0:.0f}% order book depth thinning)."
            advisory.messages.append(msg)

        # 3. Pair Correlation Break Calculation
        advisory.rolling_correlation = state.rolling_correlation
        corr_drop = state.expected_correlation - state.rolling_correlation
        raw_corr_anomaly = (corr_drop >= corr_thresh)
        if state.corr_debounce_count >= corr_target and raw_corr_anomaly:
            advisory.correlation_break = True
            msg = f"Caution: correlation break observed between {state.symbol} and {state.pair_symbol} (rolling correlation dropped from {state.expected_correlation:.2f} to {state.rolling_correlation:.2f})."
            advisory.messages.append(msg)

        # 4. Severity & Active Flag
        vol_score = max(0.0, min(40.0, (advisory.volatility_expansion_ratio - 1.0) * 20.0))
        liq_score = advisory.depth_thinning_pct * 30.0
        corr_score = max(0.0, min(30.0, corr_drop * 30.0))

        advisory.anomaly_severity = max(0.0, min(100.0, vol_score + liq_score + corr_score))
        advisory.advisory_active = advisory.volatility_anomaly or advisory.liquidity_anomaly or advisory.correlation_break

        return {
            "symbol": state.symbol,
            "pair_symbol": state.pair_symbol,
            "advisory_active": advisory.advisory_active,
            "anomaly_severity": advisory.anomaly_severity,
            "volatility_anomaly": advisory.volatility_anomaly,
            "liquidity_anomaly": advisory.liquidity_anomaly,
            "correlation_break": advisory.correlation_break,
            "volatility_expansion_ratio": advisory.volatility_expansion_ratio,
            "depth_thinning_pct": advisory.depth_thinning_pct,
            "rolling_correlation": advisory.rolling_correlation,
            "messages": advisory.messages
        }

    def postprocess(self, result_data: dict) -> dict:
        """Postprocesses the result for output formats."""
        return result_data

    def finalize(self, result_data: dict) -> dict:
        """Finalizes trace steps or outputs."""
        return result_data
