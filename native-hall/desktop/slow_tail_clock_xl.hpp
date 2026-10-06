#pragma once
#include "fast_control_timing_xl.hpp"
#include "decay_compiler_xl.hpp"

namespace cineol::xl {
struct SlowTailMemoryXL {
    uint8_t reconcile=0,menu_flags=0,dip=0;
    std::array<uint8_t,2> timers{};
};
// Post-ramp slow stage through the panel-service dispatch boundary. Nested
// compiler/display/menu routines run in their own clocks. No deadline or
// coefficient write is folded into an instantaneous dynamics tick.
class SlowTailClockXL {
public:
    enum class Kind:uint8_t {parameters,feedback,compiler_return,display,dip,configure,finished};
    struct Event {Kind kind;uint64_t work_state;};
    void reset(FastControlMemoryXL& memory,SlowTailMemoryXL& tail,uint64_t entry,uint16_t decay_time) noexcept {
        memory_=&memory;tail_=&tail;at_=entry;decay_time_=decay_time;
        at_+=31;
        if(tail.reconcile) {
            --tail.reconcile;at_+=12;
            if(!tail.reconcile){at_+=17;kind_=Kind::parameters;return;}
            at_+=11;
        }
        period();
    }
    Event next()const noexcept{return {kind_,at_};}
    void complete_compiler(unsigned work,uint8_t compiled_period,uint16_t decay_time) noexcept {
        if(kind_!=Kind::parameters && kind_!=Kind::feedback)return;
        compiler_=kind_;at_+=work;memory_->dynamics.period=compiled_period;
        decay_time_=decay_time;kind_=Kind::compiler_return;
    }
    void resume_after_compiler() noexcept {
        if(kind_!=Kind::compiler_return)return;
        if(compiler_==Kind::parameters)period();
        else {if(!memory_->dynamics.stopped)at_+=10;display();}
    }
    void complete_display(unsigned work) noexcept {
        if(kind_!=Kind::display)return;
        at_+=work;memory_->headroom_left=memory_->headroom_right=0;
        auto& s=memory_->dynamics;at_+=60;
        if(--s.peak_divider){finish();return;}
        s.peak_divider=8;at_+=40;
        if(s.peak_input!=32) {
            const auto division=decay_compiler_detail::divide(16,uint16_t(decay_time_<<4));
            unsigned release=division.quotient;at_+=93+division.work+19;
            if(!release){release=1;at_+=5;}
            at_+=81;
            if(s.held<release){s.held=0;at_+=10;}else s.held=uint16_t(s.held-release);
            at_+=50;
            if(s.trigger_peak<3){s.trigger_peak=0;at_+=7;}else s.trigger_peak=uint8_t(s.trigger_peak-3);
            at_+=7;
        }
        for(unsigned i=0;i<2;++i) {
            at_+=31;
            if(!tail_->timers[i])continue;
            --tail_->timers[i];at_+=20;
            if(!tail_->timers[i]){tail_->menu_flags|=i?1:32;at_+=41;}
        }
        at_+=29;kind_=Kind::dip;
    }
    void complete_dip(uint8_t byte) noexcept {
        if(kind_!=Kind::dip)return;
        const uint8_t value=uint8_t(~byte)&15;at_+=13+38;
        if(value==tail_->dip){finish();return;}
        tail_->dip=value;at_+=21;
        if(!value){finish();return;}
        at_+=27;kind_=Kind::configure;
    }
    void complete_configure(unsigned work) noexcept {
        if(kind_!=Kind::configure)return;
        at_+=work;finish();
    }
private:
    void period() noexcept {
        auto& s=memory_->dynamics;at_+=40;
        if(s.peak_input!=32) {
            at_+=27;
            bool slow=decay_time_>>8;
            if(!slow){at_+=30;slow=uint8_t(decay_time_)>=120;}
            if(slow){s.period=20;at_+=10;}
        }
        at_+=32;
        if(--s.divider){display();return;}
        s.divider=s.period;at_+=43;
        if(!s.stopped) {
            at_+=21;
            if(!s.amount){display();return;}
            --s.amount;at_+=10+17;kind_=Kind::feedback;return;
        }
        at_+=37;
        if(s.peak_input!=32) {
            at_+=17;
            if(s.amount>=12){display();return;}
        }
        at_+=17;
        if(s.amount>=20){display();return;}
        ++s.amount;at_+=5+7+17;kind_=Kind::feedback;
    }
    void display() noexcept {at_+=17;kind_=Kind::display;}
    void finish() noexcept {kind_=Kind::finished;}
    FastControlMemoryXL* memory_=nullptr;
    SlowTailMemoryXL* tail_=nullptr;
    uint64_t at_=0;
    uint16_t decay_time_=0;
    Kind kind_=Kind::finished,compiler_=Kind::parameters;
};
} // namespace cineol::xl
