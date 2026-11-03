"""Shared validation at market-advisory trust boundaries.

Defaults apply to absent fields only. Supplied malformed evidence is rejected
before arithmetic, clamping or state mutation can manufacture healthy data.
"""
import math


def finite(value, name, minimum=None, maximum=None):
    try:
        if isinstance(value, (bool, str, bytes, bytearray)):
            raise ValueError
        number = float(value)
    except (ValueError, TypeError, OverflowError) as error:
        raise ValueError(f'{name} must be a finite number') from error
    if not math.isfinite(number):
        raise ValueError(f'{name} must be a finite number')
    if minimum is not None and number < minimum or maximum is not None and number > maximum:
        raise ValueError(f'{name} is outside its permitted domain')
    return number


def integer(value, name, minimum=0, maximum=2**64-1):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f'{name} must be an integer in [{minimum}, {maximum}]')
    return value


def boolean(value, name):
    if type(value) is not bool:
        raise ValueError(f'{name} must be a boolean')
    return value


def mapping(data):
    if not isinstance(data, dict):
        raise ValueError('advisory input must be a dictionary')
    return data


def finite_output(result):
    """Reject non-representable derived evidence before returning/committing."""
    if isinstance(result, dict):
        for value in result.values():
            finite_output(value)
    elif isinstance(result, (list, tuple)):
        for value in result:
            finite_output(value)
    elif isinstance(result, float) and not math.isfinite(result):
        raise ValueError('advisory arithmetic produced non-finite evidence')
    return result
