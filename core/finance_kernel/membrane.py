# Copyright (c) 2026 Don Michael Feeney Jr
# License: MIT (see LICENSE)
"""Layer 15 — Deterministic Deformable Membrane & Compute-Aware Governor (UFO Integration)."""

import math

# ============================================================================
# CONSTANTS & VERSION
# ============================================================================
DEFORMABLE_MEMBRANE_V1 = "DEFORMABLE_MEMBRANE_V1"
COMPUTE_ENVELOPE_V1    = "COMPUTE_ENVELOPE_V1"

class MembraneFacet:
    DEPTH = 0
    PRECISION = 1
    CONTEXT_SENSITIVITY = 2
    INITIATIVE = 3
    EXPLORATION = 4
    CREATIVITY = 5
    TONE = 6
    TRANSPARENCY = 7
    TECHNICAL_DETAIL = 8
    STRUCTURAL_RIGOR = 9
    EMOTIONAL_WARMTH = 10
    CONCISENESS = 11

# ============================================================================
# DATA CLASSES / STRUCTS
# ============================================================================

class MembraneState:
    def __init__(self, activations=None, base_radius: float = 1.0):
        self.activations = [0.0] * 12 if activations is None else list(activations)
        self.base_radius = float(base_radius)
        self.asymmetry_index = 0.0
        self.average_tension = 0.0

class MembraneMetrics:
    def __init__(self):
        self.asymmetry_index = 0.0
        self.max_curvature = 0.0
        self.mean_curvature = 0.0
        self.lyapunov_energy = 0.0
        self.tension = 0.0
        self.compute_envelope_state = 1.0

class ComputeEnvelopeState:
    def __init__(self, api_latency: float = 0.0, rest_ws_load: float = 0.0, model_eval_cost: float = 0.0, runtime_stress: float = 0.0):
        self.api_latency = float(api_latency)
        self.rest_ws_load = float(rest_ws_load)
        self.model_eval_cost = float(model_eval_cost)
        self.runtime_stress = float(runtime_stress)
        self.clamp_exposure = 1.0

class MembraneTraceStep:
    def __init__(self, timestep: int = 0, energy: float = 0.0, max_deviation: float = 0.0, transition_symbol: str = "", description: str = ""):
        self.timestep = int(timestep)
        self.energy = float(energy)
        self.max_deviation = float(max_deviation)
        self.transition_symbol = str(transition_symbol)
        self.description = str(description)

class MembraneResult:
    def __init__(self):
        self.state = MembraneState()
        self.metrics = MembraneMetrics()
        self.envelope = ComputeEnvelopeState()
        self.trace_steps = []

# ============================================================================
# LAYER 15 CORE RUNTIME INTERFACE
# ============================================================================

def get_string_angle(index: int) -> float:
    return float(index) * (math.pi / 6.0)

def evaluate_membrane_governance(current_state: MembraneState, env_inputs: ComputeEnvelopeState, timestep: int) -> MembraneResult:
    """Pure functional Membrane Evaluation matching C++ exactly."""
    result = MembraneResult()
    result.state.activations = list(current_state.activations)
    result.state.base_radius = current_state.base_radius

    base_r = float(current_state.base_radius) if current_state.base_radius > 0.0 else 1.0

    # 1. Calculate Membrane Geometry (polar radius, curvature, etc.)
    radii = [0.0] * 12
    max_r = 0.0
    min_r = 9999.0

    for j in range(12):
        theta_j = get_string_angle(j)
        r_j = base_r
        for i in range(12):
            theta_i = get_string_angle(i)
            diff = theta_j - theta_i
            phi_i = 0.5 * (1.0 + math.cos(diff))
            r_j += phi_i * float(current_state.activations[i])
        radii[j] = r_j
        if r_j > max_r:
            max_r = r_j
        if r_j < min_r:
            min_r = r_j

    asymmetry = 0.0
    if max_r > 0.0:
        asymmetry = 1.0 - (min_r / max_r)

    # Curvature (second derivative)
    max_k = 0.0
    sum_k = 0.0
    for j in range(12):
        theta_j = get_string_angle(j)
        k_j = 0.0
        for i in range(12):
            theta_i = get_string_angle(i)
            diff = theta_j - theta_i
            k_j += -0.5 * math.cos(diff) * float(current_state.activations[i])
        abs_k = abs(k_j)
        if abs_k > max_k:
            max_k = abs_k
        sum_k += abs_k
    mean_k = sum_k / 12.0

    # 2. Compute Tension and Directional Pressure Vector mapping
    Tx = 0.0
    Ty = 0.0
    for i in range(12):
        theta_i = get_string_angle(i)
        act = float(current_state.activations[i])
        Tx += act * math.cos(theta_i)
        Ty += act * math.sin(theta_i)
    tension = math.sqrt(Tx * Tx + Ty * Ty)

    # 3. Compute-Aware Governor
    s_api = float(env_inputs.api_latency)
    s_load = float(env_inputs.rest_ws_load)
    s_model = float(env_inputs.model_eval_cost)
    s_stress = float(env_inputs.runtime_stress)

    s_compute = 0.3 * s_api + 0.3 * s_load + 0.2 * s_model + 0.2 * s_stress
    clamp_factor = 1.0 - s_compute
    if clamp_factor < 0.1:
        clamp_factor = 0.1
    if clamp_factor > 1.0:
        clamp_factor = 1.0

    # 4. Lyapunov Energy Function V
    sum_act_sq = sum(float(a) ** 2 for a in current_state.activations)
    lyapunov_energy = sum_act_sq + (0.5 * tension * tension) + (0.2 * s_compute)

    # 5. Letter-Depth Encoding
    q_analytical = current_state.activations[0] + current_state.activations[1] + current_state.activations[8] + current_state.activations[9]
    q_contextual = current_state.activations[2] + current_state.activations[7]
    q_generative = current_state.activations[3] + current_state.activations[4] + current_state.activations[5]
    q_interpersonal = current_state.activations[6] + current_state.activations[10] + current_state.activations[11]

    dominant_quadrant = "LDE:ANALYTICAL"
    max_q = q_analytical
    if q_contextual > max_q:
        max_q = q_contextual
        dominant_quadrant = "LDE:CONTEXTUAL"
    if q_generative > max_q:
        max_q = q_generative
        dominant_quadrant = "LDE:GENERATIVE"
    if q_interpersonal > max_q:
        max_q = q_interpersonal
        dominant_quadrant = "LDE:INTERPERSONAL"

    # Populate results
    result.metrics.asymmetry_index = asymmetry
    result.metrics.max_curvature = max_k
    result.metrics.mean_curvature = mean_k
    result.metrics.lyapunov_energy = lyapunov_energy
    result.metrics.tension = tension
    result.metrics.compute_envelope_state = clamp_factor

    result.envelope.api_latency = s_api
    result.envelope.rest_ws_load = s_load
    result.envelope.model_eval_cost = s_model
    result.envelope.runtime_stress = s_stress
    result.envelope.clamp_exposure = clamp_factor

    result.state.asymmetry_index = asymmetry
    result.state.average_tension = tension

    # Trace step
    step = MembraneTraceStep(
        timestep=timestep,
        energy=lyapunov_energy,
        max_deviation=(max_r - base_r),
        transition_symbol=dominant_quadrant,
        description=f"V={lyapunov_energy:.2f}, Tension={tension:.2f}"
    )
    result.trace_steps.append(step)

    return result
