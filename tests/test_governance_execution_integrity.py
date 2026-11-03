import copy
import json
import math
from fractions import Fraction
from decimal import Decimal
from types import SimpleNamespace

import pytest

from core.finance_kernel.portfolio_constraints import AssetAllocation, RiskBudget, apply_portfolio_constraints
from core.finance_kernel.temporal_consistency import TemporalState, TemporalStates, TemporalPortfolioState, TemporalResiduals, TemporalTraceSteps, enforce_temporal_consistency
from core.finance_kernel.meta_governance import apply_meta_governance_lock
from core.finance_kernel.volume_execution import VolumeExecutionOperator
from core.finance_kernel.unified_runtime import UnifiedRuntimeOperator
from core.finance_kernel.sync_adapter import SyncAdapter
from core.finance_kernel.wnfs import WNFSOperator
from core.finance_kernel.kernel_registry import LedgerOperator, KernelRegistry
from core.finance_kernel.kernel_context import FinanceKernelContext
from core.finance_kernel.kernel_config import FinanceKernelConfig
from core.finance_kernel.finance_kernel import FinanceRuntimeKernel


@pytest.mark.parametrize("bad", [math.nan, math.inf, -math.inf, -1.0])
def test_unsafe_budget_cannot_report_clean_constraints(bad):
    with pytest.raises(ValueError):
        apply_portfolio_constraints([AssetAllocation(1, 1.0, 50.0)], [], [], [], RiskBudget(bad))


@pytest.mark.parametrize("threshold", [math.nan, math.inf, -1.0])
def test_temporal_failure_is_atomic(threshold):
    curr = TemporalStates([TemporalState(1, 0.5, 50.0)])
    state = TemporalPortfolioState()
    residual, trace = TemporalResiduals(), TemporalTraceSteps()
    before = copy.deepcopy(curr.states[0].__dict__)
    with pytest.raises(ValueError):
        enforce_temporal_consistency(TemporalStates(), curr, TemporalPortfolioState(), state, residual, trace, threshold)
    assert curr.states[0].__dict__ == before
    assert residual.count == trace.count == 0


@pytest.mark.parametrize("bad", [Decimal("NaN"), Decimal("Infinity"), complex(math.nan, 1)])
def test_unsupported_numeric_encoding_cannot_hide_in_governance(bad):
    budget = RiskBudget(1.0)
    budget.max_portfolio_risk = bad
    with pytest.raises(ValueError):
        apply_portfolio_constraints([AssetAllocation(1, 1.0, 50.0)], [], [], [], budget)


def test_temporal_overflow_is_atomic_after_staged_arithmetic():
    curr = TemporalStates([TemporalState(1, 1e308, 1e308)])
    state = TemporalPortfolioState()
    before = copy.deepcopy(curr.states[0].__dict__)
    with pytest.raises(ValueError):
        enforce_temporal_consistency(TemporalStates(), curr, TemporalPortfolioState(), state,
                                     TemporalResiduals(), TemporalTraceSteps(), 1e308)
    assert curr.states[0].__dict__ == before
    assert state.portfolio_risk == 0.0


def test_missing_meta_evidence_cannot_authorize_execution():
    with pytest.raises(ValueError, match="Missing"):
        apply_meta_governance_lock({}, {}, {}, [], {})


def test_empty_list_trace_does_not_hide_missing_stress_override():
    state, _ = apply_meta_governance_lock({"total_residual": 0.0},
        {"summary": {"remaining_violations": 0, "final_portfolio_risk": 0.0, "max_risk_budget": 1.0}},
        {"stress_level": 1}, [], {"residual_sum": 0.0})
    assert state.execution_ready == 0


@pytest.mark.parametrize("field", ["max_position_usd", "max_daily_drawdown_pct", "risk_reduce_factor", "hysteresis_bars", "enable_auto_execute"])
@pytest.mark.parametrize("bad", [math.nan, math.inf, "false", True])
def test_execution_config_is_validated_at_construction(field, bad, tmp_path):
    if field == "enable_auto_execute" and bad is True:
        return
    with pytest.raises(ValueError):
        VolumeExecutionOperator(**{field: bad}, audit_log_file=str(tmp_path / "audit"))


def test_python_cannot_claim_unconfigured_broker_execution():
    with pytest.raises(ValueError, match="no broker provider"):
        VolumeExecutionOperator(mock_mode=False)


