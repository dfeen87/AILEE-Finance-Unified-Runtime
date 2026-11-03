"""Deterministic simulation and mathematical ingress dependency contracts."""
import io
import json

import numpy as np
import pandas as pd
import pytest

from src.agents import create_agent_population
from src.market_sim import MarketSimulator
from scripts.generate_upsidemd import format_agent_summary
from ailee_finance.math_engine.validation import finite_number, MathValidationError


def test_seeded_market_outputs_and_reporting_contract():
    agents = create_agent_population({name: 2 for name in (
        'bullish_adopters', 'profit_takers', 'panic_sellers',
        'liquidity_providers', 'arbitrageurs')}, {}, 100.0)
    frame, portfolio, indicators = MarketSimulator(
        'audit', {'T': 100, 'seed': 42, 'market_model': 'lob'},
        {'initial_price': 100.0}).run(agents)
    # Recorded on NumPy 1.24.0 / pandas 2.0.0, Python 3.11.
    assert indicators['final_price'] == 142.66
    assert indicators['total_volume'] == 11720.5
    assert indicators['max_drawdown_pct'] == pytest.approx(0.24662038728535826, abs=1e-12, rel=0)
    assert indicators['mean_sell_ratio'] == pytest.approx(0.3967409758826386, abs=1e-12, rel=0)
    assert indicators['max_elevated_sell_duration_steps'] == 3
    assert indicators['recovery_time_steps'] == 1
    assert json.loads(json.dumps(indicators)) == indicators
    restored = pd.read_csv(io.StringIO(frame.to_csv(index=False)))
    np.testing.assert_allclose(restored['price'], frame['price'], rtol=0, atol=1e-12)
    assert restored['step'].tolist() == list(range(1, 101))
    summary = format_agent_summary(portfolio)
    assert 'Bullish Adopter' in summary
    assert 'Liquidity Provider' in summary


@pytest.mark.parametrize('value', [np.float32(1), np.int64(1), np.float64(np.nan), np.float64(np.inf)])
def test_numpy_values_do_not_bypass_mathematical_input_validation(value):
    with pytest.raises(MathValidationError):
        finite_number(value, 'audit')


def test_numpy_float64_accepted_as_finite_python_float():
    result = finite_number(np.float64(1.25), 'audit')
    assert type(result) is float
    assert result == 1.25
