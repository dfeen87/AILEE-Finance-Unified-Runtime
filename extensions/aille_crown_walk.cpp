#include "aille_crown_walk.hpp"
#include "v7_3_pipeline.hpp"
#include "aille_spire.hpp"
#include "aille_lantern.hpp"

namespace aillee_crown_walk {

    CrownWalkView walk() noexcept {
        CrownWalkView view{};

        // Fixed deterministic values for lower layers
        view.foundational_stability = 1.0;
        view.secondary_stability = 0.95;
        view.secondary_intelligence = 0.90;

        // Dynamic readings from upper layers
        view.resonance_bell = aillee_spire::get_resonance_bell();
        view.spire_pulse = aillee_spire::get_snapshot().resonance_bell;
        view.lantern_pulse = aillee_spire::get_lantern().pulse;

        return view;
    }

} // namespace aillee_crown_walk
