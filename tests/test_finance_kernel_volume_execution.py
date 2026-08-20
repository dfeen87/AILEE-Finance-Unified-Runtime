# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Unit tests for the Python Volume Execution Operator."""

import os
import pytest
from core.finance_kernel.volume_execution import VolumeExecutionOperator

def test_volume_execution_operator_dry_run():
    exec_op = VolumeExecutionOperator(
        enable_auto_execute=False,
        audit_log_file="test_audit.log"
    )
    adv_dict = {
        "recommended_weight": 0.8,
        "risk_score": 20.0,
        "risk_elevated": False,
        "growth_favorable": True,
        "contrarian_buy_signal": False
    }

    # First bar (pending hysteresis)
    exec_op.process_tick(adv_dict, 500.0)
    assert exec_op.current_position_side == "FLAT"

    # Second bar (confirmed hysteresis but auto_execute is False)
    exec_op.process_tick(adv_dict, 501.0)
    assert exec_op.current_position_side == "FLAT"

    if os.path.exists("test_audit.log"):
        os.remove("test_audit.log")


def test_volume_execution_operator_auto_execute():
    exec_op = VolumeExecutionOperator(
        enable_auto_execute=True,
        hysteresis_bars=1,
        max_position_usd=10000.0,
        audit_log_file="test_audit.log"
    )
    adv_dict = {
        "recommended_weight": 1.0,
        "risk_score": 0.0,
        "risk_elevated": False,
        "growth_favorable": True,
        "contrarian_buy_signal": False
    }

    exec_op.process_tick(adv_dict, 500.0)
    assert exec_op.current_position_side == "BUY"

    if os.path.exists("test_audit.log"):
        os.remove("test_audit.log")


def test_volume_execution_operator_lockout():
    exec_op = VolumeExecutionOperator(
        enable_auto_execute=True,
        max_daily_drawdown_pct=0.05,
        audit_log_file="test_audit.log"
    )
    exec_op.peak_equity = 100000.0
    exec_op.current_equity = 90000.0 # 10% drawdown

    adv_dict = {
        "recommended_weight": 1.0,
        "risk_score": 0.0,
        "risk_elevated": False,
        "growth_favorable": True,
        "contrarian_buy_signal": False
    }

    exec_op.process_tick(adv_dict, 500.0)
    assert exec_op.locked_out is True
    assert "Daily drawdown threshold breached" in exec_op.lockout_reason

    if os.path.exists("test_audit.log"):
        os.remove("test_audit.log")
