# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Core Finance Runtime Kernel orchestrator for the AILEE Finance framework."""

import time
import logging
import copy
from core.finance_kernel.kernel_context import FinanceKernelContext
from core.finance_kernel.kernel_config import FinanceKernelConfig
from core.finance_kernel.kernel_registry import KernelRegistry
from core.finance_kernel.kernel_errors import (
    KernelConfigurationError,
    KernelContextError,
    OperatorNotFoundError,
    OperatorExecutionError,
    DeterminismViolationError
)

logger = logging.getLogger("FinanceRuntimeKernel")

class OperatorResult:
    """Standardized representation of an individual operator's execution output."""
    def __init__(self, operator_name: str, status: str, data: dict, duration: float, error: str = None):
        self.operator_name = operator_name
        self.status = status
        self.data = data
        self.duration = duration
        self.error = error

    def to_dict(self) -> dict:
        """Serializes results to a clean dictionary."""
        return {
            "operator_name": self.operator_name,
            "status": self.status,
            "data": self.data,
            "duration": self.duration,
            "error": self.error
        }


class PipelineResult:
    """Standardized representation of sequential operators pipeline execution output."""
    def __init__(self, results: list, final_data: dict, total_duration: float):
        self.results = results
        self.final_data = final_data
        self.total_duration = total_duration

    def to_dict(self) -> dict:
        """Serializes results to a clean dictionary."""
        return {
            "results": [r.to_dict() if hasattr(r, "to_dict") else r for r in self.results],
            "final_data": self.final_data,
            "total_duration": self.total_duration
        }


def make_json_compatible(val):
    """Recursively converts structures to clean JSON-compatible equivalents."""
    if isinstance(val, (str, int, float, bool, type(None))):
        return val
    elif isinstance(val, dict):
        return {str(k): make_json_compatible(v) for k, v in val.items()}
    elif isinstance(val, (list, tuple, set)):
        return [make_json_compatible(v) for v in val]
    elif hasattr(val, "to_dict"):
        return make_json_compatible(val.to_dict())
    else:
        # Fallback to string representations for non-serializable objects (e.g. custom engines/decisions)
        try:
            # Check if it has dict-like access or attributes
            if hasattr(val, "__dict__"):
                return make_json_compatible(val.__dict__)
        except Exception:
            pass
        return str(val)


class FinanceRuntimeKernel:
    """Main orchestrator that enforces deterministic sequencing and coordinates operators."""

    def __init__(self, context: FinanceKernelContext, config: FinanceKernelConfig, registry: KernelRegistry):
        if not isinstance(context, FinanceKernelContext):
            raise KernelContextError("Provided context must be an instance of FinanceKernelContext")
        if not isinstance(config, FinanceKernelConfig):
            raise KernelConfigurationError("Provided config must be an instance of FinanceKernelConfig")
        if not isinstance(registry, KernelRegistry):
            raise KernelConfigurationError("Provided registry must be an instance of KernelRegistry")

        self.context = context
        self.config = config
        self.registry = registry

        # Configure dynamic logging level
        level = getattr(logging, self.config.logging_level.upper(), logging.INFO)
        logger.setLevel(level)

    def execute_operator(self, name: str, input_data: dict) -> OperatorResult:
        """Executes an operator with deterministic sequencing: validate -> preprocess -> execute -> postprocess -> finalize."""
        logger.debug(f"Initiating operator: '{name}'")

        try:
            op_class = self.registry.get_operator(name)
        except OperatorNotFoundError as e:
            logger.error(f"Failed to find operator '{name}': {e}")
            raise

        # Copy inputs to avoid sharing reference state and preserve strict determinism
        try:
            copied_input = copy.deepcopy(input_data)
        except Exception as e:
            if self.config.strict_determinism:
                raise DeterminismViolationError(f"Strict determinism violation: input contains non-serializable state: {e}")
            copied_input = input_data

        op_instance = op_class(context=self.context, config=self.config)

        start_time = time.perf_counter()
        status = "SUCCESS"
        error_msg = None
        result_data = {}

        try:
            # Enforce sequencing
            validated_input = op_instance.validate(copied_input)
            preprocessed_input = op_instance.preprocess(validated_input)

            executed_result = op_instance.execute(preprocessed_input)

            postprocessed_result = op_instance.postprocess(executed_result)
            finalized_result = op_instance.finalize(postprocessed_result)

            result_data = finalized_result
        except Exception as e:
            status = "ERROR"
            error_msg = str(e)
            logger.error(f"Execution error on operator '{name}': {e}", exc_info=True)
            if isinstance(e, (KernelContextError, KernelConfigurationError, DeterminismViolationError, OperatorNotFoundError)):
                raise
            raise OperatorExecutionError(f"Operator '{name}' failed during pipeline stage: {e}") from e
        finally:
            end_time = time.perf_counter()
            duration = end_time - start_time
            logger.info(f"Operator '{name}' completed in {duration:.6f}s (Status: {status})")

        res_obj = OperatorResult(
            operator_name=name,
            status=status,
            data=result_data,
            duration=duration,
            error=error_msg
        )

        if self.config.json_compat_mode:
            return make_json_compatible(res_obj.to_dict())

        return res_obj

    def execute_pipeline(self, operator_names: list, input_data: dict) -> PipelineResult:
        """Executes a list of operators in deterministic sequence passing intermediate outputs."""
        logger.debug(f"Initiating pipeline sequence: {operator_names}")
        if not operator_names:
            raise OperatorExecutionError("Pipeline execution requires at least one operator.")

        start_time = time.perf_counter()
        results = []
        current_data = copy.deepcopy(input_data)

        for name in operator_names:
            op_res = self.execute_operator(name, current_data)
            results.append(op_res)

            # Pass the intermediate outputs to next operator
            if isinstance(op_res, dict):
                current_data = op_res.get("data", {})
            else:
                current_data = op_res.data

        end_time = time.perf_counter()
        total_duration = end_time - start_time
        logger.info(f"Pipeline sequence completed in {total_duration:.6f}s")

        res_obj = PipelineResult(
            results=results,
            final_data=current_data,
            total_duration=total_duration
        )

        if self.config.json_compat_mode:
            return make_json_compatible(res_obj.to_dict())

        return res_obj
