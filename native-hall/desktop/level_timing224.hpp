#pragma once
#include "control_timing224.hpp"

namespace native_hall {
struct LoopGainTiming224 {unsigned quarters,limit;};
// The supported networks use mean, 5/4-mean or 3/2-mean loop gains and
// fixed gain caps. These are native controller laws, not ROM descriptors.
inline LoopGainTiming224 loop_gain_timing224(unsigned program,unsigned loop) noexcept {
    if(program==1 && loop>=2)return {6,19};
    if(program==3)return {5,loop<2?10u:11u};
    if(program==4)return {loop<2?6u:4u,16};
    return {5,16};
}
inline unsigned bit_count224(unsigned value) noexcept {
    unsigned n=0;for(;value;value>>=1)n+=value&1;return n;
}
inline unsigned multiply_cycles224(unsigned multiplier) noexcept {
    return 346+10*bit_count224(multiplier&255);
}
template<class Write> unsigned loop_gain_cycles224(const ProgramBank& bank,unsigned program,
        uint8_t mean,uint8_t amount,bool half,unsigned cycles,Write&& write) noexcept {
    cycles+=85;unsigned previous_type=128;int gain=4;
    for(unsigned loop=0;loop<4;++loop) {
        const auto spec=loop_gain_timing224(program,loop);
        const unsigned type=spec.quarters*32+spec.limit;
        cycles+=65;
        if(type!=previous_type) {
            previous_type=type;
            cycles+=78+multiply_cycles224(spec.quarters*32)+52;
            if(half)cycles+=14;
            const unsigned raw=(mean*spec.quarters/4)>>(half?1:0);
            cycles+=28;if(spec.limit>=raw)cycles+=5;
            gain=int(std::min(raw,spec.limit))-amount;
            cycles+=60;
            if(gain<4){gain=4;cycles+=7;}
            cycles+=68+multiply_cycles224(unsigned(gain)*2)+59;
        }
        cycles+=40;
        for(unsigned coefficient=0;coefficient<3;++coefficient) {
            // Read the coefficient template from ordinary ROM (seven
            // states), then preserve its other two bits in the WCS write.
            cycles+=87+7+31;
            const int value=coefficient==0?gain:coefficient==1?32-gain*gain/32:-gain;
            const auto row=uint8_t(bank.programs[program].loop_rows[loop]+coefficient);
            cycles+=control_write224(write,cycles,{ControlWrite224::Kind::Coefficient,row,uint8_t(value)});cycles+=20;
            if(coefficient<2)cycles+=55;
        }
        cycles+=25;
    }
    return cycles+10;
}
// Includes the fixed headroom-read prefix and the complete level-controller
// return. Stable panel mode is assumed: no remote/display-bank transition.
// The display cartridge check and half-gain flag are explicit context;
// a free-running scheduler must retain them rather than sample oracle RAM.
template<class Write> unsigned level_cycles224(const ProgramBank& bank,unsigned program,
        DecayState state,uint16_t word,uint8_t mean,bool enabled,uint8_t period,
        bool display_check,bool half,Write&& write) noexcept {
    const uint8_t level=DecayController::level_from_word(word);
    unsigned cycles=373+69+27*((271u-level)/16)+45;
    if(level>=state.held) {
        cycles+=58;state.held=level;state.stopped=0;
        if(state.amount) {
            cycles+=22;
            if(level>=128) {
                cycles+=64;state.amount=state.peak_divider=state.diffusion_divider=1;
                state.diffusion_period=period;
            }
        }
    }
    cycles+=30;bool skip_peak=false;
    if(enabled) {
        cycles+=48;
        if(!(uint8_t(state.held-level-22)&128)) {
            cycles+=21;
            if(!state.stopped) {
                state.stopped=1;cycles+=46;
                const uint8_t drop=uint8_t(state.held-state.history[0]);
                if(drop&128)skip_peak=true;
                else {
                    cycles+=17;
                    if(drop<24)skip_peak=true;
                    else {cycles+=15;state.diffusion_period=15;}
                }
            }
        }
        if(!skip_peak)cycles+=456;
    }
    if(!skip_peak) {
        cycles+=30;
        if(--state.peak_divider==0) {
            cycles+=40+209;
            if(display_check)cycles+=40;
            const unsigned quotient=mean?16384u/mean:65535u;
            cycles+=83+1790+28*bit_count224(quotient)+55;
        }
    }
    cycles+=40;if(mean>=25)cycles+=10;
    cycles+=27;
    if(--state.diffusion_divider!=0)return cycles+6;
    cycles+=43;
    if(!state.stopped) {
        if(!state.amount)return cycles+22;
        cycles+=36;--state.amount;
    } else {
        if(state.amount>=12)return cycles+25;
        cycles+=41;++state.amount;
    }
    return loop_gain_cycles224(bank,program,mean,state.amount,half,cycles,write);
}
} // namespace native_hall
