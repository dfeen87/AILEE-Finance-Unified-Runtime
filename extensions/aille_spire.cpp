#include "aille_spire.hpp"
#include "v7_3_pipeline.hpp"

namespace {
    // Static instances of the bell-tower modules, representing the deterministic state
    // Zero dynamic allocation, no exceptions
    AILLE::GlobalResonanceBeacon static_beacon;
    AILLE::HarmonicSync static_sync;
    AILLE::EchoDampener static_dampener;
}

namespace aillee_spire {

    double get_resonance_bell() noexcept {
        return static_beacon.bell();
    }

    double get_sync_tick() noexcept {
        return static_sync.tick();
    }

    double get_dampened_state() noexcept {
        return static_dampener.dampened();
    }

    AILLEE_Snapshot get_snapshot() noexcept {
        return {
            get_resonance_bell(),
            get_sync_tick(),
            get_dampened_state()
        };
    }

    Lantern get_lantern() noexcept {
        return compute_lantern();
    }

    aillee_crown_walk::CrownWalkView get_crown_walk() noexcept {
        return aillee_crown_walk::walk();
    }

} // namespace aillee_spire
