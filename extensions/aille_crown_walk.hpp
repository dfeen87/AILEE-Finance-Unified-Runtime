#ifndef AILLEE_CROWN_WALK_HPP
#define AILLEE_CROWN_WALK_HPP

#include <cstddef>

namespace aillee_crown_walk {

    struct alignas(64) CrownWalkView {
        double foundational_stability;   // from v7.0 core
        double secondary_stability;      // from v7.1
        double secondary_intelligence;   // from v7.2
        double resonance_bell;           // from v7.3
        double spire_pulse;              // from v7.4 / spire
        double lantern_pulse;            // from v7.5
    };

    CrownWalkView walk() noexcept;

} // namespace aillee_crown_walk

#endif // AILLEE_CROWN_WALK_HPP
