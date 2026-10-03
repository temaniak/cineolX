#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include "decay.hpp"

namespace native_hall {
// Prepared on the desktop, then preloaded before audio on Daisy. No 8080
// instructions are executed by the core. Keep generated profiles out of Git.
struct Profile {
    static constexpr uint32_t version = 2;
    std::array<int8_t, 100> coefficients{};
    std::array<uint16_t, 100> offsets{};
    // Original control compiler's results: feedback losses and four loop APs.
    std::array<std::array<std::array<int8_t, 6>, 32>, 32> tail{};
    std::array<std::array<int8_t, 4>, 32> crossover{};
    std::array<std::array<int8_t, 4>, 32> treble{};
    std::array<std::array<int8_t, 8>, 72> depth{};
    std::array<std::array<int8_t, 4>, 64> diffusion{};
    std::array<uint8_t, 4096> modulation_sequence{};
    std::array<uint8_t, 10> modulation_descriptors{};
    uint16_t modulation_index = 0;
    uint8_t modulation_period = 1, modulation_hold = 32, modulation_step = 4;
    uint8_t modulation_mask = 0, initial_mod_divider=1, initial_random_divider=8, initial_random_hold=1;
    uint8_t decay_amount = 5;
    DecayState initial_decay{};
    // Tenths of Hz, measured offline with the original factory control loop.
    // Index: Mode Enhancement bit 0, Decay Optimization bit 1. Enabling
    // modulation makes the 8080 work loop substantially slower.
    std::array<uint16_t,4> level_rate_tenths{};
};

inline uint32_t profile_checksum(const void* ptr, size_t size) noexcept {
    auto p = static_cast<const uint8_t*>(ptr);
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < size; ++i) h = (h ^ p[i]) * 16777619u;
    return h;
}
struct ProfileHeader {
    char magic[8] = {'H','A','L','L','2','2','4',0};
    uint32_t version = Profile::version, size = sizeof(Profile), checksum = 0;
};
static_assert(sizeof(ProfileHeader)==20 && sizeof(Profile)==11674, "profile ABI changed");
// Load only in setup. Fixed-size copies; profile bytes use the little-endian
// ABI shared by the macOS prototype and Cortex-M7. The checksum catches corruption.
inline bool read_profile(const void* data, size_t size, Profile& result) noexcept {
    if (!data || size != sizeof(ProfileHeader) + sizeof(Profile)) return false;
    ProfileHeader h;
    std::memcpy(&h, data, sizeof h);
    const ProfileHeader expected;
    if (std::memcmp(h.magic, expected.magic, 8) || h.version != Profile::version || h.size != sizeof(Profile))
        return false;
    const auto* payload = static_cast<const uint8_t*>(data) + sizeof h;
    if (profile_checksum(payload, sizeof(Profile)) != h.checksum) return false;
    std::memcpy(&result, payload, sizeof result);
    // A profile describes this one specialized network, not arbitrary code.
    // Reject bad tap descriptors before they can index the coefficient array.
    for(unsigned j=0;j<2;++j) {
        unsigned at=j*5;
        unsigned address=result.modulation_descriptors[at] |
            unsigned(result.modulation_descriptors[at+1])<<8;
        if(address!=(j==0?0x41bb:0x40f3)) return false;
    }
    if(result.modulation_index>=4096 || !result.modulation_period || !result.modulation_hold ||
       !result.modulation_step || !result.initial_mod_divider || !result.initial_random_divider ||
       !result.initial_random_hold || !result.decay_amount) return false;
    for(auto offset:result.offsets) if(offset>=16384) return false;
    for(auto rate:result.level_rate_tenths) if(rate<100 || rate>2000) return false;
    return true;
}
} // namespace native_hall
