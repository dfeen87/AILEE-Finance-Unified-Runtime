#include "aille_lantern.hpp"
#include "aille_spire.hpp"
#include <cstring>
#include <algorithm>

namespace aillee_spire {

    // Deterministic mixing function using classic avalanche multipliers
    std::uint64_t mix(std::uint64_t a, std::uint64_t b, std::uint64_t c) noexcept {
        return (a * 0x9E3779B185EBCA87ULL) ^ (b * 0xC2B2AE3D27D4EB4FULL) ^ c;
    }

    Lantern compute_lantern() noexcept {
        // Fetch inputs from Spire interface
        double res = get_resonance_bell();
        double sync = get_sync_tick();
        double damp = get_dampened_state();

        // Convert doubles to uint64_t securely via memcpy equivalent to avoid undefined behavior
        std::uint64_t res_bits = 0;
        std::uint64_t sync_bits = 0;
        std::uint64_t damp_bits = 0;

        std::memcpy(&res_bits, &res, sizeof(double));
        std::memcpy(&sync_bits, &sync, sizeof(double));
        std::memcpy(&damp_bits, &damp, sizeof(double));

        // Generate SSI (Signature Stability Identity)
        std::uint64_t ssi_high = mix(res_bits, sync_bits, damp_bits);
        std::uint64_t ssi_low = mix(damp_bits, res_bits, sync_bits); // Different mix order for low bits

        // Calculate pulse as normalized scalar
        double pulse_val = std::min(1.0, (res + sync + damp) / 3.0);

        // Ensure pulse is not negative (though inputs shouldn't be negative according to test expectations)
        pulse_val = std::max(0.0, pulse_val);

        Lantern lantern;
        lantern.ssi_high = ssi_high;
        lantern.ssi_low = ssi_low;
        lantern.pulse = pulse_val;

        return lantern;
    }

} // namespace aillee_spire
