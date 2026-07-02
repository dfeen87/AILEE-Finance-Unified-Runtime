#include "extensions/aille_spire.hpp"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "========================================\n";
    std::cout << " AILLEE Spire Interface Demo (v7.4.0) \n";
    std::cout << "========================================\n\n";

    // Call individual external accessors
    double bell = aillee_spire::get_resonance_bell();
    double tick = aillee_spire::get_sync_tick();
    double dampened = aillee_spire::get_dampened_state();

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "--- Individual Spire Signals ---\n";
    std::cout << "Resonance Bell  : " << bell << "\n";
    std::cout << "Sync Tick       : " << tick << "\n";
    std::cout << "Dampened State  : " << dampened << "\n\n";

    // Call snapshot
    aillee_spire::AILLEE_Snapshot snapshot = aillee_spire::get_snapshot();

    std::cout << "--- AILLEE Snapshot ---\n";
    std::cout << "Resonance Bell  : " << snapshot.resonance_bell << "\n";
    std::cout << "Sync Tick       : " << snapshot.sync_tick << "\n";
    std::cout << "Dampened State  : " << snapshot.dampened_state << "\n\n";

    std::cout << "Spire Demo completed successfully.\n";

    return 0;
}
