#include "aille_pilgrimage.hpp"
#include "aille_spire.hpp"
#include <cmath>

namespace aillee_pilgrimage {

    PilgrimageReport perform() noexcept {
        // Read external state via Spire
        auto lantern = aillee_spire::get_lantern();
        double resonance = aillee_spire::get_resonance_bell();
        double sync_tick = aillee_spire::get_sync_tick();
        double dampened = aillee_spire::get_dampened_state();

        // Construct Handshake
        PilgrimageHandshake handshake{};
        handshake.ssi_high = lantern.ssi_high;
        handshake.ssi_low  = lantern.ssi_low;
        handshake.pulse    = lantern.pulse;

        // Construct Sync
        PilgrimageSync sync{};
        sync.resonance_alignment = 1.0 - std::abs(resonance - lantern.pulse);
        sync.sync_alignment = 1.0 - std::abs(sync_tick - lantern.pulse);
        sync.dampening_alignment = 1.0 - std::abs(dampened - lantern.pulse);

        // Construct and return Report
        PilgrimageReport report{};
        report.handshake = handshake;
        report.sync = sync;

        return report;
    }

} // namespace aillee_pilgrimage
