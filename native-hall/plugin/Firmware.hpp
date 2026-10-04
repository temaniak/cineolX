#pragma once
#include <array>
#include <string_view>

namespace cineol {
enum class Hardware {original224,series224X};
struct Firmware {
    std::string_view id,name;
    Hardware hardware;
    bool selectable;
};
// Stable identities used by presets. XL exposes a native preview of five
// verified algorithms; this flag does not imply all firmware programs exist.
inline constexpr std::array<Firmware,4> firmware_catalog={{
    {"224-v4.4","224 v4.4",Hardware::original224,true},
    {"224x-v8.1","224X v8.1",Hardware::series224X,false},
    {"224xl-v8.1a","224XL v8.1A",Hardware::series224X,false},
    {"224xl-v8.21","224XL v8.21",Hardware::series224X,true},
}};
inline constexpr auto current_firmware=firmware_catalog[0];
inline constexpr const Firmware* find_firmware(std::string_view id) noexcept {
    for(const auto& firmware:firmware_catalog) if(firmware.id==id) return &firmware;
    return nullptr;
}
}
