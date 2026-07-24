#ifndef AILLE_MEMBRANE_HPP
#define AILLE_MEMBRANE_HPP

#include <cstdint>
#include <cstddef>
#include <vector>
#include <mutex>

namespace AILLE {

// ============================================================================
// CONSTANTS & VERSION
// ============================================================================
constexpr const char* DEFORMABLE_MEMBRANE_V1 = "DEFORMABLE_MEMBRANE_V1";
constexpr const char* COMPUTE_ENVELOPE_V1    = "COMPUTE_ENVELOPE_V1";

// The 12 behavioral facets/strings as defined in UFO
enum class MembraneFacet : uint8_t {
    DEPTH = 0,
    PRECISION = 1,
    CONTEXT_SENSITIVITY = 2,
    INITIATIVE = 3,
    EXPLORATION = 4,
    CREATIVITY = 5,
    TONE = 6,
    TRANSPARENCY = 7,
    TECHNICAL_DETAIL = 8,
    STRUCTURAL_RIGOR = 9,
    EMOTIONAL_WARMTH = 10,
    CONCISENESS = 11
};

// ============================================================================
// STRUCTS (64-byte Cache-Aligned)
// ============================================================================

struct alignas(64) MembraneState {
    float activations[12];     // 48 bytes (activations of the 12 behavioral strings)
    float base_radius;         // 4 bytes
    float asymmetry_index;     // 4 bytes
    float average_tension;     // 4 bytes
    uint8_t reserved[4];       // 4 bytes padding -> 64 bytes total
};
static_assert(sizeof(MembraneState) == 64, "MembraneState must be exactly 64 bytes");

struct alignas(64) MembraneMetrics {
    double asymmetry_index;     // 8 bytes
    double max_curvature;       // 8 bytes
    double mean_curvature;      // 8 bytes
    double lyapunov_energy;     // 8 bytes
    double tension;             // 8 bytes
    double compute_envelope_state; // 8 bytes (clamp factor, e.g. E_clamp)
    uint8_t reserved[16];       // 16 bytes padding -> 64 bytes total
};
static_assert(sizeof(MembraneMetrics) == 64, "MembraneMetrics must be exactly 64 bytes");

struct alignas(64) ComputeEnvelopeState {
    double api_latency;         // 8 bytes
    double rest_ws_load;        // 8 bytes
    double model_eval_cost;     // 8 bytes
    double runtime_stress;      // 8 bytes
    double clamp_exposure;      // 8 bytes
    uint8_t reserved[24];       // 24 bytes padding -> 64 bytes total
};
static_assert(sizeof(ComputeEnvelopeState) == 64, "ComputeEnvelopeState must be exactly 64 bytes");

struct alignas(64) MembraneTraceStep {
    uint32_t timestep;          // 4 bytes
    float energy;               // 4 bytes
    float max_deviation;        // 4 bytes
    char transition_symbol[20]; // 20 bytes (Letter-Depth Encoding style state representation)
    char description[32];       // 32 bytes compact logging description -> 4 + 4 + 4 + 20 + 32 = 64 bytes total
};
static_assert(sizeof(MembraneTraceStep) == 64, "MembraneTraceStep must be exactly 64 bytes");

// ============================================================================
// LAYER 15 TRACE CONTAINER
// ============================================================================
struct MembraneTrace {
    static constexpr size_t MAX_STEPS = 64;
    MembraneTraceStep steps[MAX_STEPS];
    size_t step_count{0};
};

struct MembraneResult {
    MembraneState state;
    MembraneMetrics metrics;
    ComputeEnvelopeState envelope;
    MembraneTrace trace;
};

// ============================================================================
// LAYER 15 CORE RUNTIME INTERFACE
// ============================================================================
MembraneResult evaluate_membrane_governance(
    const MembraneState& current_state,
    const ComputeEnvelopeState& env_inputs,
    uint32_t timestep
);

// Thread-safe global access to the latest membrane state
void set_latest_membrane_result(const MembraneResult& res);
MembraneResult get_latest_membrane_result();

} // namespace AILLE

#endif // AILLE_MEMBRANE_HPP
