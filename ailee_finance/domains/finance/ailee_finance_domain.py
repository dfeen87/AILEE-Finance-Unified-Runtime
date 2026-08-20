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


class SellGovernanceDecision:
    """
    SELL Governance Decision Data Structure
    """
    def __init__(self, level, allowed_sell_amount, trust_score,
                 manipulation_score, consensus_score, reason):
        self.level = int(level)
        self.allowed_sell_amount = float(allowed_sell_amount)
        self.trust_score = float(trust_score)
        self.manipulation_score = float(manipulation_score)
        self.consensus_score = float(consensus_score)
        self.reason = str(reason)

    def to_dict(self):
        return {
            "level": self.level,
            "allowed_sell_amount": self.allowed_sell_amount,
            "trust_score": self.trust_score,
            "manipulation_score": self.manipulation_score,
            "consensus_score": self.consensus_score,
            "reason": self.reason
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

    def __init__(self, log_path="logs/ailee_finance_sell_audit.log"):
        self.log_path = log_path

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

        position_size = float(signals.get("position_size", 0.0))
        allowed_sell_amount = compute_sell_ceiling(
            level, position_size
        )

        volatility = float(signals.get("volatility", 0.0))
        allowed_sell_amount = grace_layer_sell_adjustment(
            volatility, allowed_sell_amount
        )

        decision = SellGovernanceDecision(
            level=level,
            allowed_sell_amount=allowed_sell_amount,
            trust_score=trust_score,
            manipulation_score=manipulation_score,
            consensus_score=consensus_score,
            reason=reason
        )

        self.log_sell_audit(decision)

        return decision
