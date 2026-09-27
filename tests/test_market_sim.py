"""
Unit and Integration Tests for Market Microstructure Simulator and Agents
"""

import os
import json
import numpy as np
import pandas as pd
import pytest
import yaml

from src.market_sim import (
    LimitOrderBook,
    ImpactMarketModel,
    Order,
    OrderSide,
    OrderType,
    generate_adoption_curve,
    generate_sentiment_curve,
    MarketSimulator,
)
from src.agents import (
    BullishAdopter,
    ProfitTaker,
    PanicSeller,
    LiquidityProvider,
    AlgorithmicArbitrageur,
    create_agent_population,
)


def test_order_book_matching():
    lob = LimitOrderBook(tick_size=0.01)

    # Place limit sell order
    sell_order = Order(
        order_id=1,
        agent_id="seller_1",
        agent_type="profit_taker",
        side=OrderSide.SELL,
        order_type=OrderType.LIMIT,
        price=105.0,
        quantity=10.0,
        timestamp=1,
    )
    lob.process_order(sell_order)
    assert lob.get_best_ask() == 105.0
    assert len(lob.asks) == 1

    # Place matching limit buy order
    buy_order = Order(
        order_id=2,
        agent_id="buyer_1",
        agent_type="bullish_adopter",
        side=OrderSide.BUY,
        order_type=OrderType.LIMIT,
        price=105.0,
        quantity=6.0,
        timestamp=2,
    )
    trades = lob.process_order(buy_order)

    assert len(trades) == 1
    assert trades[0].price == 105.0
    assert trades[0].quantity == 6.0
    assert lob.last_traded_price == 105.0
    assert lob.asks[0].remaining_quantity == 4.0


def test_impact_market_model():
    model = ImpactMarketModel(initial_price=100.0, impact_coefficient=0.01)
    orders = [
        Order(1, "buyer_1", "bullish_adopter", OrderSide.BUY, OrderType.MARKET, None, 100.0, 1),
        Order(2, "seller_1", "profit_taker", OrderSide.SELL, OrderType.MARKET, None, 50.0, 1),
    ]
    trades, new_price = model.process_orders(orders, timestamp=1)

    assert len(trades) == 1
    assert trades[0].quantity == 50.0
    assert new_price > 100.0  # Net positive buy imbalance increases price


def test_order_book_rejects_invalid_orders_and_tick_size():
    with pytest.raises(ValueError, match="tick_size"):
        LimitOrderBook(tick_size=0.0)

    lob = LimitOrderBook()
    invalid_limit = Order(1, "buyer", "test", OrderSide.BUY, OrderType.LIMIT, None, 1.0, 1)
    with pytest.raises(ValueError, match="limit orders"):
        lob.process_order(invalid_limit)

    invalid_quantity = Order(2, "buyer", "test", OrderSide.BUY, OrderType.MARKET, None, 0.0, 1)
    with pytest.raises(ValueError, match="quantity"):
        lob.process_order(invalid_quantity)


def test_impact_model_uses_only_remaining_order_quantity():
    model = ImpactMarketModel(initial_price=100.0, impact_coefficient=0.01)
    buy = Order(1, "buyer", "test", OrderSide.BUY, OrderType.MARKET, None, 10.0, 1, filled_quantity=6.0)
    sell = Order(2, "seller", "test", OrderSide.SELL, OrderType.MARKET, None, 4.0, 1)

    trades, _ = model.process_orders([buy, sell], timestamp=1)

    assert len(trades) == 1
    assert trades[0].quantity == 4.0
    assert buy.filled_quantity == 10.0


def test_curves_generation():
    config = {
        "type": "logistic",
        "initial_value": 100.0,
        "target_value": 200.0,
        "midpoint": 50,
        "growth_rate": 0.1,
    }
    A = generate_adoption_curve(config, T=100)
    assert len(A) == 100
    assert A[0] < A[50] < A[99]

    S = generate_sentiment_curve(A, {"baseline": 0.5, "sensitivity_to_adoption": 0.5})
    assert len(S) == 100
    assert np.all(S >= 0.0) and np.all(S <= 1.0)


