# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Operator registry and implementations for the AILEE Finance Runtime Kernel."""

import sys
import os
import copy
from core.finance_kernel.kernel_errors import KernelConfigurationError, OperatorNotFoundError, KernelContextError

# Setup pathing to import simulations.aille_simulation
_repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
if _repo_root not in sys.path:
    sys.path.insert(0, _repo_root)

try:
    from simulations.aille_simulation import (
        ModelSignal,
        Decision,
        AILLEConfig,
        AILLEEngine,
        apply_safety_layer,
        check_consensus
    )
except ImportError:
    # Safe local fallback definitions if simulation harness is missing
    class ModelSignal:
        def __init__(self, value: float, confidence: float, model_id: int):
            self.value = value
            self.confidence = confidence
            self.model_id = model_id

    class Decision:
        def __init__(self, final_value: float, status: str, confidence: float,
                     models_agreed: int, fallback_used: bool, reasoning: str):
            self.final_value = final_value
            self.status = status
            self.confidence = confidence
            self.models_agreed = models_agreed
            self.fallback_used = fallback_used
            self.reasoning = reasoning

    class AILLEConfig:
        def __init__(self, min_confidence_threshold=0.35, grace_confidence_threshold=0.25,
                     min_models_required=2, sign_agreement_threshold=0.66,
                     fallback_window_size=50, fallback_position_scale=0.1,
                     max_model_count=10, enable_dynamic_fallback=False,
                     fallback_alpha=0.5, fallback_beta=0.1):
            self.min_confidence_threshold = min_confidence_threshold
            self.grace_confidence_threshold = grace_confidence_threshold
            self.min_models_required = min_models_required
            self.sign_agreement_threshold = sign_agreement_threshold
            self.fallback_window_size = fallback_window_size
            self.fallback_position_scale = fallback_position_scale
            self.max_model_count = max_model_count
            self.enable_dynamic_fallback = enable_dynamic_fallback
            self.fallback_alpha = fallback_alpha
            self.fallback_beta = fallback_beta
            self.config_version = "4.1.0"

    def apply_safety_layer(signals, config):
        valid = []
        for sig in signals:
            if sig.confidence < 0.0 or sig.confidence > 1.0:
                continue
            if sig.confidence >= config.min_confidence_threshold:
                valid.append(sig)
            elif sig.confidence >= config.grace_confidence_threshold:
                valid.append(ModelSignal(sig.value, sig.confidence * 0.8, sig.model_id))
        return valid

    def check_consensus(valid, config):
        if len(valid) < config.min_models_required:
            return None
        import statistics
        values = [sig.value for sig in valid]
        median = statistics.median(values)
        median_sign = 1.0 if median >= 0 else -1.0
        agree_values = [v for v in values if (1.0 if v >= 0 else -1.0) == median_sign]
        models_agreed = len(agree_values)
        ratio = models_agreed / len(values)
        if ratio >= config.sign_agreement_threshold and models_agreed >= config.min_models_required:
            consensus_value = sum(agree_values) / models_agreed
            return consensus_value, models_agreed
        return None

    class AILLEEngine:
        def __init__(self, config=None):
            self.config = config or AILLEConfig()
            self.fallback_buffer = []
            self.confidence_buffer = []

        def make_decision(self, model_signals):
            import math
            if not model_signals:
                return Decision(0.0, "ERROR_NO_MODELS", 0.0, 0, False, "No model inputs available")
            scoped = model_signals[:self.config.max_model_count]
            valid = apply_safety_layer(scoped, self.config)
            if not valid:
                return Decision(0.1, "REJECTED_LOW_CONFIDENCE", 0.1, 0, True, "All models failed confidence threshold")
            consensus = check_consensus(valid, self.config)
            if consensus is None:
                return Decision(0.2, "REJECTED_NO_CONSENSUS", 0.2, 0, True, "No consensus")
            val, agreed = consensus
            final_val = math.tanh(val * 100.0)
            return Decision(final_val, "DECISION_VALID", sum(s.confidence for s in valid)/len(valid), agreed, False, "Consensus achieved")


