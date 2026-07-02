#ifndef AILLEE_SPIRE_HPP
#define AILLEE_SPIRE_HPP

#include <cstddef>
#include "aille_lantern.hpp"
#include "aille_crown_walk.hpp"
#include "aille_weathering.hpp"

namespace aillee_spire {

    // Returns the bell tower’s resonance signal
    double get_resonance_bell() noexcept;

    // Returns the harmonic sync tick
    double get_sync_tick() noexcept;

    // Returns the dampened stability scalar
    double get_dampened_state() noexcept;

    // AILLEE's external state snapshot
    struct alignas(64) AILLEE_Snapshot {
        double resonance_bell;
        double sync_tick;
        double dampened_state;
    };

    // Returns a full snapshot of AILLEE’s external state
    AILLEE_Snapshot get_snapshot() noexcept;

    // Returns the deterministic Lantern Layer identity and pulse
    Lantern get_lantern() noexcept;

    // Returns a full Crown Walk Traversal read-only view
    aillee_crown_walk::CrownWalkView get_crown_walk() noexcept;

    // Returns the deterministic Weathering Layer resilience report
    aillee_weathering::WeatheringReport get_weathering() noexcept;

} // namespace aillee_spire

#endif // AILLEE_SPIRE_HPP
