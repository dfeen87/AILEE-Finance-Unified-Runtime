"""
AILEE Finance Minimal Trust Pipeline - Version 5.0.0
Coordinates domain evaluation for SELL governance and handles pipeline exceptions with fallback protection.
"""

from ailee_finance.domains.finance.ailee_finance_domain import (
    AileeFinanceDomain,
    SellGovernanceDecision,
)


class AileeFinanceTrustPipeline:
    """
    Minimal Trust Pipeline processing SELL governance decisions with protective fallback.
    """
    def __init__(self, domain=None):
        if domain is None:
            domain = AileeFinanceDomain()
        self.domain = domain

    def process_sell(self, signals):
        """
        Process a SELL operation through the trust domain pipeline.
        In case of unhandled errors/exceptions, triggers Level 3 protective mode fallback.
        """
        try:
            decision = self.domain.evaluate_sell(signals)
            return decision
        except Exception as e:
            position_size = 0.0
            if isinstance(signals, dict):
                try:
                    position_size = float(signals.get("position_size", 0.0))
                except (ValueError, TypeError):
                    position_size = 0.0

            # Fallback: Level 3 protective mode (10% ceiling)
            return SellGovernanceDecision(
                level=3,
                allowed_sell_amount=position_size * 0.1,
                trust_score=0.0,
                manipulation_score=1.0,
                consensus_score=0.0,
                reason=f"Fallback triggered: {str(e)}"
            )
