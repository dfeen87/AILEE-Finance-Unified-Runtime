# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Operator registry and implementations for the AILEE Finance Runtime Kernel."""

import copy
import hashlib
import json
import math
import threading
from dataclasses import asdict
from core.finance_kernel.kernel_errors import KernelConfigurationError, OperatorNotFoundError, KernelContextError

from core.finance_kernel.model_pipeline import (
    ModelSignal, Decision, AILLEConfig, AILLEEngine,
    apply_safety_layer, check_consensus, snapshot_signals,
)


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

        signals = snapshot_signals(raw_signals)

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

        signals = snapshot_signals(raw_signals)

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
        signals = snapshot_signals(raw_signals)

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
    """Canonical SHA-256 audit chain scoped to this operator instance.

    Records are snapshotted and linked in memory. Persistence, external anchors
    and authenticated storage are deployment responsibilities, not supplied by
    an unkeyed digest or by the ordinary per-call kernel factory.
    """
    role = "ledger"

    def __init__(self, context=None, config=None):
        super().__init__(context, config)
        self._records = []
        self._ledger_lock = threading.Lock()

    @staticmethod
    def _canonical(value):
        # Reject unsupported objects, cycles, ambiguous non-string map keys,
        # and non-finite nested evidence rather than stringify unknown data.
        def validate(item):
            if item is None or type(item) in (str, bool, int):
                return
            if type(item) is float:
                if not math.isfinite(item):
                    raise ValueError("ledger numeric evidence must be finite")
                return
            if isinstance(item, list):
                for child in item:
                    validate(child)
                return
            if isinstance(item, dict) and all(type(key) is str for key in item):
                for child in item.values():
                    validate(child)
                return
            raise ValueError("ledger evidence must use JSON values with string map keys")
        try:
            validate(value)
            return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False, allow_nan=False)
        except (TypeError, ValueError, OverflowError, RecursionError) as error:
            raise ValueError("ledger evidence must have a finite canonical JSON encoding") from error

    @classmethod
    def _verify(cls, records):
        previous = "0" * 64
        try:
            for sequence, record in enumerate(records, 1):
                evidence = {key: value for key, value in record.items() if key != "hash"}
                if evidence["sequence"] != sequence or evidence["previous_hash"] != previous:
                    return False
                digest = hashlib.sha256(cls._canonical(evidence).encode("utf-8")).hexdigest()
                if record["hash"] != digest:
                    return False
                previous = digest
            return True
        except (ValueError, TypeError, KeyError):
            return False

    def verify_integrity(self):
        with self._ledger_lock:
            return self._verify(self._records)

    def get_audit_trail(self):
        with self._ledger_lock:
            return copy.deepcopy(self._records)

    def execute(self, input_data: dict) -> dict:
        if not isinstance(input_data, dict):
            raise ValueError("ledger input must be a dictionary")
        decision = input_data.get("decision")
        if isinstance(decision, Decision):
            decision = asdict(decision)
        if not isinstance(decision, dict):
            raise ValueError("ledger requires a decision object or dictionary")
        ctx = input_data.get("context")
        if ctx is None:
            ctx = {}
        if not isinstance(ctx, dict):
            raise ValueError("ledger context must be a dictionary")
        # JSON round-trip snapshots all supplied evidence before history access.
        evidence = json.loads(self._canonical({"decision": decision, "context": ctx}))
        decision, ctx = evidence["decision"], evidence["context"]
        for field in ("final_value", "confidence"):
            value = decision.get(field)
            if type(value) not in (float, int):
                raise ValueError(f"ledger decision {field} must be a finite number")
            try:
                valid = math.isfinite(value)
            except OverflowError:
                valid = False
            if not valid:
                raise ValueError(f"ledger decision {field} must be a finite number")
        if not 0 <= decision["confidence"] <= 1:
            raise ValueError("ledger confidence must be in [0, 1]")
        if type(decision.get("status")) is not str or not decision["status"]:
            raise ValueError("ledger decision status must be a non-empty string")
        with self._ledger_lock:
            if not self._verify(self._records):
                raise ValueError("ledger chain integrity failure")
            record = {
                "ledger_id": ctx.get("ledger_id", "default-ledger"),
                "session_id": ctx.get("session_id", "default-session"),
                "environment": ctx.get("environment", "dev"),
                "final_value": decision["final_value"], "status": decision["status"],
                "confidence": decision["confidence"], "decision": decision, "context": ctx,
                "schema": "ailee-finance-ledger/v2", "sequence": len(self._records) + 1,
                "previous_hash": self._records[-1]["hash"] if self._records else "0" * 64,
            }
            digest = hashlib.sha256(self._canonical(record).encode("utf-8")).hexdigest()
            record["hash"] = digest
            self._records.append(copy.deepcopy(record))
            return {"status": "recorded", "hash": digest, "record": record}


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
    from core.finance_kernel.volume_advisory import IntradayVolumeAdvisory
    from core.finance_kernel.wnfs import WNFSOperator
    from core.finance_kernel.unified_runtime import UnifiedRuntimeOperator
    reg = KernelRegistry()
    reg.register_operator("risk_operator", RiskOperator)
    reg.register_operator("governor_operator", GovernorOperator)
    reg.register_operator("transaction_operator", TransactionOperator)
    reg.register_operator("ledger_operator", LedgerOperator)
    reg.register_operator("volume_operator", IntradayVolumeAdvisory, role="governor")
    reg.register_operator("wnfs_operator", WNFSOperator, role="governor")
    reg.register_operator("unified_runtime_operator", UnifiedRuntimeOperator, role="governor")
    return reg
