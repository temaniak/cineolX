#pragma once
#include "dynamics.hpp"
#include "scan_timing_xl.hpp"

namespace cineol::xl {
struct FastControlMemoryXL {
    DynamicsState dynamics{};
    uint8_t headroom_left=0,headroom_right=0,headroom_accum=0;
};
// Native fast-controller branch/state work. Reads are requested at their IN
// starts. Feedback/restore callbacks own separately timed coefficient work;
// no CPU/opcode or ROM execution is performed by this control law.
template<class Read,class Feedback,class Restore>
unsigned fast_control_work(FastControlMemoryXL& memory,uint8_t special,uint8_t reset_period,
    const std::array<uint8_t,48>& raw,Read&& read,Feedback&& feedback,Restore&& restore) noexcept {
    auto& s=memory.dynamics;
    if(special&64)return 31;
    unsigned at=25+11+13+5+13+4+10;
    if(!s.amount && !s.flags)return at+10+10;
    at+=11+11+13+5+13+5;
    const uint8_t left=read(8,at);at+=10+5+4+5;
    const uint8_t right=read(9,at);
    memory.headroom_left|=left;memory.headroom_right|=right;
    const uint8_t bits=left|right;memory.headroom_accum|=bits;
    const uint8_t previous=s.peak_input;s.peak_input=bits;
    at+=10+5+4+5+13+5+13+5+4+5+10+7+7+5+7+4+5+7+10;
    constexpr unsigned exit=10+10+10+10;
    if(previous>=bits)return at+exit;
    at+=4+10;
    if(!bits)return at+exit;
    at+=5+7+8*(10+4+5+10)+5+13+5+17;
    const auto encoded=level_work(Dynamics::detector_word(bits));at+=encoded.work_states;
    const uint8_t observed=encoded.level;
    at+=5+11+13+4+10;
    if(s.amount) {
        at+=5+4+10;
        if(observed>=uint8_t(s.held>>8)) {
            s.held=uint16_t(uint8_t(observed-uint8_t(s.held>>8)))<<8;s.stopped=0;
            at+=10+10+5+7+5+5+10+5+13+13;
            // Return-to-zero is compiled in one/two-unit steps. Preserve
            // every intermediate compilation instead of collapsing to zero.
            s.period=reset_period;
            for(unsigned i=0;i<129;++i) {
                at+=10+7+4+10;
                if(!s.amount)break;
                --s.amount;at+=10+10;
                if(s.amount){--s.amount;at+=10;}
                at+=17;at+=feedback(at,s);at+=10;
            }
        }
    }
    at+=10+13+4+10;
    if(!s.flags)return at+exit;
    at+=5+13+5+5+4+5+10;
    if(observed<s.average)return at+exit;
    at+=7+4+10;
    if(observed<=188) {
        at+=5+7+10;
        if(observed-s.average<39)return at+exit;
    }
    s.trigger_peak=observed;s.stop_counter=raw[45];
    at+=5+13+13+13+16+13+4+10;
    if(s.mid>=raw[1])return at+exit;
    s.low=raw[0];s.mid=raw[1];at+=16+17;at+=restore(at,s);
    s.flags=0;at+=4+13+10;
    return at+exit;
}
} // namespace cineol::xl
