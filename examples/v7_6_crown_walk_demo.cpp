#include <iostream>
#include <iomanip>
#include "../extensions/aille_spire.hpp"
#include "../extensions/aille_crown_walk.hpp"

int main() {
    std::cout << "=========================================\n";
    std::cout << " AILLEE v7.6.0 Crown Walk Traversal Demo\n";
    std::cout << "=========================================\n\n";

    // Obtain the read-only traversal view
    aillee_crown_walk::CrownWalkView view = aillee_spire::get_crown_walk();

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "--- Cathedral Layers --------------------\n";
    std::cout << " Foundational Stability (v7.0) : " << view.foundational_stability << "\n";
    std::cout << " Secondary Stability    (v7.1) : " << view.secondary_stability << "\n";
    std::cout << " Secondary Intelligence (v7.2) : " << view.secondary_intelligence << "\n";
    std::cout << " Resonance Bell         (v7.3) : " << view.resonance_bell << "\n";
    std::cout << " Spire Pulse            (v7.4) : " << view.spire_pulse << "\n";
    std::cout << " Lantern Pulse          (v7.5) : " << view.lantern_pulse << "\n";
    std::cout << "-----------------------------------------\n";

    std::cout << "\n✓ Crown Walk traversal completed successfully.\n";

    return 0;
}
