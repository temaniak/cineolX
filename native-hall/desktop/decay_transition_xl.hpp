#pragma once
#include "dynamics.hpp"

namespace cineol::xl {
// Resumable LF/MID transition. The caller owns coefficient compilation and
// elapsed wall time; this law owns the gradual control steps between calls.
// Serialized DynamicsState stays unchanged. No instructions or ROM data are
// interpreted here, and no measured compiler duration is stored in this law.
class DecayTransitionXL {
public:
    enum class Kind:uint8_t {compile,compiler_return,finished};
    struct Event {Kind kind;uint64_t work_state;};
    void reset(DynamicsState& state,uint64_t entry,bool shared_stop,
        const std::array<uint8_t,48>& raw) noexcept {
        state_=&state;at_=entry;shared_stop_=shared_stop;
        normal_low_=raw[0];normal_mid_=raw[1];
        stop_low_=raw[shared_stop?6:12];stop_mid_=raw[shared_stop?7:13];
        advance();
    }
    Event next() const noexcept {return {kind_,at_};}
    // The enclosing compiler updates its own control/cache fields before
    // completing. Keep return observable: the next step must not appear at
    // the preceding compiler's entry, or before its writes finish.
    void complete_compiler(unsigned work_states) noexcept {
        if(kind_!=Kind::compile)return;
        at_+=work_states;kind_=Kind::compiler_return;
    }
    void resume_after_compiler() noexcept {
        if(kind_!=Kind::compiler_return)return;
        at_+=30;
        if(state_->flags&0xc0)advance();
        else {at_+=10;kind_=Kind::finished;}
    }
private:
    // Return true while another eight-position step remains. An upward
    // transition installs the requested normal value in one compile; a
    // downward transition first quantizes, then subtracts eight. The last
    // downward step can sit below an unaligned stop value (e.g. 18 -> 16).
    bool step(uint8_t& value,uint8_t requested) noexcept {
        uint8_t quantized=value&248;at_+=7+4+10;
        if(requested>=quantized) {value=requested;at_+=7+4+10;return false;}
        at_+=7+4+10+5+7+10;
        quantized=uint8_t(quantized-8);value=quantized;
        at_+=5+7+4+7+10;
        return (uint8_t(requested-quantized)&248)!=0;
    }
    bool delay() noexcept {
        at_+=10+7+4+10;
        if(!state_->stop_counter)return false;
        --state_->stop_counter;at_+=10+10;kind_=Kind::finished;return true;
    }
    void advance() noexcept {
        auto& s=*state_;uint8_t flags=s.flags;
        at_+=13+5+7+10;
        if(!(flags&0xc0)) {kind_=Kind::finished;return;}
        at_+=7+10;
        if(flags&128) {
            at_+=13+7+5+5+4+10;
            uint8_t requested=normal_mid_;
            if(flags&1) {
                if(delay())return;
                at_+=13+7+10+10;
                if(!shared_stop_)at_+=10+10;
                requested=stop_mid_;
            } else at_+=10;
            at_+=17;
            const bool more=step(s.mid,requested);at_+=5+13+5+10;
            if(!more) {flags&=127;at_+=7+5;}
        }
        at_+=5+7+5+10;
        if(flags&64) {
            at_+=13+7+5+5+4+10;
            uint8_t requested=normal_low_;
            if(flags&1) {
                if(delay())return;
                at_+=10+13+7+10;
                if(!shared_stop_)at_+=10+10;
                requested=stop_low_;
            } else at_+=10;
            at_+=17;
            const bool more=step(s.low,requested);at_+=5+13+5+10;
            if(!more) {flags&=191;at_+=7;}
        }
        s.flags=flags;at_+=13+17;kind_=Kind::compile;
    }
    DynamicsState* state_=nullptr;
    uint64_t at_=0;
    uint8_t normal_low_=0,normal_mid_=0,stop_low_=0,stop_mid_=0;
    bool shared_stop_=true;
    Kind kind_=Kind::finished;
};
} // namespace cineol::xl
