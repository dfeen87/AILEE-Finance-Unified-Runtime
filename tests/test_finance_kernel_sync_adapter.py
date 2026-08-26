"""
Unit tests for AILEE Finance Sync Adapter Operator (core/finance_kernel/sync_adapter.py).
"""

import pytest
from core.finance_kernel.sync_adapter import (
    SyncAdapter,
    SyncAdapterConfig,
    SYNC_FLAG_ALIGNED,
    SYNC_FLAG_EXTERNAL,
    SYNC_FLAG_FALLBACK,
    SYNC_FLAG_DRIFT_WARN,
    SYNC_FLAG_GAP_DETECTED
)


def test_sync_adapter_standalone_cadence():
    adapter = SyncAdapter()

    tick1 = adapter.advance_tick()
    assert tick1.tick_index == 1
    assert tick1.drift_ns == 0
    assert tick1.confidence == 1.0
    assert (tick1.alignment_flags & SYNC_FLAG_ALIGNED) != 0
    assert (tick1.alignment_flags & SYNC_FLAG_FALLBACK) != 0

    tick2 = adapter.advance_tick()
    assert tick2.tick_index == 2
    assert tick2.timestamp_ns == tick1.timestamp_ns + 10000000


def test_sync_adapter_external_clock_ingestion():
    adapter = SyncAdapter()
    ts = 1000000000

    tick1 = adapter.ingest_protocol_clock(tick_index=10, timestamp_ns=ts, wave_phase=1.57, confidence=1.0)
    assert tick1.tick_index == 10
    assert tick1.timestamp_ns == ts
    assert (tick1.alignment_flags & SYNC_FLAG_EXTERNAL) != 0
    assert tick1.degraded == 0

    # Drift & gap
    ts_drifted = ts + 20000000  # 20ms delta vs 10ms expected -> 10ms drift
    tick2 = adapter.ingest_protocol_clock(tick_index=15, timestamp_ns=ts_drifted, wave_phase=3.14, confidence=1.0)
    assert tick2.degraded == 1
    assert tick2.escalate_stress == 1
    assert (tick2.alignment_flags & SYNC_FLAG_GAP_DETECTED) != 0
    assert (tick2.alignment_flags & SYNC_FLAG_DRIFT_WARN) != 0
