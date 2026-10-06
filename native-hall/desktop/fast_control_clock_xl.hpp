#pragma once
#include "fast_control_timing_xl.hpp"

namespace cineol::xl {
// Resumable fast controller. Detector reads and individual coefficient
// compilations are yielded in order; the enclosing native scan owns wall
// time, IRQs and DSP-row/commit advancement. Memory is retained by the caller.
class FastControlClockXL {
public:
    enum class Kind:uint8_t {left,right,feedback,restore,feedback_return,restore_return,finished};
    struct Event {Kind kind;uint64_t work_state;};
    void reset(FastControlMemoryXL& memory,uint64_t entry,uint8_t special,uint8_t reset_period,
        const std::array<uint8_t,48>& raw) noexcept {
        memory_=&memory;at_=entry;reset_period_=reset_period;
        low_=raw[0];mid_=raw[1];stop_delay_=raw[45];left_=observed_=0;
        if(special&64){at_+=31;kind_=Kind::finished;return;}
        at_+=81;
        if(!state().amount && !state().flags){at_+=20;kind_=Kind::finished;return;}
        at_+=58;kind_=Kind::left;
    }
    Event next()const noexcept{return {kind_,at_};}
    void complete_read(uint8_t value) noexcept {
        if(kind_==Kind::left){left_=value;at_+=24;kind_=Kind::right;return;}
        if(kind_!=Kind::right)return;
        auto& s=state();memory_->headroom_left|=left_;memory_->headroom_right|=value;
        const uint8_t bits=left_|value;memory_->headroom_accum|=bits;
        const uint8_t previous=s.peak_input;s.peak_input=bits;at_+=131;
        if(previous>=bits){finish();return;}
        at_+=14;
        if(!bits){finish();return;}
        at_+=5+7+8*(10+4+5+10)+5+13+5+17;
        const auto encoded=level_work(Dynamics::detector_word(bits));at_+=encoded.work_states;observed_=encoded.level;
        at_+=43;
        if(s.amount) {
            at_+=19;
            if(observed_>=uint8_t(s.held>>8)) {
                s.held=uint16_t(uint8_t(observed_-uint8_t(s.held>>8)))<<8;s.stopped=0;
                at_+=83;s.period=reset_period_;next_feedback();return;
            }
        }
        trigger();
    }
    // Period is produced by the native coefficient compiler/control profile.
    // Its duration includes native READY waits, excluding serial IRQ states.
    void complete_compiler(unsigned work,uint8_t period) noexcept {
        auto& s=state();
        if(kind_==Kind::feedback) {
            at_+=work;s.period=period;s.feedback_mid=s.mid;s.feedback_amount=s.amount;
            kind_=Kind::feedback_return;
        } else if(kind_==Kind::restore) {
            at_+=work;s.amount=1;s.peak_input=uint8_t(ControlProfile::decay_index(s.mid));s.period=period;
            s.feedback_mid=s.mid;s.feedback_amount=s.amount;kind_=Kind::restore_return;
        }
    }
    // Keep the compiler's return boundary observable before executing the
    // following native branch or beginning another coefficient compilation.
    void resume_after_compiler() noexcept {
        if(kind_==Kind::feedback_return){at_+=10;next_feedback();}
        else if(kind_==Kind::restore_return){state().flags=0;at_+=27;finish();}
    }
private:
    DynamicsState& state() noexcept{return memory_->dynamics;}
    void finish() noexcept {at_+=40;kind_=Kind::finished;}
    void next_feedback() noexcept {
        auto& s=state();at_+=31;
        if(!s.amount){trigger();return;}
        --s.amount;at_+=20;
        if(s.amount){--s.amount;at_+=10;}
        at_+=17;kind_=Kind::feedback;
    }
    void trigger() noexcept {
        auto& s=state();at_+=37;
        if(!s.flags){finish();return;}
        at_+=47;
        if(observed_<s.average){finish();return;}
        at_+=21;
        if(observed_<=188){at_+=22;if(observed_-s.average<39){finish();return;}}
        s.trigger_peak=observed_;s.stop_counter=stop_delay_;at_+=87;
        if(s.mid>=mid_){finish();return;}
        s.low=low_;s.mid=mid_;at_+=33;kind_=Kind::restore;
    }
    FastControlMemoryXL* memory_=nullptr;
    uint64_t at_=0;
    uint8_t reset_period_=0,low_=0,mid_=0,stop_delay_=0,left_=0,observed_=0;
    Kind kind_=Kind::finished;
};
} // namespace cineol::xl
