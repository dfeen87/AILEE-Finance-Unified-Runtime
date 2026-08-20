/*
 * AILLE Framework - Deterministic Math Extension
 * AI-Load Integrity and Layered Evaluation
 *
 * Advisory-only deterministic math functions for risk logic smoothing.
 *
 * Copyright (c) Don Michael Feeney Jr.
 * Licensed under the MIT License.
 */

#ifndef AILLE_MATH_HPP
#define AILLE_MATH_HPP

#include <cstdint>
#include <cstddef>
#include "../aille.hpp"

namespace AILLE {
namespace Math {

// Advisory-only constexpr deterministic math
// All outputs are completely independent of heap or undefined state.

constexpr float GOLDEN_RATIO = 1.618033988749895f;

[[nodiscard]] constexpr float golden_ratio_weighted_average(const float* values, const float* confidences, size_t count) noexcept {
    if (count == 0 || values == nullptr || confidences == nullptr) {
        return 0.0f;
    }

    float weighted_sum = 0.0f;
    float weight_total = 0.0f;
    float current_weight = 1.0f;

    for (size_t i = 0; i < count; ++i) {
        weighted_sum += values[i] * confidences[i] * current_weight;
        weight_total += confidences[i] * current_weight;
        current_weight /= GOLDEN_RATIO;
    }

    if (weight_total == 0.0f) {
        return 0.0f;
    }

    return weighted_sum / weight_total;
}

namespace Internal {
    constexpr size_t MAX_FIB_TABLE_SIZE = 64; // Max capped at 64 as per rules

    constexpr std::array<float, MAX_FIB_TABLE_SIZE> precompute_fibonacci() {
        std::array<float, MAX_FIB_TABLE_SIZE> table{};
        table[0] = 1.0f;
        table[1] = 1.0f;
        for (size_t i = 2; i < MAX_FIB_TABLE_SIZE; ++i) {
            table[i] = table[i - 1] + table[i - 2];
        }
        return table;
    }

    constexpr auto FIB_TABLE = precompute_fibonacci();
} // namespace Internal

[[nodiscard]] constexpr float fibonacci_weighted_average(const float* values, const float* confidences, size_t count) noexcept {
    if (count == 0 || values == nullptr || confidences == nullptr) {
        return 0.0f;
    }

    size_t limit = count < Internal::MAX_FIB_TABLE_SIZE ? count : Internal::MAX_FIB_TABLE_SIZE;

    float weighted_sum = 0.0f;
    float weight_total = 0.0f;

    for (size_t i = 0; i < limit; ++i) {
        // Reverse fibonacci weights (most recent/confident has highest weight)
        float fib_weight = Internal::FIB_TABLE[limit - 1 - i];
        weighted_sum += values[i] * confidences[i] * fib_weight;
        weight_total += confidences[i] * fib_weight;
    }

    if (weight_total == 0.0f) {
        return 0.0f;
    }

    return weighted_sum / weight_total;
}

// ============================================================================
// HIGH FREQUENCY TRADING (HFT) AILEE MATH IMPULSE FORMULA
// Δv = Isp * η * e^(-α * v0^2) * ∫0^tf [P_input(t) * e^(-α * w(t)^2) * e^(2 * α * v0) * v(t)] / M(t) dt
// ============================================================================

struct HFTSampleTick {
    float p_input{0.0f};  ///< Price action signal power/intensity P_input(t)
    float w{0.0f};        ///< Resistance / risk factor w(t)
    float v{0.0f};        ///< Volume velocity / flow rate v(t)
    float M{1.0f};        ///< Dynamic liquidity mass / inertia M(t) (floored at >= 1e-6)
    float dt{0.001f};     ///< Micro-tick interval duration dt in seconds (default: 1 ms)
};

/**
 * Calculates high-frequency impulse velocity Δv across micro-tick price action & volume stream.
 * Allocator-free and branch-minimal for execution speed through the AILEE Governance pipeline.
 */
[[nodiscard]] inline float calculate_hft_delta_v(
    float isp,
    float efficiency,
    float alpha,
    float v0,
    const HFTSampleTick* ticks,
    size_t count,
    float min_mass_floor = 1e-6f
) noexcept {
    if (count == 0 || ticks == nullptr || efficiency <= 0.0f) {
        return 0.0f;
    }

    float integral_sum = 0.0f;
    float exp_2_alpha_v0 = std::exp(2.0f * alpha * v0);
    float mass_floor = (min_mass_floor > 1e-9f) ? min_mass_floor : 1e-6f;

    for (size_t i = 0; i < count; ++i) {
        float m_safe = (ticks[i].M > mass_floor) ? ticks[i].M : mass_floor;
        float exp_neg_alpha_w2 = std::exp(-alpha * ticks[i].w * ticks[i].w);
        float dt_val = (ticks[i].dt > 0.0f) ? ticks[i].dt : 0.001f;

        float integrand = (ticks[i].p_input * exp_neg_alpha_w2 * exp_2_alpha_v0 * ticks[i].v) / m_safe;
        integral_sum += integrand * dt_val;
    }

    float exp_neg_alpha_v02 = std::exp(-alpha * v0 * v0);
    return isp * efficiency * exp_neg_alpha_v02 * integral_sum;
}

/**
 * Single-tick evaluation helper for continuous high-frequency streaming ticks.
 */
[[nodiscard]] inline float calculate_hft_tick_delta_v(
    float isp,
    float efficiency,
    float alpha,
    float v0,
    const HFTSampleTick& tick,
    float min_mass_floor = 1e-6f
) noexcept {
    return calculate_hft_delta_v(isp, efficiency, alpha, v0, &tick, 1, min_mass_floor);
}

} // namespace Math
} // namespace AILLE

#endif // AILLE_MATH_HPP
