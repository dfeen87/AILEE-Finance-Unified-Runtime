"""
SELL Governance Module - Version 5.0.0
Provides SELL-side trust governance, manipulation detection, dynamic ceilings,
volatility grace adjustments, and consensus feed validation.
"""


def validate_sell_intent(signals):
    """
    Ensure SELL trigger is legitimate by validating market context, liquidity,
    volatility, and signal flags.

    Returns:
        dict: {"intent_valid": bool, "reason": str}
    """
    if not isinstance(signals, dict):
        return {"intent_valid": False, "reason": "Invalid signals payload: expected dict"}

    # Intent flag check
    if not signals.get("intent_flag", True):
        return {"intent_valid": False, "reason": signals.get("intent_reason", "SELL trigger explicitly invalidated")}

    position_size = signals.get("position_size", 0.0)
    if position_size <= 0.0:
        return {"intent_valid": False, "reason": "Non-positive position size for SELL operation"}

    # Market context check
    market = signals.get("market", {})
    liquidity = market.get("liquidity", signals.get("liquidity", 1.0)) if isinstance(market, dict) else 1.0
    if liquidity <= 0.01:
        return {"intent_valid": False, "reason": "Critical bid liquidity collapse detected; SELL intent invalid"}

    volatility = signals.get("volatility", 0.0)
    if volatility < 0.0:
        return {"intent_valid": False, "reason": "Invalid negative volatility value"}

    return {
        "intent_valid": True,
        "reason": "SELL intent validated with legitimate context and liquidity"
    }


def compute_sell_ceiling(trust_level, position_size):
    """
    Govern how much of a position can be sold based on trust level:
    - Level 0: up to 100%
    - Level 1: up to 60%
    - Level 2: up to 30%
    - Level 3: up to 10% (protective mode)

    Returns:
        float: Maximum allowed sell quantity
    """
    ceilings = {0: 1.0, 1: 0.6, 2: 0.3, 3: 0.1}
    cap_ratio = ceilings.get(trust_level, 0.1)
    return max(0.0, float(position_size) * cap_ratio)


def detect_sell_manipulation(market_data):
    """
    Detect manipulation patterns on the sell/bid side:
    - spoofed bids
    - collapsing bid-side liquidity
    - MEV patterns
    - abnormal spread widening

    Returns:
        float: Manipulation score 0.0 to 1.0
    """
    if not isinstance(market_data, dict):
        return 1.0  # Safe default if market data is corrupt or missing

    score = 0.0

    # Spoofed bids detection
    if market_data.get("spoofed_bids", False):
        score += 0.35

    # Collapsing bid liquidity detection
    bid_liquidity_drop = float(market_data.get("bid_liquidity_drop", 0.0))
    if bid_liquidity_drop > 0.0:
        score += min(0.35, bid_liquidity_drop * 0.5)

    # MEV activity detection
    if market_data.get("mev_detected", False) or market_data.get("mev_activity", False):
        score += 0.25

    # Spread widening detection
    spread_widening = float(market_data.get("spread_widening", 0.0))
    if spread_widening > 0.0:
        score += min(0.20, spread_widening * 0.4)

    return max(0.0, min(1.0, float(score)))


def grace_layer_sell_adjustment(volatility, sell_amount):
    """
    If volatility is elevated/temporary:
    - reduce SELL size / apply grace tolerance dampening

    Returns:
        float: Adjusted sell amount
    """
    sell_amt = max(0.0, float(sell_amount))
    vol = max(0.0, float(volatility))

    if vol <= 0.20:
        # Normal volatility - no grace reduction needed
        return sell_amt
    elif vol <= 0.50:
        # Moderate volatility - mild grace adjustment (e.g., 10-25% reduction)
        dampening_factor = 1.0 - 0.5 * (vol - 0.20)
        return max(0.0, sell_amt * dampening_factor)
    else:
        # High volatility - stronger grace reduction
        dampening_factor = max(0.20, 0.85 - 0.8 * (vol - 0.50))
        return max(0.0, sell_amt * dampening_factor)


def consensus_validation(feeds):
    """
    Cross-check multiple feeds to calculate consensus score 0.0–1.0.

    Returns:
        float: Consensus score 0.0 to 1.0
    """
    if not feeds or not isinstance(feeds, (list, tuple)):
        return 0.0

    valid_prices = []
    confidences = []

    for item in feeds:
        if isinstance(item, dict):
            price = item.get("price")
            conf = item.get("confidence", 1.0)
            if price is not None and isinstance(price, (int, float)) and price > 0:
                valid_prices.append(float(price))
                confidences.append(float(conf))
        elif isinstance(item, (int, float)) and item > 0:
            valid_prices.append(float(item))
            confidences.append(1.0)

    if not valid_prices:
        return 0.0

    if len(valid_prices) == 1:
        return max(0.0, min(1.0, confidences[0] * 0.70))

    mean_price = sum(valid_prices) / len(valid_prices)
    if mean_price == 0:
        return 0.0

    # Calculate variance / relative deviation
    variance = sum((p - mean_price) ** 2 for p in valid_prices) / len(valid_prices)
    std_dev = variance ** 0.5
    relative_std = std_dev / mean_price

    # High relative std dev indicates low consensus
    price_consensus = max(0.0, 1.0 - (relative_std * 5.0))
    avg_confidence = sum(confidences) / len(confidences)

    consensus_score = price_consensus * avg_confidence
    return max(0.0, min(1.0, float(consensus_score)))
