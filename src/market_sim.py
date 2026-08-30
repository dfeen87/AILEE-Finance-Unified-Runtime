"""
AILEE Finance Runtime - Market Microstructure Simulator

Provides discrete-time market execution engines:
1. LimitOrderBook: Price-time priority matching engine with partial fills.
2. ImpactMarketModel: Impact-driven price dynamics based on net order flow imbalance.
3. MarketSimulator: Discrete-time simulation coordinator and metric exporter.
"""

from dataclasses import dataclass, field
from enum import Enum
import math
from typing import Dict, List, Optional, Tuple, Any
import numpy as np
import pandas as pd


class OrderSide(Enum):
    BUY = "BUY"
    SELL = "SELL"


class OrderType(Enum):
    LIMIT = "LIMIT"
    MARKET = "MARKET"


@dataclass
class Order:
    order_id: int
    agent_id: str
    agent_type: str
    side: OrderSide
    order_type: OrderType
    price: Optional[float]
    quantity: float
    timestamp: int
    filled_quantity: float = 0.0

    @property
    def remaining_quantity(self) -> float:
        return max(0.0, self.quantity - self.filled_quantity)


@dataclass
class Trade:
    trade_id: int
    buy_order_id: int
    sell_order_id: int
    buyer_id: str
    seller_id: str
    buyer_type: str
    seller_type: str
    price: float
    quantity: float
    timestamp: int


class LimitOrderBook:
    def __init__(self, tick_size: float = 0.01):
        self.tick_size = tick_size
        self.bids: List[Order] = []  # Sorted descending by price, then ascending by timestamp
        self.asks: List[Order] = []  # Sorted ascending by price, then ascending by timestamp
        self.trades: List[Trade] = []
        self.trade_counter = 0
        self.last_traded_price: Optional[float] = None

    def _round_price(self, price: float) -> float:
        return round(round(price / self.tick_size) * self.tick_size, 4)

    def get_best_bid(self) -> Optional[float]:
        return self.bids[0].price if self.bids else None

    def get_best_ask(self) -> Optional[float]:
        return self.asks[0].price if self.asks else None

    def get_mid_price(self, fallback_price: float = 100.0) -> float:
        best_bid = self.get_best_bid()
        best_ask = self.get_best_ask()
        if best_bid is not None and best_ask is not None:
            return round((best_bid + best_ask) / 2.0, 4)
        elif best_bid is not None:
            return best_bid
        elif best_ask is not None:
            return best_ask
        elif self.last_traded_price is not None:
            return self.last_traded_price
        return fallback_price

    def get_spread(self) -> Optional[float]:
        best_bid = self.get_best_bid()
        best_ask = self.get_best_ask()
        if best_bid is not None and best_ask is not None:
            return round(best_ask - best_bid, 4)
        return None

    def get_depth(self, levels: int = 10) -> Dict[str, List[Tuple[float, float]]]:
        bid_depth: Dict[float, float] = {}
        for o in self.bids:
            p = o.price
            bid_depth[p] = bid_depth.get(p, 0.0) + o.remaining_quantity

        ask_depth: Dict[float, float] = {}
        for o in self.asks:
            p = o.price
            ask_depth[p] = ask_depth.get(p, 0.0) + o.remaining_quantity

        sorted_bids = sorted(bid_depth.items(), key=lambda x: x[0], reverse=True)[:levels]
        sorted_asks = sorted(ask_depth.items(), key=lambda x: x[0])[:levels]
        return {"bids": sorted_bids, "asks": sorted_asks}

    def process_order(self, order: Order) -> List[Trade]:
        if order.order_type == OrderType.LIMIT and order.price is not None:
            order.price = self._round_price(order.price)

        executed_trades: List[Trade] = []

        if order.side == OrderSide.BUY:
            # Match against asks
            while order.remaining_quantity > 1e-6 and self.asks:
                best_ask = self.asks[0]
                # If limit order, price must be >= best ask
                if order.order_type == OrderType.LIMIT and order.price is not None:
                    if order.price < best_ask.price:
                        break

                match_price = best_ask.price
                match_qty = min(order.remaining_quantity, best_ask.remaining_quantity)

                order.filled_quantity += match_qty
                best_ask.filled_quantity += match_qty

                self.trade_counter += 1
                trade = Trade(
                    trade_id=self.trade_counter,
                    buy_order_id=order.order_id,
                    sell_order_id=best_ask.order_id,
                    buyer_id=order.agent_id,
                    seller_id=best_ask.agent_id,
                    buyer_type=order.agent_type,
                    seller_type=best_ask.agent_type,
                    price=match_price,
                    quantity=match_qty,
                    timestamp=order.timestamp,
                )
                executed_trades.append(trade)
                self.trades.append(trade)
                self.last_traded_price = match_price

                if best_ask.remaining_quantity <= 1e-6:
                    self.asks.pop(0)

            # If limit order still has remaining qty, insert into bids queue
            if order.order_type == OrderType.LIMIT and order.remaining_quantity > 1e-6 and order.price is not None:
                self.bids.append(order)
                self.bids.sort(key=lambda o: (-(o.price or 0.0), o.timestamp))

        elif order.side == OrderSide.SELL:
            # Match against bids
            while order.remaining_quantity > 1e-6 and self.bids:
                best_bid = self.bids[0]
                if order.order_type == OrderType.LIMIT and order.price is not None:
                    if order.price > best_bid.price:
                        break

                match_price = best_bid.price
                match_qty = min(order.remaining_quantity, best_bid.remaining_quantity)

                order.filled_quantity += match_qty
                best_bid.filled_quantity += match_qty

                self.trade_counter += 1
                trade = Trade(
                    trade_id=self.trade_counter,
                    buy_order_id=best_bid.order_id,
                    sell_order_id=order.order_id,
                    buyer_id=best_bid.agent_id,
                    seller_id=order.agent_id,
                    buyer_type=best_bid.agent_type,
                    seller_type=order.agent_type,
                    price=match_price,
                    quantity=match_qty,
                    timestamp=order.timestamp,
                )
                executed_trades.append(trade)
                self.trades.append(trade)
                self.last_traded_price = match_price

                if best_bid.remaining_quantity <= 1e-6:
                    self.bids.pop(0)

            # If limit order still has remaining qty, insert into asks queue
            if order.order_type == OrderType.LIMIT and order.remaining_quantity > 1e-6 and order.price is not None:
                self.asks.append(order)
                self.asks.sort(key=lambda o: (o.price if o.price is not None else float('inf'), o.timestamp))

        return executed_trades

    def clear_expired_orders(self, max_age: int = 5, current_time: int = 0):
        self.bids = [o for o in self.bids if (current_time - o.timestamp) <= max_age and o.remaining_quantity > 1e-6]
        self.asks = [o for o in self.asks if (current_time - o.timestamp) <= max_age and o.remaining_quantity > 1e-6]


