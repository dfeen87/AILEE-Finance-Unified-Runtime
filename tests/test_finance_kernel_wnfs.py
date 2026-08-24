# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Unit tests for Python Finance Kernel WNFS Operator (Layer 18)."""

import pytest
from core.finance_kernel.wnfs import (
    WNFSOperator,
    WNFS_STATUS_HEALTHY,
    WNFS_STATUS_DEGRADED,
    WNFS_STATUS_CORRUPTED,
    WNFS_STATUS_LOCKED,
    WNFS_FLAG_GAP
)


def test_wnfs_normal_frame_processing():
    op = WNFSOperator()
    frame = {
        "sequence_id": 1,
        "bid_price": 450.0,
        "ask_price": 450.05,
        "bid_size": 100.0,
        "ask_size": 100.0,
        "last_price": 450.02,
        "last_size": 10.0,
        "expected_sequence": 1,
        "processed_frames": 0,
        "gap_count": 0,
        "channel_status": WNFS_STATUS_HEALTHY
    }
    pre = op.preprocess(frame)
    res = op.execute(pre)

    assert res["ingestion_confidence"] == 1.0
    assert not res["stream_degraded"]
    assert not res["hft_freeze_required"]
    assert not res["trigger_stress_escalation"]
    assert res["expected_sequence"] == 2
    assert res["processed_frames"] == 1


def test_wnfs_sequence_gap_detection():
    op = WNFSOperator()
    frame = {
        "sequence_id": 10, # Jump by 9
        "expected_sequence": 1,
        "processed_frames": 1,
        "gap_count": 0,
        "channel_status": WNFS_STATUS_HEALTHY,
        "max_sequence_gaps": 5
    }
    pre = op.preprocess(frame)
    res = op.execute(pre)

    assert res["stream_degraded"]
    assert res["hft_freeze_required"]
    assert res["trigger_stress_escalation"]
    assert res["channel_status"] == WNFS_STATUS_CORRUPTED
    assert res["gap_count"] == 9


def test_wnfs_multi_clone_consensus_risk_off():
    op = WNFSOperator()
    frame = {
        "sequence_id": 1,
        "expected_sequence": 1,
        "processed_frames": 0,
        "clone_status_mask": 0x02, # Clone 1 degraded
        "degraded_clone_count": 1,
        "channel_status": WNFS_STATUS_HEALTHY
    }
    pre = op.preprocess(frame)
    res = op.execute(pre)

    assert res["ingestion_confidence"] == 0.0
    assert res["wave_energy_factor"] == 0.0
    assert res["stream_degraded"]
    assert res["hft_freeze_required"]
    assert res["trigger_stress_escalation"]


def test_wnfs_out_of_order_rejection():
    op = WNFSOperator()
    frame = {
        "sequence_id": 1, # Stale / out-of-order sequence (expected is 5)
        "expected_sequence": 5,
        "processed_frames": 4,
        "channel_status": WNFS_STATUS_HEALTHY
    }
    pre = op.preprocess(frame)
    res = op.execute(pre)

    assert res["stream_degraded"]
    assert res["hft_freeze_required"]
    assert res["ingestion_confidence"] == 0.0
    assert res["wave_energy_factor"] == 0.0