def test_stepwise_curve_ignores_negative_step_indices():
    curve = generate_adoption_curve(
        {
            "type": "stepwise",
            "initial_value": 100.0,
            "steps": [{"step": -1, "value": 999.0}, {"step": 2, "value": 120.0}],
        },
        T=4,
    )

    assert curve.tolist() == [100.0, 100.0, 120.0, 120.0]


def test_market_simulator_rejects_empty_horizon():
    with pytest.raises(ValueError, match="positive integer"):
        MarketSimulator("empty", {"T": 0}, {"initial_price": 100.0})


def test_agent_decisions_and_executions():
    rng = np.random.default_rng(42)
    buyer = BullishAdopter("bull_1", initial_cash=10000.0, initial_inventory=100.0)
    seller = ProfitTaker("profit_1", initial_cash=5000.0, initial_inventory=500.0, profit_target_pct=0.10)

    buy_orders = buyer.decide_orders(
        timestamp=1,
        market_price=110.0,
        mid_price=110.0,
        fundamental_value=120.0,
        adoption_level=120.0,
        sentiment=0.8,
        volatility=0.01,
        rng=rng,
    )
    sell_orders = seller.decide_orders(
        timestamp=1,
        market_price=130.0,
        mid_price=130.0,
        fundamental_value=100.0,
        adoption_level=100.0,
        sentiment=0.5,
        volatility=0.01,
        rng=rng,
    )

    assert isinstance(buy_orders, list)
    assert isinstance(sell_orders, list)

    # Test trade execution updates
    buyer.on_trade_execution(OrderSide.BUY, price=100.0, quantity=10.0)
    assert buyer.cash == 10000.0 - 1000.0
    assert buyer.inventory == 110.0


def test_arbitrageur_order_type_and_price_are_consistent():
    agent = AlgorithmicArbitrageur("arb", initial_cash=100000.0, initial_inventory=1000.0)
    rng = np.random.default_rng(7)

    orders = []
    for timestamp in range(1, 50):
        orders.extend(
            agent.decide_orders(
                timestamp=timestamp,
                market_price=80.0,
                mid_price=80.0,
                fundamental_value=100.0,
                adoption_level=100.0,
                sentiment=0.5,
                volatility=0.01,
                rng=rng,
            )
        )

    assert orders
    assert all((order.price is not None) == (order.order_type == OrderType.LIMIT) for order in orders)


def test_market_simulator_run():
    default_settings = {"initial_price": 100.0, "tick_size": 0.01, "market_model": "lob"}
    scenario_config = {
        "seed": 42,
        "T": 50,
        "market_model": "lob",
        "adoption_curve": {"type": "logistic", "initial_value": 100.0, "target_value": 150.0, "midpoint": 25, "growth_rate": 0.05},
        "sentiment": {"baseline": 0.5, "sensitivity_to_adoption": 0.5},
        "agents": {"bullish_adopters": 10, "profit_takers": 10, "panic_sellers": 5, "liquidity_providers": 5, "arbitrageurs": 5},
        "initial_allocations": {
            "bullish_adopters": {"cash": 100000.0, "inventory": 1000.0},
            "profit_takers": {"cash": 50000.0, "inventory": 800.0},
            "panic_sellers": {"cash": 30000.0, "inventory": 500.0},
            "liquidity_providers": {"cash": 200000.0, "inventory": 2000.0},
            "arbitrageurs": {"cash": 150000.0, "inventory": 1200.0},
        },
    }

    agents = create_agent_population(scenario_config["agents"], scenario_config["initial_allocations"], 100.0)
    simulator = MarketSimulator("test_sc", scenario_config, default_settings)

    timeseries_df, agent_df, indicators = simulator.run(agents)

    assert len(timeseries_df) == 50
    assert "price" in timeseries_df.columns
    assert "sell_ratio" in timeseries_df.columns
    assert len(agent_df) == len(agents)
    assert "max_drawdown_pct" in indicators
