# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Unit tests for Layer 9 — Deterministic Liquidity Routing (Finance Runtime Kernel)."""

import math
import pytest
from core.finance_kernel.arbitration_layer import AssetId
from core.finance_kernel.routing_layer import (
    StressLevel,
    LiquidityCap,
    RoutingRule,
    ShockBounds,
    CrossAssetDecision,
    StressProfile,
    LiquidityState,
    route_liquidity
)

def test_layer9_routing_deterministic_walk_consistency():
    # 1. Setup decisions
    decisions = [
        CrossAssetDecision(asset_id=AssetId.BTC, target_allocation_ratio=0.30),
        CrossAssetDecision(asset_id=AssetId.GOLD, target_allocation_ratio=0.50)
    ]

    # 2. Setup stress profile
    stress = StressProfile(stress_level=StressLevel.NORMAL)

    # 3. Setup liquidity states (Portfolio value = 600,000)
    states = [
        # BTC current: 50% allocation (value = 500,000)
        LiquidityState(asset_id=AssetId.BTC, current_liquidity_value=500000.0, current_allocation_ratio=0.50, flags=0),
        # GOLD current: 10% allocation (value = 100,000)
        LiquidityState(asset_id=AssetId.GOLD, current_liquidity_value=100000.0, current_allocation_ratio=0.10, flags=0)
    ]

    # 4. Setup liquidity caps
    caps = [
        # BTC max outflow = 15% (i.e. 75,000 limit)
        LiquidityCap(asset_id=AssetId.BTC, max_outflow_ratio=0.15, max_inflow_ratio=1.0, stress_level=StressLevel.NORMAL),
        # GOLD max inflow = 50% (i.e. 50,000 limit)
        LiquidityCap(asset_id=AssetId.GOLD, max_outflow_ratio=1.0, max_inflow_ratio=0.50, stress_level=StressLevel.NORMAL)
    ]

    # 5. Setup routing table
    table = [
        RoutingRule(source=AssetId.BTC, primary_target=AssetId.GOLD, fallback_target=AssetId.CASH, stress_level=StressLevel.NORMAL, preferred_flow_ratio=1.0)
    ]

    # 6. Setup shock bounds
    bounds = ShockBounds(max_portfolio_liquidity_shift_per_step=0.10, max_asset_liquidity_shift_per_step=0.20)

    # Verification 1: Both targets blocked.
    # Surplus of BTC: (0.50 - 0.30) * 600,000 = 120,000
    # Cap outflow limit: 500k * 0.15 = 75,000
    # Movable = min(120k, 75k) = 75,000
    # GOLD max inflow = 100k * 0.50 = 50,000
    # Since 75,000 > 50,000, primary target GOLD is blocked.
    # CASH is not in states so it's also blocked. No flow occurs.
    res = route_liquidity(decisions, stress, states, caps, table, bounds)
    assert res.flow_count == 0
    assert len(res.flows) == 0

    # Verification 2: Non-blocked target, asset shock clamping.
    # Let's adjust GOLD max inflow cap to 80% (i.e. 80,000 limit).
    caps[1].max_inflow_ratio = 0.80
    res = route_liquidity(decisions, stress, states, caps, table, bounds)

    # Now, movable is 75,000. Primary target (GOLD) is NOT blocked.
    # Net flow proposed: BTC = -75k, GOLD = +75k.
    # Asset bounds limits:
    # BTC limit: 500k * 0.20 = 100,000. (75k <= 100k)
    # GOLD limit: 100k * 0.20 = 20,000. (75k > 20k, scale = 20/75 = 0.266667)
    # Scaled flow is 20,000.
    # Portfolio shift limit: 600k * 0.10 = 60,000. 20,000 <= 60,000.
    # Final flow = 20,000 from BTC to GOLD!
    assert res.flow_count == 1
    assert len(res.flows) == 1
    flow = res.flows[0]
    assert flow.source == AssetId.BTC
    assert flow.target == AssetId.GOLD
    assert math.isclose(flow.amount, 20000.0, rel_tol=1e-5)
    assert math.isclose(res.summary.total_shift_value, 20000.0, rel_tol=1e-5)
