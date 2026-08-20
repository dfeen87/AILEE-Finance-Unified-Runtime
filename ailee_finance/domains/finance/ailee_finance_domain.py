"""
AILEE Finance Domain Module - Version 5.0.0
Implements SELL Governance evaluation pipeline, decision structure, governance level thresholds,
and audit logging.
"""

import os
import json
import datetime
from ailee_finance.domains.finance.sell_governance import (
    validate_sell_intent,
    compute_sell_ceiling,
    detect_sell_manipulation,
    grace_layer_sell_adjustment,
    consensus_validation,
)
from core.finance_kernel.hft_bias import is_bullish_mode_allowed


class SellGovernanceDecision:
    """
    SELL Governance Decision Data Structure
    """
    def __init__(self, level, allowed_sell_amount, trust_score,
                 manipulation_score, consensus_score, reason,
                 bullish_mode_active=False, bullish_multiplier_price=1.05,
                 bullish_multiplier_volume=1.05, bullish_execution_scale=1.10,
                 bullish_sell_ceiling_factor=0.80):
        self.level = int(level)
        self.allowed_sell_amount = float(allowed_sell_amount)
        self.trust_score = float(trust_score)
        self.manipulation_score = float(manipulation_score)
        self.consensus_score = float(consensus_score)
        self.reason = str(reason)
        self.bullish_mode_active = bool(bullish_mode_active)
        self.bullish_multiplier_price = float(bullish_multiplier_price)
        self.bullish_multiplier_volume = float(bullish_multiplier_volume)
        self.bullish_execution_scale = float(bullish_execution_scale)
        self.bullish_sell_ceiling_factor = float(bullish_sell_ceiling_factor)

    def to_dict(self):
        return {
            "level": self.level,
            "allowed_sell_amount": self.allowed_sell_amount,
            "trust_score": self.trust_score,
            "manipulation_score": self.manipulation_score,
            "consensus_score": self.consensus_score,
            "reason": self.reason,
            "bullish_mode_active": self.bullish_mode_active,
            "bullish_multiplier_price": self.bullish_multiplier_price,
            "bullish_multiplier_volume": self.bullish_multiplier_volume,
            "bullish_execution_scale": self.bullish_execution_scale,
            "bullish_sell_ceiling_factor": self.bullish_sell_ceiling_factor
        }

    def __repr__(self):
        return (f"<SellGovernanceDecision level={self.level} "
                f"allowed_sell_amount={self.allowed_sell_amount:.4f} "
                f"trust_score={self.trust_score:.2f} "
                f"manipulation_score={self.manipulation_score:.2f} "
                f"consensus_score={self.consensus_score:.2f} "
                f"reason='{self.reason}'>")


