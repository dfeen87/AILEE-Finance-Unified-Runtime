# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Unit tests for the Finance Runtime Kernel Config."""

import os
import json
import math
import sys
import pytest
from core.finance_kernel.kernel_config import FinanceKernelConfig, validate_hft_bias_config
from core.finance_kernel.kernel_errors import KernelConfigurationError

def test_config_defaults():
    config = FinanceKernelConfig()
    assert config.operator_timeout == 30.0
    assert config.max_concurrent_operators == 4
    assert config.logging_level == "INFO"
    assert config.strict_determinism is True
    assert config.json_compat_mode is False


def test_config_from_env(monkeypatch):
    monkeypatch.setenv("FINANCE_OPERATOR_TIMEOUT", "15.5")
    monkeypatch.setenv("FINANCE_MAX_CONCURRENT_OPERATORS", "8")
    monkeypatch.setenv("FINANCE_LOGGING_LEVEL", "DEBUG")
    monkeypatch.setenv("FINANCE_STRICT_DETERMINISM", "false")
    monkeypatch.setenv("FINANCE_JSON_COMPAT_MODE", "true")

    config = FinanceKernelConfig()
    config.load_from_env()

    assert config.operator_timeout == 15.5
    assert config.max_concurrent_operators == 8
    assert config.logging_level == "DEBUG"
    assert config.strict_determinism is False
    assert config.json_compat_mode is True


def test_config_from_env_invalid(monkeypatch):
    monkeypatch.setenv("FINANCE_OPERATOR_TIMEOUT", "abc")
    config = FinanceKernelConfig()
    with pytest.raises(KernelConfigurationError):
        config.load_from_env()


def test_config_from_file(tmp_path):
    p = tmp_path / "test_config.json"
    cfg_data = {
        "operator_timeout": 45.0,
        "max_concurrent_operators": 2,
        "logging_level": "WARNING",
        "strict_determinism": False,
        "json_compat_mode": True
    }
    p.write_text(json.dumps(cfg_data))

    config = FinanceKernelConfig()
    config.load_from_file(str(p))

    assert config.operator_timeout == 45.0
    assert config.max_concurrent_operators == 2
    assert config.logging_level == "WARNING"
    assert config.strict_determinism is False
    assert config.json_compat_mode is True


def test_config_merge_overrides():
    config = FinanceKernelConfig()
    config.merge_overrides({
        "operator_timeout": 12.0,
        "json_compat_mode": True
    })

    assert config.operator_timeout == 12.0
    assert config.max_concurrent_operators == 4  # unchanged
    assert config.json_compat_mode is True


def test_config_resolution_precedence(monkeypatch, tmp_path):
    # Order: explicit overrides > file config > environment defaults > system defaults

    # 1. System Defaults
    config = FinanceKernelConfig()
    assert config.operator_timeout == 30.0

    # 2. Env Overrides
    monkeypatch.setenv("FINANCE_OPERATOR_TIMEOUT", "10.0")
    config.load_from_env()
    assert config.operator_timeout == 10.0

    # 3. File Overrides
    p = tmp_path / "config.json"
    p.write_text(json.dumps({"operator_timeout": 20.0}))
    config.load_from_file(str(p))
    assert config.operator_timeout == 20.0

    # 4. Explicit overrides
    config.merge_overrides({"operator_timeout": 5.0})
    assert config.operator_timeout == 5.0


@pytest.mark.parametrize("value", [0, -1, float("nan"), float("inf")])
def test_operator_timeout_rejects_non_positive_or_non_finite_values(value):
    with pytest.raises(KernelConfigurationError):
        FinanceKernelConfig(operator_timeout=value)


@pytest.mark.parametrize("value", [0, -1, True, False, 4.9, float("nan"), float("inf"), "4", None])
def test_max_concurrent_operators_requires_positive_value(value):
    with pytest.raises(KernelConfigurationError):
        FinanceKernelConfig(max_concurrent_operators=value)


