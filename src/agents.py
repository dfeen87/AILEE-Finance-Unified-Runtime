"""
AILEE Finance Runtime - Agent Personas for Microstructure Simulation

Defines heterogeneous agent personas:
- BullishAdopters: Demand scales with adoption A(t) and sentiment S(t).
- ProfitTakers: Sell into strength when price exceeds fair value or trailing baseline.
- PanicSellers: Trigger stop-loss sell cascades during drawdowns and volatility spikes.
- LiquidityProviders: Quoting two-sided bids and asks around mid-price with volatility spread widening.
- AlgorithmicArbitrageurs: Exploit price vs fundamental mispricing with configurable momentum/mean-reversion.
"""

import abc
import math
import random
from typing import Dict, List, Optional, Any
import numpy as np

from src.market_sim import Order, OrderSide, OrderType


class Agent(abc.ABC):
    def __init__(
        self,
        agent_id: str,
        agent_type: str,
        initial_cash: float = 100000.0,
        initial_inventory: float = 1000.0,
        reference_price: float = 100.0,
        risk_tolerance: float = 0.5,
    ):
        self.agent_id = agent_id
        self.agent_type = agent_type
        self.cash = float(initial_cash)
        self.inventory = float(initial_inventory)
        self.initial_cash = float(initial_cash)
        self.initial_inventory = float(initial_inventory)
        self.reference_price = float(reference_price)
        self.risk_tolerance = float(risk_tolerance)
        self.realized_pnl = 0.0
        self.total_cost_basis = float(initial_inventory * reference_price)
        self.peak_portfolio_value = float(initial_cash + initial_inventory * reference_price)
        self.order_counter = 0

    def get_portfolio_value(self, current_price: float) -> float:
        return self.cash + self.inventory * current_price

    def get_unrealized_pnl(self, current_price: float) -> float:
        if self.inventory > 0:
            avg_cost = self.total_cost_basis / max(1e-6, self.inventory)
            return (current_price - avg_cost) * self.inventory
        return 0.0

    def on_trade_execution(self, side: OrderSide, price: float, quantity: float):
        if side == OrderSide.BUY:
            self.cash -= price * quantity
            self.inventory += quantity
            self.total_cost_basis += price * quantity
        else:  # SELL
            avg_cost = (self.total_cost_basis / max(1e-6, self.inventory)) if self.inventory > 0 else price
            cost_of_sold = avg_cost * quantity
            self.cash += price * quantity
            self.inventory -= quantity
            self.total_cost_basis = max(0.0, self.total_cost_basis - cost_of_sold)
            self.realized_pnl += (price - avg_cost) * quantity

    def create_order(
        self,
        side: OrderSide,
        order_type: OrderType,
        quantity: float,
        price: Optional[float],
        timestamp: int,
    ) -> Order:
        self.order_counter += 1
        order_id_str = f"{self.agent_id}_{self.order_counter}"
        # Numeric order_id hash or integer counter
        numeric_id = hash((self.agent_id, self.order_counter, timestamp)) & 0x7FFFFFFF
        return Order(
            order_id=numeric_id,
            agent_id=self.agent_id,
            agent_type=self.agent_type,
            side=side,
            order_type=order_type,
            price=price,
            quantity=quantity,
            timestamp=timestamp,
        )

    @abc.abstractmethod
    def decide_orders(
        self,
        timestamp: int,
        market_price: float,
        mid_price: float,
        fundamental_value: float,
        adoption_level: float,
        sentiment: float,
        volatility: float,
        rng: np.random.Generator,
    ) -> List[Order]:
        pass