def test_simulated_execution_hard_cap_and_audit_atomicity(tmp_path):
    path = tmp_path / "audit"
    op = VolumeExecutionOperator(enable_auto_execute=True, hysteresis_bars=1, max_position_usd=100.0, audit_log_file=str(path))
    op.process_tick({"growth_favorable": True, "recommended_weight": 10.0}, 30.0)
    record = json.loads(path.read_text())
    assert record["action"] == "ORDER_SUBMITTED"
    assert "Qty: 3" in record["details"]
    failed = VolumeExecutionOperator(enable_auto_execute=True, hysteresis_bars=1, audit_log_file=str(tmp_path))
    with pytest.raises(OSError):
        failed.process_tick({"growth_favorable": True}, 30.0)
    assert failed.current_position_side == "FLAT"
    assert failed.order_counter == 1
    assert failed.locked_out


@pytest.mark.parametrize("bad", [math.nan, math.inf, -math.inf, True, 0.0])
def test_bad_execution_price_never_advances_state(bad, tmp_path):
    op = VolumeExecutionOperator(enable_auto_execute=True, hysteresis_bars=1, audit_log_file=str(tmp_path / "audit"))
    with pytest.raises(ValueError):
        op.process_tick({"growth_favorable": True}, bad)
    assert op.current_position_side == "FLAT"
    assert op.order_counter == 1
    assert op.consecutive_bars == 0


def test_unified_rejects_real_replay_then_recovers_on_fresh_evidence():
    op = UnifiedRuntimeOperator()
    assert op.execute({"cycle_sequence_id": 10, "timestamp_ns": 10})["execution_permitted"]
    for request in ({"cycle_sequence_id": 10, "timestamp_ns": 11}, {"cycle_sequence_id": 9, "timestamp_ns": 11},
                    {"cycle_sequence_id": 11, "timestamp_ns": 10}):
        with pytest.raises(ValueError, match="replay"):
            op.execute(request)
    assert op.execute({"cycle_sequence_id": 11, "timestamp_ns": 11})["execution_permitted"]


@pytest.mark.parametrize("bad", [math.nan, math.inf, True, Decimal("NaN")])
def test_protocol_clock_rejects_malformed_evidence_atomically(bad):
    adapter = SyncAdapter()
    before = copy.deepcopy(adapter.state.__dict__)
    with pytest.raises(ValueError):
        adapter.ingest_protocol_clock(1, 10, 0.0, bad)
    assert adapter.state.__dict__ == before


def test_protocol_clock_replay_does_not_reset_trusted_cadence():
    adapter = SyncAdapter()
    adapter.ingest_protocol_clock(2, 20, 0.0)
    with pytest.raises(ValueError, match="replayed"):
        adapter.ingest_protocol_clock(1, 19, 0.0)
    assert adapter.state.current_tick_index == 2


def test_stream_replay_cannot_reset_cursor_using_supplied_defaults():
    op = WNFSOperator()
    first = op.execute({"sequence_id": 10})
    assert first["ingestion_confidence"] == 1.0
    replay = op.execute({"sequence_id": 10, "expected_sequence": 1, "processed_frames": 0})
    assert replay["ingestion_confidence"] == 0.0
    assert replay["hft_freeze_required"]


def test_kernel_retains_ledger_chain_and_hash_covers_full_evidence():
    registry = KernelRegistry()
    registry.register_operator("ledger", LedgerOperator)
    kernel = FinanceRuntimeKernel(FinanceKernelContext("ledger", "session"), FinanceKernelConfig(), registry)
    payload = {"decision": {"final_value": 0.1, "confidence": 0.9, "status": "DECISION_VALID", "models_agreed": 2},
               "context": {"metadata": {"source": "a"}}}
    first = kernel.execute_operator("ledger", payload).data
    second = kernel.execute_operator("ledger", payload).data
    assert second["record"]["sequence"] == 2
    assert second["record"]["previous_hash"] == first["hash"]
    independent = LedgerOperator()
    changed = copy.deepcopy(payload)
    changed["context"]["metadata"]["source"] = "b"
    assert independent.execute(changed)["hash"] != first["hash"]
    audit = kernel._operator_instances["ledger"]
    audit._records[0]["decision"]["models_agreed"] = 1
    assert not audit.verify_integrity()
    with pytest.raises(ValueError, match="integrity"):
        audit.execute(payload)
