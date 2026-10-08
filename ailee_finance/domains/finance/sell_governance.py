"""
SELL Governance Module - Version 5.0.0
Provides SELL-side trust governance, manipulation detection, dynamic ceilings,
volatility grace adjustments, and consensus feed validation.
"""

import math


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


def compute_sell_ceiling(trust_level, position_size, bullish_active=False, bullish_sell_ceiling_factor=0.8):
    """
    Govern how much of a position can be sold based on trust level:
    - Level 0: up to 100%
    - Level 1: up to 60%
    - Level 2: up to 30%
    - Level 3: up to 10% (protective mode)

    When bullish mode is active, reduces sell ceiling by bullish_sell_ceiling_factor (default 0.8),
    except for Level 3 (protective mode) which overrides bullish bias and restores full safety behavior.

    Returns:
        float: Maximum allowed sell quantity
    """
    ceilings = {0: 1.0, 1: 0.6, 2: 0.3, 3: 0.1}
    cap_ratio = ceilings.get(trust_level, 0.1)
    allowed = max(0.0, float(position_size) * cap_ratio)

    if bullish_active and trust_level != 3:
        allowed *= float(bullish_sell_ceiling_factor)

    return max(0.0, allowed)


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
    score = _validated_sell_manipulation(market_data)
    return 1.0 if score is None else score


def _validated_sell_manipulation(market_data):
    """Keep invalid evidence distinct from a legitimate maximum risk score."""
    if not isinstance(market_data, dict):
        return None

    try:
        bid_liquidity_drop = float(market_data.get("bid_liquidity_drop", 0.0))
        spread_widening = float(market_data.get("spread_widening", 0.0))
    except (TypeError, ValueError, OverflowError):
        return None
    if not all(math.isfinite(value) for value in (bid_liquidity_drop, spread_widening)):
        return None

    score = 0.0

    # Spoofed bids detection
    if market_data.get("spoofed_bids", False):
        score += 0.35

    # Collapsing bid liquidity detection
    if bid_liquidity_drop > 0.0:
        score += min(0.35, bid_liquidity_drop * 0.5)

    # MEV activity detection
    if market_data.get("mev_detected", False) or market_data.get("mev_activity", False):
        score += 0.25

    # Spread widening detection
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

    if not all(math.isfinite(value) for value in valid_prices + confidences):
        return 0.0

    if len(valid_prices) == 1:
        return max(0.0, min(1.0, confidences[0] * 0.70))

    # Relative deviation is invariant to scaling. Normalize before computing
    # means and squares so finite prices do not overflow or lose variance to
    # underflow, including subnormal positive prices.
    price_scale = max(valid_prices)
    scaled_prices = [price / price_scale for price in valid_prices]
    mean_price = math.fsum(scaled_prices) / len(scaled_prices)
    variance = math.fsum((price - mean_price) ** 2 for price in scaled_prices) / len(scaled_prices)
    std_dev = math.sqrt(variance)
    relative_std = std_dev / mean_price

    # High relative std dev indicates low consensus
    price_consensus = max(0.0, 1.0 - (relative_std * 5.0))
    try:
        avg_confidence = math.fsum(confidences) / len(confidences)
    except OverflowError:
        # Divide first only when required; doing so for subnormal confidence
        # would round each term to zero before the sum. Keep signed cancellation.
        try:
            avg_confidence = math.fsum(confidence / len(confidences) for confidence in confidences)
        except OverflowError:
            # At the largest finite boundary, rounded weighted terms can sum
            # just beyond the representation even though their mean is bounded.
            confidence_scale = max(abs(confidence) for confidence in confidences)
            avg_confidence = (math.fsum(confidence / confidence_scale for confidence in confidences)
                              / len(confidences) * confidence_scale)

    consensus_score = price_consensus * avg_confidence
    return max(0.0, min(1.0, float(consensus_score)))
