#include "aille_weathering.hpp"
#include "aille_spire.hpp"
#include <cmath>
#include <algorithm>

namespace aillee_weathering {

    WeatheringReport evaluate() noexcept {
        WeatheringReport report{};

        // Fetch read-only signals from the Spire and Lantern layers
        auto resonance = aillee_spire::get_resonance_bell();
        auto sync = aillee_spire::get_sync_tick();
        auto dampened = aillee_spire::get_dampened_state();
        // The Lantern pulse is available but not explicitly used in the given calculation spec
        auto pulse = aillee_spire::get_lantern().pulse;
        (void)pulse; // Mute unused warning

        // ShockFront Calculation
        report.shock.resonance_surge = std::abs(resonance - 1.0);
        report.shock.sync_distortion = std::abs(sync - 1.0);
        report.shock.dampening_anomaly = std::abs(dampened - 1.0);

        // StressTest Calculation
        report.stress.structural_load =
            report.shock.resonance_surge +
            report.shock.sync_distortion +
            report.shock.dampening_anomaly;

        // Normalize volatility
        report.stress.volatility_factor = std::min(1.0, report.stress.structural_load);

        // Compute resilience
        report.stress.resilience_score = 1.0 - report.stress.volatility_factor;

        return report;
    }

} // namespace aillee_weathering
