"""Dependency-free model trust pipeline shared by the SDK and simulations.

Malformed evidence raises before accepted history changes. Ordinary trust
rejection selects a bounded fallback without adding it to accepted history.
"""

import math
import statistics
from dataclasses import dataclass, replace
from numbers import Real
from typing import List, Optional, Tuple


def _number(value, name):
    if isinstance(value, bool) or not isinstance(value, Real):
        raise ValueError(f"{name} must be a finite real number")
    try:
        number = float(value)
    except (TypeError, ValueError, OverflowError) as error:
        raise ValueError(f"{name} must be a finite real number") from error
    if not math.isfinite(number):
        raise ValueError(f"{name} must be a finite real number")
    return number


@dataclass
class ModelSignal:
    value: float
    confidence: float
    model_id: int


@dataclass
class Decision:
    final_value: float
    status: str
    confidence: float
    models_agreed: int
    fallback_used: bool
    reasoning: str


@dataclass
class AILLEConfig:
    min_confidence_threshold: float = 0.35
    grace_confidence_threshold: float = 0.25
    min_models_required: int = 2
    sign_agreement_threshold: float = 0.66
    fallback_window_size: int = 50
    fallback_position_scale: float = 0.1
    max_model_count: int = 10
    enable_dynamic_fallback: bool = False
    fallback_alpha: float = 0.5
    fallback_beta: float = 0.1
    config_version: str = "4.1.0"
    max_position_abs: float = 1.0

    def __post_init__(self):
        for name in ("min_confidence_threshold", "sign_agreement_threshold", "max_position_abs"):
            if not 0 < _number(getattr(self, name), name) <= 1:
                raise ValueError(f"{name} must be in (0, 1]")
        if not 0 <= _number(self.grace_confidence_threshold, "grace_confidence_threshold") <= self.min_confidence_threshold:
            raise ValueError("grace_confidence_threshold must be in [0, min_confidence_threshold]")
        for name in ("fallback_window_size", "max_model_count", "min_models_required"):
            value = getattr(self, name)
            if type(value) is not int or value < 1:
                raise ValueError(f"{name} must be a positive integer")
        if self.min_models_required > self.max_model_count:
            raise ValueError("min_models_required must be reachable within max_model_count")
        if not 0 <= _number(self.fallback_position_scale, "fallback_position_scale") <= 1:
            raise ValueError("fallback_position_scale must be in [0, 1]")
        for name in ("fallback_alpha", "fallback_beta"):
            if _number(getattr(self, name), name) < 0:
                raise ValueError(f"{name} must be non-negative")
        if type(self.enable_dynamic_fallback) is not bool:
            raise ValueError("enable_dynamic_fallback must be a boolean")


def _config(config):
    if not isinstance(config, AILLEConfig):
        raise ValueError("config must be an AILLEConfig")
    # Copy and revalidate mutable policy before processing a call.
    return replace(config)


def snapshot_signals(signals):
    """Copy evidence without coercing booleans, strings or model identities."""
    if not isinstance(signals, (list, tuple)):
        raise ValueError("signals must be a list or tuple")
    snapshot = []
    identities = set()
    for index, signal in enumerate(signals):
        if isinstance(signal, dict):
            if not {"value", "confidence", "model_id"} <= signal.keys():
                raise ValueError(f"signals[{index}] requires value, confidence and model_id")
            value, confidence, identity = signal["value"], signal["confidence"], signal["model_id"]
        elif isinstance(signal, ModelSignal):
            value, confidence, identity = signal.value, signal.confidence, signal.model_id
        else:
            raise ValueError(f"signals[{index}] must be a ModelSignal or dictionary")
        value = _number(value, f"signals[{index}].value")
        confidence = _number(confidence, f"signals[{index}].confidence")
        if not 0 <= confidence <= 1:
            raise ValueError(f"signals[{index}].confidence must be in [0, 1]")
        if type(identity) is not int or identity in identities:
            raise ValueError(f"signals[{index}].model_id must be an independent integer identity")
        identities.add(identity)
        snapshot.append(ModelSignal(value, confidence, identity))
    return snapshot


def _mean(values):
    # Preserve ordinary arithmetic and use exact stdlib accumulation only when
    # a representable finite mean has an overflowing intermediate sum.
    result = sum(values) / len(values)
    return result if math.isfinite(result) else float(statistics.mean(values))


def smooth_position(signal: float, scale: float = 100.0) -> float:
    signal, scale = _number(signal, "signal"), _number(scale, "scale")
    if scale < 0:
        raise ValueError("scale must be non-negative")
    # tanh of a finite overflowing positive/negative product has its exact
    # limiting value; no infinite evidence is retained in the result.
    return math.tanh(signal * scale)


def _safety(signals, config):
    valid = []
    for signal in signals[:config.max_model_count]:
        if signal.confidence >= config.min_confidence_threshold:
            valid.append(signal)
        elif signal.confidence >= config.grace_confidence_threshold:
            valid.append(ModelSignal(signal.value, signal.confidence * 0.8, signal.model_id))
    return valid