class ImpactMarketModel:
    def __init__(self, initial_price: float = 100.0, impact_coefficient: float = 0.005):
        self.current_price = initial_price
        self.impact_coefficient = impact_coefficient
        self.trades: List[Trade] = []
        self.trade_counter = 0

    def process_orders(self, orders: List[Order], timestamp: int) -> Tuple[List[Trade], float]:
        buy_qty = sum(o.quantity for o in orders if o.side == OrderSide.BUY)
        sell_qty = sum(o.quantity for o in orders if o.side == OrderSide.SELL)
        net_imbalance = buy_qty - sell_qty

        # Impact formula: dP = price * impact_coef * (net_imbalance / total_volume) or absolute imbalance
        tot_vol = buy_qty + sell_qty
        if tot_vol > 0:
            price_change = self.current_price * self.impact_coefficient * (net_imbalance / (tot_vol + 100.0))
        else:
            price_change = 0.0

        self.current_price = max(0.01, round(self.current_price + price_change, 4))
        executed_trades: List[Trade] = []

        # Execute matched volume at updated price
        matched_volume = min(buy_qty, sell_qty)
        if matched_volume > 0:
            buys = [o for o in orders if o.side == OrderSide.BUY]
            sells = [o for o in orders if o.side == OrderSide.SELL]

            b_idx, s_idx = 0, 0
            while b_idx < len(buys) and s_idx < len(sells):
                bo = buys[b_idx]
                so = sells[s_idx]
                qty = min(bo.remaining_quantity, so.remaining_quantity)
                if qty > 1e-6:
                    bo.filled_quantity += qty
                    so.filled_quantity += qty
                    self.trade_counter += 1
                    t = Trade(
                        trade_id=self.trade_counter,
                        buy_order_id=bo.order_id,
                        sell_order_id=so.order_id,
                        buyer_id=bo.agent_id,
                        seller_id=so.agent_id,
                        buyer_type=bo.agent_type,
                        seller_type=so.agent_type,
                        price=self.current_price,
                        quantity=qty,
                        timestamp=timestamp,
                    )
                    executed_trades.append(t)
                    self.trades.append(t)
                if bo.remaining_quantity <= 1e-6:
                    b_idx += 1
                if so.remaining_quantity <= 1e-6:
                    s_idx += 1

        return executed_trades, self.current_price


