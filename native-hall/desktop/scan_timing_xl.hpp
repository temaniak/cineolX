#pragma once
#include <cstdint>

namespace cineol::xl {
struct MonitorPassXL {unsigned work_states;uint16_t peak;};
struct LevelWorkXL {uint8_t level;unsigned work_states;};
// Native logarithmic headroom encoding and its data-dependent work cost.
constexpr LevelWorkXL level_work(uint16_t word) noexcept {
    word|=2;uint8_t value=7;unsigned work=24;
    for(unsigned i=0;i<16;++i) {
        value=uint8_t(value-16);work+=27;
        const bool carry=word&0x8000;word=uint16_t(word<<1);
        if(carry) {
            if(word&0x8000)return {uint8_t(value+8),work+32};
            return {value,work+21};
        }
    }
    return {value,work}; // The forced bit above makes this unreachable.
}
// One native Mod/monitor pass. Callbacks execute the separate Mod and
// auxiliary control laws and return their work states, including READY waits.
// Port reads receive IN instruction starts; their sample is nine CPU states
// plus phi2 rise later. The caller owns interrupt and converter chronology.
template<class Mod,class Auxiliary,class Read>
MonitorPassXL monitor_pass_work(unsigned descriptors,uint16_t peak,
    Mod&& mod,Auxiliary&& auxiliary,Read&& read) noexcept {
    const unsigned count=descriptors&15;
    unsigned at=17;at+=mod(at);
    at+=37;
    if(count<2)at+=count?502:1112;
    at+=17;at+=auxiliary(at);
    const uint8_t high=read(7,at);at+=10+4+10;
    const uint8_t magnitude_high=(high&128)?uint8_t(~high):high;
    if(high&128)at+=8;
    at+=5+7+10;
    uint8_t magnitude_low=0;
    if(!magnitude_high) {
        const uint8_t low=read(6,at);at+=10+4+10;
        magnitude_low=(low&128)?uint8_t(~low):low;
        if(low&128)at+=4;
        at+=5;
    }
    const uint16_t magnitude=uint16_t(unsigned(magnitude_high)<<8|magnitude_low);
    at+=16+17+38+10;
    if(magnitude>peak){peak=magnitude;at+=20;}
    at+=13+7+7;
    if(count<3){at+=17;at+=mod(at);}else at+=11;
    return {at+10,peak};
}

// Auxiliary early returns. An active descriptor ramp has its own control
// law and is deliberately not represented by a constant cost here.
constexpr unsigned auxiliary_idle_work(uint8_t layout,uint8_t state,unsigned descriptors) noexcept {
    if(layout&128)return 31;
    if(!(state&1))return 58;
    if(!(descriptors&15))return 99;
    return 0;
}

// Instruction work between calls in the nine-pass scan. These constants
// exclude the invoked routines and serial interrupt service.
constexpr unsigned slow_to_first_pass_work=35;
constexpr unsigned pass_to_fast_work=42;
constexpr unsigned fast_to_next_pass_work=53;
constexpr unsigned even_pass_to_next_pass_work=89;
} // namespace cineol::xl