def apply_safety_layer(signals: List[ModelSignal], config: AILLEConfig) -> List[ModelSignal]:
    return _safety(snapshot_signals(signals), _config(config))


def _consensus(valid, config):
    if len(valid) < config.min_models_required:
        return None
    values = [signal.value for signal in valid]
    ordered = sorted(values)
    midpoint = len(ordered) // 2
    if len(ordered) % 2:
        median = ordered[midpoint]
    else:
        pair = ordered[midpoint - 1:midpoint + 1]
        median = _mean(pair)
    median_sign = 1.0 if median >= 0 else -1.0
    agree_values = [value for value in values if (1.0 if value >= 0 else -1.0) == median_sign]
    agreed = len(agree_values)
    if agreed >= config.min_models_required and agreed / len(values) >= config.sign_agreement_threshold:
        return _mean(agree_values), agreed
    return None


def check_consensus(valid: List[ModelSignal], config: AILLEConfig) -> Optional[Tuple[float, int]]:
    config = _config(config)
    return _consensus(_safety(snapshot_signals(valid), config), config)


class AILLEEngine:
    def __init__(self, config: Optional[AILLEConfig] = None) -> None:
        self.config = AILLEConfig() if config is None else config
        _config(self.config)
        self.fallback_buffer: List[float] = []
        self.confidence_buffer: List[float] = []

    def _history(self, config):
        if len(self.fallback_buffer) != len(self.confidence_buffer):
            raise ValueError("accepted value/confidence histories are inconsistent")
        values = [_number(value, "fallback history") for value in self.fallback_buffer]
        confidences = [_number(value, "confidence history") for value in self.confidence_buffer]
        if any(abs(value) > 1 for value in values) or any(not 0 <= value <= 1 for value in confidences):
            raise ValueError("accepted history exceeds its numeric domain")
        return values[-config.fallback_window_size:], confidences[-config.fallback_window_size:]

    def _calculate_fallback_value(self) -> float:
        values, _ = self._history(_config(self.config))
        return _mean(values) if values else 0.0

    def _get_fallback_value(self, current_confidence: float = 0.0) -> float:
        config = _config(self.config)
        values, confidences = self._history(config)
        return self._fallback(config, values, confidences)

    @staticmethod
    def _fallback(config, values, confidences):
        if not values:
            return 0.0
        sign = 1.0 if _mean(values) >= 0 else -1.0
        scale = config.fallback_position_scale
        if config.enable_dynamic_fallback:
            candidate = config.fallback_alpha * _mean(confidences) + config.fallback_beta
            scale = max(0.1, min(0.5, candidate))
        return sign * min(scale, config.max_position_abs)

    def _update_fallback_buffer(self, value: float, confidence: float) -> None:
        config = _config(self.config)
        values, confidences = self._history(config)
        value, confidence = _number(value, "final_value"), _number(confidence, "confidence")
        if abs(value) > config.max_position_abs or not 0 <= confidence <= 1:
            raise ValueError("decision is outside its finite numeric envelope")
        self.fallback_buffer = (values + [value])[-config.fallback_window_size:]
        self.confidence_buffer = (confidences + [confidence])[-config.fallback_window_size:]

    def make_decision(self, model_signals: List[ModelSignal]) -> Decision:
        config = _config(self.config)
        scoped = snapshot_signals(model_signals)[:config.max_model_count]
        values, confidences = self._history(config)
        if not scoped:
            return Decision(0.0, "ERROR_NO_MODELS", 0.0, 0, False, "No model inputs available")
        valid = _safety(scoped, config)
        if not valid:
            return Decision(self._fallback(config, values, confidences), "REJECTED_LOW_CONFIDENCE", 0.1, 0, True,
                            "All models failed confidence threshold – fallback activated")
        consensus = _consensus(valid, config)
        if consensus is None:
            return Decision(self._fallback(config, values, confidences), "REJECTED_NO_CONSENSUS", 0.2, 0, True,
                            "Models failed to reach consensus – fallback activated")
        consensus_value, models_agreed = consensus
        final_value = max(-config.max_position_abs, min(config.max_position_abs, smooth_position(consensus_value)))
        avg_confidence = _mean([signal.confidence for signal in valid])
        decision = Decision(final_value, "DECISION_VALID", avg_confidence, models_agreed, False,
                            f"Consensus achieved with {models_agreed} models")
        if not math.isfinite(decision.final_value) or not math.isfinite(decision.confidence):
            raise ValueError("decision must contain finite numeric evidence")
        # Commit only after every input, intermediate and completed result has
        # been validated. The selected call snapshot governs both buffers.
        self.fallback_buffer = (values + [final_value])[-config.fallback_window_size:]
        self.confidence_buffer = (confidences + [avg_confidence])[-config.fallback_window_size:]
        return decision
