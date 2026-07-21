# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Unit tests for the Finance Runtime Kernel Context."""

import pytest
from core.finance_kernel.kernel_context import FinanceKernelContext
from core.finance_kernel.kernel_errors import KernelContextError

def test_context_creation():
    ctx = FinanceKernelContext(
        ledger_id="test-ledger",
        session_id="test-session",
        user_id="user123",
        environment="test",
        metadata={"source": "api"}
    )
    assert ctx.ledger_id == "test-ledger"
    assert ctx.session_id == "test-session"
    assert ctx.user_id == "user123"
    assert ctx.environment == "test"
    assert ctx.metadata == {"source": "api"}


def test_context_validation():
    # ledger_id cannot be empty
    with pytest.raises(KernelContextError):
        FinanceKernelContext("", "session-123")

    # session_id cannot be empty
    with pytest.raises(KernelContextError):
        FinanceKernelContext("ledger-123", "")


def test_context_immutability():
    ctx = FinanceKernelContext("ledger-1", "session-1")

    with pytest.raises(KernelContextError):
        ctx.ledger_id = "ledger-2"

    with pytest.raises(KernelContextError):
        ctx.environment = "prod"

    with pytest.raises(KernelContextError):
        del ctx.session_id


def test_context_serialization_reconstruction():
    ctx = FinanceKernelContext(
        ledger_id="ledger-1",
        session_id="session-1",
        user_id="user-1",
        environment="prod",
        metadata={"version": 1}
    )

    d = ctx.to_dict()
    assert d["ledger_id"] == "ledger-1"
    assert d["session_id"] == "session-1"
    assert d["user_id"] == "user-1"
    assert d["environment"] == "prod"
    assert d["metadata"] == {"version": 1}

    # Reconstruct
    rebuilt = FinanceKernelContext.from_dict(d)
    assert rebuilt.ledger_id == "ledger-1"
    assert rebuilt.session_id == "session-1"
    assert rebuilt.user_id == "user-1"
    assert rebuilt.environment == "prod"
    assert rebuilt.metadata == {"version": 1}


def test_context_spawn_updates():
    ctx = FinanceKernelContext("ledger-1", "session-1", environment="dev")

    # Spawn updated
    updated = ctx.with_updates(environment="prod", user_id="user-999", metadata={"new": True})

    # Original is unchanged
    assert ctx.environment == "dev"
    assert ctx.user_id is None
    assert ctx.metadata == {}

    # New one has updates
    assert updated.ledger_id == "ledger-1"
    assert updated.session_id == "session-1"
    assert updated.environment == "prod"
    assert updated.user_id == "user-999"
    assert updated.metadata == {"new": True}
