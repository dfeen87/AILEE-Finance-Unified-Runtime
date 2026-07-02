#ifndef AILLE_PILGRIMAGE_HPP
#define AILLE_PILGRIMAGE_HPP

#include <cstdint>

namespace aillee_pilgrimage {

    struct alignas(64) PilgrimageHandshake {
        std::uint64_t ssi_high;     // lantern identity high bits
        std::uint64_t ssi_low;      // lantern identity low bits
        double pulse;               // lantern pulse
    };

    struct alignas(64) PilgrimageSync {
        double resonance_alignment; // alignment between nodes
        double sync_alignment;      // harmonic sync alignment
        double dampening_alignment; // dampened state alignment
    };

    struct alignas(64) PilgrimageReport {
        PilgrimageHandshake handshake;
        PilgrimageSync sync;
    };

    PilgrimageReport perform() noexcept;

} // namespace aillee_pilgrimage

#endif // AILLE_PILGRIMAGE_HPP