class BullishAdopter(Agent):
    """
    Long-term holder whose demand scales strongly with adoption A(t) and sentiment S(t).
    Buys aggressively when fundamentals improve; rarely sells.
    """

    def __init__(self, agent_id: str, **kwargs):
        super().__init__(agent_id, "bullish_adopter", **kwargs)

    def decide_orders(
        self,
        timestamp: int,
        market_price: float,
        mid_price: float,
        fundamental_value: float,
        adoption_level: float,
        sentiment: float,
        volatility: float,
        rng: np.random.Generator,
    ) -> List[Order]:
        orders = []

        # Demand factor based on sentiment and adoption premium over reference price
        adoption_gain = (fundamental_value - self.reference_price) / max(1.0, self.reference_price)
        buy_conviction = sentiment * (1.0 + max(0.0, adoption_gain)) * self.risk_tolerance

        # Small probability to act each step to avoid deterministic crowding
        if rng.random() < 0.35 and self.cash > 1000.0:
            budget = self.cash * min(0.20, buy_conviction * 0.10)
            target_price = mid_price * (1.0 + rng.normal(0.001, 0.002))
            quantity = budget / max(1.0, target_price)

            if quantity >= 0.1:
                # 60% limit order near bid/mid, 40% market order
                if rng.random() < 0.60:
                    orders.append(
                        self.create_order(
                            side=OrderSide.BUY,
                            order_type=OrderType.LIMIT,
                            quantity=round(quantity, 2),
                            price=round(target_price, 2),
                            timestamp=timestamp,
                        )
                    )
                else:
                    orders.append(
                        self.create_order(
                            side=OrderSide.BUY,
                            order_type=OrderType.MARKET,
                            quantity=round(quantity, 2),
                            price=None,
                            timestamp=timestamp,
                        )
                    )

        # Rarely rebalance (sell max 2% inventory if heavily overallocated and price >> fundamental)
        elif rng.random() < 0.02 and market_price > 1.3 * fundamental_value and self.inventory > 100.0:
            sell_qty = self.inventory * 0.02
            orders.append(
                self.create_order(
                    side=OrderSide.SELL,
                    order_type=OrderType.LIMIT,
                    quantity=round(sell_qty, 2),
                    price=round(mid_price * 1.01, 2),
                    timestamp=timestamp,
                )
            )

        return orders


class ProfitTaker(Agent):
    """
    Sells into strength when price exceeds fair fundamental value F(t)
    or trailing entry baseline by target percentage.
    """

    def __init__(self, agent_id: str, profit_target_pct: float = 0.15, **kwargs):
        super().__init__(agent_id, "profit_taker", **kwargs)
        self.profit_target_pct = profit_target_pct
        self.trailing_entry_price = kwargs.get("reference_price", 100.0)

    def decide_orders(
        self,
        timestamp: int,
        market_price: float,
        mid_price: float,
        fundamental_value: float,
        adoption_level: float,
        sentiment: float,
        volatility: float,
        rng: np.random.Generator,
    ) -> List[Order]:
        orders = []

        # Calculate deviation above fundamental value / trailing cost
        avg_cost = (self.total_cost_basis / max(1e-6, self.inventory)) if self.inventory > 0 else self.trailing_entry_price
        price_gain_pct = (market_price - avg_cost) / max(1e-6, avg_cost)
        fund_gain_pct = (market_price - fundamental_value) / max(1e-6, fundamental_value)

        # Triggers profit taking when price > entry * (1 + target) or price > 1.15 * fundamental
        if (price_gain_pct > self.profit_target_pct or fund_gain_pct > 0.10) and self.inventory > 1.0:
            if rng.random() < 0.40:  # Probability of taking profit this tick
                # Scaled sell fraction based on degree of overvaluation
                sell_fraction = min(0.35, max(0.05, (price_gain_pct - self.profit_target_pct) * 1.5))
                sell_qty = max(1.0, self.inventory * sell_fraction)

                # Mix of limit sell slightly above mid and aggressive market sell
                if rng.random() < 0.70:
                    limit_p = mid_price * (1.0 + abs(rng.normal(0.001, 0.002)))
                    orders.append(
                        self.create_order(
                            side=OrderSide.SELL,
                            order_type=OrderType.LIMIT,
                            quantity=round(sell_qty, 2),
                            price=round(limit_p, 2),
                            timestamp=timestamp,
                        )
                    )
                else:
                    orders.append(
                        self.create_order(
                            side=OrderSide.SELL,
                            order_type=OrderType.MARKET,
                            quantity=round(sell_qty, 2),
                            price=None,
                            timestamp=timestamp,
                        )
                    )

        # Dip buy if price drops significantly below fundamental value
        elif market_price < 0.90 * fundamental_value and self.cash > 1000.0 and rng.random() < 0.20:
            buy_qty = (self.cash * 0.15) / max(1.0, mid_price)
            orders.append(
                self.create_order(
                    side=OrderSide.BUY,
                    order_type=OrderType.LIMIT,
                    quantity=round(buy_qty, 2),
                    price=round(mid_price * 0.998, 2),
                    timestamp=timestamp,
                )
            )

        return orders


