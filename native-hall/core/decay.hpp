#pragma once
#include <array>
#include <algorithm>
#include <cstdint>

namespace native_hall {
// RAM 3E32..3E42, in address order. The v4.4 0755..0897 controller
// measures a logarithmic level, releases its held peak, and changes the
// diffusion reduction through a separate divider. All arithmetic is 8 bit.
struct DecayState {
    uint8_t peak_divider=8,held=31,stopped=0,amount=0;
    uint8_t diffusion_divider=5,diffusion_period=5;
    std::array<uint8_t,11> history{};
};
static_assert(sizeof(DecayState)==17,"ROM decay-state layout changed");
class DecayController {
public:
    void reset(const DecayState& state) noexcept {state_=state;transfer_peak_=0;detectors_=0;}
    const DecayState& state() const noexcept {return state_;}
    void observe(int16_t left,int16_t right,unsigned detectors) noexcept {
        transfer_peak_=std::max({transfer_peak_,transfer_magnitude(left),transfer_magnitude(right)});
        detectors_|=uint8_t(detectors&31);
    }
    bool poll(uint8_t mean,bool enabled,uint8_t period) noexcept {
        uint8_t level=level_from_word(uint16_t(transfer_peak_|uint16_t(detectors_)<<11));
        transfer_peak_=0;detectors_=0;
        return step(level,mean,enabled,period);
    }
    // Diagnostic entry: observed level from the actual ROM's 0779 boundary.
    bool step(uint8_t level,uint8_t mean,bool enabled,uint8_t period) noexcept {
        auto& s=state_;uint8_t before=s.amount,c=s.held;
        if(level>=c) {
            s.held=level;s.stopped=0;
            if(s.amount && level>=128) {
                s.amount=s.peak_divider=s.diffusion_divider=1;s.diffusion_period=period;
            }
        }
        bool skip_peak=false;
        if(enabled) {
            c=s.held;
            uint8_t drop=uint8_t(c-level);
            // JM after CPI 16: preserve the 8080's sign test, including wrap.
            if(!(uint8_t(drop-22)&128) && !s.stopped) {
                s.stopped=1;
                uint8_t history_drop=uint8_t(c-s.history[0]);
                if((history_drop&128) || history_drop<24) skip_peak=true;
                else s.diffusion_period=15;
            }
            if(!skip_peak) {
                for(unsigned i=0;i+1<s.history.size();++i) s.history[i]=s.history[i+1];
                s.history.back()=c;
            }
        }
        if(!skip_peak && --s.peak_divider==0) {
            s.peak_divider=8;
            // 084E..086C releases the held peak. Without this, optimization
            // stays latched and permanently alters the tail (the v0.1 bug).
            unsigned release=mean?64u/mean:255u;
            s.held=uint8_t(c-std::max(1u,release));
        }
        if(mean>=25) s.diffusion_period=20;
        if(--s.diffusion_divider==0) {
            s.diffusion_divider=s.diffusion_period;
            if(!s.stopped) {if(s.amount) --s.amount;}
            else if(s.amount<12) ++s.amount;
        }
        return before!=s.amount;
    }
    static uint16_t transfer_magnitude(int16_t value) noexcept {
        // IN 07/06 sees the inverted XREG bus; 018F complements signed
        // bytes independently and reads low only when high is zero.
        uint16_t inverted=uint16_t(~uint16_t(value));
        uint8_t high=uint8_t(inverted>>8);if(high&128) high^=255;
        if(high) return uint16_t(high)<<8;
        uint8_t low=uint8_t(inverted);if(low&128) low^=255;
        return low;
    }
    static uint8_t level_from_word(uint16_t value) noexcept {
        value|=2;uint8_t level=15;
        for(;;) {level=uint8_t(level-16);bool carry=value&0x8000;value=uint16_t(value<<1);if(carry) return level;}
    }
private:
    DecayState state_{};
    uint16_t transfer_peak_=0;
    uint8_t detectors_=0;
};
} // namespace native_hall
