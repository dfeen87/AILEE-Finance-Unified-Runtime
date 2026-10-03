# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Configuration resolution and validation for the AILEE Finance Runtime Kernel."""

import os
import json
import math
from core.finance_kernel.kernel_errors import KernelConfigurationError

class FinanceKernelConfig:
    """Finance Runtime Kernel configuration container with deterministic resolution."""

    def __init__(self,
                 operator_timeout: float = 30.0,
                 max_concurrent_operators: int = 4,
                 logging_level: str = "INFO",
                 strict_determinism: bool = True,
                 json_compat_mode: bool = False,
                 enable_contrarian_oversold: bool = False,
                 contrarian_oversold_aggressiveness: float = 1.0,
                 hft_bias: dict = None):
        try:
            self.operator_timeout = float(operator_timeout)
            self.contrarian_oversold_aggressiveness = float(contrarian_oversold_aggressiveness)
        except (TypeError, ValueError) as e:
            raise KernelConfigurationError(f"Invalid numeric configuration: {e}") from e
        self.max_concurrent_operators = _require_positive_integer(
            "max_concurrent_operators", max_concurrent_operators
        )
        self.logging_level = str(logging_level)
        self.strict_determinism = _require_bool("strict_determinism", strict_determinism)
        self.json_compat_mode = _require_bool("json_compat_mode", json_compat_mode)
        self.enable_contrarian_oversold = _require_bool("enable_contrarian_oversold", enable_contrarian_oversold)

        default_hft_bias = {
            "enabled": True,
            "bullish_multiplier_price": 1.05,
            "bullish_multiplier_volume": 1.05,
            "bullish_execution_scale": 1.10,
            "bullish_sell_ceiling_factor": 0.80,
            "trust_threshold_bullish": 0.70,
            "manipulation_threshold": 0.30,
        }
        if hft_bias is not None:
            default_hft_bias.update(hft_bias)
        self.hft_bias = validate_hft_bias_config(default_hft_bias)
        self._validate()

    def load_from_env(self) -> "FinanceKernelConfig":
        """Overrides configuration values with environment variables if present."""
        updates = {}
        if "FINANCE_OPERATOR_TIMEOUT" in os.environ:
            updates["operator_timeout"] = os.environ["FINANCE_OPERATOR_TIMEOUT"]

        if "FINANCE_MAX_CONCURRENT_OPERATORS" in os.environ:
            updates["max_concurrent_operators"] = _parse_env_positive_integer(
                "FINANCE_MAX_CONCURRENT_OPERATORS"
            )

        if "FINANCE_LOGGING_LEVEL" in os.environ:
            updates["logging_level"] = os.environ["FINANCE_LOGGING_LEVEL"]

        if "FINANCE_STRICT_DETERMINISM" in os.environ:
            updates["strict_determinism"] = _parse_env_bool("FINANCE_STRICT_DETERMINISM")

        if "FINANCE_JSON_COMPAT_MODE" in os.environ:
            updates["json_compat_mode"] = _parse_env_bool("FINANCE_JSON_COMPAT_MODE")

        if "FINANCE_ENABLE_CONTRARIAN_OVERSOLD" in os.environ:
            updates["enable_contrarian_oversold"] = _parse_env_bool("FINANCE_ENABLE_CONTRARIAN_OVERSOLD")

        if "FINANCE_CONTRARIAN_OVERSOLD_AGGRESSIVENESS" in os.environ:
            updates["contrarian_oversold_aggressiveness"] = os.environ["FINANCE_CONTRARIAN_OVERSOLD_AGGRESSIVENESS"]

        return self.merge_overrides(updates)

    def load_from_file(self, path: str) -> "FinanceKernelConfig":
        """Loads and overrides configuration values from a JSON or YAML file."""
        if not path:
            return self

        data = parse_config_file(path)
        if not isinstance(data, dict):
            raise KernelConfigurationError(f"Configuration file content must be a dictionary: {path}")

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
        candidate = self.to_dict()
        if "operator_timeout" in data:
            try:
                candidate["operator_timeout"] = float(data["operator_timeout"])
            except (TypeError, ValueError) as e:
                raise KernelConfigurationError(f"Invalid operator_timeout: {e}")

        if "max_concurrent_operators" in data:
            candidate["max_concurrent_operators"] = _require_positive_integer(
                "max_concurrent_operators", data["max_concurrent_operators"]
            )

        if "logging_level" in data:
            candidate["logging_level"] = str(data["logging_level"])

        if "strict_determinism" in data:
            candidate["strict_determinism"] = _require_bool("strict_determinism", data["strict_determinism"])

        if "json_compat_mode" in data:
            candidate["json_compat_mode"] = _require_bool("json_compat_mode", data["json_compat_mode"])

        if "enable_contrarian_oversold" in data:
            candidate["enable_contrarian_oversold"] = _require_bool("enable_contrarian_oversold", data["enable_contrarian_oversold"])

        if "contrarian_oversold_aggressiveness" in data:
            try:
                candidate["contrarian_oversold_aggressiveness"] = float(data["contrarian_oversold_aggressiveness"])
            except (TypeError, ValueError) as e:
                raise KernelConfigurationError(f"Invalid contrarian_oversold_aggressiveness: {e}")

        if "hft_bias" in data:
            if not isinstance(data["hft_bias"], dict):
                raise KernelConfigurationError("hft_bias must be a dictionary")
            candidate["hft_bias"].update(data["hft_bias"])

        validated = FinanceKernelConfig(**candidate)
        self.__dict__.update(validated.__dict__)

    def _validate(self):
        if not math.isfinite(self.operator_timeout) or self.operator_timeout <= 0.0:
            raise KernelConfigurationError("operator_timeout must be finite and greater than zero")
        if self.max_concurrent_operators < 1:
            raise KernelConfigurationError("max_concurrent_operators must be at least one")
        if self.logging_level.upper() not in {"CRITICAL", "ERROR", "WARNING", "INFO", "DEBUG", "NOTSET"}:
            raise KernelConfigurationError(f"Unsupported logging_level: {self.logging_level}")
        self.logging_level = self.logging_level.upper()
        if not math.isfinite(self.contrarian_oversold_aggressiveness) or self.contrarian_oversold_aggressiveness < 0.0:
            raise KernelConfigurationError("contrarian_oversold_aggressiveness must be finite and non-negative")

    def to_dict(self) -> dict:
        """Serializes current configuration to a dictionary."""
        return {
            "operator_timeout": self.operator_timeout,
            "max_concurrent_operators": self.max_concurrent_operators,
            "logging_level": self.logging_level,
            "strict_determinism": self.strict_determinism,
            "json_compat_mode": self.json_compat_mode,
            "enable_contrarian_oversold": self.enable_contrarian_oversold,
            "contrarian_oversold_aggressiveness": self.contrarian_oversold_aggressiveness,
            "hft_bias": dict(self.hft_bias)
        }


