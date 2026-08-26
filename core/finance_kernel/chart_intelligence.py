# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Deterministic Real-Time Chart Intelligence & Environment Diagnostics Operator (Layer 17 - V15 Expansion).

Generates environment-driven technical indicators derived from anomaly intelligence,
volatility tracking, liquidity displacement metrics, correlation divergence analysis,
allocator-free structural-stress indicators, and regime-aware diagnostics.
These indicators are strictly diagnostic, not predictive, and imply no trading intent.
"""

import math
import json
from enum import IntEnum
from typing import List, Dict, Any, Optional

from core.finance_kernel.kernel_context import FinanceKernelContext
from core.finance_kernel.kernel_config import FinanceKernelConfig
from core.finance_kernel.kernel_registry import BaseOperator
from core.finance_kernel.fibonacci import compute_retracements, compute_extensions


class ChartIndicatorType(IntEnum):
    VolatilityExpansionBands = 0
    LiquidityDisplacementZones = 1
    CorrelationDivergenceIndex = 2
    BaselineStrengthMeter = 3
    PatternEnvironmentScore = 4
    VolatilityInstability = 5
    LiquidityErosion = 6
    CorrelationBreakdown = 7
    BaselineDeterioration = 8
    StructuralFatigue = 9
    WaveNativeFinanceStream = 10


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
    # Stress State Band (V15 Grouped Expansion)
    StateStable = 12
    StateUnstable = 13
    StateChaotic = 14
    StatePreserved = 15
    StateEroding = 16
    StateDepleted = 17
    StateWeakening = 18
    StateDeteriorating = 19
    StateLowFatigue = 20
    StateMediumFatigue = 21
    StateHighFatigue = 22


class PatternHintGroup(IntEnum):
    ExpansionGroup = 0
    StressGroup = 1


class PatternHint(IntEnum):
    NoneHint = 0
    # Expansion Group
    CupHandleLike = 1
    PennantLike = 2
    FlagLike = 3
    # Stress Group (V15 Expansion)
    BreakdownLike = 4
    ExhaustionLike = 5
    StressConsolidationLike = 6


class VolatilityRegime(IntEnum):
    Low = 0
    Medium = 1
    High = 2


class LiquidityRegime(IntEnum):
    Thin = 0
    Normal = 1
    Deep = 2


class CorrelationRegime(IntEnum):
    Stable = 0
    Transitional = 1
    Unstable = 2


INDICATOR_TYPE_MAP = {
    ChartIndicatorType.VolatilityExpansionBands: "VolatilityExpansionBands",
    ChartIndicatorType.LiquidityDisplacementZones: "LiquidityDisplacementZones",
    ChartIndicatorType.CorrelationDivergenceIndex: "CorrelationDivergenceIndex",
    ChartIndicatorType.BaselineStrengthMeter: "BaselineStrengthMeter",
    ChartIndicatorType.PatternEnvironmentScore: "PatternEnvironmentScore",
    ChartIndicatorType.VolatilityInstability: "VolatilityInstability",
    ChartIndicatorType.LiquidityErosion: "LiquidityErosion",
    ChartIndicatorType.CorrelationBreakdown: "CorrelationBreakdown",
    ChartIndicatorType.BaselineDeterioration: "BaselineDeterioration",
    ChartIndicatorType.StructuralFatigue: "StructuralFatigue",
    ChartIndicatorType.WaveNativeFinanceStream: "WaveNativeFinanceStream",
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
    ChartConditionState.StateStable: "StateStable",
    ChartConditionState.StateUnstable: "StateUnstable",
    ChartConditionState.StateChaotic: "StateChaotic",
    ChartConditionState.StatePreserved: "StatePreserved",
    ChartConditionState.StateEroding: "StateEroding",
    ChartConditionState.StateDepleted: "StateDepleted",
    ChartConditionState.StateWeakening: "StateWeakening",
    ChartConditionState.StateDeteriorating: "StateDeteriorating",
    ChartConditionState.StateLowFatigue: "StateLowFatigue",
    ChartConditionState.StateMediumFatigue: "StateMediumFatigue",
    ChartConditionState.StateHighFatigue: "StateHighFatigue",
}

PATTERN_HINT_MAP = {
    PatternHint.NoneHint: "None",
    PatternHint.CupHandleLike: "CupHandleLike",
    PatternHint.PennantLike: "PennantLike",
    PatternHint.FlagLike: "FlagLike",
    PatternHint.BreakdownLike: "BreakdownLike",
    PatternHint.ExhaustionLike: "ExhaustionLike",
    PatternHint.StressConsolidationLike: "StressConsolidationLike",
}

PATTERN_HINT_GROUP_MAP = {
    PatternHintGroup.ExpansionGroup: "ExpansionGroup",
    PatternHintGroup.StressGroup: "StressGroup",
}

VOLATILITY_REGIME_MAP = {
    VolatilityRegime.Low: "Low",
    VolatilityRegime.Medium: "Medium",
    VolatilityRegime.High: "High",
}

LIQUIDITY_REGIME_MAP = {
    LiquidityRegime.Thin: "Thin",
    LiquidityRegime.Normal: "Normal",
    LiquidityRegime.Deep: "Deep",
}

CORRELATION_REGIME_MAP = {
    CorrelationRegime.Stable: "Stable",
    CorrelationRegime.Transitional: "Transitional",
    CorrelationRegime.Unstable: "Unstable",
}


class RegimeModifier:
    """Regime modifier adjusting indicator evaluation thresholds dynamically."""
    def __init__(self, volatility_regime_factor: float = 1.0, liquidity_regime_factor: float = 1.0,
                 correlation_regime_factor: float = 1.0, volatility_regime: VolatilityRegime = VolatilityRegime.Medium,
                 liquidity_regime: LiquidityRegime = LiquidityRegime.Normal,
                 correlation_regime: CorrelationRegime = CorrelationRegime.Stable):
        self.volatility_regime_factor = volatility_regime_factor
        self.liquidity_regime_factor = liquidity_regime_factor
        self.correlation_regime_factor = correlation_regime_factor
        self.volatility_regime = volatility_regime
        self.liquidity_regime = liquidity_regime
        self.correlation_regime = correlation_regime


class StressRegimePayload:
    """32-byte unified payload bridging stress indicators and regime diagnostics."""
    def __init__(self, volatility_regime: VolatilityRegime = VolatilityRegime.Low,
                 liquidity_regime: LiquidityRegime = LiquidityRegime.Normal,
                 correlation_regime: CorrelationRegime = CorrelationRegime.Stable,
                 stress_score: float = 0.0, deterioration_score: float = 0.0,
                 instability_score: float = 0.0, regime_confidence: float = 1.0,
                 reserved_flags: int = 0, timestamp_ns: int = 0):
        self.volatility_regime = volatility_regime
        self.liquidity_regime = liquidity_regime
        self.correlation_regime = correlation_regime
        self.stress_score = stress_score
        self.deterioration_score = deterioration_score
        self.instability_score = instability_score
        self.regime_confidence = regime_confidence
        self.reserved_flags = reserved_flags
        self.timestamp_ns = timestamp_ns

    def to_dict(self) -> Dict[str, Any]:
        return {
            "volatility_regime": VOLATILITY_REGIME_MAP.get(self.volatility_regime, "Medium"),
            "liquidity_regime": LIQUIDITY_REGIME_MAP.get(self.liquidity_regime, "Normal"),
            "correlation_regime": CORRELATION_REGIME_MAP.get(self.correlation_regime, "Stable"),
            "stress_score": round(self.stress_score, 2),
            "deterioration_score": round(self.deterioration_score, 2),
            "instability_score": round(self.instability_score, 2),
            "regime_confidence": round(self.regime_confidence, 2),
            "reserved_flags": self.reserved_flags,
            "timestamp_ns": self.timestamp_ns
        }

    def to_json(self) -> str:
        return json.dumps(self.to_dict())


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
                 symmetry_score: float, pattern_group: PatternHintGroup = PatternHintGroup.ExpansionGroup,
                 timestamp_ns: int = 0, contrarian_buy_zone: bool = False):
        self.pattern_hint = pattern_hint
        self.pattern_group = pattern_group
        self.prior_expansion_score = prior_expansion_score
        self.consolidation_score = consolidation_score
        self.support_strength_score = support_strength_score
        self.symmetry_score = symmetry_score
        self.timestamp_ns = timestamp_ns
        self.contrarian_buy_zone = contrarian_buy_zone

    def to_dict(self) -> Dict[str, Any]:
        return {
            "pattern_hint": PATTERN_HINT_MAP.get(self.pattern_hint, "None"),
            "pattern_group": PATTERN_HINT_GROUP_MAP.get(self.pattern_group, "ExpansionGroup"),
            "scores": {
                "prior_expansion": round(self.prior_expansion_score, 2),
                "consolidation": round(self.consolidation_score, 2),
                "support_strength": round(self.support_strength_score, 2),
                "symmetry": round(self.symmetry_score, 2)
            },
            "contrarian_buy_zone": self.contrarian_buy_zone,
            "timestamp_ns": self.timestamp_ns
        }

    def to_json(self) -> str:
        return json.dumps(self.to_dict())


class ChartIntelligenceOperator(BaseOperator):
    """Deterministic Operator deriving environment technical indicators and pattern diagnostics."""

    def __init__(self):
        super().__init__()
        self._prev_modifier: Optional[RegimeModifier] = None

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
            "trust_score": float(input_data.get("trust_score", 0.85)),
            "manipulation_score": float(input_data.get("manipulation_score", 0.0)),
            "layer_locks_engaged": bool(input_data.get("layer_locks_engaged", False)),
            "hft_bias_config": input_data.get("hft_bias_config", {}),
            "baseline": BaselineState(
                vol_5m=vol_5m, vol_1h=vol_1h, vol_30d=vol_30d,
                liq_5m=liq_5m, liq_1h=liq_1h, liq_30d=liq_30d,
                vol_corr_5m=corr_5m, vol_corr_1h=corr_1h, vol_corr_30d=corr_30d
            )
        }

    def _compute_regime_modifier(self, input_data: dict, baseline: BaselineState) -> RegimeModifier:
        """Computes regime classifications once per evaluation cycle with transition hysteresis."""
        ewma_vol = input_data["ewma_volatility"]
        base_vol = baseline.vol_30d if baseline.vol_30d > 1e-6 else input_data["baseline_volatility"]
        vol_ratio = ewma_vol / base_vol if base_vol > 1e-6 else 1.0

        prev_v = self._prev_modifier.volatility_regime if self._prev_modifier else VolatilityRegime.Medium

        if prev_v == VolatilityRegime.Low:
            if vol_ratio >= 0.75:
                v_regime = VolatilityRegime.High if vol_ratio >= 1.85 else VolatilityRegime.Medium
            else:
                v_regime = VolatilityRegime.Low
        elif prev_v == VolatilityRegime.High:
            if vol_ratio <= 1.70:
                v_regime = VolatilityRegime.Low if vol_ratio <= 0.65 else VolatilityRegime.Medium
            else:
                v_regime = VolatilityRegime.High
        else:
            if vol_ratio >= 1.85:
                v_regime = VolatilityRegime.High
            elif vol_ratio <= 0.65:
                v_regime = VolatilityRegime.Low
            else:
                v_regime = VolatilityRegime.Medium

        v_factor = 1.30 if v_regime == VolatilityRegime.High else (0.85 if v_regime == VolatilityRegime.Low else 1.00)

        curr_depth = input_data["bid_size"] + input_data["ask_size"]
        base_depth = baseline.liq_30d if baseline.liq_30d > 1e-6 else input_data["baseline_depth"]
        depth_ratio = curr_depth / base_depth if base_depth > 1e-6 else 1.0

        prev_l = self._prev_modifier.liquidity_regime if self._prev_modifier else LiquidityRegime.Normal

        if prev_l == LiquidityRegime.Thin:
            if depth_ratio >= 0.55:
                l_regime = LiquidityRegime.Deep if depth_ratio >= 1.55 else LiquidityRegime.Normal
            else:
                l_regime = LiquidityRegime.Thin
        elif prev_l == LiquidityRegime.Deep:
            if depth_ratio <= 1.40:
                l_regime = LiquidityRegime.Thin if depth_ratio <= 0.45 else LiquidityRegime.Normal
            else:
                l_regime = LiquidityRegime.Deep
        else:
            if depth_ratio <= 0.45:
                l_regime = LiquidityRegime.Thin
            elif depth_ratio >= 1.55:
                l_regime = LiquidityRegime.Deep
            else:
                l_regime = LiquidityRegime.Normal

        l_factor = 1.25 if l_regime == LiquidityRegime.Thin else (0.85 if l_regime == LiquidityRegime.Deep else 1.00)

        roll_corr = max(-1.0, min(1.0, input_data["rolling_correlation"]))
        exp_corr = max(-1.0, min(1.0, baseline.vol_corr_30d if baseline.vol_corr_30d < 1.0 else input_data["expected_correlation"]))
        corr_drop = max(0.0, exp_corr - roll_corr)

        prev_c = self._prev_modifier.correlation_regime if self._prev_modifier else CorrelationRegime.Stable

        if prev_c == CorrelationRegime.Stable:
            if corr_drop >= 0.28 or roll_corr <= 0.0:
                c_regime = CorrelationRegime.Unstable if (corr_drop >= 0.53 or roll_corr <= 0.0) else CorrelationRegime.Transitional
            else:
                c_regime = CorrelationRegime.Stable
        elif prev_c == CorrelationRegime.Unstable:
            if corr_drop <= 0.47 and roll_corr > 0.0:
                c_regime = CorrelationRegime.Stable if corr_drop <= 0.22 else CorrelationRegime.Transitional
            else:
                c_regime = CorrelationRegime.Unstable
        else:
            if corr_drop >= 0.53 or roll_corr <= 0.0:
                c_regime = CorrelationRegime.Unstable
            elif corr_drop <= 0.22:
                c_regime = CorrelationRegime.Stable
            else:
                c_regime = CorrelationRegime.Transitional

        c_factor = 1.30 if c_regime == CorrelationRegime.Unstable else (1.15 if c_regime == CorrelationRegime.Transitional else 1.00)

        modifier = RegimeModifier(
            volatility_regime_factor=v_factor,
            liquidity_regime_factor=l_factor,
            correlation_regime_factor=c_factor,
            volatility_regime=v_regime,
            liquidity_regime=l_regime,
            correlation_regime=c_regime
        )
        self._prev_modifier = modifier
        return modifier

    def execute(self, input_data: dict) -> dict:
        """Evaluates structural chart indicators, structural stress indicators, and pattern diagnostics."""
        baseline: BaselineState = input_data["baseline"]
        timestamp_ns = input_data["timestamp_ns"]

        # Compute regime modifier once per evaluation cycle
        modifier = self._compute_regime_modifier(input_data, baseline)

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

        # 5. Volatility Instability Indicator
        vol_spike = abs(ewma_vol - base_vol)
        raw_instability = max(0.0, min(10.0, vol_spike / base_vol if base_vol > 1e-6 else 0.0))
        vi_score = max(0.0, min(100.0, raw_instability * 40.0 * modifier.volatility_regime_factor))
        if vi_score >= 70.0:
            vi_state = ChartConditionState.StateChaotic
        elif vi_score >= 35.0:
            vi_state = ChartConditionState.StateUnstable
        else:
            vi_state = ChartConditionState.StateStable

        payload_vi = ChartConditionPayload(
            indicator_type=ChartIndicatorType.VolatilityInstability,
            condition_state=vi_state,
            normalized_score=vi_score,
            raw_metrics=[ewma_vol, base_vol, raw_instability],
            timestamp_ns=timestamp_ns
        )

        # 6. Liquidity Erosion Indicator
        c_depth = max(0.001, curr_depth)
        b_depth = max(0.001, base_depth)
        thin_rate = max(0.0, min(1.0, (b_depth - c_depth) / b_depth if c_depth < b_depth else 0.0))
        vol_anom = max(0.1, min(20.0, input_data["volume_anomaly_ratio"]))
        le_score = max(0.0, min(100.0, thin_rate * 80.0 * modifier.liquidity_regime_factor))
        if le_score >= 60.0 or thin_rate >= 0.60:
            le_state = ChartConditionState.StateDepleted
        elif le_score >= 30.0 or thin_rate >= 0.25:
            le_state = ChartConditionState.StateEroding
        else:
            le_state = ChartConditionState.StatePreserved

        payload_le = ChartConditionPayload(
            indicator_type=ChartIndicatorType.LiquidityErosion,
            condition_state=le_state,
            normalized_score=le_score,
            raw_metrics=[thin_rate, c_depth, vol_anom],
            timestamp_ns=timestamp_ns
        )

        # 7. Correlation Breakdown Indicator
        cb_score = max(0.0, min(100.0, corr_drop * 60.0 * modifier.correlation_regime_factor))
        if cb_score >= 65.0 or corr_drop >= 0.60:
            cb_state = ChartConditionState.StateDeteriorating
        elif cb_score >= 30.0 or corr_drop >= 0.25:
            cb_state = ChartConditionState.StateWeakening
        else:
            cb_state = ChartConditionState.StateStable

        payload_cb = ChartConditionPayload(
            indicator_type=ChartIndicatorType.CorrelationBreakdown,
            condition_state=cb_state,
            normalized_score=cb_score,
            raw_metrics=[roll_corr, exp_corr, corr_drop],
            timestamp_ns=timestamp_ns
        )

        # 8. Baseline Deterioration Indicator
        vol_5m = max(0.0, min(100.0, baseline.vol_5m))
        vol_1h = max(0.0, min(100.0, baseline.vol_1h))
        vol_30d = max(1e-6, min(100.0, baseline.vol_30d))
        drift_s = abs(vol_5m - vol_30d) / vol_30d
        drift_m = abs(vol_1h - vol_30d) / vol_30d
        bd_score = max(0.0, min(100.0, (drift_s * 40.0 + drift_m * 30.0) * modifier.volatility_regime_factor))
        if bd_score >= 60.0:
            bd_state = ChartConditionState.StateDeteriorating
        elif bd_score >= 30.0:
            bd_state = ChartConditionState.StateWeakening
        else:
            bd_state = ChartConditionState.Strong

        payload_bd = ChartConditionPayload(
            indicator_type=ChartIndicatorType.BaselineDeterioration,
            condition_state=bd_state,
            normalized_score=bd_score,
            raw_metrics=[drift_s, drift_m, vol_30d],
            timestamp_ns=timestamp_ns
        )

        # 9. Structural Fatigue Indicator
        sf_score = max(0.0, min(100.0, (vi_score * 0.35 + le_score * 0.40 + bd_score * 0.25) * modifier.volatility_regime_factor))
        if sf_score >= 65.0:
            sf_state = ChartConditionState.StateHighFatigue
        elif sf_score >= 35.0:
            sf_state = ChartConditionState.StateMediumFatigue
        else:
            sf_state = ChartConditionState.StateLowFatigue

        payload_sf = ChartConditionPayload(
            indicator_type=ChartIndicatorType.StructuralFatigue,
            condition_state=sf_state,
            normalized_score=sf_score,
            raw_metrics=[vi_score, le_score, bd_score],
            timestamp_ns=timestamp_ns
        )

        # 10. StressRegimePayload
        unified_stress = max(vi_score, le_score, bd_score, sf_score)
        stress_payload = StressRegimePayload(
            volatility_regime=modifier.volatility_regime,
            liquidity_regime=modifier.liquidity_regime,
            correlation_regime=modifier.correlation_regime,
            stress_score=unified_stress,
            deterioration_score=bd_score,
            instability_score=vi_score,
            regime_confidence=1.0,
            reserved_flags=0,
            timestamp_ns=timestamp_ns
        )

        # 11. Pattern Diagnostic Engine Expansion
        prior_exp = max(0.0, min(100.0, (vol_ratio - 1.0) * 60.0 if vol_ratio > 1.0 else 0.0))
        comp_term = (1.0 - vol_ratio) * 100.0 if vol_ratio <= 1.0 else 0.0
        depth_term = (1.0 - thinning_pct) * 50.0
        consol_score = max(0.0, min(100.0, comp_term * 0.5 + depth_term))
        supp_score = max(0.0, min(100.0, base_score * 0.7 + (1.0 - corr_drop) * 30.0))
        sym_score = max(0.0, min(100.0, 100.0 - (abs(vol_ratio - 1.0) * 30.0 + thinning_pct * 40.0)))

        if corr_drop >= 0.50 and le_score >= 40.0 and vi_score >= 40.0:
            pattern_hint = PatternHint.BreakdownLike
            pattern_group = PatternHintGroup.StressGroup
        elif bd_score >= 50.0 and sf_score >= 50.0:
            pattern_hint = PatternHint.ExhaustionLike
            pattern_group = PatternHintGroup.StressGroup
        elif consol_score >= 45.0 and base_score <= 45.0 and thinning_pct >= 0.30:
            pattern_hint = PatternHint.StressConsolidationLike
            pattern_group = PatternHintGroup.StressGroup
        elif supp_score >= 65.0 and consol_score >= 50.0 and thinning_pct < 0.30:
            pattern_hint = PatternHint.CupHandleLike
            pattern_group = PatternHintGroup.ExpansionGroup
        elif prior_exp >= 40.0 and consol_score >= 60.0 and sym_score >= 60.0:
            pattern_hint = PatternHint.PennantLike
            pattern_group = PatternHintGroup.ExpansionGroup
        elif prior_exp >= 35.0 and consol_score >= 40.0 and supp_score >= 45.0:
            pattern_hint = PatternHint.FlagLike
            pattern_group = PatternHintGroup.ExpansionGroup
        else:
            pattern_hint = PatternHint.NoneHint
            pattern_group = PatternHintGroup.ExpansionGroup

        # Secondary contrarian buy zone assessment
        hft_bias_cfg = input_data.get("hft_bias_config", {})
        trust_score = float(input_data.get("trust_score", 0.85))
        manip_score = float(input_data.get("manipulation_score", 0.0))
        locks_unlocked = not bool(input_data.get("layer_locks_engaged", False))
        mode = str(hft_bias_cfg.get("bullishness_mode", "STANDARD")).upper() if isinstance(hft_bias_cfg, dict) else "STANDARD"

        contrarian_buy_zone = False
        if mode in ("CONTRARIAN", "HYPER") and pattern_group == PatternHintGroup.StressGroup:
            if trust_score >= 0.70 and manip_score <= 0.30 and locks_unlocked:
                contrarian_buy_zone = True

        pattern_payload = PatternConditionPayload(
            pattern_hint=pattern_hint,
            pattern_group=pattern_group,
            prior_expansion_score=prior_exp,
            consolidation_score=consol_score,
            support_strength_score=supp_score,
            symmetry_score=sym_score,
            timestamp_ns=timestamp_ns,
            contrarian_buy_zone=contrarian_buy_zone
        )

        payloads = [
            payload_vol, payload_liq, payload_corr, payload_base,
            payload_vi, payload_le, payload_cb, payload_bd, payload_sf
        ]

        return {
            "symbol": input_data["symbol"],
            "pair_symbol": input_data["pair_symbol"],
            "payloads": [p.to_dict() for p in payloads],
            "pattern_diagnostics": pattern_payload.to_dict(),
            "stress_regime_payload": stress_payload.to_dict(),
            "volatility_regime": VOLATILITY_REGIME_MAP[modifier.volatility_regime],
            "liquidity_regime": LIQUIDITY_REGIME_MAP[modifier.liquidity_regime],
            "correlation_regime": CORRELATION_REGIME_MAP[modifier.correlation_regime]
        }


class FibAdvisory:
    """Deterministic Fibonacci & Golden Ratio Technical Advisory Result."""
    def __init__(
        self,
        fib_zone_active: bool = False,
        fib_buy_signal: bool = False,
        fib_sell_signal: bool = False,
        contrarian_fib_buy_zone: bool = False,
        hyper_fib_breakout: bool = False
    ):
        self.fib_zone_active = fib_zone_active
        self.fib_buy_signal = fib_buy_signal
        self.fib_sell_signal = fib_sell_signal
        self.contrarian_fib_buy_zone = contrarian_fib_buy_zone
        self.hyper_fib_breakout = hyper_fib_breakout

    def to_dict(self) -> Dict[str, Any]:
        return {
            "fib_zone_active": self.fib_zone_active,
            "fib_buy_signal": self.fib_buy_signal,
            "fib_sell_signal": self.fib_sell_signal,
            "contrarian_fib_buy_zone": self.contrarian_fib_buy_zone,
            "hyper_fib_breakout": self.hyper_fib_breakout
        }


def compute_fib_advisory(
    current_price: float,
    recent_high: float,
    recent_low: float,
    current_volume: float,
    avg_volume: float,
    mode: str = "STANDARD"
) -> FibAdvisory:
    adv = FibAdvisory()
    if recent_high <= recent_low or current_price <= 0.0:
        return adv

    ret = compute_retracements(recent_high, recent_low)
    ext = compute_extensions(recent_high, recent_low)

    rng = recent_high - recent_low
    epsilon = 0.002 * recent_high

    near_ret236 = abs(current_price - ret.level_236) <= epsilon
    near_ret382 = abs(current_price - ret.level_382) <= epsilon
    near_ret618 = abs(current_price - ret.level_618) <= epsilon
    near_ret786 = abs(current_price - ret.level_786) <= epsilon

    near_ext1272 = abs(current_price - ext.level_1272) <= epsilon
    near_ext1618 = abs(current_price - ext.level_1618) <= epsilon
    near_ext2618 = abs(current_price - ext.level_2618) <= epsilon

    near_retracement = near_ret236 or near_ret382 or near_ret618 or near_ret786
    near_extension = near_ext1272 or near_ext1618 or near_ext2618

    adv.fib_zone_active = near_retracement or near_extension

    vol_confirmed_1_2 = (avg_volume > 0.0) and (current_volume >= 1.2 * avg_volume)
    vol_confirmed_1_3 = (avg_volume > 0.0) and (current_volume >= 1.3 * avg_volume)
    vol_exhaustion = (avg_volume > 0.0) and (current_volume <= 0.8 * avg_volume)

    mode_str = str(mode).upper()
    if mode_str == "STANDARD":
        adv.fib_buy_signal = False
        adv.fib_sell_signal = False
    elif mode_str == "CONSERVATIVE":
        if adv.fib_zone_active and vol_confirmed_1_2 and near_retracement:
            adv.fib_buy_signal = True
        if near_extension and vol_exhaustion:
            adv.fib_sell_signal = True
    elif mode_str == "HYPER":
        if adv.fib_zone_active and vol_confirmed_1_3 and (current_price >= ret.level_382 or near_extension):
            adv.hyper_fib_breakout = True
        if near_extension:
            adv.fib_sell_signal = True
    elif mode_str == "CONTRARIAN":
        if adv.fib_zone_active and vol_confirmed_1_3 and (current_price <= ret.level_618 or current_price <= recent_low + 0.236 * rng):
            adv.contrarian_fib_buy_zone = True
        if near_extension and vol_exhaustion:
            adv.fib_sell_signal = True

    return adv
