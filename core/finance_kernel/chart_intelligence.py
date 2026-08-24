# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Deterministic Real-Time Chart Intelligence & Environment Diagnostics Operator (Layer 17).

Generates environment-driven technical indicators derived from anomaly intelligence,
volatility tracking, liquidity displacement metrics, and correlation divergence analysis.
These indicators are strictly diagnostic, not predictive, and imply no trading intent.
"""

import math
import json
from enum import IntEnum
from typing import List, Dict, Any, Optional

from core.finance_kernel.kernel_context import FinanceKernelContext
from core.finance_kernel.kernel_config import FinanceKernelConfig
from core.finance_kernel.kernel_registry import BaseOperator


class ChartIndicatorType(IntEnum):
    VolatilityExpansionBands = 0
    LiquidityDisplacementZones = 1
    CorrelationDivergenceIndex = 2
    BaselineStrengthMeter = 3
    PatternEnvironmentScore = 4


class ChartConditionState(IntEnum):
    Neutral = 0
    Compression = 1
    Expansion = 2
    Displaced = 3
    Diverging = 4
    Broken = 5
    StrongSupport = 6
    Consolidation = 7
    Stressed = 8
    Weak = 9
    Average = 10
    Strong = 11


class PatternHint(IntEnum):
    NoneHint = 0
    CupHandleLike = 1
    PennantLike = 2
    FlagLike = 3


INDICATOR_TYPE_MAP = {
    ChartIndicatorType.VolatilityExpansionBands: "VolatilityExpansionBands",
    ChartIndicatorType.LiquidityDisplacementZones: "LiquidityDisplacementZones",
    ChartIndicatorType.CorrelationDivergenceIndex: "CorrelationDivergenceIndex",
    ChartIndicatorType.BaselineStrengthMeter: "BaselineStrengthMeter",
    ChartIndicatorType.PatternEnvironmentScore: "PatternEnvironmentScore",
}

CONDITION_STATE_MAP = {
    ChartConditionState.Neutral: "Neutral",
    ChartConditionState.Compression: "Compression",
    ChartConditionState.Expansion: "Expansion",
    ChartConditionState.Displaced: "Displaced",
    ChartConditionState.Diverging: "Diverging",
    ChartConditionState.Broken: "Broken",
    ChartConditionState.StrongSupport: "StrongSupport",
    ChartConditionState.Consolidation: "Consolidation",
    ChartConditionState.Stressed: "Stressed",
    ChartConditionState.Weak: "Weak",
    ChartConditionState.Average: "Average",
    ChartConditionState.Strong: "Strong",
}

PATTERN_HINT_MAP = {
    PatternHint.NoneHint: "None",
    PatternHint.CupHandleLike: "CupHandleLike",
    PatternHint.PennantLike: "PennantLike",
    PatternHint.FlagLike: "FlagLike",
}


class BaselineState:
    """Multi-horizon baselines (5m, 1h, 30d)."""
    def __init__(self, vol_5m: float = 0.0, vol_1h: float = 0.0, vol_30d: float = 0.01,
                 liq_5m: float = 0.0, liq_1h: float = 0.0, liq_30d: float = 1000.0,
                 vol_corr_5m: float = 1.0, vol_corr_1h: float = 1.0, vol_corr_30d: float = 1.0):
        self.vol_5m = vol_5m
        self.vol_1h = vol_1h
        self.vol_30d = vol_30d
        self.liq_5m = liq_5m
        self.liq_1h = liq_1h
        self.liq_30d = liq_30d
        self.vol_corr_5m = vol_corr_5m
        self.vol_corr_1h = vol_corr_1h
        self.vol_corr_30d = vol_corr_30d


class ChartConditionPayload:
    """Diagnostic chart overlay condition payload."""
    def __init__(self, indicator_type: ChartIndicatorType, condition_state: ChartConditionState,
                 normalized_score: float, raw_metrics: List[float], timestamp_ns: int = 0,
                 clamped_flags: int = 0):
        self.indicator_type = indicator_type
        self.condition_state = condition_state
        self.normalized_score = normalized_score
        self.raw_metrics = raw_metrics
        self.timestamp_ns = timestamp_ns
        self.clamped_flags = clamped_flags

    def to_dict(self) -> Dict[str, Any]:
        return {
            "indicator": INDICATOR_TYPE_MAP.get(self.indicator_type, "Unknown"),
            "state": CONDITION_STATE_MAP.get(self.condition_state, "Unknown"),
            "score": round(self.normalized_score, 2),
            "metrics": [round(m, 4) for m in self.raw_metrics],
            "timestamp_ns": self.timestamp_ns,
            "clamped_flags": self.clamped_flags
        }

    def to_json(self) -> str:
        return json.dumps(self.to_dict())


class PatternConditionPayload:
    """Pattern diagnostic resemblance payload."""
    def __init__(self, pattern_hint: PatternHint, prior_expansion_score: float,
                 consolidation_score: float, support_strength_score: float,
                 symmetry_score: float, timestamp_ns: int = 0):
        self.pattern_hint = pattern_hint
        self.prior_expansion_score = prior_expansion_score
        self.consolidation_score = consolidation_score
        self.support_strength_score = support_strength_score
        self.symmetry_score = symmetry_score
        self.timestamp_ns = timestamp_ns

    def to_dict(self) -> Dict[str, Any]:
        return {
            "pattern_hint": PATTERN_HINT_MAP.get(self.pattern_hint, "None"),
            "scores": {
                "prior_expansion": round(self.prior_expansion_score, 2),
                "consolidation": round(self.consolidation_score, 2),
                "support_strength": round(self.support_strength_score, 2),
                "symmetry": round(self.symmetry_score, 2)
            },
            "timestamp_ns": self.timestamp_ns
        }

    def to_json(self) -> str:
        return json.dumps(self.to_dict())


class ChartIntelligenceOperator(BaseOperator):
    """Deterministic Operator deriving environment technical indicators and pattern diagnostics."""

    def validate(self, input_data: dict) -> dict:
        """Validates input metrics for chart indicator calculation."""
        required = ["last_price", "ewma_volatility", "baseline_volatility"]
        for field in required:
            if field not in input_data:
                raise ValueError(f"Missing required chart intelligence metric field: {field}")
        return input_data

    def preprocess(self, input_data: dict) -> dict:
        """Sanitizes inputs and ensures multi-horizon baseline integrity."""
        def _safe_float(val, default=0.0):
            try:
                f = float(val)
                if math.isnan(f) or math.isinf(f):
                    return default
                return f
            except (TypeError, ValueError):
                return default

        symbol = str(input_data.get("symbol", "SPY")).upper()
        pair_symbol = str(input_data.get("pair_symbol", "QQQ")).upper()

        ewma_vol = max(0.0, _safe_float(input_data.get("ewma_volatility", 0.0)))
        base_vol = max(1e-6, _safe_float(input_data.get("baseline_volatility", 1e-6)))

        bid_sz = max(0.0, _safe_float(input_data.get("bid_size", 0.0)))
        ask_sz = max(0.0, _safe_float(input_data.get("ask_size", 0.0)))
        base_depth = max(0.0, _safe_float(input_data.get("baseline_depth", 0.0)))

        roll_corr = max(-1.0, min(1.0, _safe_float(input_data.get("rolling_correlation", 1.0), 1.0)))
        exp_corr = max(-1.0, min(1.0, _safe_float(input_data.get("expected_correlation", 1.0), 1.0)))

        # Baseline horizons
        vol_5m = max(0.0, _safe_float(input_data.get("vol_5m", ewma_vol)))
        vol_1h = max(0.0, _safe_float(input_data.get("vol_1h", ewma_vol)))
        vol_30d = max(1e-6, _safe_float(input_data.get("vol_30d", base_vol)))

        liq_5m = max(0.0, _safe_float(input_data.get("liq_5m", bid_sz + ask_sz)))
        liq_1h = max(0.0, _safe_float(input_data.get("liq_1h", bid_sz + ask_sz)))
        liq_30d = max(0.0, _safe_float(input_data.get("liq_30d", base_depth)))

        corr_5m = max(-1.0, min(1.0, _safe_float(input_data.get("vol_corr_5m", roll_corr))))
        corr_1h = max(-1.0, min(1.0, _safe_float(input_data.get("vol_corr_1h", roll_corr))))
        corr_30d = max(-1.0, min(1.0, _safe_float(input_data.get("vol_corr_30d", exp_corr))))

        return {
            "symbol": symbol,
            "pair_symbol": pair_symbol,
            "ewma_volatility": ewma_vol,
            "baseline_volatility": base_vol,
            "bid_size": bid_sz,
            "ask_size": ask_sz,
            "baseline_depth": base_depth,
            "rolling_correlation": roll_corr,
            "expected_correlation": exp_corr,
            "volume_anomaly_ratio": max(0.0, _safe_float(input_data.get("volume_anomaly_ratio", 1.0), 1.0)),
            "timestamp_ns": int(input_data.get("timestamp_ns", 0)),
            "baseline": BaselineState(
                vol_5m=vol_5m, vol_1h=vol_1h, vol_30d=vol_30d,
                liq_5m=liq_5m, liq_1h=liq_1h, liq_30d=liq_30d,
                vol_corr_5m=corr_5m, vol_corr_1h=corr_1h, vol_corr_30d=corr_30d
            )
        }

    def execute(self, input_data: dict) -> dict:
        """Evaluates structural chart indicators and pattern diagnostic resemblance."""
        baseline: BaselineState = input_data["baseline"]
        timestamp_ns = input_data["timestamp_ns"]

        # 1. Volatility Expansion Bands
        ewma_vol = input_data["ewma_volatility"]
        base_vol = baseline.vol_30d if baseline.vol_30d > 1e-6 else input_data["baseline_volatility"]
        vol_ratio = ewma_vol / base_vol if base_vol > 1e-6 else 1.0
        vol_score = max(0.0, min(100.0, (vol_ratio - 1.0) * 50.0 + 50.0))

        if vol_ratio >= 2.0:
            vol_state = ChartConditionState.Expansion
        elif vol_ratio <= 0.60:
            vol_state = ChartConditionState.Compression
        else:
            vol_state = ChartConditionState.Neutral

        payload_vol = ChartConditionPayload(
            indicator_type=ChartIndicatorType.VolatilityExpansionBands,
            condition_state=vol_state,
            normalized_score=vol_score,
            raw_metrics=[vol_ratio, ewma_vol, base_vol],
            timestamp_ns=timestamp_ns
        )

        # 2. Liquidity Displacement Zones
        curr_depth = input_data["bid_size"] + input_data["ask_size"]
        base_depth = baseline.liq_30d if baseline.liq_30d > 1e-6 else input_data["baseline_depth"]
        vol_accel = input_data["volume_anomaly_ratio"]

        thinning_pct = 0.0
        if base_depth > 1e-6:
            depth_ratio = curr_depth / base_depth
            if depth_ratio < 1.0:
                thinning_pct = max(0.0, min(1.0, 1.0 - depth_ratio))

        liq_score = max(0.0, min(100.0, thinning_pct * 70.0 + (max(0.0, vol_accel - 2.0) * 15.0)))
        if thinning_pct >= 0.50 or liq_score >= 60.0:
            liq_state = ChartConditionState.Displaced
        elif thinning_pct >= 0.25 or liq_score >= 35.0:
            liq_state = ChartConditionState.Stressed
        else:
            liq_state = ChartConditionState.Neutral

        payload_liq = ChartConditionPayload(
            indicator_type=ChartIndicatorType.LiquidityDisplacementZones,
            condition_state=liq_state,
            normalized_score=liq_score,
            raw_metrics=[thinning_pct, curr_depth, vol_accel],
            timestamp_ns=timestamp_ns
        )

        # 3. Correlation Divergence Index
        roll_corr = input_data["rolling_correlation"]
        exp_corr = baseline.vol_corr_30d if baseline.vol_corr_30d < 1.0 else input_data["expected_correlation"]
        corr_drop = max(0.0, exp_corr - roll_corr)
        corr_score = max(0.0, min(100.0, corr_drop * 50.0))

        if corr_drop >= 0.70 or roll_corr <= 0.0:
            corr_state = ChartConditionState.Broken
        elif corr_drop >= 0.35:
            corr_state = ChartConditionState.Diverging
        else:
            corr_state = ChartConditionState.Neutral

        payload_corr = ChartConditionPayload(
            indicator_type=ChartIndicatorType.CorrelationDivergenceIndex,
            condition_state=corr_state,
            normalized_score=corr_score,
            raw_metrics=[roll_corr, exp_corr, corr_drop],
            timestamp_ns=timestamp_ns
        )

        # 4. Baseline Strength Meter
        r_short = (baseline.vol_5m / baseline.vol_30d) if baseline.vol_30d > 1e-6 else 1.0
        r_med = (baseline.vol_1h / baseline.vol_30d) if baseline.vol_30d > 1e-6 else 1.0
        instability = (abs(r_short - 1.0) + abs(r_med - 1.0)) * 50.0
        base_score = max(0.0, min(100.0, 100.0 - instability))

        if base_score >= 70.0:
            base_state = ChartConditionState.Strong
        elif base_score >= 40.0:
            base_state = ChartConditionState.Average
        else:
            base_state = ChartConditionState.Weak

        payload_base = ChartConditionPayload(
            indicator_type=ChartIndicatorType.BaselineStrengthMeter,
            condition_state=base_state,
            normalized_score=base_score,
            raw_metrics=[r_short, r_med, baseline.vol_30d],
            timestamp_ns=timestamp_ns
        )

        # 5. Pattern Diagnostic Engine
        prior_exp = max(0.0, min(100.0, (vol_ratio - 1.0) * 60.0 if vol_ratio > 1.0 else 0.0))
        comp_term = (1.0 - vol_ratio) * 100.0 if vol_ratio <= 1.0 else 0.0
        depth_term = (1.0 - thinning_pct) * 50.0
        consol_score = max(0.0, min(100.0, comp_term * 0.5 + depth_term))
        supp_score = max(0.0, min(100.0, base_score * 0.7 + (1.0 - corr_drop) * 30.0))
        sym_score = max(0.0, min(100.0, 100.0 - (abs(vol_ratio - 1.0) * 30.0 + thinning_pct * 40.0)))

        if supp_score >= 65.0 and consol_score >= 50.0 and thinning_pct < 0.30:
            pattern_hint = PatternHint.CupHandleLike
        elif prior_exp >= 40.0 and consol_score >= 60.0 and sym_score >= 60.0:
            pattern_hint = PatternHint.PennantLike
        elif prior_exp >= 35.0 and consol_score >= 40.0 and supp_score >= 45.0:
            pattern_hint = PatternHint.FlagLike
        else:
            pattern_hint = PatternHint.NoneHint

        pattern_payload = PatternConditionPayload(
            pattern_hint=pattern_hint,
            prior_expansion_score=prior_exp,
            consolidation_score=consol_score,
            support_strength_score=supp_score,
            symmetry_score=sym_score,
            timestamp_ns=timestamp_ns
        )

        payloads = [payload_vol, payload_liq, payload_corr, payload_base]

        return {
            "symbol": input_data["symbol"],
            "pair_symbol": input_data["pair_symbol"],
            "payloads": [p.to_dict() for p in payloads],
            "pattern_diagnostics": pattern_payload.to_dict(),
            "volatility_regime": INDICATOR_TYPE_MAP[ChartIndicatorType.VolatilityExpansionBands] + ":" + CONDITION_STATE_MAP[vol_state],
            "liquidity_regime": INDICATOR_TYPE_MAP[ChartIndicatorType.LiquidityDisplacementZones] + ":" + CONDITION_STATE_MAP[liq_state],
            "correlation_regime": INDICATOR_TYPE_MAP[ChartIndicatorType.CorrelationDivergenceIndex] + ":" + CONDITION_STATE_MAP[corr_state]
        }
