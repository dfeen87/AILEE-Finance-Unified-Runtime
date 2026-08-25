import pytest

def test_bullishness_mode_weighting_multipliers():
    # Test multipliers for STANDARD, CONSERVATIVE, HYPER
    modes = {
        "STANDARD": {"vol_mult": 1.0, "depth_mult": 1.0},
        "CONSERVATIVE": {"vol_mult": 1.08, "depth_mult": 1.10},
        "HYPER": {"vol_mult": 1.25, "depth_mult": 1.30}
    }

    base_vol = 10.0
    base_depth = 100.0

    # Conservative
    c_vol = base_vol * modes["CONSERVATIVE"]["vol_mult"]
    c_depth = base_depth * modes["CONSERVATIVE"]["depth_mult"]
    assert abs(c_vol - 10.8) < 1e-6
    assert abs(c_depth - 110.0) < 1e-6

    # Hyper
    h_vol = base_vol * modes["HYPER"]["vol_mult"]
    h_depth = base_depth * modes["HYPER"]["depth_mult"]
    assert h_vol == 12.5
    assert h_depth == 130.0
