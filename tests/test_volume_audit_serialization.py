# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Synthetic, offline tests of the public volume-audit record boundary."""

import json
import logging
import math
import time
from decimal import Decimal
from pathlib import Path
from types import SimpleNamespace

import pytest

from core.finance_kernel import volume_execution


def _operator(tmp_path):
    # Auto-execution is deliberately disabled; the operator has no broker.
    return volume_execution.VolumeExecutionOperator(
        enable_auto_execute=False,
        mock_mode=True,
        hysteresis_bars=1,
        symbol="SYNTH",
        audit_log_file=str(tmp_path / "audit.jsonl"),
    )


def test_valid_audit_preserves_schema_bytes_and_determinism(tmp_path, monkeypatch):
    op = _operator(tmp_path)
    fixed_time = time.struct_time((2000, 1, 1, 0, 0, 0, 5, 1, 0))
    monkeypatch.setattr(
        volume_execution, "time",
        SimpleNamespace(strftime=time.strftime, gmtime=lambda: fixed_time),
    )
    advisory = {
        "recommended_weight": 0.5,
        "risk_score": 0.25,
        "hft_delta_v": 2**53 + 1,
    }
    details = "synthetic record\n\N{GREEK SMALL LETTER PI}"
    for _ in range(2):
        op.log_audit(
            "SYNTHETIC", details, advisory, 1.25,
            trust_score=0.875, manipulation_score=0.125,
        )
    bias = op.hft_bias_config
    expected = {
        "timestamp": "2000-01-01T00:00:00Z",
        "symbol": "SYNTH",
        "action": "SYNTHETIC",
        "price": 1.25,
        "rec_weight": 0.5,
        "execution_weight": 0.5,
        "trust_score": 0.875,
        "manipulation_score": 0.125,
        "risk_score": 0.25,
        "risk_elevated": False,
        "contrarian_buy": False,
        "growth_favorable": False,
        "hft_active": False,
        "hft_delta_v": 2**53 + 1,
        "bullish_mode_active": False,
        "bullish_multiplier_price": float(bias["bullish_multiplier_price"]),
        "bullish_multiplier_volume": float(bias["bullish_multiplier_volume"]),
        "bullish_execution_scale": float(bias["bullish_execution_scale"]),
        "bullish_sell_ceiling_factor": float(bias["bullish_sell_ceiling_factor"]),
        "details": details,
        "reason": details,
    }
    expected_line = json.dumps(expected, allow_nan=False) + "\n"
    assert Path(op.audit_log_file).read_text(encoding="utf-8") == expected_line * 2


@pytest.mark.parametrize(
    "ambiguous",
    [{1: "first", "1": "second"}, {"nested": [{None: 1, "null": 2}]}],
    ids=["integer-key-collision", "nested-null-key-collision"],
)
def test_audit_rejects_duplicate_json_members_before_logging_or_append(
    tmp_path, caplog, ambiguous,
):
    op = _operator(tmp_path)
    path = Path(op.audit_log_file)
    original = '{"synthetic": true}\n'
    path.write_text(original, encoding="utf-8")
    with caplog.at_level(logging.INFO, logger=volume_execution.logger.name):
        with pytest.raises(ValueError, match="duplicate"):
            op.log_audit("SYNTHETIC", "ambiguous", {"hft_delta_v": ambiguous}, 1.0)
    assert path.read_text(encoding="utf-8") == original
    assert not any("[AUDIT]" in record.message for record in caplog.records)


@pytest.mark.parametrize(
    "value",
    [math.nan, math.inf, -math.inf, {"nested": [math.nan]}, Decimal("1.5"), object()],
    ids=["nan", "positive-inf", "negative-inf", "nested-nan", "decimal", "object"],
)
def test_audit_still_rejects_nonfinite_or_unsupported_values(tmp_path, caplog, value):
    op = _operator(tmp_path)
    with caplog.at_level(logging.INFO, logger=volume_execution.logger.name):
        with pytest.raises((ValueError, TypeError)):
            op.log_audit("SYNTHETIC", "invalid", {"hft_delta_v": value}, 1.0)
    assert not Path(op.audit_log_file).exists()
    assert not any("[AUDIT]" in record.message for record in caplog.records)


def test_ambiguous_audit_in_dry_run_preserves_state_and_fails_closed(tmp_path):
    op = _operator(tmp_path)
    before = (op.current_position_side, op.pending_side, op.consecutive_bars, op.order_counter)
    with pytest.raises(ValueError, match="duplicate"):
        op.process_tick({"growth_favorable": True, "hft_delta_v": {1: 1, "1": 2}}, 1.0)
    assert (op.current_position_side, op.pending_side, op.consecutive_bars, op.order_counter) == before
    assert op.locked_out
    assert not Path(op.audit_log_file).exists()


def test_valid_dry_run_does_not_advance_position_or_order_counter(tmp_path):
    op = _operator(tmp_path)
    op.process_tick({"growth_favorable": True, "recommended_weight": 0.5}, 1.0)
    record = json.loads(Path(op.audit_log_file).read_text(encoding="utf-8"))
    assert record["action"] == "DRY_RUN_SIGNAL"
    assert op.current_position_side == "FLAT"
    assert op.order_counter == 1
    assert not op.locked_out


def test_existing_audit_io_failure_still_rolls_back_and_locks(tmp_path):
    op = _operator(tmp_path)
    op.audit_log_file = str(tmp_path)  # a directory, not a writable audit file
    before = (op.current_position_side, op.pending_side, op.consecutive_bars, op.order_counter)
    with pytest.raises(OSError):
        op.process_tick({"growth_favorable": True}, 1.0)
    assert (op.current_position_side, op.pending_side, op.consecutive_bars, op.order_counter) == before
    assert op.locked_out


@pytest.mark.parametrize(
    "encoded",
    [
        "[]", "null", '{"v": NaN}', '{"v": Infinity}', '{"v": -Infinity}',
        '{"v": 1e309}', '{"v": -1e309}', '{"v": 1, "v": 2}',
        '{"nested": {"v": 1, "v": 2}}', '{"v":', '{"v": 1}{"v": 2}',
    ],
)
def test_strict_decoder_rejects_malformed_or_ambiguous_json(encoded):
    with pytest.raises(ValueError):
        volume_execution._validate_audit_json(encoded)


def test_strict_decoder_accepts_finite_numeric_boundaries():
    volume_execution._validate_audit_json(
        '{"large":1e308,"tiny":5e-324,"integer":9007199254740993,'
        '"negative_zero":-0.0,"flag":true,"null":null}'
    )
