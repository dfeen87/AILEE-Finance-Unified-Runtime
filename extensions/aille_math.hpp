/*
 * AILLE Framework - Deterministic Math Extension
 * AI-Load Integrity and Layered Evaluation
 *
 * Advisory-only deterministic math functions for risk logic smoothing.
 *
 * Copyright (c) 2026 Don Michael Feeney Jr
 * License: MIT (see LICENSE)
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

} // namespace Math
} // namespace AILLE

#endif // AILLE_MATH_HPP
