# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Unit tests for Layer 15 — Deterministic Deformable Membrane & Compute-Aware Governor."""

import pytest
import math
from core.finance_kernel.membrane import (
    MembraneState,
    MembraneMetrics,
    ComputeEnvelopeState,
    MembraneTraceStep,
    evaluate_membrane_governance,
    get_string_angle
)

def test_membrane_neutral_state():
    # When activations are 0, radius should equal base_radius (e.g. 1.0) and asymmetry should be 0.
    state = MembraneState(base_radius=1.0)
    env = ComputeEnvelopeState()

    result = evaluate_membrane_governance(state, env, timestep=1)

    assert result.metrics.asymmetry_index == pytest.approx(0.0)
    assert result.metrics.tension == pytest.approx(0.0)
    assert result.metrics.lyapunov_energy == pytest.approx(0.0)
    assert result.metrics.max_curvature == pytest.approx(0.0)
    assert result.metrics.mean_curvature == pytest.approx(0.0)
    assert result.metrics.compute_envelope_state == pytest.approx(1.0) # no load

    assert len(result.trace_steps) == 1
    assert result.trace_steps[0].timestep == 1
    assert result.trace_steps[0].energy == pytest.approx(0.0)
    assert result.trace_steps[0].transition_symbol == "LDE:ANALYTICAL" # analytical by default since tie-breaker is analytical

def test_membrane_directional_pressure():
    # Activate DEPTH (index 0) to 0.5 and PRECISION (index 1) to 0.5
    state = MembraneState(base_radius=1.0)
    state.activations[0] = 0.5
    state.activations[1] = 0.5

    env = ComputeEnvelopeState(api_latency=0.1, rest_ws_load=0.2, model_eval_cost=0.1, runtime_stress=0.0)

    result = evaluate_membrane_governance(state, env, timestep=2)

    # Tension should be > 0.0
    assert result.metrics.tension > 0.0

    # Check asymmetry index is positive
    assert result.metrics.asymmetry_index > 0.0

    # Compute stress = 0.3 * 0.1 + 0.3 * 0.2 + 0.2 * 0.1 + 0.2 * 0.0 = 0.03 + 0.06 + 0.02 = 0.11
    # Clamp factor = 1.0 - 0.11 = 0.89
    assert result.metrics.compute_envelope_state == pytest.approx(0.89)
    assert result.envelope.clamp_exposure == pytest.approx(0.89)

    # L.D.E. dominant quadrant should be analytical (DEPTH & PRECISION belong to Analytical)
    assert result.trace_steps[0].transition_symbol == "LDE:ANALYTICAL"

def test_membrane_determinism_and_boundaries():
    state1 = MembraneState(base_radius=1.0)
    state1.activations[3] = 0.8 # INITIATIVE (Generative)

    env1 = ComputeEnvelopeState(api_latency=0.5, rest_ws_load=0.5, model_eval_cost=0.5, runtime_stress=0.5)

    result1 = evaluate_membrane_governance(state1, env1, timestep=10)
    result2 = evaluate_membrane_governance(state1, env1, timestep=10)

    # Verify exact determinism
    assert result1.metrics.asymmetry_index == result2.metrics.asymmetry_index
    assert result1.metrics.tension == result2.metrics.tension
    assert result1.metrics.lyapunov_energy == result2.metrics.lyapunov_energy

    # High load (0.3 * 0.5 * 2 + 0.2 * 0.5 * 2 = 0.3 + 0.2 = 0.5)
    # Clamp factor = 1.0 - 0.5 = 0.5
    assert result1.metrics.compute_envelope_state == pytest.approx(0.5)

    # Dominant quadrant should be Generative
    assert result1.trace_steps[0].transition_symbol == "LDE:GENERATIVE"
