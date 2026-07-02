#include <iostream>
#include <iomanip>
#include "../extensions/aille_spire.hpp"
#include "../extensions/aille_weathering.hpp"

void print_weathering_report() {
    std::cout << "==========================================\n";
    std::cout << " AILLEE WEATHERING LAYER (v7.7) DEMO      \n";
    std::cout << "==========================================\n\n";

    // Obtain the deterministic resilience evaluation
    auto report = aillee_spire::get_weathering();

    std::cout << std::fixed << std::setprecision(6);

    std::cout << "--- ShockFront Metrics ---\n";
    std::cout << " Resonance Surge    : " << report.shock.resonance_surge << "\n";
    std::cout << " Sync Distortion    : " << report.shock.sync_distortion << "\n";
    std::cout << " Dampening Anomaly  : " << report.shock.dampening_anomaly << "\n\n";

    std::cout << "--- StressTest Metrics ---\n";
    std::cout << " Structural Load    : " << report.stress.structural_load << "\n";
    std::cout << " Volatility Factor  : " << report.stress.volatility_factor << "\n";
    std::cout << " Resilience Score   : " << report.stress.resilience_score << "\n\n";

    std::cout << "Weathering check complete.\n";
}

int main() {
    print_weathering_report();
    return 0;
}
