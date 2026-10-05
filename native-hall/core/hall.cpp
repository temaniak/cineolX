#include "hall.hpp"
#include <cmath>

namespace native_hall {
void Hall::reset() noexcept {
    memory_.fill(0);
    registers_.fill(0); output_.fill(0);
    position_=1; operand_=partial_=0; acc_=-1; rr_=0;
    previous_magnitude_=0; previous_negative_=false; first_=true;
    saturations_=0; mod_clock_=level_clock_=0;
    if(algorithm_) {
        c_=algorithm_->coefficients;offsets_=algorithm_->offsets;
        mod_=algorithm_->modulation_descriptors;mod_index_=algorithm_->modulation_index;
        mod_divider_=algorithm_->initial_mod_divider;random_divider_=algorithm_->initial_random_divider;
        random_hold_=algorithm_->initial_random_hold;decay_.reset(algorithm_->initial_decay);
    } else if(profile_) {
        c_=profile_->coefficients; offsets_=profile_->offsets;
        mod_=profile_->modulation_descriptors; mod_index_=profile_->modulation_index;
        mod_divider_=profile_->initial_mod_divider; random_divider_=profile_->initial_random_divider;
        random_hold_=profile_->initial_random_hold;
        decay_.reset(profile_->initial_decay);
    }
}
void Hall::select_program(unsigned program) noexcept {
    if(!bank_) return;
    algorithm_=&bank_->programs[std::min(program,program_count-1)];reset();set_controls(controls_);
}
void Hall::set_loop_diffusion(unsigned reduction) noexcept {
    if(algorithm_) {
        for(unsigned i=0;i<algorithm_->loop_count;++i) {
            unsigned r=algorithm_->loop_rows[i];int g=std::max(4,int(loop_base_[i])-int(reduction));
            c_[r]=int8_t(g);c_[r+1]=int8_t(32-g*g/32);c_[r+2]=int8_t(-g);
        }
        return;
    }
    int g=std::max(4,int(profile_->tail[controls_.bass][controls_.mid][4])-int(reduction));
    int complement=32-(g*g/32);
    for(unsigned r:{6u,10u,56u,60u}) {c_[r]=int8_t(g); c_[r+1]=int8_t(complement); c_[r+2]=int8_t(-g);}
}
void Hall::set_controls(const Controls& next) noexcept {
    const int requested_predelay=next.predelay_ms; // next may alias controls_ during selection.
    controls_=next;
    controls_.bass=std::clamp(next.bass,1,31); controls_.mid=std::clamp(next.mid,0,31);
    controls_.crossover=std::clamp(next.crossover,0,31); controls_.treble=std::clamp(next.treble,0,31);
    controls_.depth=std::clamp(next.depth,0,71); controls_.predelay_ms=std::clamp(requested_predelay,24,152);
    controls_.diffusion=std::clamp(next.diffusion,1,63);
    if(algorithm_) {
        const auto& p=*algorithm_;
        p.tail.apply(unsigned(controls_.bass*32+controls_.mid),c_);
        p.crossover.apply(unsigned(controls_.crossover),c_);p.treble.apply(unsigned(controls_.treble),c_);
        p.depth.apply(unsigned(controls_.depth),c_);p.diffusion.apply(unsigned(controls_.diffusion),c_);
        for(unsigned i=0;i<p.loop_count;++i) loop_base_[i]=c_[p.loop_rows[i]];
        set_loop_diffusion(decay_.state().amount);
        unsigned index=unsigned(std::clamp(requested_predelay-predelay_minima[algorithm_-bank_->programs.data()],0,128));
        controls_.predelay_ms=int(index)+predelay_minima[algorithm_-bank_->programs.data()];
        for(unsigned i=0;i<p.predelay_count;++i) offsets_[p.predelay_rows[i]]=p.predelay[index][i];
        return;
    }
    if(!profile_) return;
    const auto& t=profile_->tail[controls_.bass][controls_.mid];
    const unsigned tail_rows[]={37,38,87,88};
    for(unsigned j=0;j<4;++j) c_[tail_rows[j]]=t[j];
    set_loop_diffusion(decay_.state().amount);
    const unsigned cross_rows[]={35,36,85,86}, treble_rows[]={40,41,90,91};
    const unsigned depth_rows[]={42,43,44,45,92,93,94,95};
    for(unsigned j=0;j<4;++j) {
        c_[cross_rows[j]]=profile_->crossover[controls_.crossover][j];
        c_[treble_rows[j]]=profile_->treble[controls_.treble][j];
    }
    for(unsigned j=0;j<8;++j) c_[depth_rows[j]]=profile_->depth[controls_.depth][j];
    const auto& d=profile_->diffusion[controls_.diffusion];
    for(unsigned shift:{0u,50u}) {
        c_[28+shift]=d[0]; c_[29+shift]=d[1]; c_[30+shift]=int8_t(-d[0]);
        c_[32+shift]=d[2]; c_[33+shift]=d[3]; c_[34+shift]=int8_t(-d[2]);
    }
    // 20 delay words per displayed ms, as in the original 10CA compiler.
    // v0.1 applies changes at a sample boundary; firmware's retiming ramp is
    // deliberately not claimed to be reproduced by this prototype.
    offsets_[39]=uint16_t(profile_->offsets[39]+20*(controls_.predelay_ms-24))&0x3fff;
    offsets_[89]=uint16_t(profile_->offsets[89]+20*(controls_.predelay_ms-24))&0x3fff;
}
void Hall::process(int16_t left,int16_t right,int16_t outputs[4],unsigned detectors) noexcept {
    process_uncontrolled(left,right,outputs);
    if(!profile_ && !algorithm_) return;
    decay_.observe(left,right,detectors);update_decay();
    // Per-program/mode nominal ROM clocks, measured offline. Signal-dependent
    // CPU timing remains an approximation. Legacy Hall retains its 1.067 kHz.
    unsigned mode=unsigned(controls_.mode_enhancement)|unsigned(controls_.decay_optimization)<<1;
    mod_clock_+=algorithm_?algorithm_->modulation_rate_tenths[mode]:10670;
    if(mod_clock_>=sample_rate*10) {mod_clock_-=sample_rate*10;if(controls_.mode_enhancement) update_modulation();}
}
void Hall::process_uncontrolled(int16_t left,int16_t right,int16_t outputs[4]) noexcept {
    if(!profile_ && !algorithm_) {std::fill_n(outputs,4,int16_t(0));return;}
    if(first_) first_=false; else ++position_;
    const unsigned network=algorithm_?algorithm_->network:0;
    #include "program_networks.inc"
    std::copy(output_.begin(),output_.end(),outputs);
}
void Hall::update_modulation() noexcept {
    if(--random_divider_==0) {
        random_divider_=8;
        if(--random_hold_==0) {random_hold_=algorithm_?algorithm_->modulation_hold:profile_->modulation_hold; mod_index_=(mod_index_+1)&4095;}
    }
    if(--mod_divider_!=0) return;
    mod_divider_=algorithm_?algorithm_->modulation_period:profile_->modulation_period;
    uint8_t random=bank_?bank_->modulation_sequence[mod_index_]:profile_->modulation_sequence[mod_index_];
    // v4.4's 0x82 descriptor: the second tap walks in the opposite direction.
    const unsigned flags=algorithm_?algorithm_->modulation_flags:0x82;
    const unsigned count=flags&15;
    for(unsigned j=0;j<count;++j) {
        if(!algorithm_ || algorithm_->identity!=8) {
            unsigned descriptor=flags-j;
            if((descriptor&128) && (descriptor&1)) random^=1;
            else random=uint8_t((random>>1)|(random<<7));
        }
        unsigned at=j*5;
        unsigned address=unsigned(mod_[at]) | unsigned(mod_[at+1])<<8;
        const unsigned row=127-((address-0x4000)/4);
        uint8_t cap=mod_[at+2], cached=mod_[at+3], phase=mod_[at+4];
        bool up=(random&1)!=0;
        int value=int(phase);
        bool advance=false; int delta=0;
        const int step=algorithm_?algorithm_->modulation_step:profile_->modulation_step;
        if(up && !(phase&1)) {value+=step; advance=value>255; delta=2;}
        if(up && (phase&1)) {value-=step; advance=uint8_t(value)<cap; delta=1;}
        if(!up && (phase&1)) {value+=step; advance=value>255; delta=-2;}
        if(!up && !(phase&1)) {value-=step; advance=uint8_t(value)<cap; delta=-1;}
        if(advance) {
            uint8_t candidate=uint8_t(int(cached)+delta);
            // ROM 0D47: do not cross a boundary marked by the phase's bits.
            if((delta>0 && int(cached)+delta>255) || (delta<0 && int(cached)+delta<0) ||
               ((candidate^cached)&(algorithm_?algorithm_->modulation_mask:profile_->modulation_mask))) {
                random_hold_=random_divider_=1;
                continue;
            }
            if(delta==2 || delta==-2) mod_[at+3]=candidate;
            // Address bytes are not complemented in the CPU WCS image.
            unsigned moved=(delta==2 || delta==-2)?row:row+1;
            offsets_[moved]=uint16_t((offsets_[moved]&0x3f00)|candidate);
            mod_[at+4]=phase^1;
            // As in 0D20, address movement is followed by a fractional step.
            phase=mod_[at+4];
            value=int(phase)+(((up && !(phase&1)) || (!up && (phase&1)))?step:-step);
        }
        phase=uint8_t(value); mod_[at+4]=phase;
        uint8_t first=uint8_t((phase&0xfc)|(cap&3));
        uint8_t second=uint8_t(((cap|3)-4-first)|3);
        c_[row]=int8_t((~first&255)>>2);
        c_[row+1]=int8_t((~second&255)>>2);
    }
}
void Hall::update_decay() noexcept {
    unsigned mode=unsigned(controls_.mode_enhancement) | unsigned(controls_.decay_optimization)<<1;
    level_clock_+=algorithm_?algorithm_->level_rate_tenths[mode]:profile_->level_rate_tenths[mode];
    if(level_clock_<sample_rate*10) return;
    level_clock_-=sample_rate*10;
    if(decay_.poll(uint8_t((controls_.bass+controls_.mid)/2),controls_.decay_optimization,algorithm_?algorithm_->decay_amount:profile_->decay_amount))
        set_loop_diffusion(decay_.state().amount);
}
} // namespace native_hall
