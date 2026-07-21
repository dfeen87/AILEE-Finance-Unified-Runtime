# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Custom exception types for the AILEE Finance Runtime Kernel."""

class FinanceKernelError(Exception):
    """Base exception for all Finance Kernel errors."""
    def __init__(self, message: str, details: dict = None):
        super().__init__(message)
        self.message = message
        self.details = details or {}

    def __str__(self):
        if self.details:
            return f"{self.message} (Details: {self.details})"
        return self.message


class KernelConfigurationError(FinanceKernelError):
    """Raised when kernel configuration is invalid or fails to load."""
    pass


class KernelContextError(FinanceKernelError):
    """Raised when kernel context is invalid, missing required fields, or mutated illegally."""
    pass


class OperatorNotFoundError(FinanceKernelError):
    """Raised when a requested operator cannot be found in the registry."""
    pass


class OperatorExecutionError(FinanceKernelError):
    """Raised when an operator fails during validation, preprocessing, execution, postprocessing, or finalization."""
    pass


class DeterminismViolationError(FinanceKernelError):
    """Raised when strict determinism guarantees are violated during execution."""
    pass
