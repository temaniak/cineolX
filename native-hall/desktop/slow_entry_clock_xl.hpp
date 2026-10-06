#pragma once
#include "fast_control_timing_xl.hpp"

namespace cineol::xl {
// Slow input/trigger stage, ending before the separately timed gradual
// decay transition. Reads consume fresh hardware headroom; only the fast
// controller's software accumulator is combined with them for level encoding.
class SlowEntryClockXL {
public:
    enum class Kind:uint8_t {left,right,feedback,feedback_return,finished};
    struct Event {Kind kind;uint64_t work_state;};
    void reset(FastControlMemoryXL& memory,uint64_t entry,uint16_t monitor_peak,
        uint8_t special,uint8_t reset_period,uint8_t stop_delay,bool dynamic,bool optimization) noexcept {
        memory_=&memory;at_=entry+17;monitor_peak_=monitor_peak;special_=special;
        reset_period_=reset_period;stop_delay_=stop_delay;dynamic_=dynamic;optimization_=optimization;
        left_=observed_=display_=0;kind_=Kind::left;
    }
    Event next()const noexcept{return {kind_,at_};}
    uint8_t observed()const noexcept{return observed_;}
    uint8_t display()const noexcept{return display_;}
    uint16_t monitor_peak()const noexcept{return monitor_peak_;}
    void complete_read(uint8_t value) noexcept {
        if(kind_==Kind::left) {
            left_=value;memory_->headroom_left|=value;at_+=45;kind_=Kind::right;return;
        }
        if(kind_!=Kind::right)return;
        auto& s=memory_->dynamics;memory_->headroom_right|=value;
        const uint8_t bits=left_|value|memory_->headroom_accum;memory_->headroom_accum=0;
        at_+=55+313;auto word=Dynamics::detector_word(bits);
        if(!word){at_+=37;if((monitor_peak_>>8)>7){word=10;at_+=10;}}
        const auto encoded=level_work(word);observed_=encoded.level;
        at_+=17+encoded.work_states+53;
        display_=observed_;if(display_<188){display_=16;at_+=7;}
        at_+=43;monitor_peak_=0;
        if(special_&64){observed_=16;at_+=7;}
        at_+=37;
        if(observed_>=uint8_t(s.held>>8)) {
            s.held=uint16_t(observed_)<<8;s.stopped=0;at_+=78;
            if(s.amount) {
                at_+=22;
                if(observed_>=128) {
                    s.period=reset_period_;at_+=37;feedback_loop();return;
                }
            }
        }
        finish_trigger();
    }
    // Nested feedback compiler owns coefficient writes/READY waits. Retain
    // its return boundary before another amount step or trigger calculation.
    void complete_compiler(unsigned work,uint8_t period) noexcept {
        if(kind_!=Kind::feedback)return;
        at_+=work;memory_->dynamics.period=period;kind_=Kind::feedback_return;
    }
    void resume_after_compiler() noexcept {
        if(kind_!=Kind::feedback_return)return;
        at_+=10;feedback_loop();
    }
private:
    void feedback_loop() noexcept {
        auto& s=memory_->dynamics;at_+=31;
        if(!s.amount){at_+=10;finish_trigger();return;}
        --s.amount;at_+=20;
        if(s.amount){--s.amount;at_+=10;}
        at_+=17;kind_=Kind::feedback;
    }
    void finish_trigger() noexcept {
        auto& s=memory_->dynamics;
        s.average=uint8_t(s.average-(s.average>>3)+(display_>>3));at_+=109+48;
        bool triggered=display_>=s.average;
        if(triggered) {
            at_+=21;
            if(display_<=188){at_+=22;triggered=display_-s.average>=39;}
        }
        if(triggered) {
            s.trigger_peak=display_;at_+=48;
            if(s.flags&1){s.flags=192;s.stop_counter=stop_delay_;at_+=56;}
        } else {
            at_+=32;
            if(s.trigger_peak<display_){s.trigger_peak=display_;at_+=31;}
            at_+=35;
            if(uint8_t(s.trigger_peak-display_)>=39) {
                at_+=30;
                if(dynamic_) {
                    at_+=30;
                    if(!(s.flags&1)){s.flags=193;at_+=20;}
                }
            }
        }
        at_+=71;
        if(optimization_) {
            const uint8_t held=uint8_t(s.held>>8);at_+=27;
            if(uint8_t(held-observed_)>=22) {
                at_+=21;
                if(!s.stopped) {
                    s.stopped=1;at_+=46;
                    if(held>=s.history[0]) {
                        at_+=17;
                        if(held-s.history[0]>=24){s.period=15;at_+=15;}
                    }
                }
            }
            for(unsigned i=0;i<10;++i)s.history[i]=s.history[i+1];
            s.history[10]=held;at_+=456;
        }
        kind_=Kind::finished;
    }
    FastControlMemoryXL* memory_=nullptr;
    uint64_t at_=0;
    uint16_t monitor_peak_=0;
    uint8_t special_=0,reset_period_=0,stop_delay_=0,left_=0,observed_=0,display_=0;
    bool dynamic_=false,optimization_=false;
    Kind kind_=Kind::finished;
};
} // namespace cineol::xl
