#pragma once
#include "fader_transaction_xl.hpp"
#include <algorithm>
#include <cassert>

namespace cineol::xl {
struct FaderCalibrationMemoryXL {
    FaderPickupMemoryXL pickup{};
    uint8_t limit=0,physical=0;
};
struct FaderCalibrationWorkXL {unsigned work_states=0;bool accepted=false;};
// Ordinary numeric pages. Size pairs, variable pre-delay and effect-specific
// mappings have separate workers and must not use this partial calibration.
constexpr bool ordinary_fader_calibration(uint8_t type,uint8_t slot,
    uint8_t maximum) noexcept {
    if(type==5 || type==13)return false;
    const bool scaled=type==10 || ((type==1 || type==2) && slot==5);
    return !scaled || maximum!=255;
}
constexpr FaderCalibrationWorkXL calibrate_fader_work(FaderCalibrationMemoryXL& memory,
    uint8_t type,uint8_t slot,uint8_t maximum,uint8_t physical) noexcept {
    assert(ordinary_fader_calibration(type,slot,maximum));
    memory.limit=maximum;memory.physical=physical;
    unsigned work=146;
    bool minimum=false,scaled=false;
    // Page dispatch, followed by the normal page's slot dispatch.
    if(type==1 || type==2) {
        work+=type==1?17:34;
        work+=5+7+10;
        if(slot==5)scaled=true;
        else {
            work+=7+10;
            if(slot==2)minimum=true;
            else {work+=7+10;minimum=slot==3;}
        }
    } else if(type==3) {
        work+=51+5+7+10;
        if(slot==3){work+=10;minimum=true;}
    } else if(type==9) {work+=85;minimum=true;}
    else if(type==10) {work+=102;scaled=true;}
    else work+=119;
    if(minimum) {
        work+=7+4+10;
        if(memory.physical<8){memory.physical=8;work+=7+10;}
    }
    if(scaled) {
        work+=13+7+10+7+4+4+4+13;
        memory.limit=uint8_t((maximum&31)*8);
        work+=4+10;
        if(memory.limit>=memory.physical)work+=5;
        memory.physical=std::min(memory.limit,memory.physical);
        work+=5+11+17;
        const auto result=fader_pickup_work(memory.pickup,memory.physical);
        return {work+result.work_states+10+10,result.accepted};
    }
    work+=13+4+10;
    if(memory.limit>=memory.physical)work+=5;
    const auto result=fader_pickup_work(memory.pickup,std::min(memory.limit,memory.physical));
    return {work+17+result.work_states+10,result.accepted};
}
} // namespace cineol::xl
