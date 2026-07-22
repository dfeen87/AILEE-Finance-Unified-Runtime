# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Unit tests for Layer 11 — Deterministic Portfolio-Wide Constraint Engine."""

import pytest
from core.finance_kernel.arbitration_layer import AssetId
from core.finance_kernel.portfolio_constraints import (
    ConstraintRule,
    SectorDefinition,
    CorrelationProfile,
    RiskBudget,
    AssetAllocation,
    ConstraintStage,
    ConstraintAction,
    SectorId,
    apply_portfolio_constraints,
    map_asset_to_sector
)

def test_map_asset_to_sector():
    assert map_asset_to_sector(AssetId.CASH) == SectorId.FIAT
    assert map_asset_to_sector(AssetId.BTC) == SectorId.CRYPTO
    assert map_asset_to_sector(AssetId.ETH) == SectorId.CRYPTO
    assert map_asset_to_sector(AssetId.GOLD) == SectorId.PRECIOUS_METALS
    assert map_asset_to_sector(AssetId.OIL) == SectorId.ENERGY
    assert map_asset_to_sector(AssetId.EQUITY_HIGH_RISK) == SectorId.OTHER

def test_layer11_deterministic_walkthrough():
    """
    Example walkthrough where the portfolio tries to over-allocate to a correlated crypto cluster:
    BTC (35% allocation, risk 60) and ETH (25% allocation, risk 80)
    We configure:
    - Max long exposure rule for BTC of 0.40 (not violated by 0.35) and ETH of 0.30 (not violated by 0.25).
    - Sector Cap definition for CRYPTO (SectorId.CRYPTO) of 0.50 max exposure.
    - Pairwise Correlation Profile between BTC and ETH of 0.85 (which is > CORR_CLUSTER_THRESHOLD 0.70).
      This forms a CRYPTO cluster. The sum of cluster allocations (after sector caps) will be limited to 0.30 of the portfolio.
    - Risk budget max_risk of 25.0.
    """
    proposed = [
        AssetAllocation(AssetId.BTC, 0.35, 60.0), # 0.35 * 60 = 21.0 risk
        AssetAllocation(AssetId.ETH, 0.25, 80.0), # 0.25 * 80 = 20.0 risk
        AssetAllocation(AssetId.CASH, 0.40, 0.0)
    ]

    rules = [
        ConstraintRule(AssetId.BTC, max_long_exposure=0.40, max_short_exposure=0.0),
        ConstraintRule(AssetId.ETH, max_long_exposure=0.30, max_short_exposure=0.0)
    ]

    sectors = [
        SectorDefinition(SectorId.CRYPTO, max_sector_exposure=0.50, sector_name="CRYPTO")
    ]

    correlations = [
        CorrelationProfile(AssetId.BTC, AssetId.ETH, correlation_score=0.85)
    ]

    budget = RiskBudget(max_portfolio_risk=25.0)

    # Run constraints
    result = apply_portfolio_constraints(proposed, rules, sectors, correlations, budget)

    # Initial risk: (0.35*60) + (0.25*80) = 41.0
    assert result.summary.initial_portfolio_risk == 41.0

    # Stage 1: Max-Exposure Clamping
    # BTC (0.35 <= 0.40) -> 0.35
    # ETH (0.25 <= 0.30) -> 0.25

    # Stage 2: Sector Caps Enforcement
    # CRYPTO sum = 0.35 + 0.25 = 0.60.
    # Max sector exposure is 0.50.
    # Scale factor = 0.50 / 0.60 = 5/6.
    # BTC becomes 0.35 * (5/6) = 0.291666...
    # ETH becomes 0.25 * (5/6) = 0.208333...

    # Stage 3: Pairwise Correlation Dampening
    # Pairwise correlation BTC/ETH is 0.85 > 0.70 threshold -> they are in same cluster.
    # Cluster sum is now 0.291666... + 0.208333... = 0.50.
    # Cluster limit CORR_CLUSTER_MAX_ALLOCATION = 0.30.
    # Scale factor = 0.30 / 0.50 = 0.60.
    # BTC becomes 0.291666... * 0.60 = 0.175
    # ETH becomes 0.208333... * 0.60 = 0.125
    # Cluster Sum is exactly 0.30.

    # Stage 4: Risk-Budget Enforcement
    # Portfolio risk is now (0.175 * 60) + (0.125 * 80) = 10.5 + 10.0 = 20.5.
    # 20.5 is <= 25.0 budget limit.
    # Thus, no further scaling is applied!

    # Final allocations:
    btc_alloc = next(a for a in result.allocations if a.asset_id == AssetId.BTC)
    eth_alloc = next(a for a in result.allocations if a.asset_id == AssetId.ETH)

    assert pytest.approx(btc_alloc.allocation) == 0.175
    assert pytest.approx(eth_alloc.allocation) == 0.125
    assert pytest.approx(result.summary.final_portfolio_risk) == 20.5

    # Trace steps must be captured
    assert len(result.trace_steps) > 0
    assert any(step.stage == ConstraintStage.CORRELATION_DAMPEN and step.action_taken == ConstraintAction.DAMPENED for step in result.trace_steps)