class AileeFinanceDomain:
    """
    AILEE Finance Domain logic evaluator
    """
    VERSION = "5.0.0"

    def __init__(self, log_path="logs/ailee_finance_sell_audit.log", hft_bias_config=None):
        self.log_path = log_path
        if hft_bias_config is None:
            self.hft_bias_config = {
                "enabled": True,
                "bullish_multiplier_price": 1.05,
                "bullish_multiplier_volume": 1.05,
                "bullish_execution_scale": 1.10,
                "bullish_sell_ceiling_factor": 0.80,
                "trust_threshold_bullish": 0.70,
                "manipulation_threshold": 0.30,
            }
        else:
            self.hft_bias_config = dict(hft_bias_config)

    def compute_trust_score(self, signals):
        """
        Compute aggregate trust score from signals payload.
        """
        if not isinstance(signals, dict):
            return 0.0

        if "trust_score" in signals:
            return max(0.0, min(1.0, float(signals["trust_score"])))

        # Fallback composite trust score calculation from sub-metrics
        telemetry_trust = float(signals.get("telemetry_trust", 0.85))
        hardware_integrity = float(signals.get("hardware_integrity", 1.0))
        model_confidence = float(signals.get("model_confidence", 0.80))

        trust = (telemetry_trust * 0.4) + (hardware_integrity * 0.3) + (model_confidence * 0.3)
        return max(0.0, min(1.0, trust))

    def determine_governance_level(self, trust_score, manipulation_score, consensus_score):
        """
        Determine governance level 0..3 based on trust, manipulation, and consensus scores:
        - Level 0: High trust (>=0.85), low manipulation (<=0.20), high consensus (>=0.80) -> 100% position cap
        - Level 1: Moderate high trust (>=0.70), low/mod manipulation (<=0.40), mod consensus (>=0.60) -> 60% cap
        - Level 2: Moderate trust (>=0.50), mod manipulation (<=0.60), lower consensus (>=0.40) -> 30% cap
        - Level 3: Protective mode -> 10% cap
        """
        if trust_score >= 0.85 and manipulation_score <= 0.20 and consensus_score >= 0.80:
            return 0
        elif trust_score >= 0.70 and manipulation_score <= 0.40 and consensus_score >= 0.60:
            return 1
        elif trust_score >= 0.50 and manipulation_score <= 0.60 and consensus_score >= 0.40:
            return 2
        else:
            return 3

    def log_sell_audit(self, decision):
        """
        Write SELL governance decision audit entry to log file in JSON format.
        """
        try:
            log_dir = os.path.dirname(self.log_path)
            if log_dir:
                os.makedirs(log_dir, exist_ok=True)

            log_entry = {
                "timestamp": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                "level": decision.level,
                "allowed_sell_amount": round(decision.allowed_sell_amount, 4),
                "trust_score": round(decision.trust_score, 4),
                "manipulation_score": round(decision.manipulation_score, 4),
                "consensus_score": round(decision.consensus_score, 4),
                "bullish_mode_active": decision.bullish_mode_active,
                "bullish_multiplier_price": decision.bullish_multiplier_price,
                "bullish_multiplier_volume": decision.bullish_multiplier_volume,
                "bullish_execution_scale": decision.bullish_execution_scale,
                "bullish_sell_ceiling_factor": decision.bullish_sell_ceiling_factor,
                "reason": decision.reason
            }

            with open(self.log_path, "a", encoding="utf-8") as f:
                f.write(json.dumps(log_entry) + "\n")
        except Exception:
            pass  # Ensure audit logging never crashes evaluation pipeline

    def evaluate_sell(self, signals):
        """
        Main SELL evaluation pipeline:
        1. Validate SELL intent
        2. Compute composite trust score
        3. Detect manipulation heuristics
        4. Validate multi-feed consensus
        5. Determine governance level
        6. Compute ceiling cap
        7. Apply volatility grace adjustment
        8. Audit log and return decision
        """
        if not isinstance(signals, dict):
            signals = {}

        intent = validate_sell_intent(signals)
        trust_score = self.compute_trust_score(signals)
        market = signals.get("market", {}) if isinstance(signals.get("market"), dict) else {}
        feeds = signals.get("feeds", []) if isinstance(signals.get("feeds"), (list, tuple)) else []

        manipulation_score = detect_sell_manipulation(market)
        consensus_score = consensus_validation(feeds)

        if not intent.get("intent_valid", True):
            level = 3
            reason = f"Invalid sell intent: {intent.get('reason', 'Unknown reason')}"
        else:
            level = self.determine_governance_level(
                trust_score, manipulation_score, consensus_score
            )
            reason = intent.get("reason", "SELL intent evaluated successfully")

        drawdown_state = signals.get("drawdown_state", False)
        hft_cfg = signals.get("hft_bias_config", self.hft_bias_config)

        bullish_active = is_bullish_mode_allowed(
            trust_score=trust_score,
            manipulation_score=manipulation_score,
            drawdown_state=drawdown_state,
            hft_bias_config=hft_cfg
        )

        sell_ceiling_factor = float(hft_cfg.get("bullish_sell_ceiling_factor", 0.80)) if hft_cfg else 0.80

        position_size = float(signals.get("position_size", 0.0))
        allowed_sell_amount = compute_sell_ceiling(
            level, position_size, bullish_active=bullish_active, bullish_sell_ceiling_factor=sell_ceiling_factor
        )

        volatility = float(signals.get("volatility", 0.0))
        allowed_sell_amount = grace_layer_sell_adjustment(
            volatility, allowed_sell_amount
        )

        # Increased SELL sensitivity to downward manipulation
        if manipulation_score > 0.0:
            allowed_sell_amount *= max(0.0, 1.0 - 0.5 * manipulation_score)
            if allowed_sell_amount > (position_size * 0.3) and consensus_score < 0.70:
                allowed_sell_amount *= max(0.1, consensus_score)

        allowed_sell_amount = max(0.0, allowed_sell_amount)

        p_mult = float(hft_cfg.get("bullish_multiplier_price", 1.05)) if hft_cfg else 1.05
        v_mult = float(hft_cfg.get("bullish_multiplier_volume", 1.05)) if hft_cfg else 1.05
        e_scale = float(hft_cfg.get("bullish_execution_scale", 1.10)) if hft_cfg else 1.10

        decision = SellGovernanceDecision(
            level=level,
            allowed_sell_amount=allowed_sell_amount,
            trust_score=trust_score,
            manipulation_score=manipulation_score,
            consensus_score=consensus_score,
            reason=reason,
            bullish_mode_active=bullish_active,
            bullish_multiplier_price=p_mult,
            bullish_multiplier_volume=v_mult,
            bullish_execution_scale=e_scale,
            bullish_sell_ceiling_factor=sell_ceiling_factor
        )

        self.log_sell_audit(decision)

        return decision
