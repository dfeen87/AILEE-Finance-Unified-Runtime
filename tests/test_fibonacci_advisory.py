# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Unit tests for Fibonacci & Golden Ratio Technical Advisory Layer."""

import pytest
from core.finance_kernel.fibonacci import (
    compute_retracements,
    compute_extensions,
    golden_ratio,
    project_trend
)
from core.finance_kernel.chart_intelligence import compute_fib_advisory, FibAdvisory
from core.finance_kernel.volume_advisory import evaluate_oversold


def test_fibonacci_math():
    high, low = 200.0, 100.0
    ret = compute_retracements(high, low)
    assert ret.level_236 == pytest.approx(176.4)
    assert ret.level_382 == pytest.approx(161.8)
    assert ret.level_618 == pytest.approx(138.2)
    assert ret.level_786 == pytest.approx(121.4)

    ext = compute_extensions(high, low)
    assert ext.level_1272 == pytest.approx(327.2)
    assert ext.level_1618 == pytest.approx(361.8)
    assert ext.level_2618 == pytest.approx(461.8)

    assert golden_ratio() == pytest.approx(1.61803398875)
    assert project_trend(100.0, 10.0) == pytest.approx(116.1803398875)


def test_fib_advisory_modes():
    high, low = 100.0, 50.0
    ret = compute_retracements(high, low)
    price_at_618 = ret.level_618  # 100 - 50*0.618 = 69.1

    # Standard mode
    adv_std = compute_fib_advisory(
        current_price=price_at_618,
        recent_high=high,
        recent_low=low,
        current_volume=150000.0,
        avg_volume=100000.0,
        mode="STANDARD"
    )
    assert adv_std.fib_zone_active is True
    assert adv_std.fib_buy_signal is False

    # Conservative mode
    adv_cons = compute_fib_advisory(
        current_price=price_at_618,
        recent_high=high,
        recent_low=low,
        current_volume=130000.0,
        avg_volume=100000.0,
        mode="CONSERVATIVE"
    )
    assert adv_cons.fib_zone_active is True
    assert adv_cons.fib_buy_signal is True

    # Contrarian mode
    adv_contra = compute_fib_advisory(
        current_price=price_at_618,
        recent_high=high,
        recent_low=low,
        current_volume=140000.0,
        avg_volume=100000.0,
        mode="CONTRARIAN"
    )
    assert adv_contra.fib_zone_active is True
    assert adv_contra.contrarian_fib_buy_zone is True


def test_vam_fib_modulation():
    fib = FibAdvisory(
        fib_zone_active=True,
        fib_buy_signal=True,
        contrarian_fib_buy_zone=True
    )

    res_base = evaluate_oversold(100.0, 120000.0, 100000.0, mode="CONTRARIAN", fib=None)
    res_mod = evaluate_oversold(100.0, 120000.0, 100000.0, mode="CONTRARIAN", fib=fib)

    assert res_mod["buy_threshold"] < res_base["buy_threshold"]
    assert res_mod["recommended_weight"] > res_base["recommended_weight"]
    assert res_mod["contrarian_buy_signal"] is True
