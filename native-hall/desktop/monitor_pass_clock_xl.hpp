#pragma once
#include <cstdint>

namespace cineol::xl {
// Resumable native Mod/monitor work clock. An audio engine can run rows up
// to the next event, supply a freshly sampled port or completed control-law
// work duration, then continue. No future input or reference event is stored.
// Work states include READY waits supplied by the native nested controllers;
// the surrounding scan separately accounts for serial IRQ service.
class MonitorPassClockXL {
public:
    enum class Kind:uint8_t {mod_first,auxiliary,monitor_high,monitor_low,mod_second,finished};
    struct Event {Kind kind;uint64_t work_state;};
    void reset(uint64_t entry,unsigned descriptors,uint16_t peak) noexcept {
        at_=entry+17;count_=uint8_t(descriptors&15);peak_=peak;
        magnitude_=0;kind_=Kind::mod_first;
    }
    Event next()const noexcept{return {kind_,at_};}
    uint16_t peak()const noexcept{return peak_;}
    // Complete exactly the currently pending event. For a control routine
    // value is its work duration; for a read it is the sampled byte.
    void complete(unsigned value) noexcept {
        switch(kind_) {
        case Kind::mod_first:
            at_+=uint64_t(value)+37+(count_<2?(count_?502:1112):0)+17;
            kind_=Kind::auxiliary;break;
        case Kind::auxiliary:
            at_+=value;kind_=Kind::monitor_high;break;
        case Kind::monitor_high: {
            const uint8_t byte=uint8_t(value);
            const unsigned high=(byte&128)?uint8_t(~byte):byte;
            at_+=10+4+10+((byte&128)?8:0)+5+7+10;
            magnitude_=uint16_t(high<<8);
            if(!high)kind_=Kind::monitor_low;else finish_monitor();
            break;
        }
        case Kind::monitor_low: {
            const uint8_t byte=uint8_t(value);
            magnitude_|=(byte&128)?uint8_t(~byte):byte;
            at_+=10+4+10+((byte&128)?4:0)+5;
            finish_monitor();break;
        }
        case Kind::mod_second:
            at_+=uint64_t(value)+10;kind_=Kind::finished;break;
        case Kind::finished:break;
        }
    }
private:
    void finish_monitor() noexcept {
        at_+=16+17+38+10;
        if(magnitude_>peak_){peak_=magnitude_;at_+=20;}
        at_+=13+7+7;
        if(count_<3){at_+=17;kind_=Kind::mod_second;}
        else {at_+=11+10;kind_=Kind::finished;}
    }
    uint64_t at_=0;
    uint16_t peak_=0,magnitude_=0;
    uint8_t count_=0;
    Kind kind_=Kind::finished;
};
} // namespace cineol::xl
