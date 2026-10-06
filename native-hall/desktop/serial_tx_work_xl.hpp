#pragma once
#include "headroom_display_clock_xl.hpp"
#include <cstdint>
#include <limits>

namespace cineol::xl {
struct SerialTxMemoryXL {uint8_t remaining=0,text_remaining=0;};
struct SerialTxWorkXL {
    enum class Action:uint8_t {indicator,header,text,echo,disable};
    unsigned work_states=0,write_start=0;
    Action action=Action::disable;
};
// Settled TX-ready interrupt law. Receive/error interrupts belong to a
// separate input transaction stage. The enclosing clock supplies a legal
// instruction boundary, and owns UART character deadlines and DI/EI spans.
// Counts include the 11-state acknowledge and the complete interrupt return.
constexpr SerialTxWorkXL serial_tx_work(PanelPublishMemoryXL& panel,SerialTxMemoryXL& serial) noexcept {
    constexpr unsigned prefix=11+10+44+10+5+7+10+5+7+10+5+7+10;
    constexpr unsigned suffix=54;
    unsigned at=prefix+34;
    if(panel.flags&1) {
        const unsigned write=at+16+7;at+=84;
        if(!--serial.remaining){panel.flags&=254;at+=41;}
        return {at+suffix,write,SerialTxWorkXL::Action::indicator};
    }
    at+=24;
    if(panel.flags&2) {
        panel.flags&=253;
        return {at+64+suffix,at+13,SerialTxWorkXL::Action::echo};
    }
    at+=24;
    if(panel.flags&4) {
        at+=58;
        if(serial.text_remaining)--serial.text_remaining;
        else {panel.flags&=251;at+=38;}
        return {at+27+suffix,at+7,SerialTxWorkXL::Action::text};
    }
    at+=24;
    if(panel.flags&8) {
        panel.flags=uint8_t((panel.flags|1)&247);serial.remaining=4;
        return {at+101+suffix,at+81,SerialTxWorkXL::Action::header};
    }
    panel.flags=128;
    return {at+47+suffix,at+7,SerialTxWorkXL::Action::disable};
}
// Inherited complete-character 8N2 UART boundary, also used by the pinned
// reference board. This is a hardware deadline, not a measured poll rate.
inline constexpr uint64_t serial_character_states_xl=2292;
} // namespace cineol::xl
