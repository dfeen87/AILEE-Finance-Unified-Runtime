"""Finite evidence and staged updates for the pure Python governance layers."""
import copy
import functools
import math
from numbers import Number


def finite_evidence(value, path="evidence", seen=None):
    seen = set() if seen is None else seen
    if isinstance(value, Number) and not isinstance(value, (int, float, bool)):
        raise ValueError(f"{path} uses an unsupported numeric encoding")
    if isinstance(value, (float, int)) and not isinstance(value, bool):
        try:
            valid = math.isfinite(value)
        except OverflowError:
            valid = False
        if not valid:
            raise ValueError(f"{path} must be finite")
    elif isinstance(value, (list, tuple, dict)) or hasattr(value, "__dict__"):
        if id(value) in seen:
            return
        seen.add(id(value))
        items = value.items() if isinstance(value, dict) else (vars(value).items() if hasattr(value, "__dict__") else enumerate(value))
        for name, child in items:
            finite_evidence(child, f"{path}.{name}", seen)


def bounded(value, name, minimum=0.0, maximum=None):
    if isinstance(value, (bool, str, bytes)) or not isinstance(value, (int, float)):
        raise ValueError(f"{name} must be numeric")
    finite_evidence(value, name)
    if value < minimum or (maximum is not None and value > maximum):
        raise ValueError(f"{name} outside its safety domain")


def _commit(original, staged, seen):
    pair = (id(original), id(staged))
    if pair in seen:
        return
    seen.add(pair)
    if type(original) is not type(staged):
        return
    if hasattr(original, "__dict__"):
        _commit(vars(original), vars(staged), seen)
    elif isinstance(original, dict):
        for key in list(original):
            if key not in staged:
                del original[key]
        for key, value in staged.items():
            if key in original and type(original[key]) is type(value) and (hasattr(value, "__dict__") or isinstance(value, (list, dict))):
                _commit(original[key], value, seen)
            else:
                original[key] = value
    elif isinstance(original, list):
        for index in range(min(len(original), len(staged))):
            value = staged[index]
            if type(original[index]) is type(value) and (hasattr(value, "__dict__") or isinstance(value, (list, dict))):
                _commit(original[index], value, seen)
            else:
                original[index] = value
        original[len(staged):] = []
        original.extend(staged[len(original):])


def finite_governance(*, mutating=False):
    """Reject bad evidence and overflow before publishing results or updates.

    Mutating reference algorithms run against staged copies, then commit into
    their plain data containers while retaining existing allocation identities.
    This is an in-process numerical boundary, not a storage transaction.
    """
    def decorate(function):
        @functools.wraps(function)
        def guarded(*args, **kwargs):
            finite_evidence((args, kwargs))
            if not mutating:
                result = function(*args, **kwargs)
                finite_evidence(result, "result")
                return result
            staged_args, staged_kwargs = copy.deepcopy((args, kwargs))
            result = function(*staged_args, **staged_kwargs)
            finite_evidence((staged_args, staged_kwargs, result), "result")
            seen = set()
            for original, staged in zip(args, staged_args):
                _commit(original, staged, seen)
            for key in kwargs:
                _commit(kwargs[key], staged_kwargs[key], seen)
            return result
        return guarded
    return decorate
