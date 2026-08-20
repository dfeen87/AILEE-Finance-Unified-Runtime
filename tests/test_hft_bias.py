# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Unit and regression tests for HFT Bullish Bias Layer."""

import os
import pytest
from core.finance_kernel.hft_bias import is_bullish_mode_allowed
from core.finance_kernel.kernel_config import FinanceKernelConfig, validate_hft_bias_config, parse_config_file, KernelConfigurationError
from core.finance_kernel.volume_advisory import IntradayVolumeAdvisory, calculate_hft_delta_v
from core.finance_kernel.volume_execution import VolumeExecutionOperator
from ailee_finance.domains.finance.sell_governance import compute_sell_ceiling, detect_sell_manipulation
from ailee_finance.domains.finance.ailee_finance_domain import AileeFinanceDomain, SellGovernanceDecision


def test_is_bullish_mode_allowed_gating():
    cfg = {
        "enabled": True,
        "trust_threshold_bullish": 0.70,
        "manipulation_threshold": 0.30,
    }

    # 1. Normal allowed conditions
    assert is_bullish_mode_allowed(0.80, 0.10, False, cfg) is True

    # 2. Disabled config
    disabled_cfg = dict(cfg, enabled=False)
    assert is_bullish_mode_allowed(0.80, 0.10, False, disabled_cfg) is False

    # 3. Trust score below threshold
    assert is_bullish_mode_allowed(0.65, 0.10, False, cfg) is False

    # 4. Manipulation score above threshold
    assert is_bullish_mode_allowed(0.80, 0.35, False, cfg) is False

    # 5. Drawdown limit near breach / breached
    assert is_bullish_mode_allowed(0.80, 0.10, True, cfg) is False
    assert is_bullish_mode_allowed(0.80, 0.10, 0.045, cfg) is False
    assert is_bullish_mode_allowed(0.80, 0.10, {"near_breach": True}, cfg) is False


def test_config_validation_and_yaml_loading(tmp_path):
    # Valid config
    valid = validate_hft_bias_config({
        "enabled": True,
        "bullish_multiplier_price": 1.10,
        "bullish_multiplier_volume": 1.05,
        "bullish_execution_scale": 1.15,
        "bullish_sell_ceiling_factor": 0.85,
        "trust_threshold_bullish": 0.75,
        "manipulation_threshold": 0.25,
    })
    assert valid["bullish_multiplier_price"] == 1.10

    # Invalid multiplier out of bounds
    with pytest.raises(KernelConfigurationError):
        validate_hft_bias_config({"bullish_multiplier_price": 1.80})

    # Invalid threshold out of bounds
    with pytest.raises(KernelConfigurationError):
        validate_hft_bias_config({"trust_threshold_bullish": 1.50})

    # Test loading YAML config file
    yaml_file = tmp_path / "ailee_hft_config.yaml"
    yaml_file.write_text("""
hft_bias:
  enabled: true
  bullish_multiplier_price: 1.08
  bullish_multiplier_volume: 1.08
  bullish_execution_scale: 1.12
  bullish_sell_ceiling_factor: 0.75
  trust_threshold_bullish: 0.72
  manipulation_threshold: 0.28
""")
    kernel_cfg = FinanceKernelConfig()
    kernel_cfg.load_from_file(str(yaml_file))
    assert kernel_cfg.hft_bias["bullish_multiplier_price"] == 1.08
    assert kernel_cfg.hft_bias["bullish_sell_ceiling_factor"] == 0.75


def test_ailee_math_delta_v_unmodified():
    # Verify calculate_hft_delta_v math function behaves consistently
    ticks = [{
        "p_input": 0.08,
        "w": 0.1,
        "v": 2.0,
        "M": 1.0,
        "dt": 0.001
    }]
    v1 = calculate_hft_delta_v(1.0, 0.95, 0.1, 0.0, ticks)
    v2 = calculate_hft_delta_v(1.0, 0.95, 0.1, 0.0, ticks)
    assert v1 > 0.0
    assert v1 == v2  # Strict determinism


