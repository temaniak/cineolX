#pragma once
#include "dynamics.hpp"

namespace cineol::xl {
struct FaderPickupMemoryXL {uint8_t logical=0,pickup=1;};
struct FaderPickupWorkXL {unsigned work_states;bool accepted;};
// The enclosing calibration supplies a clamped logical fader value. A live
// fader accepts it; an unlatched fader accepts equality or crossing the stored
// value. No phase, detector history or controller clock is reset here.
constexpr FaderPickupWorkXL fader_pickup_work(FaderPickupMemoryXL& memory,
    uint8_t value) noexcept {
    unsigned work=29;
    bool accepted=memory.pickup==1;
    if(!accepted) {
        work+=29;
        if(value==memory.logical)accepted=true;
        else {
            work+=10+7+10;
            const bool waiting=value<memory.logical?memory.pickup==4:memory.pickup==2;
            if(waiting)return {work+4+10,false};
            accepted=true;
        }
    }
    memory.logical=value;memory.pickup=1;
    return {work+7+7+7+4+10,accepted};
}
// Normal fader worker suffix, after calibration and any nested compilation/
// record copy. It requests deferred LF/MID refresh and retains other state.
constexpr unsigned finish_fader_transaction(DynamicsState& state) noexcept {
    state.flags=192;
    return 7+13+10+10;
}
} // namespace cineol::xl
