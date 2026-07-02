#ifndef AILLE_LANTERN_HPP
#define AILLE_LANTERN_HPP

#include <cstdint>

namespace aillee_spire {

    // The Lantern Layer: AILLEE's Signature Stability Identity (SSI) and Lantern Pulse (LP)
    // Deterministic, allocator-free, branch-minimal, cache-aligned.
    struct alignas(64) Lantern {
        std::uint64_t ssi_high; // upper 64 bits of identity
        std::uint64_t ssi_low;  // lower 64 bits of identity
        double pulse;           // lantern pulse signal [0.0, 1.0]
    };

    // Computes the deterministic 128-bit identity and pulse from existing bell-tower and spire signals.
    Lantern compute_lantern() noexcept;

    // Deterministic, branch-minimal mixing function for SSI.
    std::uint64_t mix(std::uint64_t a, std::uint64_t b, std::uint64_t c) noexcept;

} // namespace aillee_spire

#endif // AILLE_LANTERN_HPP
