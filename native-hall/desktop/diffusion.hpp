#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace cineol::xl {
// Offline-prepared targets for the v8.21 B000..B0AD allpass compiler.
// Targets and their scale/cap bytes are read from the user's ROM, never
// embedded in the graph. The controller only changes coefficient magnitudes.
struct DiffusionProfile {
    static constexpr unsigned max_targets=15;
    struct Target {
        uint8_t row=0,scale_cap=0;
        std::array<bool,3> negative{};
    };
    std::array<Target,max_targets> targets{};
    uint8_t count=0;
    bool separate_stop=false,half_scale=false;
    bool valid(unsigned rows) const noexcept {
        if(count>max_targets) return false;
        for(unsigned i=0;i<count;++i) if(unsigned(targets[i].row)+2>=rows) return false;
        return true;
    }
    static uint8_t feedback_index(uint8_t decay_raw,uint8_t definition) noexcept {
        // Feedback allpasses follow normal/stop MID decay. Definition caps
        // their 32-position indices; inputs are stored record bytes after
        // the firmware's parameter-range limits. The independent Diffusion
        // fader uses the separate 64-position compiler below.
        return uint8_t(std::min(std::max(1u,unsigned(decay_raw)>>3),31u-(unsigned(definition)>>3)));
    }
    static uint8_t diffusion_index(uint8_t raw) noexcept {return raw>>2;}
    template<class Settings>
    void apply(Settings& settings,uint8_t normal_index,uint8_t stop_index,
               uint8_t reduction=1) const noexcept {
        for(unsigned i=0;i<count;++i) {
            const auto& target=targets[i];
            // The firmware switches indices after completing the first target
            // with three remaining, so only the final two use stop decay.
            const unsigned value=separate_stop && count-i<=2?stop_index:normal_index;
            unsigned scaled=(unsigned(target.scale_cap&224)*uint8_t(value*2))>>8;
            if(half_scale) scaled>>=1;
            const unsigned capped=std::min(unsigned(target.scale_cap&31),scaled);
            const unsigned outer=capped>reduction?capped-reduction:0;
            const unsigned middle=32-((outer*outer*8+128)>>8);
            const unsigned magnitudes[3]={outer,middle,outer};
            for(unsigned j=0;j<3;++j) settings.coefficients[target.row+j]=
                int8_t(target.negative[j]?-int(magnitudes[j]):int(magnitudes[j]));
        }
    }
};
} // namespace cineol::xl