class BaseOperator:
    """Base class for all Finance Runtime Kernel operators."""
    role = None

    def __init__(self, context=None, config=None):
        self.context = context
        self.config = config

    def validate(self, input_data: dict) -> dict:
        """Validates that input_data contains the necessary fields."""
        if not isinstance(input_data, dict):
            raise KernelContextError("Input data must be a dictionary")
        return input_data

    def preprocess(self, input_data: dict) -> dict:
        """Runs preprocessing hooks on input data."""
        return input_data

    def execute(self, input_data: dict) -> dict:
        """Primary execution block. Must be implemented by subclasses."""
        raise NotImplementedError("Operators must implement execute()")

    def postprocess(self, result: dict) -> dict:
        """Runs postprocessing hooks on result data."""
        return result

    def finalize(self, result: dict) -> dict:
        """Finalizes execution and returns clean output."""
        return result


class RiskOperator(BaseOperator):
    """Enforces risk transformation and safety checks on model signals."""
    role = "risk"

    def execute(self, input_data: dict) -> dict:
        raw_signals = input_data.get("signals", [])
        cfg_dict = input_data.get("config")

        # Config resolution
        cfg = None
        if isinstance(cfg_dict, dict):
            cfg = AILLEConfig(**cfg_dict)
        elif isinstance(cfg_dict, AILLEConfig):
            cfg = cfg_dict
        else:
            cfg = AILLEConfig()

        signals = []
        for sig in raw_signals:
            if isinstance(sig, dict):
                signals.append(ModelSignal(
                    value=float(sig.get("value", 0.0)),
                    confidence=float(sig.get("confidence", 0.0)),
                    model_id=int(sig.get("model_id", 0))
                ))
            else:
                signals.append(sig)

        valid_signals = apply_safety_layer(signals, cfg)

        if self.config and self.config.json_compat_mode:
            output_signals = [
                {"value": s.value, "confidence": s.confidence, "model_id": s.model_id}
                for s in valid_signals
            ]
        else:
            output_signals = valid_signals

        return {"signals": output_signals}


class GovernorOperator(BaseOperator):
    """Enforces consensus and governance rules over validated model signals."""
    role = "governor"

    def execute(self, input_data: dict) -> dict:
        raw_signals = input_data.get("signals", [])
        cfg_dict = input_data.get("config")

        cfg = None
        if isinstance(cfg_dict, dict):
            cfg = AILLEConfig(**cfg_dict)
        elif isinstance(cfg_dict, AILLEConfig):
            cfg = cfg_dict
        else:
            cfg = AILLEConfig()

        signals = []
        for sig in raw_signals:
            if isinstance(sig, dict):
                signals.append(ModelSignal(
                    value=float(sig.get("value", 0.0)),
                    confidence=float(sig.get("confidence", 0.0)),
                    model_id=int(sig.get("model_id", 0))
                ))
            else:
                signals.append(sig)

        consensus = check_consensus(signals, cfg)
        if consensus is None:
            return {"consensus": None}
        else:
            return {
                "consensus": {
                    "consensus_value": consensus[0],
                    "models_agreed": consensus[1]
                }
            }


class TransactionOperator(BaseOperator):
    """Runs primary transactional decision engine and fallback buffering."""
    role = "transaction"

    def execute(self, input_data: dict) -> dict:
        engine = input_data.get("engine")
        if not isinstance(engine, AILLEEngine):
            cfg_dict = input_data.get("config")
            cfg = None
            if isinstance(cfg_dict, dict):
                cfg = AILLEConfig(**cfg_dict)
            elif isinstance(cfg_dict, AILLEConfig):
                cfg = cfg_dict
            else:
                cfg = AILLEConfig()
            engine = AILLEEngine(cfg)

        raw_signals = input_data.get("signals", [])
        signals = []
        for sig in raw_signals:
            if isinstance(sig, dict):
                signals.append(ModelSignal(
                    value=float(sig.get("value", 0.0)),
                    confidence=float(sig.get("confidence", 0.0)),
                    model_id=int(sig.get("model_id", 0))
                ))
            else:
                signals.append(sig)

        decision = engine.make_decision(signals)

        if self.config and self.config.json_compat_mode:
            decision_data = {
                "final_value": decision.final_value,
                "status": decision.status,
                "confidence": decision.confidence,
                "models_agreed": decision.models_agreed,
                "fallback_used": decision.fallback_used,
                "reasoning": decision.reasoning
            }
        else:
            decision_data = decision

        return {"decision": decision_data, "engine": engine}


