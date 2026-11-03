"""
AILEE Finance Minimal Trust Pipeline - Version 5.0.0
Coordinates domain evaluation for SELL governance and handles pipeline exceptions with fallback protection.
"""

from ailee_finance.domains.finance.ailee_finance_domain import (
    AileeFinanceDomain,
    SellGovernanceDecision,
)
from ailee_finance.domains.finance.sell_governance import compute_sell_ceiling
from dataclasses import asdict
from ailee_finance.math_engine import compute_delta_v, build_delta_v_evidence, MathValidationError


class AileeFinanceTrustPipeline:
    """
    Minimal Trust Pipeline processing SELL governance decisions with protective fallback.
    """
    def __init__(self, domain=None):
        if domain is None:
            domain = AileeFinanceDomain()
        self.domain = domain

    def process_sell(self, signals, *, math_parameters=None, math_samples=None):
        """
        Process a SELL operation through the trust domain pipeline.
        In case of unhandled errors/exceptions, triggers Level 3 protective mode fallback.
        """
        mathematical_evidence = None
        if math_parameters is not None or math_samples is not None:
            if math_parameters is None or math_samples is None:
                raise MathValidationError("mathematical parameters and samples must be provided together")
            # Math failures remain explicit and occur before governance/audit.
            # Neither the result nor caller-supplied evidence sets permission.
            mathematical_evidence = asdict(build_delta_v_evidence(compute_delta_v(math_parameters, math_samples)))
        try:
            decision = (self.domain.evaluate_sell(signals) if mathematical_evidence is None else
                        self.domain.evaluate_sell(signals, mathematical_evidence=mathematical_evidence))
            decision.validate()
            position = signals.get("position_size", 0.0) if isinstance(signals, dict) else 0.0
            if decision.allowed_sell_amount > compute_sell_ceiling(decision.level, position):
                raise ValueError("SELL decision exceeds the governing position ceiling")
            return decision
        except Exception as e:
            position_size = signals.get("position_size", 0.0) if isinstance(signals, dict) else 0.0

            # Fallback: Level 3 protective mode (10% ceiling)
            return SellGovernanceDecision(
                level=3,
                allowed_sell_amount=compute_sell_ceiling(3, position_size),
                trust_score=0.0,
                manipulation_score=1.0,
                consensus_score=0.0,
                reason=f"Fallback triggered: {str(e)}",
                mathematical_evidence=mathematical_evidence,
            )
