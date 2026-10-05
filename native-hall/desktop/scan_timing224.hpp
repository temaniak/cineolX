#pragma once
#include "level_timing224.hpp"

namespace native_hall {
// The panel is stable and predelay has reached its target. The one-tap
// plate programs insert an extra delay before the transfer-register read.
inline unsigned transfer_gap224(unsigned flags) noexcept {
    return flags>=2?178:flags==1?500:930;
}
// Cost of a stable panel comparison. A pot can retain a value up to two
// codes below the current ADC value. Zero means a compiler/button action,
// which is outside this steady-control clock model.
inline unsigned panel_cycles224(unsigned channel,uint8_t raw,uint8_t cached) noexcept {
    if(channel<6) {
        if(raw==cached)return 58;
        if(unsigned(raw)==unsigned(cached)+1)return 90;
        if(unsigned(raw)==unsigned(cached)+2)return 112;
        return 0;
    }
    if(raw!=cached)return 0;
    return channel==6?(raw==255?78:92):48;
}
} // namespace native_hall
