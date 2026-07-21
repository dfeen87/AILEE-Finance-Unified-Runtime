# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Context propagation for the AILEE Finance Runtime Kernel."""

import copy
from core.finance_kernel.kernel_errors import KernelContextError

class FinanceKernelContext:
    """Immutable context propagation container for finance operations."""

    def __init__(self, ledger_id: str, session_id: str, user_id: str = None, environment: str = "dev", metadata: dict = None):
        if not ledger_id:
            raise KernelContextError("ledger_id is required and cannot be empty")
        if not session_id:
            raise KernelContextError("session_id is required and cannot be empty")

        self.__dict__["_locked"] = False
        self.__dict__["ledger_id"] = str(ledger_id)
        self.__dict__["session_id"] = str(session_id)
        self.__dict__["user_id"] = str(user_id) if user_id is not None else None
        self.__dict__["environment"] = str(environment)
        self.__dict__["metadata"] = copy.deepcopy(metadata) if metadata is not None else {}
        self.__dict__["_locked"] = True

    def __setattr__(self, name: str, value):
        if getattr(self, "_locked", False):
            raise KernelContextError("FinanceKernelContext is immutable once created. Use with_updates() to spawn an updated context.")
        super().__setattr__(name, value)

    def __delattr__(self, name: str):
        if getattr(self, "_locked", False):
            raise KernelContextError("FinanceKernelContext is immutable once created.")
        super().__delattr__(name)

    def to_dict(self) -> dict:
        """Serializes the context to a standard Python dictionary."""
        return {
            "ledger_id": self.ledger_id,
            "session_id": self.session_id,
            "user_id": self.user_id,
            "environment": self.environment,
            "metadata": copy.deepcopy(self.metadata)
        }

    @classmethod
    def from_dict(cls, data: dict) -> "FinanceKernelContext":
        """Reconstructs a context instance from a serialized dictionary."""
        if not isinstance(data, dict):
            raise KernelContextError("Context data must be a dictionary")

        required = ["ledger_id", "session_id"]
        for field in required:
            if field not in data:
                raise KernelContextError(f"Missing required field in context serialization: {field}")

        return cls(
            ledger_id=data["ledger_id"],
            session_id=data["session_id"],
            user_id=data.get("user_id"),
            environment=data.get("environment", "dev"),
            metadata=data.get("metadata")
        )

    def with_updates(self, **kwargs) -> "FinanceKernelContext":
        """Spawns a new updated, immutable context with the given attribute overrides."""
        current = self.to_dict()
        current.update(kwargs)
        return self.from_dict(current)
