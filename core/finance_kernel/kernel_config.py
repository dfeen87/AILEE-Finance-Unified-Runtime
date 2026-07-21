# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Configuration resolution and validation for the AILEE Finance Runtime Kernel."""

import os
import json
from core.finance_kernel.kernel_errors import KernelConfigurationError

class FinanceKernelConfig:
    """Finance Runtime Kernel configuration container with deterministic resolution."""

    def __init__(self,
                 operator_timeout: float = 30.0,
                 max_concurrent_operators: int = 4,
                 logging_level: str = "INFO",
                 strict_determinism: bool = True,
                 json_compat_mode: bool = False):
        self.operator_timeout = float(operator_timeout)
        self.max_concurrent_operators = int(max_concurrent_operators)
        self.logging_level = str(logging_level)
        self.strict_determinism = bool(strict_determinism)
        self.json_compat_mode = bool(json_compat_mode)

    def load_from_env(self) -> "FinanceKernelConfig":
        """Overrides configuration values with environment variables if present."""
        if "FINANCE_OPERATOR_TIMEOUT" in os.environ:
            try:
                self.operator_timeout = float(os.environ["FINANCE_OPERATOR_TIMEOUT"])
            except ValueError as e:
                raise KernelConfigurationError(f"Invalid FINANCE_OPERATOR_TIMEOUT environment variable: {e}")

        if "FINANCE_MAX_CONCURRENT_OPERATORS" in os.environ:
            try:
                self.max_concurrent_operators = int(os.environ["FINANCE_MAX_CONCURRENT_OPERATORS"])
            except ValueError as e:
                raise KernelConfigurationError(f"Invalid FINANCE_MAX_CONCURRENT_OPERATORS environment variable: {e}")

        if "FINANCE_LOGGING_LEVEL" in os.environ:
            self.logging_level = str(os.environ["FINANCE_LOGGING_LEVEL"])

        if "FINANCE_STRICT_DETERMINISM" in os.environ:
            val = os.environ["FINANCE_STRICT_DETERMINISM"].lower()
            self.strict_determinism = val in ("true", "1", "yes", "on")

        if "FINANCE_JSON_COMPAT_MODE" in os.environ:
            val = os.environ["FINANCE_JSON_COMPAT_MODE"].lower()
            self.json_compat_mode = val in ("true", "1", "yes", "on")

        return self

    def load_from_file(self, path: str) -> "FinanceKernelConfig":
        """Loads and overrides configuration values from a JSON file."""
        if not path:
            return self

        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except Exception as e:
            raise KernelConfigurationError(f"Failed to load or parse configuration file {path}: {e}")

        if not isinstance(data, dict):
            raise KernelConfigurationError("Configuration file content must be a JSON dictionary")

        self._apply_dict(data)
        return self

    def merge_overrides(self, overrides_dict: dict) -> "FinanceKernelConfig":
        """Merges explicit dictionary overrides into the configuration."""
        if not overrides_dict:
            return self
        if not isinstance(overrides_dict, dict):
            raise KernelConfigurationError("Configuration overrides must be a dictionary")

        self._apply_dict(overrides_dict)
        return self

    def _apply_dict(self, data: dict):
        if "operator_timeout" in data:
            try:
                self.operator_timeout = float(data["operator_timeout"])
            except ValueError as e:
                raise KernelConfigurationError(f"Invalid operator_timeout: {e}")

        if "max_concurrent_operators" in data:
            try:
                self.max_concurrent_operators = int(data["max_concurrent_operators"])
            except ValueError as e:
                raise KernelConfigurationError(f"Invalid max_concurrent_operators: {e}")

        if "logging_level" in data:
            self.logging_level = str(data["logging_level"])

        if "strict_determinism" in data:
            self.strict_determinism = bool(data["strict_determinism"])

        if "json_compat_mode" in data:
            self.json_compat_mode = bool(data["json_compat_mode"])

    def to_dict(self) -> dict:
        """Serializes current configuration to a dictionary."""
        return {
            "operator_timeout": self.operator_timeout,
            "max_concurrent_operators": self.max_concurrent_operators,
            "logging_level": self.logging_level,
            "strict_determinism": self.strict_determinism,
            "json_compat_mode": self.json_compat_mode
        }
