#include <iostream>
#include "../extensions/aille_spire.hpp"

int main() {
    std::cout << "--- AILLEE V7.8 Pilgrimage Layer Demo ---\n\n";

    // Retrieve pilgrimage report from Spire interface
    auto report = aillee_spire::get_pilgrimage();

    std::cout << "[ Pilgrimage Handshake ]\n";
    std::cout << "  ssi_high: " << report.handshake.ssi_high << "\n";
    std::cout << "  ssi_low:  " << report.handshake.ssi_low << "\n";
    std::cout << "  pulse:    " << report.handshake.pulse << "\n\n";

    std::cout << "[ Pilgrimage Sync Alignment ]\n";
    std::cout << "  resonance_alignment: " << report.sync.resonance_alignment << "\n";
    std::cout << "  sync_alignment:      " << report.sync.sync_alignment << "\n";
    std::cout << "  dampening_alignment: " << report.sync.dampening_alignment << "\n\n";

    std::cout << "Pilgrimage complete.\n";

    return 0;
}