def parse_config_file(path: str) -> dict:
    """Parses JSON or YAML configuration files."""
    if not path or not os.path.exists(path):
        raise KernelConfigurationError(f"Configuration file not found: {path}")

    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    try:
        return json.loads(content)
    except Exception:
        pass

    try:
        import yaml
        parsed = yaml.safe_load(content)
        if isinstance(parsed, dict):
            return parsed
    except ImportError:
        pass

    result = {}
    current_section = None
    for line in content.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if stripped.endswith(":") and not ":" in stripped[:-1]:
            sec = stripped[:-1].strip()
            current_section = {}
            result[sec] = current_section
        elif ":" in stripped:
            parts = stripped.split(":", 1)
            k = parts[0].strip()
            v_str = parts[1].strip()
            if v_str.lower() == "true":
                val = True
            elif v_str.lower() == "false":
                val = False
            else:
                try:
                    val = float(v_str) if "." in v_str else int(v_str)
                except ValueError:
                    val = v_str
            if current_section is not None and not line.startswith(k):
                current_section[k] = val
            else:
                current_section = None
                result[k] = val
    return result


def validate_hft_bias_config(cfg_dict: dict) -> dict:
    """Validates hft_bias configuration dictionary against strict bounds."""
    hft_bias = cfg_dict.get("hft_bias", cfg_dict) if isinstance(cfg_dict, dict) else {}
    enabled = _require_bool("hft_bias.enabled", hft_bias.get("enabled", True))

    try:
        price_mult = float(hft_bias.get("bullish_multiplier_price", 1.05))
        vol_mult = float(hft_bias.get("bullish_multiplier_volume", 1.05))
        exec_scale = float(hft_bias.get("bullish_execution_scale", 1.10))
        sell_factor = float(hft_bias.get("bullish_sell_ceiling_factor", 0.80))
        trust_thresh = float(hft_bias.get("trust_threshold_bullish", 0.70))
        manip_thresh = float(hft_bias.get("manipulation_threshold", 0.30))
    except (TypeError, ValueError) as e:
        raise KernelConfigurationError(f"Invalid hft_bias numeric configuration: {e}") from e

    numeric_values = {
        "bullish_multiplier_price": price_mult,
        "bullish_multiplier_volume": vol_mult,
        "bullish_execution_scale": exec_scale,
        "bullish_sell_ceiling_factor": sell_factor,
        "trust_threshold_bullish": trust_thresh,
        "manipulation_threshold": manip_thresh,
    }
    for name, value in numeric_values.items():
        if not math.isfinite(value):
            raise KernelConfigurationError(f"{name} must be finite: {value}")

    if price_mult < 1.0 or price_mult > 1.5:
        raise KernelConfigurationError(f"bullish_multiplier_price out of bounds [1.0, 1.5]: {price_mult}")
    if vol_mult < 1.0 or vol_mult > 1.5:
        raise KernelConfigurationError(f"bullish_multiplier_volume out of bounds [1.0, 1.5]: {vol_mult}")
    if exec_scale < 1.0 or exec_scale > 1.5:
        raise KernelConfigurationError(f"bullish_execution_scale out of bounds [1.0, 1.5]: {exec_scale}")
    if sell_factor < 0.1 or sell_factor > 1.0:
        raise KernelConfigurationError(f"bullish_sell_ceiling_factor out of bounds [0.1, 1.0]: {sell_factor}")
    if trust_thresh < 0.0 or trust_thresh > 1.0:
        raise KernelConfigurationError(f"trust_threshold_bullish out of bounds [0.0, 1.0]: {trust_thresh}")
    if manip_thresh < 0.0 or manip_thresh > 1.0:
        raise KernelConfigurationError(f"manipulation_threshold out of bounds [0.0, 1.0]: {manip_thresh}")

    return {
        "enabled": enabled,
        "bullish_multiplier_price": price_mult,
        "bullish_multiplier_volume": vol_mult,
        "bullish_execution_scale": exec_scale,
        "bullish_sell_ceiling_factor": sell_factor,
        "trust_threshold_bullish": trust_thresh,
        "manipulation_threshold": manip_thresh,
    }


def _require_bool(name: str, value) -> bool:
    if not isinstance(value, bool):
        raise KernelConfigurationError(f"{name} must be a boolean")
    return value


def _require_positive_integer(name: str, value) -> int:
    """Validate a structured integer without truthiness or truncation coercion."""
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise KernelConfigurationError(f"{name} must be a positive integer")
    if isinstance(value, float) and (not math.isfinite(value) or not value.is_integer()):
        raise KernelConfigurationError(f"{name} must be a finite positive integer")
    result = int(value)
    if result < 1:
        raise KernelConfigurationError(f"{name} must be at least one")
    return result


def _parse_env_positive_integer(name: str) -> int:
    """Parse the documented decimal-integer environment-variable surface."""
    value = os.environ[name].strip()
    if not value.isdecimal():
        raise KernelConfigurationError(f"Invalid {name} positive integer: {os.environ[name]}")
    return _require_positive_integer(name, int(value))


def _parse_env_bool(name: str) -> bool:
    value = os.environ[name].strip().lower()
    if value in ("true", "1", "yes", "on"):
        return True
    if value in ("false", "0", "no", "off"):
        return False
    raise KernelConfigurationError(f"Invalid {name} boolean: {os.environ[name]}")