def generate_adoption_curve(config: Dict[str, Any], T: int) -> np.ndarray:
    curve_type = config.get("type", "logistic")
    A = np.zeros(T)
    if curve_type == "logistic":
        A0 = config.get("initial_value", 100.0)
        Atarget = config.get("target_value", 250.0)
        midpoint = config.get("midpoint", T // 2)
        growth_rate = config.get("growth_rate", 0.015)
        for t in range(T):
            A[t] = A0 + (Atarget - A0) / (1.0 + math.exp(-growth_rate * (t - midpoint)))
    elif curve_type == "stepwise":
        A0 = config.get("initial_value", 100.0)
        A.fill(A0)
        steps = config.get("steps", [])
        for step_info in sorted(steps, key=lambda x: x["step"]):
            step_t = step_info["step"]
            val = step_info["value"]
            if step_t < T:
                A[step_t:] = val
    else:
        A.fill(config.get("initial_value", 100.0))
    return A


def generate_sentiment_curve(A: np.ndarray, config: Dict[str, Any]) -> np.ndarray:
    baseline = config.get("baseline", 0.5)
    sensitivity = config.get("sensitivity_to_adoption", 0.5)
    A0 = A[0] if len(A) > 0 and A[0] > 0 else 1.0
    sentiment = baseline + sensitivity * ((A - A0) / A0)
    return np.clip(sentiment, 0.0, 1.0)


class MarketSimulator:
    """
    Discrete-time simulation engine that coordinates agents and order book/impact model
    across steps t = 1 ... T.
    """

    def __init__(self, scenario_name: str, scenario_config: Dict[str, Any], default_market_settings: Dict[str, Any]):
        self.scenario_name = scenario_name
        self.config = scenario_config
        self.default_settings = default_market_settings
        self.seed = self.config.get("seed", 42)
        self.rng = np.random.default_rng(self.seed)

        self.T = self.config.get("T", 1000)
        self.initial_price = self.default_settings.get("initial_price", 100.0)
        self.market_model_type = self.config.get("market_model", self.default_settings.get("market_model", "lob"))

        # Setup curves
        self.A = generate_adoption_curve(self.config.get("adoption_curve", {}), self.T)
        self.S = generate_sentiment_curve(self.A, self.config.get("sentiment", {}))
        self.F = self.A.copy()  # Fundamental value directly tied to adoption level

        # Market Engine
        if self.market_model_type == "lob":
            tick_size = self.default_settings.get("tick_size", 0.01)
            self.lob = LimitOrderBook(tick_size=tick_size)
            self.impact_model = None
        else:
            impact_coef = self.default_settings.get("impact_coefficient", 0.005)
            self.impact_model = ImpactMarketModel(initial_price=self.initial_price, impact_coefficient=impact_coef)
            self.lob = None

        self.last_price = self.initial_price

    def run(self, agents: List[Any]) -> Tuple[pd.DataFrame, pd.DataFrame, Dict[str, Any]]:
        price_history = []
        mid_price_history = []
        volume_history = []
        buy_volume_history = []
        sell_volume_history = []
        net_flow_history = []
        volatility_history = []
        spread_history = []

        window_prices = [self.initial_price]
        agent_map = {a.agent_id: a for a in agents}

        for t in range(1, self.T + 1):
            cur_A = self.A[t - 1]
            cur_S = self.S[t - 1]
            cur_F = self.F[t - 1]

            # Compute rolling volatility over 20 steps
            if len(window_prices) > 1:
                rets = np.diff(window_prices[-20:]) / window_prices[-20:-1]
                vol = float(np.std(rets)) if len(rets) > 0 else 0.001
            else:
                vol = 0.001
            volatility_history.append(vol)

            mid_p = self.lob.get_mid_price(fallback_price=self.last_price) if self.lob else self.last_price

            # Collect orders from all agents
            step_orders: List[Order] = []
            for agent in agents:
                agent_orders = agent.decide_orders(
                    timestamp=t,
                    market_price=self.last_price,
                    mid_price=mid_p,
                    fundamental_value=cur_F,
                    adoption_level=cur_A,
                    sentiment=cur_S,
                    volatility=vol,
                    rng=self.rng,
                )
                step_orders.extend(agent_orders)

            # Process orders
            step_trades: List[Trade] = []
            if self.lob:
                # Expire stale limit orders older than 10 steps
                self.lob.clear_expired_orders(max_age=10, current_time=t)
                for order in step_orders:
                    trades = self.lob.process_order(order)
                    step_trades.extend(trades)
                mid_p = self.lob.get_mid_price(fallback_price=self.last_price)
                if self.lob.last_traded_price is not None:
                    self.last_price = self.lob.last_traded_price
                else:
                    self.last_price = mid_p
                spread = self.lob.get_spread() or 0.0
            else:
                step_trades, self.last_price = self.impact_model.process_orders(step_orders, timestamp=t)
                mid_p = self.last_price
                spread = 0.0

            window_prices.append(self.last_price)
            price_history.append(self.last_price)
            mid_price_history.append(mid_p)
            spread_history.append(spread)

            # Update agent balances and track trade volume
            step_vol = sum(tr.quantity for tr in step_trades)
            step_buy_vol = sum(o.quantity for o in step_orders if o.side == OrderSide.BUY)
            step_sell_vol = sum(o.quantity for o in step_orders if o.side == OrderSide.SELL)
            net_flow = step_buy_vol - step_sell_vol

            volume_history.append(step_vol)
            buy_volume_history.append(step_buy_vol)
            sell_volume_history.append(step_sell_vol)
            net_flow_history.append(net_flow)

            for tr in step_trades:
                if tr.buyer_id in agent_map:
                    agent_map[tr.buyer_id].on_trade_execution(OrderSide.BUY, tr.price, tr.quantity)
                if tr.seller_id in agent_map:
                    agent_map[tr.seller_id].on_trade_execution(OrderSide.SELL, tr.price, tr.quantity)

        # Build time series DataFrame
        timeseries_df = pd.DataFrame({
            "step": np.arange(1, self.T + 1),
            "price": price_history,
            "mid_price": mid_price_history,
            "fundamental_value": self.F,
            "adoption_level": self.A,
            "sentiment": self.S,
            "volume": volume_history,
            "buy_volume": buy_volume_history,
            "sell_volume": sell_volume_history,
            "net_order_flow": net_flow_history,
            "volatility": volatility_history,
            "spread": spread_history,
        })

        # Calculate Selling Pressure Indicators
        timeseries_df["sell_ratio"] = timeseries_df["sell_volume"] / np.maximum(1e-6, timeseries_df["buy_volume"] + timeseries_df["sell_volume"])

        # Max drawdown during adoption window
        prices = timeseries_df["price"].values
        cummax = np.maximum.accumulate(prices)
        drawdowns = (cummax - prices) / np.maximum(1e-6, cummax)
        max_drawdown = float(np.max(drawdowns))

        # Duration of elevated selling pressure (consecutive steps with sell_ratio > 0.55)
        elevated_mask = (timeseries_df["sell_ratio"] > 0.55).astype(int).values
        max_elevated_duration = 0
        cur_dur = 0
        for val in elevated_mask:
            if val == 1:
                cur_dur += 1
                max_elevated_duration = max(max_elevated_duration, cur_dur)
            else:
                cur_dur = 0

        # Recovery time to new high after selling cascade
        peak_idx = int(np.argmax(prices[:self.T // 2])) if len(prices) > 0 else 0
        peak_val = prices[peak_idx]
        recovery_time = -1
        for idx in range(peak_idx + 1, len(prices)):
            if prices[idx] >= peak_val:
                recovery_time = idx - peak_idx
                break

        indicators = {
            "max_drawdown_pct": max_drawdown * 100.0,
            "max_elevated_sell_duration_steps": max_elevated_duration,
            "mean_sell_ratio": float(np.mean(timeseries_df["sell_ratio"])),
            "peak_price": float(np.max(prices)),
            "final_price": float(prices[-1]),
            "total_volume": float(np.sum(volume_history)),
            "recovery_time_steps": recovery_time,
        }

        # Build Agent Statistics DataFrame
        agent_stats = []
        for a in agents:
            port_val = a.get_portfolio_value(self.last_price)
            unrealized = a.get_unrealized_pnl(self.last_price)
            turnover = (abs(a.cash - a.initial_cash) + abs(a.inventory - a.initial_inventory) * self.last_price) / max(1.0, a.initial_cash)
            agent_stats.append({
                "agent_id": a.agent_id,
                "agent_type": a.agent_type,
                "initial_cash": a.initial_cash,
                "initial_inventory": a.initial_inventory,
                "final_cash": round(a.cash, 2),
                "final_inventory": round(a.inventory, 2),
                "realized_pnl": round(a.realized_pnl, 2),
                "unrealized_pnl": round(unrealized, 2),
                "portfolio_value": round(port_val, 2),
                "turnover_rate": round(turnover, 4),
            })
        agent_df = pd.DataFrame(agent_stats)

        return timeseries_df, agent_df, indicators
