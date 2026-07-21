# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Unit tests for the Finance Runtime Kernel Registry."""

import pytest
from core.finance_kernel.kernel_registry import KernelRegistry, create_default_registry, BaseOperator
from core.finance_kernel.kernel_errors import KernelConfigurationError, OperatorNotFoundError

class DummyOperator(BaseOperator):
    role = "transaction"

def test_registry_registration():
    reg = KernelRegistry()
    reg.register_operator("dummy", DummyOperator)

    assert reg.get_operator("dummy") == DummyOperator
    assert reg.get_operator_role("dummy") == "transaction"
    assert "dummy" in reg.list_operators()


def test_registry_uniqueness():
    reg = KernelRegistry()
    reg.register_operator("dummy", DummyOperator)

    with pytest.raises(KernelConfigurationError):
        reg.register_operator("dummy", DummyOperator)


def test_registry_role_enforcement():
    reg = KernelRegistry()

    # Class without a role and no explicit role
    class RolelessOperator(BaseOperator):
        pass

    with pytest.raises(KernelConfigurationError):
        reg.register_operator("roleless", RolelessOperator)

    # Class with an invalid role
    class InvalidRoleOperator(BaseOperator):
        role = "marketing"

    with pytest.raises(KernelConfigurationError):
        reg.register_operator("invalid", InvalidRoleOperator)

    # Valid role passed explicitly
    reg.register_operator("explicit_role", RolelessOperator, role="risk")
    assert reg.get_operator_role("explicit_role") == "risk"


def test_registry_lookup_errors():
    reg = KernelRegistry()
    with pytest.raises(OperatorNotFoundError):
        reg.get_operator("nonexistent")

    with pytest.raises(OperatorNotFoundError):
        reg.get_operator_role("nonexistent")


def test_default_registry_has_standard_operators():
    reg = create_default_registry()
    ops = reg.list_operators()

    assert "risk_operator" in ops
    assert "governor_operator" in ops
    assert "transaction_operator" in ops
    assert "ledger_operator" in ops

    assert reg.get_operator_role("risk_operator") == "risk"
    assert reg.get_operator_role("governor_operator") == "governor"
    assert reg.get_operator_role("transaction_operator") == "transaction"
    assert reg.get_operator_role("ledger_operator") == "ledger"
