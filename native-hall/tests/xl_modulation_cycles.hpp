#pragma once
// Historical local-oracle names retained while the common branch-cost law
// now lives in the native timing component. This wrapper supplies no clock.
#include "../desktop/control_timing_xl.hpp"
namespace xl_test {
using InterpolationWrite=cineol::xl::ControlByteXL;
template<class Write> unsigned modulation_cycles(const cineol::xl::ModulationProfile& profile,
    const cineol::xl::ModulationState& state,bool enabled,Write&& write) {
    return cineol::xl::modulation_work_cycles(profile,state,enabled,write);
}
}
