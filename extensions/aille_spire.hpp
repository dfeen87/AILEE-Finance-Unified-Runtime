#ifndef AILLEE_SPIRE_HPP
#define AILLEE_SPIRE_HPP

#include <cstddef>

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

} // namespace aillee_spire

#endif // AILLEE_SPIRE_HPP