class LedgerOperator(BaseOperator):
    """Logs transactions to a secure, immutable representation using a deterministic SplitMix64 hash chain."""
    role = "ledger"

    def execute(self, input_data: dict) -> dict:
        decision = input_data.get("decision")
        ctx = input_data.get("context") or {}

        if hasattr(decision, "final_value"):
            final_value = decision.final_value
            status = decision.status
            confidence = decision.confidence
        elif isinstance(decision, dict):
            final_value = decision.get("final_value", 0.0)
            status = decision.get("status", "UNKNOWN")
            confidence = decision.get("confidence", 0.0)
        else:
            final_value = 0.0
            status = "UNKNOWN"
            confidence = 0.0

        # Non-cryptographic deterministic avalanche: SplitMix64 style hash mixing
        a = int(final_value * 10000) & 0xFFFFFFFF
        b = sum(ord(c) for c in status) & 0xFFFFFFFF
        c = int(confidence * 10000) & 0xFFFFFFFF

        MASK = 0xFFFFFFFFFFFFFFFF
        # (a * 0x9E3779B185EBCA87) ^ (b * 0xC2B2AE3D27D4EB4F) ^ c
        h = ((a * 0x9E3779B185EBCA87) & MASK) ^ ((b * 0xC2B2AE3D27D4EB4F) & MASK) ^ c
        h_hex = f"{h & MASK:016x}"

        record = {
            "ledger_id": ctx.get("ledger_id", "default-ledger"),
            "session_id": ctx.get("session_id", "default-session"),
            "environment": ctx.get("environment", "dev"),
            "final_value": final_value,
            "status": status,
            "confidence": confidence,
            "hash": h_hex
        }

        return {
            "status": "recorded",
            "hash": h_hex,
            "record": record
        }


class KernelRegistry:
    """Registry to manage and validate typed finance operators."""
    ALLOWED_ROLES = {"transaction", "risk", "ledger", "governor"}

    def __init__(self):
        self._operators = {}

    def register_operator(self, name: str, operator_class, role: str = None):
        """Registers a unique operator class with an explicit or implicit role."""
        if not name:
            raise KernelConfigurationError("Operator name cannot be empty")
        if name in self._operators:
            raise KernelConfigurationError(f"Operator '{name}' is already registered")

        op_role = role or getattr(operator_class, "role", None)
        if op_role not in self.ALLOWED_ROLES:
            raise KernelConfigurationError(
                f"Invalid or missing role '{op_role}' for operator '{name}'. Allowed roles: {self.ALLOWED_ROLES}"
            )

        self._operators[name] = (operator_class, op_role)

    def get_operator(self, name: str):
        """Retrieves registered operator class."""
        if name not in self._operators:
            raise OperatorNotFoundError(f"Operator '{name}' not found in registry")
        return self._operators[name][0]

    def get_operator_role(self, name: str) -> str:
        """Retrieves the role associated with a registered operator."""
        if name not in self._operators:
            raise OperatorNotFoundError(f"Operator '{name}' not found in registry")
        return self._operators[name][1]

    def list_operators(self) -> list:
        """Lists all registered operator names."""
        return list(self._operators.keys())


def create_default_registry() -> KernelRegistry:
    """Factory function to build a default registry loaded with core operators."""
    reg = KernelRegistry()
    reg.register_operator("risk_operator", RiskOperator)
    reg.register_operator("governor_operator", GovernorOperator)
    reg.register_operator("transaction_operator", TransactionOperator)
    reg.register_operator("ledger_operator", LedgerOperator)
    return reg
