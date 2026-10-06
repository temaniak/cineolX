#pragma once
#include "modulation.hpp"
#include "wcs_timing_xl.hpp"

namespace cineol::xl {
struct ControlByteXL {unsigned row,lane;uint8_t value;};
// Native AD5C control-law branch costs, in logical CPU clock states. The
// caller supplies a bounded write instruction duration; no CPU or ROM
// instruction evaluation is performed here.
template<class Write> unsigned modulation_work_cycles(const ModulationProfile& p,
    const ModulationState& state,bool enabled,Write&& write) {
    if(!enabled) return 1127; // Gate plus the 72-iteration delay and RET.
    unsigned cycles=70;
    auto index=state.index;bool startup=state.startup_lookup;
    if(state.random_divider==1) {
        cycles+=35;
        if(state.random_hold==1) {cycles+=75;index=(index+1)&4095;startup=false;}
    }
    cycles+=30;
    if(state.divider!=1) return cycles+1097;
    cycles+=80;
    const unsigned count=p.flags&15;
    if(!count) return cycles+11; // RZ taken; otherwise its five states below.
    cycles+=26;
    uint8_t random=startup?p.startup_byte:p.sequence[index];
    for(unsigned j=0;j<count;++j) {
        cycles+=125;
        const unsigned flags=p.flags-j;
        if((flags&128) && (flags&1)) {cycles+=41;random^=1;}
        else {
            if(flags&128) cycles+=14;
            cycles+=43;
            if(!(flags&64)) {cycles+=4;random=uint8_t((random>>1)|(random<<7));}
        }
        cycles+=89;
        const unsigned row=p.rows[j];const uint8_t cap=p.caps[j],cached=state.address_low[j];
        uint8_t phase=state.phase[j];const bool up=random&1;
        const bool add=(up && !(phase&1)) || (!up && (phase&1));
        cycles+=add?35:39;
        int value=int(phase)+(add?p.step:-int(p.step));
        const bool advance=add?value>255:uint8_t(value)<cap;
        bool blocked=false;
        if(advance) {
            const int delta=up?((phase&1)?1:2):((phase&1)?-2:-1);
            const int candidate=int(cached)+delta;
            cycles+=29+21;
            const bool overflow=candidate<0 || candidate>255;
            if(!overflow) cycles+=26;
            blocked=overflow || ((uint8_t(candidate)^cached)&p.mask);
            if(blocked) cycles+=73;
            else {
                cycles+=20+27;
                if(delta==2 || delta==-2) {
                    cycles+=48;
                    cycles+=write(cycles,ControlByteXL{row,0,uint8_t(candidate)});
                    cycles+=41;
                } else {
                    cycles+=107;
                    cycles+=write(cycles,ControlByteXL{row+1,0,uint8_t(candidate)});
                    cycles+=71;
                }
                cycles+=10+76;phase^=1;
                const bool next_add=(up && !(phase&1)) || (!up && (phase&1));
                cycles+=next_add?35:39;
                value=int(phase)+(next_add?p.step:-int(p.step));
            }
        }
        if(!blocked) {
            cycles+=132;
            const uint8_t first=uint8_t((uint8_t(value)&0xfc)|(cap&3));
            const uint8_t second=uint8_t(((cap|3)-4-first)|3);
            cycles+=write(cycles,ControlByteXL{row+1,3,second});
            cycles+=write(cycles,ControlByteXL{row,3,first});
            cycles+=14;
        }
        cycles+=47;
    }
    return cycles+10;
}

struct TimedModulationXL {unsigned work_states=0;uint64_t finished_state=0;};
// Resume maps local work to wall time (including native serial-IRQ stages).
// The offline oracle may supply observed IRQ spans as a labelled diagnostic;
// a free-running native scan must generate its own interrupt chronology.
template<class Resume,class Emit>
TimedModulationXL timed_modulation(const ModulationProfile& profile,const ModulationState& state,
    bool enabled,WcsTimingXL& bus,Resume&& resume,Emit&& emit) noexcept {
    const unsigned work=modulation_work_cycles(profile,state,enabled,[&](unsigned at,ControlByteXL payload) noexcept {
        const uint64_t start=resume(at);
        // STAX/MOV M's final data cycle starts four states after instruction
        // entry. Bus completion includes all independently predicted READY waits.
        const auto access=bus.access(start+4,false);
        emit(at,start,payload,access);
        return unsigned(access.finished_cpu_state-start);
    });
    return {work,resume(work)};
}
template<class Emit>
TimedModulationXL timed_modulation(const ModulationProfile& profile,const ModulationState& state,
    bool enabled,uint64_t entry,WcsTimingXL& bus,Emit&& emit) noexcept {
    return timed_modulation(profile,state,enabled,bus,[&](unsigned at) noexcept {return entry+at;},emit);
}
} // namespace cineol::xl