class PanicSeller(Agent):
    """
    Monitors short-term drawdown from recent peak and volatility spikes.
    Triggers heavy market sell cascades when loss exceeds drawdown threshold.
    """

    def __init__(self, agent_id: str, drawdown_threshold_pct: float = 0.05, **kwargs):
        super().__init__(agent_id, "panic_seller", **kwargs)
        self.drawdown_threshold_pct = drawdown_threshold_pct
        self.recent_peak_price = kwargs.get("reference_price", 100.0)

    def decide_orders(
        self,
        timestamp: int,
        market_price: float,
        mid_price: float,
        fundamental_value: float,
        adoption_level: float,
        sentiment: float,
        volatility: float,
        rng: np.random.Generator,
    ) -> List[Order]:
        orders = []

        # Track recent peak price
        if market_price > self.recent_peak_price:
            self.recent_peak_price = market_price

        drawdown = (self.recent_peak_price - market_price) / max(1e-6, self.recent_peak_price)

        # Panic sell trigger: drawdown breaches threshold OR high volatility combined with drawdown
        if (drawdown > self.drawdown_threshold_pct or (volatility > 0.03 and drawdown > 0.02)) and self.inventory > 1.0:
            if rng.random() < 0.60:  # High urgency to dump
                # Dump 40% - 100% of inventory rapidly via market order
                dump_fraction = min(1.0, 0.40 + drawdown * 5.0)
                sell_qty = max(1.0, self.inventory * dump_fraction)

                orders.append(
                    self.create_order(
                        side=OrderSide.SELL,
                        order_type=OrderType.MARKET,
                        quantity=round(sell_qty, 2),
                        price=None,
                        timestamp=timestamp,
                    )
                )

        # Reset peak when market recovers and panic subsides
        elif drawdown <= 0.01 and self.cash > 2000.0 and rng.random() < 0.15:
            # Re-enter gradually when stability returns
            buy_qty = (self.cash * 0.10) / max(1.0, mid_price)
            orders.append(
                self.create_order(
                    side=OrderSide.BUY,
                    order_type=OrderType.LIMIT,
                    quantity=round(buy_qty, 2),
                    price=round(mid_price * 0.995, 2),
                    timestamp=timestamp,
                )
            )

        return orders


class LiquidityProvider(Agent):
    """
    Market maker that posts bid/ask limit order ladders around mid-price.
    Widens spreads under high volatility or inventory skew.
    """

    def __init__(
        self,
        agent_id: str,
        half_spread_pct: float = 0.005,
        max_position: float = 50000.0,
        **kwargs
    ):
        super().__init__(agent_id, "liquidity_provider", **kwargs)
        self.half_spread_pct = half_spread_pct
        self.max_position = max_position

    def decide_orders(
        self,
        timestamp: int,
        market_price: float,
        mid_price: float,
        fundamental_value: float,
        adoption_level: float,
        sentiment: float,
        volatility: float,
        rng: np.random.Generator,
    ) -> List[Order]:
        orders = []

        # Adjust spread based on rolling volatility
        dynamic_spread = self.half_spread_pct * (1.0 + volatility * 20.0)

        # Inventory skew compensation (skew quotes to manage inventory back to neutral)
        target_inv = self.initial_inventory
        inv_skew = (self.inventory - target_inv) / max(1.0, self.max_position)
        inv_skew = np.clip(inv_skew, -0.02, 0.02)

        bid_price = mid_price * (1.0 - dynamic_spread - inv_skew)
        ask_price = mid_price * (1.0 + dynamic_spread - inv_skew)

        quote_qty = max(5.0, (self.cash * 0.05) / max(1.0, mid_price))

        # Post bid if inventory is under max limit and cash available
        if self.inventory < self.max_position and self.cash > bid_price * quote_qty:
            orders.append(
                self.create_order(
                    side=OrderSide.BUY,
                    order_type=OrderType.LIMIT,
                    quantity=round(quote_qty, 2),
                    price=round(bid_price, 2),
                    timestamp=timestamp,
                )
            )

        # Post ask if holding inventory
        if self.inventory > quote_qty:
            orders.append(
                self.create_order(
                    side=OrderSide.SELL,
                    order_type=OrderType.LIMIT,
                    quantity=round(quote_qty, 2),
                    price=round(ask_price, 2),
                    timestamp=timestamp,
                )
            )

        return orders


