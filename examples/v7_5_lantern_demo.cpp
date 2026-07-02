#include <iostream>
#include <iomanip>
#include "../extensions/aille_spire.hpp"

int main() {
    std::cout << "======================================\n";
    std::cout << " AILLEE Cathedral - v7.5 Lantern Demo\n";
    std::cout << "======================================\n";

    // Fetch the lantern state
    auto lantern = aillee_spire::get_lantern();

    // Print outputs deterministically
    std::cout << "\n[Lantern Identity]\n";
    std::cout << "SSI High : 0x" << std::setfill('0') << std::setw(16) << std::hex << lantern.ssi_high << "\n";
    std::cout << "SSI Low  : 0x" << std::setfill('0') << std::setw(16) << std::hex << lantern.ssi_low << "\n";

    std::cout << "\n[Lantern Signal]\n";
    std::cout << "Pulse    : " << std::fixed << std::setprecision(6) << lantern.pulse << "\n";

    std::cout << "\nDemo complete.\n";

    return 0;
}