def test_pre_physics_and_post_delta_v_scaling():
    op = IntradayVolumeAdvisory()
    input_data = {
        "current_volume": 20000.0,
        "avg_volume": 10000.0,
        "price_change": 0.008,
        "vwap_deviation": 0.0,
        "enable_hft": True,
        "hft_p_input": 0.08,
        "hft_mass": 1.0,
        "trust_score": 0.85,
        "manipulation_score": 0.0
    }
    processed = op.preprocess(input_data)

    # With bullish mode active
    res_bullish = op.execute(processed)
    assert res_bullish["hft_active"] is True
    assert res_bullish["hft_delta_v"] > 0.0

    # Neutral run with bias disabled
    input_data_disabled = dict(processed)
    input_data_disabled["trust_score"] = 0.50  # Below 0.70 threshold -> bullish inactive
    res_neutral = op.execute(input_data_disabled)

    # Bullish active execution weight should be measurably higher than neutral weight
    assert res_bullish["recommended_weight"] > res_neutral["recommended_weight"]


def test_sell_governance_bullish_bias_and_level_3_override(tmp_path):
    domain = AileeFinanceDomain(log_path=str(tmp_path / "sell.log"))

    # Level 0 (High trust): allowed_sell_amount is reduced by bullish_sell_ceiling_factor (0.80)
    sig_level_0 = {
        "position_size": 1000.0,
        "trust_score": 0.90,
        "volatility": 0.10,
        "market": {"liquidity": 1.0},
        "feeds": [
            {"price": 100.0, "confidence": 0.95},
            {"price": 100.1, "confidence": 0.95}
        ],
        "intent_flag": True
    }
    dec_level_0 = domain.evaluate_sell(sig_level_0)
    assert dec_level_0.level == 0
    assert dec_level_0.bullish_mode_active is True
    assert dec_level_0.allowed_sell_amount == 800.0  # 1000.0 * 1.0 * 0.80

    # Level 3 (Protective mode due to invalid intent or critical risk):
    # Level 3 overrides bullish bias and restores standard safety behavior (10% ceiling)
    sig_level_3 = dict(sig_level_0, intent_flag=False, intent_reason="Order invalidated")
    dec_level_3 = domain.evaluate_sell(sig_level_3)
    assert dec_level_3.level == 3
    assert dec_level_3.allowed_sell_amount == 100.0  # 1000.0 * 0.10 (Not reduced to 80.0!)


def test_sell_governance_downward_manipulation_sensitivity(tmp_path):
    domain = AileeFinanceDomain(log_path=str(tmp_path / "sell.log"))

    # Market with spoofed bids & liquidity drop (manipulation_score > 0)
    sig_manip = {
        "position_size": 1000.0,
        "trust_score": 0.90,
        "volatility": 0.10,
        "market": {
            "spoofed_bids": True,
            "bid_liquidity_drop": 0.50,
            "liquidity": 1.0
        },
        "feeds": [{"price": 100.0, "confidence": 0.50}],  # Low consensus
        "intent_flag": True
    }
    dec_manip = domain.evaluate_sell(sig_manip)
    assert dec_manip.manipulation_score > 0.30
    # Downward manipulation dampening reduces allowed sell amount significantly
    assert dec_manip.allowed_sell_amount < 500.0


def test_neutral_fallback_when_bias_disabled(tmp_path):
    domain = AileeFinanceDomain(
        log_path=str(tmp_path / "sell.log"),
        hft_bias_config={"enabled": False}
    )
    sig = {
        "position_size": 1000.0,
        "trust_score": 0.90,
        "volatility": 0.10,
        "market": {"liquidity": 1.0},
        "feeds": [
            {"price": 100.0, "confidence": 0.95},
            {"price": 100.1, "confidence": 0.95}
        ],
        "intent_flag": True
    }
    dec = domain.evaluate_sell(sig)
    assert dec.bullish_mode_active is False
    assert dec.allowed_sell_amount == 1000.0  # Returns to 100% neutral cap


def test_volume_execution_operator_bullish_bias_defaults_and_toggle(tmp_path):
    # Default instance: controlled bullish bias is ON by default
    op_default = VolumeExecutionOperator(audit_log_file=str(tmp_path / "audit_default.log"))
    assert op_default.hft_bias_config["enabled"] is True

    # Disabled instance: controlled bullish bias explicitly turned OFF
    op_disabled = VolumeExecutionOperator(
        audit_log_file=str(tmp_path / "audit_disabled.log"),
        hft_bias_config={"enabled": False}
    )
    assert op_disabled.hft_bias_config["enabled"] is False