class AlgorithmicArbitrageur(Agent):
    """
    Quantitative trader exploiting mispricing between current market price and fundamental value F(t).
    Can operate in mean-reversion or momentum mode based on aggression parameter.
    """

    def __init__(self, agent_id: str, aggression: float = 0.5, **kwargs):
        super().__init__(agent_id, "arbitrageur", **kwargs)
        self.aggression = aggression
        self.prev_price = kwargs.get("reference_price", 100.0)

    def decide_orders(
        self,
        timestamp: int,
        market_price: float,
        mid_price: float,
        fundamental_value: float,
        adoption_level: float,
        sentiment: float,
        volatility: float,
        rng: np.random.Generator,
    ) -> List[Order]:
        orders = []

        mispricing = (fundamental_value - market_price) / max(1e-6, market_price)
        momentum = (market_price - self.prev_price) / max(1e-6, self.prev_price)
        self.prev_price = market_price

        # High aggression can amplify momentum (chasing trend) or trade mean reversion
        signal = mispricing * (1.0 - self.aggression * 0.5) + momentum * self.aggression

        if abs(signal) > 0.005 and rng.random() < 0.50:
            trade_value = self.cash * min(0.30, abs(signal) * 5.0 * self.aggression)
            quantity = trade_value / max(1.0, mid_price)

            if signal > 0 and self.cash > trade_value and quantity >= 0.1:
                # Buy signal
                orders.append(
                    self.create_order(
                        side=OrderSide.BUY,
                        order_type=OrderType.LIMIT if rng.random() < 0.5 else OrderType.MARKET,
                        quantity=round(quantity, 2),
                        price=round(mid_price * 1.001, 2) if rng.random() < 0.5 else None,
                        timestamp=timestamp,
                    )
                )
            elif signal < 0 and self.inventory >= quantity and quantity >= 0.1:
                # Sell signal
                orders.append(
                    self.create_order(
                        side=OrderSide.SELL,
                        order_type=OrderType.LIMIT if rng.random() < 0.5 else OrderType.MARKET,
                        quantity=round(quantity, 2),
                        price=round(mid_price * 0.999, 2) if rng.random() < 0.5 else None,
                        timestamp=timestamp,
                    )
                )

        return orders


def create_agent_population(
    agent_counts: Dict[str, int],
    allocations: Dict[str, Any],
    initial_price: float,
) -> List[Agent]:
    agents: List[Agent] = []

    type_mapping = {
        "bullish_adopters": (BullishAdopter, {}),
        "profit_takers": (ProfitTaker, {}),
        "panic_sellers": (PanicSeller, {}),
        "liquidity_providers": (LiquidityProvider, {}),
        "arbitrageurs": (AlgorithmicArbitrageur, {}),
    }

    for key, count in agent_counts.items():
        if key not in type_mapping:
            continue

        cls, _ = type_mapping[key]
        alloc = allocations.get(key, {})
        cash = alloc.get("cash", 100000.0)
        inv = alloc.get("inventory", 1000.0)

        # Extract type-specific kwargs
        extra_kwargs = {}
        if "risk_tolerance" in alloc:
            extra_kwargs["risk_tolerance"] = alloc["risk_tolerance"]
        if "profit_target_pct" in alloc:
            extra_kwargs["profit_target_pct"] = alloc["profit_target_pct"]
        if "drawdown_threshold_pct" in alloc:
            extra_kwargs["drawdown_threshold_pct"] = alloc["drawdown_threshold_pct"]
        if "half_spread_pct" in alloc:
            extra_kwargs["half_spread_pct"] = alloc["half_spread_pct"]
        if "max_position" in alloc:
            extra_kwargs["max_position"] = alloc["max_position"]
        if "aggression" in alloc:
            extra_kwargs["aggression"] = alloc["aggression"]

        for i in range(count):
            agent_id = f"{key}_{i+1}"
            agent = cls(
                agent_id=agent_id,
                initial_cash=cash,
                initial_inventory=inv,
                reference_price=initial_price,
                **extra_kwargs,
            )
            agents.append(agent)

    return agents
