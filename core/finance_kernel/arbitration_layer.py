# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Layer 8 — Cross-Asset Deterministic Arbitration Layer."""

import math

class AssetId:
    CASH = 0
    BTC = 1
    ETH = 2
    GOLD = 3
    OIL = 4
    EQUITY_HIGH_RISK = 5

class AdvisoryFlags:
    NONE = 0
    HARD_BLOCK = 1
    SOFT_BLOCK = 2
    PREFERRED = 4

class Advisory:
    """Python representation of the 64-byte aligned AILLE::Advisory struct."""
    def __init__(self, asset_id: int, risk_score: float, safety_level: float,
                 liquidity_level: float, regulatory_level: float, return_score: float,
                 confidence: float, raw_flags: int = 0):
        self.asset_id = asset_id
        self.risk_score = float(risk_score)
        self.safety_level = float(safety_level)
        self.liquidity_level = float(liquidity_level)
        self.regulatory_level = float(regulatory_level)
        self.return_score = float(return_score)
        self.confidence = float(confidence)
        self.raw_flags = int(raw_flags)

class AllocationDecision:
    """Python representation of the 64-byte aligned AILLE::AllocationDecision struct."""
    def __init__(self, asset_id: int, recommended_allocation: float):
        self.asset_id = asset_id
        self.recommended_allocation = float(recommended_allocation)

class ArbitrationTraceStep:
    """Python representation of the 64-byte aligned AILLE::ArbitrationTraceStep struct."""
    def __init__(self, asset_id: int, dimension: int, input_value: float,
                 result_weight: float, log: str):
        self.asset_id = asset_id
        self.dimension = int(dimension)
        self.input_value = float(input_value)
        self.result_weight = float(result_weight)
        self.log = str(log)

class ArbitrationTrace:
    def __init__(self):
        self.steps = []
        self.step_count = 0

    def add_step(self, asset_id: int, dimension: int, input_value: float,
                 result_weight: float, log: str):
        self.steps.append(ArbitrationTraceStep(asset_id, dimension, input_value, result_weight, log))
        self.step_count += 1

class ArbitrationResult:
    def __init__(self):
        self.decisions = []
        self.decision_count = 0
        self.trace = ArbitrationTrace()

class LadderDimension:
    SAFETY = 0
    LIQUIDITY = 1
    REGULATORY = 2
    RISK = 3
    RETURN = 4

class Ladder:
    def __init__(self):
        self.version = "LADDER_V1"
        self.dimensions = [
            LadderDimension.SAFETY,
            LadderDimension.LIQUIDITY,
            LadderDimension.REGULATORY,
            LadderDimension.RISK,
            LadderDimension.RETURN
        ]

class ScalingRules:
    def __init__(self):
        self.version = "SCALING_RULESET_V1"

    @staticmethod
    def clamp(val: float, minimum: float, maximum: float) -> float:
        return max(minimum, min(val, maximum))

    @staticmethod
    def normalize_risk(raw_risk: float, r_min: float = 0.0, r_max: float = 100.0) -> float:
        return ScalingRules.clamp((raw_risk - r_min) / (r_max - r_min), 0.0, 1.0)

    @staticmethod
    def normalize_liquidity(level: float) -> float:
        return ScalingRules.clamp(level, 0.0, 1.0)

    @staticmethod
    def normalize_regulatory(level: float) -> float:
        return ScalingRules.clamp(level, 0.0, 1.0)

def arbitrate(advisories: list, ladder: Ladder, rules: ScalingRules) -> ArbitrationResult:
    """Pure functional deterministic arbitration matching C++ equivalent."""
    result = ArbitrationResult()
    if not advisories:
        return result

    asset_count = min(len(advisories), 16)
    result.decision_count = asset_count

    # State containers
    weights = [1.0] * asset_count
    safety_hurdles = []
    liquidity_factors = []
    reg_caps = []
    risk_scores = []
    return_scores = []

    # Phase 1: Canonical Scaling Ruleset (SCALING_RULESET_V1)
    for i in range(asset_count):
        adv = advisories[i]
        safety_hurdles.append(rules.clamp(adv.safety_level, 0.0, 1.0))
        liquidity_factors.append(rules.normalize_liquidity(adv.liquidity_level))
        reg_caps.append(rules.normalize_regulatory(adv.regulatory_level))
        risk_scores.append(rules.normalize_risk(adv.risk_score))
        return_scores.append(rules.clamp(adv.return_score, 0.0, 1.0))

    # Phase 2: Ladder Traversal (LADDER_V1)
    for stage in range(5):
        dim = ladder.dimensions[stage]
        for i in range(asset_count):
            if weights[i] == 0.0:
                continue

            adv = advisories[i]

            if dim == LadderDimension.SAFETY:
                if safety_hurdles[i] < 0.35:
                    weights[i] = 0.0
                    result.trace.add_step(adv.asset_id, dim, safety_hurdles[i], 0.0, "Safety failure")
                elif safety_hurdles[i] < 0.6:
                    weights[i] = weights[i] * 0.5
                    result.trace.add_step(adv.asset_id, dim, safety_hurdles[i], weights[i], "Marginal safety cap")
                else:
                    result.trace.add_step(adv.asset_id, dim, safety_hurdles[i], weights[i], "Safety pass")

            elif dim == LadderDimension.LIQUIDITY:
                weights[i] = weights[i] * liquidity_factors[i]
                result.trace.add_step(adv.asset_id, dim, liquidity_factors[i], weights[i], "Liquidity scaled")

            elif dim == LadderDimension.REGULATORY:
                if adv.raw_flags & AdvisoryFlags.HARD_BLOCK:
                    weights[i] = 0.0
                    result.trace.add_step(adv.asset_id, dim, 0.0, 0.0, "Hard block")
                elif reg_caps[i] < 0.3:
                    weights[i] = weights[i] * 0.2
                    result.trace.add_step(adv.asset_id, dim, reg_caps[i], weights[i], "Reg soft cap")
                elif adv.raw_flags & AdvisoryFlags.PREFERRED:
                    weights[i] = weights[i] * 1.2
                    result.trace.add_step(adv.asset_id, dim, reg_caps[i], weights[i], "Preferred mult")
                else:
                    result.trace.add_step(adv.asset_id, dim, reg_caps[i], weights[i], "Reg neutral")

            elif dim == LadderDimension.RISK:
                risk_damp = 1.0 - risk_scores[i]
                weights[i] = weights[i] * risk_damp
                result.trace.add_step(adv.asset_id, dim, risk_scores[i], weights[i], "Risk dampened")

            elif dim == LadderDimension.RETURN:
                return_factor = 0.5 + 0.5 * return_scores[i]
                weights[i] = weights[i] * return_factor
                result.trace.add_step(adv.asset_id, dim, return_scores[i], weights[i], "Return scaled")

    # Phase 3: Deterministic Normalization
    total_weight = sum(weights)
    for i in range(asset_count):
        final_alloc = 0.0
        if total_weight > 0.0:
            final_alloc = weights[i] / total_weight
        result.decisions.append(AllocationDecision(advisories[i].asset_id, final_alloc))

    return result