@pytest.mark.parametrize("value", [1, 4, 4.0])
def test_max_concurrent_operators_accepts_positive_integral_values(value):
    assert FinanceKernelConfig(max_concurrent_operators=value).max_concurrent_operators == int(value)


@pytest.mark.parametrize("value", ["0", "-1", "4.9", "nan", "inf", "true"])
def test_max_concurrent_operators_env_rejects_non_positive_integer(monkeypatch, value):
    monkeypatch.setenv("FINANCE_MAX_CONCURRENT_OPERATORS", value)
    with pytest.raises(KernelConfigurationError):
        FinanceKernelConfig().load_from_env()


def test_invalid_update_is_atomic():
    config = FinanceKernelConfig()
    original = config.to_dict()

    with pytest.raises(KernelConfigurationError):
        config.merge_overrides({
            "operator_timeout": 5.0,
            "hft_bias": {"trust_threshold_bullish": float("nan")},
        })

    assert config.to_dict() == original


def test_string_booleans_are_rejected_in_structured_overrides():
    config = FinanceKernelConfig()
    with pytest.raises(KernelConfigurationError):
        config.merge_overrides({"strict_determinism": "false"})


@pytest.mark.parametrize("field", ["operator_timeout", "contrarian_oversold_aggressiveness"])
@pytest.mark.parametrize("source", ["constructor", "override"])
def test_unrepresentable_numeric_configuration_uses_configuration_error(field, source):
    value = 10 ** 400
    config = FinanceKernelConfig()
    original = config.to_dict()
    with pytest.raises(KernelConfigurationError):
        if source == "constructor":
            FinanceKernelConfig(**{field: value})
        else:
            config.merge_overrides({"logging_level": "DEBUG", field: value})
    assert config.to_dict() == original


@pytest.mark.parametrize("field", [
    "bullish_multiplier_price", "bullish_multiplier_volume", "bullish_execution_scale",
    "bullish_sell_ceiling_factor", "trust_threshold_bullish", "manipulation_threshold",
])
def test_unrepresentable_hft_numeric_configuration_uses_configuration_error(field):
    with pytest.raises(KernelConfigurationError):
        FinanceKernelConfig(hft_bias={field: 10 ** 400})


@pytest.mark.parametrize("value", [[], "", 0, False, [("enabled", False)]])
def test_constructor_rejects_non_dictionary_hft_bias(value):
    with pytest.raises(KernelConfigurationError):
        FinanceKernelConfig(hft_bias=value)


@pytest.mark.parametrize("value", [None, [], "", 0, False, {"hft_bias": None}, {"hft_bias": []}])
def test_hft_validator_rejects_non_dictionary_configuration(value):
    with pytest.raises(KernelConfigurationError):
        validate_hft_bias_config(value)


@pytest.mark.parametrize("value", [[], "", 0, False])
def test_falsey_non_dictionary_overrides_are_rejected_atomically(value):
    config = FinanceKernelConfig()
    original = config.to_dict()
    with pytest.raises(KernelConfigurationError):
        config.merge_overrides(value)
    assert config.to_dict() == original


def test_valid_configuration_boundaries_and_empty_overrides_remain_supported():
    config = FinanceKernelConfig(
        operator_timeout=sys.float_info.max,
        max_concurrent_operators=4.0,
        contrarian_oversold_aggressiveness=-0.0,
        hft_bias={
            "enabled": False,
            "bullish_multiplier_price": 1.0,
            "bullish_multiplier_volume": 1.5,
            "bullish_execution_scale": 1.5,
            "bullish_sell_ceiling_factor": 0.1,
            "trust_threshold_bullish": 0.0,
            "manipulation_threshold": 1.0,
        },
    )
    original = config.to_dict()
    assert config.operator_timeout == sys.float_info.max
    assert math.copysign(1.0, config.contrarian_oversold_aggressiveness) == -1.0
    assert config.merge_overrides({}) is config
    assert config.merge_overrides(None) is config
    assert config.to_dict() == original
    assert FinanceKernelConfig(**original).to_dict() == original
    assert validate_hft_bias_config({"hft_bias": config.hft_bias}) == config.hft_bias
