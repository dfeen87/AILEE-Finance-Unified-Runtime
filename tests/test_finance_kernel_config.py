# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Unit tests for the Finance Runtime Kernel Config."""

import os
import json
import pytest
from core.finance_kernel.kernel_config import FinanceKernelConfig
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
