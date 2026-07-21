# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Public API surface and compatibility layers for the AILEE Finance Runtime Kernel."""

from core.finance_kernel.kernel_context import FinanceKernelContext
from core.finance_kernel.kernel_config import FinanceKernelConfig
from core.finance_kernel.kernel_registry import KernelRegistry, create_default_registry
from core.finance_kernel.finance_kernel import FinanceRuntimeKernel, OperatorResult, PipelineResult
from core.finance_kernel.kernel_errors import (
    FinanceKernelError,
    KernelConfigurationError,
    KernelContextError,
    OperatorNotFoundError,
    OperatorExecutionError,
    DeterminismViolationError
)

# Shared global registry
_shared_registry = create_default_registry()

def get_shared_registry() -> KernelRegistry:
    """Returns the shared global operator registry."""
    return _shared_registry

def create_finance_kernel(context_overrides: dict = None, config_overrides: dict = None) -> FinanceRuntimeKernel:
    """Constructs and returns a FinanceRuntimeKernel instance deterministically with resolving overrides."""
    ctx_data = {
        "ledger_id": "default-ledger",
        "session_id": "default-session",
        "environment": "dev",
        "metadata": {}
    }
    if context_overrides:
        ctx_data.update(context_overrides)

    context = FinanceKernelContext.from_dict(ctx_data)

    config = FinanceKernelConfig()
    config.load_from_env()

    if config_overrides and "config_file" in config_overrides:
        config.load_from_file(config_overrides["config_file"])
    if config_overrides:
        config.merge_overrides(config_overrides)

    return FinanceRuntimeKernel(context, config, _shared_registry)

def run_finance_operator(name: str, input_data: dict, context_overrides: dict = None, config_overrides: dict = None):
    """Executes a single finance operator with deterministic override resolution."""
    kernel = create_finance_kernel(context_overrides, config_overrides)
    return kernel.execute_operator(name, input_data)


# -------------------------------------------------------------------------
# COMPATIBILITY WRAPPER LAYER (Task Block 6)
# -------------------------------------------------------------------------

def run_transaction_operator(signals: list, config: dict = None, context_overrides: dict = None, config_overrides: dict = None):
    """Compatibility wrapper function to execute the transactional operator."""
    input_data = {"signals": signals}
    if config is not None:
        input_data["config"] = config
    return run_finance_operator("transaction_operator", input_data, context_overrides, config_overrides)

def run_risk_operator(signals: list, config: dict = None, context_overrides: dict = None, config_overrides: dict = None):
    """Compatibility wrapper function to execute the risk operator."""
    input_data = {"signals": signals}
    if config is not None:
        input_data["config"] = config
    return run_finance_operator("risk_operator", input_data, context_overrides, config_overrides)

def run_governor_operator(signals: list, config: dict = None, context_overrides: dict = None, config_overrides: dict = None):
    """Compatibility wrapper function to execute the governor operator."""
    input_data = {"signals": signals}
    if config is not None:
        input_data["config"] = config
    return run_finance_operator("governor_operator", input_data, context_overrides, config_overrides)

def run_ledger_operator(decision: dict, context_overrides: dict = None, config_overrides: dict = None):
    """Compatibility wrapper function to execute the ledger operator."""
    input_data = {"decision": decision, "context": context_overrides}
    return run_finance_operator("ledger_operator", input_data, context_overrides, config_overrides)
