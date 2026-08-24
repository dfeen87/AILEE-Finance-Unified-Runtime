# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Deterministic WaveNativeFinanceStream (WNFS) Operator (Layer 18).

Provides real-time streaming market data ingestion, lock-free wave channel synchronization,
sequence gap detection, and escalation to Layer 13 Stress Override and Layer 14 Meta-Governance Lock.
"""

import math
from typing import Dict, Any, List, Optional

from core.finance_kernel.kernel_context import FinanceKernelContext
from core.finance_kernel.kernel_config import FinanceKernelConfig
from core.finance_kernel.kernel_registry import BaseOperator

WNFS_VERSION = "WAVE_NATIVE_FINANCE_STREAM_V1"

WNFS_FLAG_GAP = 0x01
WNFS_FLAG_OUT_OF_ORDER = 0x02
WNFS_FLAG_CORRUPTED = 0x04

WNFS_STATUS_HEALTHY = 0
WNFS_STATUS_DEGRADED = 1
WNFS_STATUS_CORRUPTED = 2
WNFS_STATUS_LOCKED = 3


class WNFSFrame:
    """Inbound streaming tick frame representation."""
    def __init__(self, sequence_id: int = 0, timestamp_ns: int = 0,
                 bid_price: float = 0.0, ask_price: float = 0.0,
                 bid_size: float = 0.0, ask_size: float = 0.0,
                 last_price: float = 0.0, last_size: float = 0.0,
                 vwap_delta: float = 0.0, symbol_id: int = 0,
                 wave_channel_id: int = 0, frame_flags: int = 0):
        self.sequence_id = sequence_id
        self.timestamp_ns = timestamp_ns
        self.bid_price = bid_price
        self.ask_price = ask_price
        self.bid_size = bid_size
        self.ask_size = ask_size
        self.last_price = last_price
        self.last_size = last_size
        self.vwap_delta = vwap_delta
        self.symbol_id = symbol_id
        self.wave_channel_id = wave_channel_id
        self.frame_flags = frame_flags


class WNFSState:
    """Wave channel runtime state snapshot."""
    def __init__(self, expected_sequence: int = 1, processed_frames: int = 0,
                 gap_count: int = 0, wave_phase: float = 0.0,
                 wave_amplitude: float = 1.0, symbol_id: int = 0,
                 clone_status_mask: int = 0, degraded_clone_count: int = 0,
                 channel_status: int = WNFS_STATUS_HEALTHY):
        self.expected_sequence = expected_sequence
        self.processed_frames = processed_frames
        self.gap_count = gap_count
        self.wave_phase = wave_phase
        self.wave_amplitude = wave_amplitude
        self.symbol_id = symbol_id
        self.clone_status_mask = clone_status_mask
        self.degraded_clone_count = degraded_clone_count
        self.channel_status = channel_status


class WNFSAdvisory:
    """Evaluated streaming transport advisory state."""
    def __init__(self):
        self.ingestion_confidence = 1.0
        self.wave_energy_factor = 1.0
        self.tick_acceleration = 0.0
        self.stream_degraded = False
        self.trigger_stress_escalation = False
        self.hft_freeze_required = False
        self.channel_id = 0
        self.messages: List[str] = []


class WNFSOperator(BaseOperator):
    """Deterministic Operator assessing real-time WNFS stream integrity and wave sync."""

    def validate(self, input_data: dict) -> dict:
        """Validates input streaming frame data."""
        if "sequence_id" not in input_data:
            raise ValueError("Missing required field 'sequence_id'")
        return input_data

    def preprocess(self, input_data: dict) -> dict:
        """Preprocesses frame input fields and sets fallback defaults."""
        def _safe_float(val, default=0.0):
            try:
                f = float(val)
                return default if (math.isnan(f) or math.isinf(f)) else f
            except (TypeError, ValueError):
                return default

        processed = {
            "sequence_id": int(input_data.get("sequence_id", 0)),
            "timestamp_ns": int(input_data.get("timestamp_ns", 0)),
            "bid_price": _safe_float(input_data.get("bid_price", 0.0)),
            "ask_price": _safe_float(input_data.get("ask_price", 0.0)),
            "bid_size": max(0.0, _safe_float(input_data.get("bid_size", 0.0))),
            "ask_size": max(0.0, _safe_float(input_data.get("ask_size", 0.0))),
            "last_price": _safe_float(input_data.get("last_price", 0.0)),
            "last_size": max(0.0, _safe_float(input_data.get("last_size", 0.0))),
            "vwap_delta": _safe_float(input_data.get("vwap_delta", 0.0)),
            "symbol_id": int(input_data.get("symbol_id", 0)),
            "wave_channel_id": int(input_data.get("wave_channel_id", 0)),
            "frame_flags": int(input_data.get("frame_flags", 0)),
            "expected_sequence": int(input_data.get("expected_sequence", 1)),
            "processed_frames": int(input_data.get("processed_frames", 0)),
            "gap_count": int(input_data.get("gap_count", 0)),
            "clone_status_mask": int(input_data.get("clone_status_mask", 0)),
            "degraded_clone_count": int(input_data.get("degraded_clone_count", 0)),
            "channel_status": int(input_data.get("channel_status", WNFS_STATUS_HEALTHY)),
            "max_sequence_gaps": int(input_data.get("max_sequence_gaps", 5)),
        }
        return processed

    def execute(self, input_data: dict) -> dict:
        """Evaluates sequence alignment, gaps, wave phase, and escalation flags."""
        kill_switch = False
        hardware_fault = False
        if self.context:
            metadata = getattr(self.context, "metadata", {})
            kill_switch = metadata.get("kill_switch", False)
            hardware_fault = metadata.get("hardware_fault", False)

        advisory = WNFSAdvisory()
        advisory.channel_id = input_data["wave_channel_id"]

        if kill_switch or hardware_fault:
            advisory.ingestion_confidence = 0.0
            advisory.wave_energy_factor = 0.0
            advisory.stream_degraded = True
            advisory.trigger_stress_escalation = True
            advisory.hft_freeze_required = True
            advisory.messages.append("System halted: Hardware fault or kill switch active.")
            return {
                "channel_id": advisory.channel_id,
                "ingestion_confidence": advisory.ingestion_confidence,
                "wave_energy_factor": advisory.wave_energy_factor,
                "tick_acceleration": advisory.tick_acceleration,
                "stream_degraded": advisory.stream_degraded,
                "trigger_stress_escalation": advisory.trigger_stress_escalation,
                "hft_freeze_required": advisory.hft_freeze_required,
                "channel_status": WNFS_STATUS_LOCKED,
                "expected_sequence": input_data["expected_sequence"],
                "gap_count": input_data["gap_count"],
                "messages": advisory.messages
            }

        clone_mask = input_data["clone_status_mask"]
        degraded_clones = input_data["degraded_clone_count"]

        if clone_mask != 0 or degraded_clones > 0:
            advisory.ingestion_confidence = 0.0
            advisory.wave_energy_factor = 0.0
            advisory.stream_degraded = True
            advisory.hft_freeze_required = True
            advisory.trigger_stress_escalation = True
            advisory.messages.append("Multi-clone cluster consensus degraded: triggering immediate risk-off freeze.")
            return {
                "channel_id": advisory.channel_id,
                "ingestion_confidence": 0.0,
                "wave_energy_factor": 0.0,
                "tick_acceleration": 0.0,
                "stream_degraded": True,
                "trigger_stress_escalation": True,
                "hft_freeze_required": True,
                "channel_status": WNFS_STATUS_DEGRADED,
                "expected_sequence": input_data["expected_sequence"],
                "processed_frames": input_data["processed_frames"],
                "gap_count": input_data["gap_count"],
                "messages": advisory.messages
            }

        seq_id = input_data["sequence_id"]
        expected_seq = input_data["expected_sequence"]
        processed_frames = input_data["processed_frames"]
        gap_count = input_data["gap_count"]
        channel_status = input_data["channel_status"]
        max_gaps = input_data["max_sequence_gaps"]
        flags = input_data["frame_flags"]

        is_out_of_order = False
        if processed_frames > 0:
            if seq_id > expected_seq:
                gaps = seq_id - expected_seq
                gap_count += gaps
                channel_status = WNFS_STATUS_DEGRADED
                flags |= WNFS_FLAG_GAP
                advisory.messages.append(f"Sequence gap detected: skipped {gaps} frames.")
            elif seq_id < expected_seq:
                channel_status = WNFS_STATUS_DEGRADED
                flags |= WNFS_FLAG_OUT_OF_ORDER
                is_out_of_order = True
                advisory.messages.append("Out of order frame rejected.")

        if not is_out_of_order:
            expected_seq = seq_id + 1
            processed_frames += 1

        two_pi = 6.283185307179586
        wave_phase = math.fmod(seq_id * 0.1, two_pi)
        depth_sum = input_data["bid_size"] + input_data["ask_size"]
        wave_amplitude = (input_data["last_size"] / depth_sum) if depth_sum > 0.0 else 1.0

        if is_out_of_order or (flags & (WNFS_FLAG_GAP | WNFS_FLAG_CORRUPTED | WNFS_FLAG_OUT_OF_ORDER)) or channel_status != WNFS_STATUS_HEALTHY:
            advisory.stream_degraded = True
            advisory.hft_freeze_required = True

            if gap_count >= max_gaps or (flags & WNFS_FLAG_CORRUPTED):
                channel_status = WNFS_STATUS_CORRUPTED
                advisory.trigger_stress_escalation = True
                advisory.ingestion_confidence = 0.0
                advisory.wave_energy_factor = 0.0
                advisory.messages.append("CRITICAL: Wave stream corrupted or max sequence gaps exceeded. Triggering Layer 13/14 escalation.")
            else:
                advisory.ingestion_confidence = 0.0 if is_out_of_order else 0.5
                advisory.wave_energy_factor = 0.0 if is_out_of_order else wave_amplitude
        else:
            advisory.ingestion_confidence = 1.0
            advisory.wave_energy_factor = wave_amplitude
            advisory.stream_degraded = False
            advisory.hft_freeze_required = False
            advisory.trigger_stress_escalation = False

        advisory.tick_acceleration = input_data["vwap_delta"]

        return {
            "channel_id": advisory.channel_id,
            "ingestion_confidence": advisory.ingestion_confidence,
            "wave_energy_factor": advisory.wave_energy_factor,
            "tick_acceleration": advisory.tick_acceleration,
            "stream_degraded": advisory.stream_degraded,
            "trigger_stress_escalation": advisory.trigger_stress_escalation,
            "hft_freeze_required": advisory.hft_freeze_required,
            "channel_status": channel_status,
            "expected_sequence": expected_seq,
            "processed_frames": processed_frames,
            "gap_count": gap_count,
            "wave_phase": wave_phase,
            "wave_amplitude": wave_amplitude,
            "messages": advisory.messages
        }

    def postprocess(self, result_data: dict) -> dict:
        return result_data

    def finalize(self, result_data: dict) -> dict:
        return result_data
