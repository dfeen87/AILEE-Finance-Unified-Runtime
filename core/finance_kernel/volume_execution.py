"""
AILEE Finance Runtime Kernel - Volume Execution Operator
AI-Load Integrity and Layered Evaluation

Copyright (c) Don Michael Feeney Jr.
Licensed under the MIT License.
"""

import json
import logging
import os
import time
from typing import Any, Dict, Optional

logger = logging.getLogger("AILLE.FinanceKernel.VolumeExecution")


from core.finance_kernel.kernel_registry import BaseOperator
from core.finance_kernel.hft_bias import is_bullish_mode_allowed

class VolumeExecutionOperator(BaseOperator):
    role = "transaction"
    """
    Python execution operator that translates Intraday Volume Advisory (VAM) states
    and governance decisions into trading orders with risk controls, hysteresis,
    drawdown limits, and structured audit logging.
    """

    def __init__(
        self,
        enable_auto_execute: bool = False,
        mode: str = "paper",
        mock_mode: bool = True,
        enable_hft: bool = False,
        hft_frequency_hz: int = 1000,
        max_position_usd: float = 10000.0,
        max_daily_drawdown_pct: float = 0.05,
        risk_reduce_factor: float = 0.5,
        hysteresis_bars: int = 2,
        symbol: str = "SPY",
        audit_log_file: str = "volume_trader_audit_python.log",
        hft_bias_config: Optional[Dict[str, Any]] = None
    ):
        self.enable_auto_execute = enable_auto_execute
        self.mode = mode.lower()
        self.mock_mode = mock_mode
        self.enable_hft = enable_hft
        self.hft_frequency_hz = min(max(1, hft_frequency_hz), 1000)
        self.max_position_usd = max_position_usd
        self.max_daily_drawdown_pct = max_daily_drawdown_pct
        self.risk_reduce_factor = risk_reduce_factor
        self.hysteresis_bars = hysteresis_bars
        self.symbol = symbol
        self.audit_log_file = audit_log_file

        if hft_bias_config is None:
            self.hft_bias_config = {
                "enabled": True,
                "bullish_multiplier_price": 1.05,
                "bullish_multiplier_volume": 1.05,
                "bullish_execution_scale": 1.10,
                "bullish_sell_ceiling_factor": 0.80,
                "trust_threshold_bullish": 0.70,
                "manipulation_threshold": 0.30,
            }
        else:
            self.hft_bias_config = dict(hft_bias_config)

        self.current_position_side = "FLAT"
        self.pending_side = "FLAT"
        self.consecutive_bars = 0
        self.locked_out = False
        self.lockout_reason = ""

        self.peak_equity = 100000.0
        self.current_equity = 100000.0
        self.order_counter = 1

    def trigger_lockout(self, reason: str):
        self.locked_out = True
        self.lockout_reason = reason
        logger.error(f"[VolumeExecution] LOCKOUT TRIGGERED: {reason}")

    def log_audit(self, action: str, details: str, advisory_data: Dict[str, Any], price: float,
                  trust_score: float = 0.85, manipulation_score: float = 0.0, bullish_mode_active: bool = False):
        ts = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        p_mult = float(self.hft_bias_config.get("bullish_multiplier_price", 1.05)) if self.hft_bias_config else 1.05
        v_mult = float(self.hft_bias_config.get("bullish_multiplier_volume", 1.05)) if self.hft_bias_config else 1.05
        e_scale = float(self.hft_bias_config.get("bullish_execution_scale", 1.10)) if self.hft_bias_config else 1.10
        s_factor = float(self.hft_bias_config.get("bullish_sell_ceiling_factor", 0.80)) if self.hft_bias_config else 0.80

        record = {
            "timestamp": ts,
            "symbol": self.symbol,
            "action": action,
            "price": price,
            "rec_weight": advisory_data.get("recommended_weight", 1.0),
            "execution_weight": advisory_data.get("recommended_weight", 1.0),
            "trust_score": trust_score,
            "manipulation_score": manipulation_score,
            "risk_score": advisory_data.get("risk_score", 0.0),
            "risk_elevated": advisory_data.get("risk_elevated", False),
            "contrarian_buy": advisory_data.get("contrarian_buy_signal", False),
            "growth_favorable": advisory_data.get("growth_favorable", False),
            "hft_active": advisory_data.get("hft_active", False),
            "hft_delta_v": advisory_data.get("hft_delta_v", 0.0),
            "bullish_mode_active": bullish_mode_active,
            "bullish_multiplier_price": p_mult,
            "bullish_multiplier_volume": v_mult,
            "bullish_execution_scale": e_scale,
            "bullish_sell_ceiling_factor": s_factor,
            "details": details,
            "reason": details
        }
        log_line = json.dumps(record)
        logger.info(f"[AUDIT] {log_line}")

        try:
            with open(self.audit_log_file, "a") as f:
                f.write(log_line + "\n")
        except Exception as e:
            logger.error(f"Failed to write to audit log file: {e}")

    def process_tick(self, advisory_data: Dict[str, Any], current_price: float, safety_state: Optional[Dict[str, Any]] = None,
                     trust_score: float = 0.85, manipulation_score: float = 0.0):
        # 1. Safety & Drawdown Check
        if safety_state and (safety_state.get("kill_switch") or safety_state.get("hardware_fault")):
            self.trigger_lockout("Hardware fault or kill switch triggered in SafetyState")

        if self.current_equity > self.peak_equity:
            self.peak_equity = self.current_equity

        drawdown = (self.peak_equity - self.current_equity) / self.peak_equity if self.peak_equity > 0 else 0.0
        drawdown_near_breach = (drawdown >= self.max_daily_drawdown_pct * 0.8) or self.locked_out

        if drawdown >= self.max_daily_drawdown_pct:
            self.trigger_lockout(f"Daily drawdown threshold breached: {drawdown * 100:.2f}% >= {self.max_daily_drawdown_pct * 100:.2f}%")

        bullish_active = is_bullish_mode_allowed(
            trust_score=trust_score,
            manipulation_score=manipulation_score,
            drawdown_state=drawdown_near_breach,
            hft_bias_config=self.hft_bias_config
        )

        # 2. Determine signal intent
        desired_side = "FLAT"
        signal_type = "NEUTRAL"

        if self.locked_out:
            desired_side = "FLAT"
            signal_type = "LOCKOUT_FLAT"
        elif advisory_data.get("contrarian_buy_signal"):
            desired_side = "BUY"
            signal_type = "CONTRARIAN_BUY"
        elif advisory_data.get("growth_favorable") and not advisory_data.get("risk_elevated"):
            desired_side = "BUY"
            signal_type = "GROWTH_BUY"
        elif advisory_data.get("risk_elevated"):
            desired_side = "FLAT"
            signal_type = "RISK_REDUCE"

        # 3. Hysteresis filter
        if desired_side == self.pending_side:
            self.consecutive_bars += 1
        else:
            self.pending_side = desired_side
            self.consecutive_bars = 1

        if self.consecutive_bars < self.hysteresis_bars:
            self.log_audit("DEBOUNCE_WAIT", f"Signal pending confirmation: {signal_type}", advisory_data, current_price, trust_score, manipulation_score, bullish_active)
            return

        # 4. Position alignment check
        if desired_side == self.current_position_side and desired_side != "FLAT":
            self.log_audit("HOLD", f"Position already aligned with signal: {signal_type}", advisory_data, current_price, trust_score, manipulation_score, bullish_active)
            return

        if not self.enable_auto_execute:
            self.log_audit("DRY_RUN_SIGNAL", f"Auto-execution disabled. Would execute: {signal_type}", advisory_data, current_price, trust_score, manipulation_score, bullish_active)
            return
        if desired_side == "FLAT":
            if self.current_position_side != "FLAT":
                self.current_position_side = "FLAT"
                self.log_audit("FLAT_POSITION", "Position flattened successfully", advisory_data, current_price, trust_score, manipulation_score, bullish_active)
        elif desired_side == "BUY":
            alloc_usd = self.max_position_usd * advisory_data.get("recommended_weight", 1.0)
            if advisory_data.get("risk_elevated"):
                alloc_usd *= self.risk_reduce_factor

            qty = int(alloc_usd // current_price) if current_price > 0 else 0
            if qty > 0:
                order_id = f"MOCK-PY-ALPACA-{self.order_counter}"
                self.order_counter += 1
                self.current_position_side = "BUY"
                self.log_audit("ORDER_SUBMITTED", f"Order ID: {order_id} Qty: {qty}", advisory_data, current_price, trust_score, manipulation_score, bullish_active)
            else:
                self.log_audit("SKIPPED_QTY_ZERO", "Calculated order quantity is 0", advisory_data, current_price, trust_score, manipulation_score, bullish_active)

        # 5. Order execution
        if desired_side == "FLAT":
            if self.current_position_side != "FLAT":
                self.current_position_side = "FLAT"
                self.log_audit("FLAT_POSITION", "Position flattened successfully", advisory_data, current_price)
        elif desired_side == "BUY":
            alloc_usd = self.max_position_usd * advisory_data.get("recommended_weight", 1.0)
            if advisory_data.get("risk_elevated"):
                alloc_usd *= self.risk_reduce_factor

            qty = int(alloc_usd // current_price) if current_price > 0 else 0
            if qty > 0:
                order_id = f"MOCK-PY-ALPACA-{self.order_counter}"
                self.order_counter += 1
                self.current_position_side = "BUY"
                self.log_audit("ORDER_SUBMITTED", f"Order ID: {order_id} Qty: {qty}", advisory_data, current_price)
            else:
                self.log_audit("SKIPPED_QTY_ZERO", "Calculated order quantity is 0", advisory_data, current_price)
