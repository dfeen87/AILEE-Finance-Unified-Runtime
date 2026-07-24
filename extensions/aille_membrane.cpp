#include "aille_membrane.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <mutex>

namespace AILLE {

// Thread-safe state storage
static MembraneResult s_latest_result{};
static std::mutex s_membrane_mutex{};

void set_latest_membrane_result(const MembraneResult& res) {
    std::lock_guard<std::mutex> lock(s_membrane_mutex);
    s_latest_result = res;
}

MembraneResult get_latest_membrane_result() {
    std::lock_guard<std::mutex> lock(s_membrane_mutex);
    return s_latest_result;
}

// Helper to get string angle on the unit circle
static double get_string_angle(uint8_t index) {
    // 2 * M_PI / 12 = M_PI / 6 approx 0.523598775598
    static constexpr double PI_6 = 0.5235987755982988;
    return static_cast<double>(index) * PI_6;
}

MembraneResult evaluate_membrane_governance(
    const MembraneState& current_state,
    const ComputeEnvelopeState& env_inputs,
    uint32_t timestep
) {
    MembraneResult result{};
    result.state = current_state;

    double base_r = (current_state.base_radius > 0.0f) ? static_cast<double>(current_state.base_radius) : 1.0;

    // 1. Calculate Membrane Geometry (polar radius, curvature, etc.)
    double radii[12];
    double max_r = 0.0;
    double min_r = 9999.0;

    for (uint8_t j = 0; j < 12; ++j) {
        double theta_j = get_string_angle(j);
        double r_j = base_r;
        for (uint8_t i = 0; i < 12; ++i) {
            double theta_i = get_string_angle(i);
            double diff = theta_j - theta_i;
            double phi_i = 0.5 * (1.0 + std::cos(diff));
            r_j += phi_i * static_cast<double>(current_state.activations[i]);
        }
        radii[j] = r_j;
        if (r_j > max_r) max_r = r_j;
        if (r_j < min_r) min_r = r_j;
    }
    (void)radii;

    double asymmetry = 0.0;
    if (max_r > 0.0) {
        asymmetry = 1.0 - (min_r / max_r);
    }

    // First (tangent) and second (curvature) derivatives at the 12 string angles
    double max_k = 0.0;
    double sum_k = 0.0;
    for (uint8_t j = 0; j < 12; ++j) {
        double theta_j = get_string_angle(j);
        double k_j = 0.0; // second derivative
        for (uint8_t i = 0; i < 12; ++i) {
            double theta_i = get_string_angle(i);
            double diff = theta_j - theta_i;
            // derivative of phi_i is -0.5 * sin(diff), 2nd derivative is -0.5 * cos(diff)
            k_j += -0.5 * std::cos(diff) * static_cast<double>(current_state.activations[i]);
        }
        double abs_k = std::abs(k_j);
        if (abs_k > max_k) max_k = abs_k;
        sum_k += abs_k;
    }
    double mean_k = sum_k / 12.0;

    // 2. Compute Tension and Directional Pressure Vector mapping
    double Tx = 0.0;
    double Ty = 0.0;
    for (uint8_t i = 0; i < 12; ++i) {
        double theta_i = get_string_angle(i);
        double act = static_cast<double>(current_state.activations[i]);
        Tx += act * std::cos(theta_i);
        Ty += act * std::sin(theta_i);
    }
    double tension = std::sqrt(Tx * Tx + Ty * Ty);

    // 3. Compute-Aware Governor (Bounded Compute Envelope)
    // S_compute = 0.3 * API_Latency + 0.3 * RestWS_Load + 0.2 * Model_Cost + 0.2 * Runtime_Stress
    double s_api = env_inputs.api_latency;
    double s_load = env_inputs.rest_ws_load;
    double s_model = env_inputs.model_eval_cost;
    double s_stress = env_inputs.runtime_stress;

    double s_compute = 0.3 * s_api + 0.3 * s_load + 0.2 * s_model + 0.2 * s_stress;
    double clamp_factor = 1.0 - s_compute;
    if (clamp_factor < 0.1) clamp_factor = 0.1;
    if (clamp_factor > 1.0) clamp_factor = 1.0;

    // 4. Lyapunov Energy Function V
    double sum_act_sq = 0.0;
    for (uint8_t i = 0; i < 12; ++i) {
        double act = static_cast<double>(current_state.activations[i]);
        sum_act_sq += act * act;
    }
    double lyapunov_energy = sum_act_sq + (0.5 * tension * tension) + (0.2 * s_compute);

    // 5. Letter-Depth Encoding (L.D.E.) representation of active quadrants
    // Quadrants definition:
    // Analytical: DEPTH(0), PRECISION(1), TECHNICAL_DETAIL(8), STRUCTURAL_RIGOR(9)
    // Contextual: CONTEXT_SENSITIVITY(2), TRANSPARENCY(7)
    // Generative: INITIATIVE(3), EXPLORATION(4), CREATIVITY(5)
    // Interpersonal: TONE(6), EMOTIONAL_WARMTH(10), CONCISENESS(11)
    double q_analytical = current_state.activations[0] + current_state.activations[1] + current_state.activations[8] + current_state.activations[9];
    double q_contextual = current_state.activations[2] + current_state.activations[7];
    double q_generative = current_state.activations[3] + current_state.activations[4] + current_state.activations[5];
    double q_interpersonal = current_state.activations[6] + current_state.activations[10] + current_state.activations[11];

    const char* dominant_quadrant = "LDE:ANALYTICAL";
    double max_q = q_analytical;
    if (q_contextual > max_q) {
        max_q = q_contextual;
        dominant_quadrant = "LDE:CONTEXTUAL";
    }
    if (q_generative > max_q) {
        max_q = q_generative;
        dominant_quadrant = "LDE:GENERATIVE";
    }
    if (q_interpersonal > max_q) {
        max_q = q_interpersonal;
        dominant_quadrant = "LDE:INTERPERSONAL";
    }

    // Populate Results
    result.metrics.asymmetry_index = asymmetry;
    result.metrics.max_curvature = max_k;
    result.metrics.mean_curvature = mean_k;
    result.metrics.lyapunov_energy = lyapunov_energy;
    result.metrics.tension = tension;
    result.metrics.compute_envelope_state = clamp_factor;
    std::memset(result.metrics.reserved, 0, sizeof(result.metrics.reserved));

    result.envelope.api_latency = s_api;
    result.envelope.rest_ws_load = s_load;
    result.envelope.model_eval_cost = s_model;
    result.envelope.runtime_stress = s_stress;
    result.envelope.clamp_exposure = clamp_factor;
    std::memset(result.envelope.reserved, 0, sizeof(result.envelope.reserved));

    result.state.asymmetry_index = static_cast<float>(asymmetry);
    result.state.average_tension = static_cast<float>(tension);

    // Log Trace Step
    MembraneTraceStep step{};
    step.timestep = timestep;
    step.energy = static_cast<float>(lyapunov_energy);
    step.max_deviation = static_cast<float>(max_r - base_r);
    std::snprintf(step.transition_symbol, sizeof(step.transition_symbol), "%s", dominant_quadrant);

    std::snprintf(step.description, sizeof(step.description), "V=%.2f, Tension=%.2f", lyapunov_energy, tension);

    result.trace.steps[0] = step;
    result.trace.step_count = 1;

    // Save to the global storage for telemetry/APIs
    set_latest_membrane_result(result);

    return result;
}

} // namespace AILLE
