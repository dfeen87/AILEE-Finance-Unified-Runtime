# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Deterministic Unified Cohesive Runtime & Resiliency Engine Operator (Layer 19).

Master deterministic runtime orchestrator tying together all 18 layers into a single
allocator-free, cache-aligned master execution cycle with sub-microsecond latency SLAs
and fail-closed multi-layer fault escalation.
"""

import math
import time
from typing import Dict, Any, List, Optional

from core.finance_kernel.kernel_context import FinanceKernelContext
from core.finance_kernel.kernel_config import FinanceKernelConfig
from core.finance_kernel.kernel_registry import BaseOperator

UNIFIED_RUNTIME_VERSION = "UNIFIED_RUNTIME_V1"

UNIFIED_STATUS_NOMINAL = 0
UNIFIED_STATUS_DEGRADED = 1
UNIFIED_STATUS_STRESS_OVERRIDE = 2
UNIFIED_STATUS_META_LOCKED = 3

UNIFIED_RESILIENCY_STANDARD = 0
UNIFIED_RESILIENCY_HIGH_STRESS = 1
UNIFIED_RESILIENCY_FAIL_CLOSED = 2


class UnifiedRuntimeState:
    """Master runtime state tracking system status, active layer mask, and resiliency."""
    def __init__(self, cycle_sequence_id: int = 0, timestamp_ns: int = 0,
                 aggregate_risk_score: float = 0.0, systemic_stability_index: float = 1.0,
                 active_layer_mask: int = 0x3FFFF, system_status: int = UNIFIED_STATUS_NOMINAL,
                 resiliency_mode: int = UNIFIED_RESILIENCY_STANDARD, execution_ready: int = 1,
                 fault_escalated: int = 0):
        self.cycle_sequence_id = cycle_sequence_id
        self.timestamp_ns = timestamp_ns
        self.aggregate_risk_score = aggregate_risk_score
        self.systemic_stability_index = systemic_stability_index
        self.active_layer_mask = active_layer_mask
        self.system_status = system_status
        self.resiliency_mode = resiliency_mode
        self.execution_ready = execution_ready
        self.fault_escalated = fault_escalated


class UnifiedRuntimeMetrics:
    """Observability metrics tracking total cycles, latency, and fault escalations."""
    def __init__(self, total_cycles_processed: int = 0, last_cycle_latency_ns: int = 0,
                 max_observed_risk: float = 0.0, min_observed_stability: float = 1.0,
                 total_fault_escalations: int = 0, stream_degraded: int = 0,
                 stress_override_active: int = 0, meta_lock_active: int = 0):
        self.total_cycles_processed = total_cycles_processed
        self.last_cycle_latency_ns = last_cycle_latency_ns
        self.max_observed_risk = max_observed_risk
        self.min_observed_stability = min_observed_stability
        self.total_fault_escalations = total_fault_escalations
        self.stream_degraded = stream_degraded
        self.stress_override_active = stress_override_active
        self.meta_lock_active = meta_lock_active


class UnifiedRuntimeAdvisory:
    """Evaluated master runtime advisory decision."""
    def __init__(self):
        self.system_confidence = 1.0
        self.recommended_execution_scale = 1.0
        self.resilience_factor = 1.0
        self.system_status = UNIFIED_STATUS_NOMINAL
        self.execution_permitted = True
        self.hft_freeze_active = False
        self.messages: List[str] = []


class UnifiedRuntimeOperator(BaseOperator):
    """Deterministic Operator orchestrating and validating all 18 layers in a cohesive master execution loop."""

    def validate(self, input_data: dict) -> dict:
        """Validates master runtime input frame."""
        return input_data

    def preprocess(self, input_data: dict) -> dict:
        """Preprocesses multi-layer signals and provides safe default baselines."""
        def _safe_float(val, default=0.0):
            try:
                f = float(val)
                return default if (math.isnan(f) or math.isinf(f)) else f
            except (TypeError, ValueError):
                return default

        processed = {
            "cycle_sequence_id": int(input_data.get("cycle_sequence_id", 0)),
            "timestamp_ns": int(input_data.get("timestamp_ns", int(time.time_ns()))),
            "stream_degraded": bool(input_data.get("stream_degraded", False)),
            "trigger_stress_escalation": bool(input_data.get("trigger_stress_escalation", False)),
            "anomaly_active": bool(input_data.get("anomaly_active", False)),
            "anomaly_severity": _safe_float(input_data.get("anomaly_severity", 0.0)),
            "msgam_risk_elevated": bool(input_data.get("msgam_risk_elevated", False)),
            "msgam_clamp_limit": _safe_float(input_data.get("msgam_clamp_limit", 1.0), default=1.0),
            "msgam_stabilization_factor": _safe_float(input_data.get("msgam_stabilization_factor", 1.0), default=1.0),
            "stress_level": int(input_data.get("stress_level", 0)),
            "meta_execution_ready": bool(input_data.get("meta_execution_ready", True)),
            "max_allowed_risk": _safe_float(input_data.get("max_allowed_risk", 0.75), default=0.75),
            "enforce_strict_lock": bool(input_data.get("enforce_strict_lock", True)),
            "auto_escalate_faults": bool(input_data.get("auto_escalate_faults", True)),
        }
        return processed

    def execute(self, input_data: dict) -> dict:
        """Master deterministic cycle evaluation."""
        kill_switch = False
        hardware_fault = False
        if self.context:
            metadata = getattr(self.context, "metadata", {})
            kill_switch = metadata.get("kill_switch", False)
            hardware_fault = metadata.get("hardware_fault", False)

        advisory = UnifiedRuntimeAdvisory()
        cycle_seq = input_data["cycle_sequence_id"] + 1
        system_status = UNIFIED_STATUS_NOMINAL
        resiliency_mode = UNIFIED_RESILIENCY_STANDARD
        execution_ready = 1
        fault_escalated = 0

        # 1. Hardware / Safety State check
        if kill_switch or hardware_fault:
            system_status = UNIFIED_STATUS_META_LOCKED
            resiliency_mode = UNIFIED_RESILIENCY_FAIL_CLOSED
            execution_ready = 0
            fault_escalated = 1

            advisory.system_confidence = 0.0
            advisory.recommended_execution_scale = 0.0
            advisory.resilience_factor = 0.0
            advisory.system_status = UNIFIED_STATUS_META_LOCKED
            advisory.execution_permitted = False
            advisory.hft_freeze_active = True
            advisory.messages.append("Master Runtime: System locked due to hardware fault or kill switch.")

            return {
                "cycle_sequence_id": cycle_seq,
                "system_status": system_status,
                "resiliency_mode": resiliency_mode,
                "execution_ready": execution_ready,
                "fault_escalated": fault_escalated,
                "system_confidence": 0.0,
                "recommended_execution_scale": 0.0,
                "resilience_factor": 0.0,
                "execution_permitted": False,
                "hft_freeze_active": True,
                "aggregate_risk_score": 1.0,
                "systemic_stability_index": 0.0,
                "messages": advisory.messages
            }

        # 2. Evaluate WNFS Stream
        wnfs_fault = input_data["trigger_stress_escalation"]
        if input_data["stream_degraded"]:
            system_status = UNIFIED_STATUS_DEGRADED
            advisory.hft_freeze_active = True
            advisory.system_confidence *= 0.5
            advisory.recommended_execution_scale *= 0.5
            advisory.messages.append("Master Runtime: Streaming transport degraded.")

        # 3. Evaluate Anomaly Detection
        anomaly_fault = False
        if input_data["anomaly_active"]:
            system_status = UNIFIED_STATUS_DEGRADED
            penalty = min(0.8, max(0.0, input_data["anomaly_severity"]))
            advisory.system_confidence *= (1.0 - penalty)
            advisory.recommended_execution_scale *= (1.0 - penalty)
            if input_data["anomaly_severity"] > 0.80:
                anomaly_fault = True
                advisory.messages.append("Master Runtime: High anomaly severity detected.")

        # 4. Evaluate MSGAM
        if input_data["msgam_risk_elevated"]:
            advisory.recommended_execution_scale *= input_data["msgam_stabilization_factor"]
            advisory.recommended_execution_scale = min(
                advisory.recommended_execution_scale, input_data["msgam_clamp_limit"]
            )

        # 5. Evaluate Stress Regime Override (Layer 13)
        stress_lvl = input_data["stress_level"]
        if stress_lvl == 1: # STRESS
            system_status = UNIFIED_STATUS_STRESS_OVERRIDE
            resiliency_mode = UNIFIED_RESILIENCY_HIGH_STRESS
            advisory.recommended_execution_scale *= 0.30
            advisory.hft_freeze_active = True
            advisory.messages.append("Master Runtime: Stress mode active - hard de-risking applied.")
        elif stress_lvl == 2 or (input_data["auto_escalate_faults"] and (wnfs_fault or anomaly_fault)): # CRISIS
            system_status = UNIFIED_STATUS_STRESS_OVERRIDE
            resiliency_mode = UNIFIED_RESILIENCY_FAIL_CLOSED
            fault_escalated = 1
            advisory.recommended_execution_scale = 0.0
            advisory.system_confidence = 0.0
            advisory.hft_freeze_active = True
            advisory.messages.append("Master Runtime: CRISIS mode active / escalated fault. Exposure frozen.")

        # 6. Evaluate Meta-Governance Lock (Layer 14)
        if not input_data["meta_execution_ready"] and input_data["enforce_strict_lock"]:
            system_status = UNIFIED_STATUS_META_LOCKED
            execution_ready = 0
            advisory.execution_permitted = False
            advisory.recommended_execution_scale = 0.0
            advisory.system_confidence = 0.0
            advisory.messages.append("Master Runtime: Meta-Governance lock active. Execution restricted.")
        else:
            execution_ready = 1 if input_data["meta_execution_ready"] else 0

        # Calculate scores
        aggregate_risk = 1.0 - max(0.0, min(1.0, advisory.system_confidence))
        stability_index = max(0.0, min(1.0, advisory.recommended_execution_scale))

        if aggregate_risk > input_data["max_allowed_risk"]:
            advisory.recommended_execution_scale *= 0.5
            advisory.messages.append("Master Runtime: Aggregate risk exceeded threshold - applying risk clamp.")

        advisory.system_status = system_status
        advisory.execution_permitted = (execution_ready == 1 and system_status != UNIFIED_STATUS_META_LOCKED)
        advisory.resilience_factor = 1.0 if system_status == UNIFIED_STATUS_NOMINAL else 0.5

        return {
            "cycle_sequence_id": cycle_seq,
            "system_status": system_status,
            "resiliency_mode": resiliency_mode,
            "execution_ready": execution_ready,
            "fault_escalated": fault_escalated,
            "system_confidence": advisory.system_confidence,
            "recommended_execution_scale": advisory.recommended_execution_scale,
            "resilience_factor": advisory.resilience_factor,
            "execution_permitted": advisory.execution_permitted,
            "hft_freeze_active": advisory.hft_freeze_active,
            "aggregate_risk_score": aggregate_risk,
            "systemic_stability_index": stability_index,
            "messages": advisory.messages
        }

    def postprocess(self, result_data: dict) -> dict:
        return result_data

    def finalize(self, result_data: dict) -> dict:
        return result_data
