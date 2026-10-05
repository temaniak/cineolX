#pragma once
#include "../core/hall.hpp"

namespace native_hall {
// Original-224 WCS grant clock for the four supported fixed networks. The
// unprotected rows are graph properties, not ROM/WCS payload. A write grant
// displaces the next fetch; its falling/rising protect edges change the pair
// flip-flop. This matters for the following write's wait, even at steady input.
class WcsTiming224 {
public:
    void reset(unsigned network,uint64_t cycle,unsigned next_row=0) noexcept {
        network_=network;origin_=cycle-next_row;next_=origin_;
        displaced_=0;pair_=false;previous_protected_=true;
        previous_reset_=true;reset_high_=true;
        advance(cycle);
    }
    unsigned write(uint64_t cycle) noexcept {
        // STAX/MOV M starts its data cycle after four states; MWTC reaches
        // the T&C two states later. READY then adds four states
        // after the grant marker. The offline oracle checks both boundaries.
        advance(cycle+6);
        unsigned duration=0;
        for(unsigned n=0;n<100;++n) {
            const auto grant=next_;
            const bool allowed=!pair_ && !previous_protected_;
            if(allowed)displaced_=grant+2;
            fetch();
            if(allowed){duration=unsigned(grant+4-cycle);break;}
        }
        return duration;
    }
private:
    bool protected_row(unsigned row) const noexcept {
        if(network_==1)return row!=46 && row!=69 && row!=93;
        if(network_==2)return row!=94;
        return row!=46 && row!=95;
    }
    void advance(uint64_t cycle) noexcept {
        // A full unaffected pass resets the pair logic. Skip long quiet gaps
        // without iterating over the elapsed CPU states.
        if(next_+104<cycle && displaced_+100<cycle) {
            next_=cycle-(cycle-origin_)%100;
            pair_=false;previous_protected_=previous_reset_=reset_high_=true;
        }
        while(next_<cycle)fetch();
    }
    void fetch() noexcept {
        const unsigned row=unsigned((next_-origin_)%100);
        // The write window is [grant+1, grant+2), so only grant+1's fetch
        // is displaced. Operand-register holds last longer and belong to
        // the DSP boundary model, not this CPU wait clock.
        const bool displaced=displaced_ && next_+1==displaced_;
        const bool protect=!displaced && protected_row(row);
        if(!previous_protected_ && protect)pair_=reset_high_?!pair_:false;
        if(reset_high_ && previous_reset_)pair_=false;
        reset_high_=!previous_reset_;
        previous_protected_=protect;previous_reset_=!displaced && row==99;
        ++next_;
    }
    uint64_t origin_=0,next_=0,displaced_=0;
    unsigned network_=0;
    bool pair_=false,previous_protected_=true,previous_reset_=true,reset_high_=true;
};
// Native branch-cost model of the original v4.4 modulation routine. Costs
// are 8080 clock states (2.048 MHz), excluding the caller. No ROM, instruction
// dispatch or CPU state lives here. The writer supplies each bounded WCS
// access duration, including its seven instruction states and bus waits.
template<class Write> unsigned modulation_cycles224(const ProgramBank& bank,
        unsigned program,const ModulationState& state,bool enabled,Write&& write) noexcept {
    if(!enabled)return 767; // Entry test and the 48-iteration delay path.
    const auto& p=bank.programs[program];
    unsigned cycles=70;
    auto index=state.index;
    if(state.random_divider==1) {
        cycles+=35;
        if(state.hold==1){cycles+=76;index=(index+1)&4095;}
    }
    cycles+=30;
    if(state.divider!=1)return cycles+737;
    cycles+=77;
    const unsigned count=p.modulation_flags&15;
    if(!count)return cycles+6; // Taken conditional return.
    cycles+=43;
    uint8_t random=bank.modulation_sequence[index];
    for(unsigned j=0;j<count;++j) {
        cycles+=132;
        if(p.identity!=8) {
            cycles+=35; // Save descriptor flags and test its sign.
            if(p.modulation_flags&128) {
                cycles+=14;
                if((p.modulation_flags-j)&1){random^=1;cycles+=27;}
                else {random=uint8_t((random>>1)|(random<<7));cycles+=9;}
            } else {random=uint8_t((random>>1)|(random<<7));cycles+=9;}
            cycles+=13;
        }
        const unsigned at=5*j;
        const uint8_t cap=state.descriptors[at+2],cached=state.descriptors[at+3];
        auto phase=state.descriptors[at+4];
        const bool up=random&1;
        const int step=p.modulation_step;
        cycles+=76;
        const bool add=(up && !(phase&1)) || (!up && (phase&1));
        cycles+=add?35:39;
        const int value=int(phase)+(add?step:-step);
        const bool advance=add?value>255:uint8_t(value)<cap;
        bool blocked=false;
        if(advance) {
            const int delta=up?((phase&1)?1:2):((phase&1)?-2:-1);
            const int candidate=int(cached)+delta;
            cycles+=29+21;
            const bool overflow=candidate<0 || candidate>255;
            if(!overflow)cycles+=26;
            blocked=overflow || ((uint8_t(candidate)^cached)&p.modulation_mask);
            if(blocked)cycles+=73;
            else {
                cycles+=20; // Successful boundary check return.
                cycles+=27; // Restore descriptor pointer; call move helper.
                if(delta==2 || delta==-2) {
                    cycles+=48; // Helper up to its address write.
                    cycles+=write(cycles);
                    cycles+=41;
                } else {
                    cycles+=107;
                    cycles+=write(cycles);
                    cycles+=71;
                }
                cycles+=10+76; // Jump back for the fractional step.
                phase^=1;
                cycles+=((up && !(phase&1)) || (!up && (phase&1)))?35:39;
            }
        }
        if(!blocked) {
            cycles+=128; // Descriptor update and coefficient construction.
            cycles+=write(cycles);
            cycles+=write(cycles);
            cycles+=10;
        }
        cycles+=47; // Descriptor loop, including its conditional jump.
    }
    return cycles+10;
}
} // namespace native_hall
