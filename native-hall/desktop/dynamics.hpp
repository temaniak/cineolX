#pragma once
#include "controls.hpp"
#include "../core/decay.hpp"
#include <cmath>

namespace cineol::xl {
struct DynamicsProfile {
    uint32_t slow_rate_tenths=1,fast_rate_tenths=1;
    uint8_t base_period=9;
    bool shared_stop=true,enabled=true;
    bool valid() const noexcept {
        // A zero period is the firmware's 256-tick divider (CD Plates).
        return slow_rate_tenths && slow_rate_tenths<100000 &&
            fast_rate_tenths && fast_rate_tenths<100000;
    }
};
struct DynamicsState {
    uint16_t held=23*256;
    uint8_t average=16,flags=0,low=0,mid=0,trigger_peak=16,stop_counter=0;
    uint8_t stopped=0,amount=0,divider=1,period=1,peak_divider=8,peak_input=0;
    std::array<uint8_t,11> history{};
    uint8_t feedback_mid=0,feedback_amount=0;
};
// Native level-following controller. Its state and prepared coefficients are
// separate from the graph; neither ROM instructions nor a CPU run in audio.
class Dynamics {
public:
    void reset(const DynamicsState& state) noexcept {state_=state;detectors_=fast_detectors_=0;transfer_peak_=0;}
    const DynamicsState& state() const noexcept {return state_;}
    void observe(float left,float right,int16_t transfer) noexcept {
        const float peak=std::max(std::abs(left),std::abs(right));
        constexpr float thresholds[]={0.056f,0.112f,0.224f,0.448f,1.0f};
        uint8_t bits=0;for(unsigned i=0;i<5;++i) if(peak>=thresholds[i]) bits|=uint8_t(1u<<i);
        // The SBC's input bus reverses the five headroom bits.
        const uint8_t reversed=uint8_t((bits&1)<<4|(bits&2)<<2|(bits&4)|(bits&8)>>2|(bits&16)>>4);
        detectors_|=reversed;fast_detectors_|=reversed;
        transfer_peak_=std::max(transfer_peak_,native_hall::DecayController::transfer_magnitude(transfer));
    }
    static uint8_t level(uint16_t word) noexcept {
        word|=2;uint8_t result=7;
        for(;;) {
            result=uint8_t(result-16);const bool carry=word&0x8000;word=uint16_t(word<<1);
            if(carry) {if(word&0x8000) result=uint8_t(result+8);return result;}
        }
    }
    static uint16_t detector_word(uint8_t bits) noexcept {
        uint8_t reverse=0;for(unsigned i=0;i<8;++i) reverse|=uint8_t(((bits>>i)&1)<<(7-i));
        return uint16_t(reverse)<<8;
    }
    static uint8_t normal_period(const DynamicsProfile& p,const ControlProfile& controls,
        const std::array<uint8_t,48>& raw,uint8_t marker) noexcept {
        if(!controls.size.enabled) return p.base_period;
        const unsigned scale=unsigned(controls.layout(raw).scales[0])*(marker==32?4u:1u);
        return uint8_t(std::clamp((scale*p.base_period)>>8,1u,255u));
    }
    void parameters(const DynamicsProfile& p,const ControlProfile& controls,const std::array<uint8_t,48>& raw,
        bool dynamic,bool optimization,bool update_decay=true) noexcept {
        if(!dynamic) state_.flags=0;
        if(update_decay && !(state_.flags&0xc0)) {
            state_.low=raw[(state_.flags&1)?(p.shared_stop?6:12):0];
            state_.mid=raw[(state_.flags&1)?(p.shared_stop?7:13):1];
            state_.peak_input=uint8_t(ControlProfile::decay_index(state_.mid));
            state_.period=normal_period(p,controls,raw,state_.peak_input);
        }
        if(!optimization) {state_.amount=0;state_.stopped=0;}
        // Parameter transactions prepare a complete coherent control snapshot.
        if(update_decay || (!optimization && state_.feedback_amount)) update_feedback();
    }
    bool fast_poll(const DynamicsProfile& p,const ControlProfile& controls,const std::array<uint8_t,48>& raw) noexcept {
        const uint8_t bits=fast_detectors_;fast_detectors_=0;
        return fast_step(bits,p,controls,raw);
    }
    bool slow_poll(const DynamicsProfile& p,const ControlProfile& controls,const std::array<uint8_t,48>& raw,
        bool dynamic,bool optimization) noexcept {
        auto word=detector_word(detectors_);detectors_=0;fast_detectors_=0;
        if(!word && (transfer_peak_>>8)>7) word=10;
        transfer_peak_=0;
        const uint8_t observed=level(word),display=observed>=188?observed:16;
        return slow_step(state_.mid>=253?16:observed,display,p,controls,raw,dynamic,optimization);
    }
    bool fast_step(uint8_t bits,const DynamicsProfile& p,const ControlProfile& controls,
        const std::array<uint8_t,48>& raw) noexcept {
        auto& s=state_;const auto before=s;
        if(s.mid>=253 || (!s.amount && !s.flags)) return false;
        const uint8_t previous=s.peak_input;s.peak_input=bits;
        if(bits>previous && bits) {
            const uint8_t observed=level(detector_word(bits));
            if(s.amount && observed>=uint8_t(s.held>>8)) {
                s.held=uint16_t(uint8_t(observed-uint8_t(s.held>>8)))<<8;s.stopped=0;s.amount=0;
                update_feedback();
                s.period=normal_period(p,controls,raw,s.peak_input);
            }
            if(s.flags && observed>=s.average && (observed>188 || observed-s.average>=39)) {
                s.trigger_peak=observed;s.stop_counter=raw[45];
                if(s.mid<raw[1]) {
                    s.low=raw[0];s.mid=raw[1];s.flags=0;
                    s.amount=1;
                    s.peak_input=uint8_t(ControlProfile::decay_index(s.mid));
                    s.period=normal_period(p,controls,raw,s.peak_input);
                }
            }
        }
        return changed(before);
    }
    bool slow_step(uint8_t observed,uint8_t display,const DynamicsProfile& p,const ControlProfile& controls,
        const std::array<uint8_t,48>& raw,bool dynamic,bool optimization) noexcept {
        auto& s=state_;const auto before=s;
        if(observed>=uint8_t(s.held>>8)) {
            s.held=uint16_t(observed)<<8;s.stopped=0;
            if(s.amount && observed>=128) {s.amount=0;update_feedback();s.period=normal_period(p,controls,raw,s.peak_input);}
        }
        s.average=uint8_t(s.average-(s.average>>3)+(display>>3));
        if(display>=s.average && (display>188 || display-s.average>=39)) {
            s.trigger_peak=display;
            if(s.flags&1) {s.flags=0xc0;s.stop_counter=raw[45];}
        } else {
            s.trigger_peak=std::max(s.trigger_peak,display);
            if(uint8_t(s.trigger_peak-display)>=39 && dynamic && !(s.flags&1)) s.flags=0xc1;
        }
        const uint8_t held=uint8_t(s.held>>8);
        if(optimization) {
            if(uint8_t(held-observed)>=22 && !s.stopped) {
                s.stopped=1;
                if(held>=s.history[0] && held-s.history[0]>=24) s.period=15;
            }
            std::move(s.history.begin()+1,s.history.end(),s.history.begin());s.history.back()=held;
        }
        // A fall is compiled in eight-position steps by the controller. The
        // bounded sequence completes within this tick, retaining the final
        // quantized position. The stop-delay counter postpones both bands.
        if(s.flags&0xc0) {
            if((s.flags&1) && s.stop_counter) --s.stop_counter;
            else {
                const unsigned base=(s.flags&1)?(p.shared_stop?6:12):0;
                auto target=[](uint8_t current,uint8_t requested) {
                    current&=248;return requested>=current?requested:uint8_t(requested&248);
                };
                const uint8_t previous_mid=s.mid;
                if(s.flags&128) s.mid=target(s.mid,raw[base+1]);
                if(s.flags&64) s.low=target(s.low,raw[base]);
                s.flags&=1;
                if(s.mid!=previous_mid) s.peak_input=uint8_t(ControlProfile::decay_index(s.mid));
                s.amount=1;
                s.period=normal_period(p,controls,raw,s.peak_input);
            }
        }
        auto effective=raw;effective[1]=s.mid;
        const unsigned time=controls.decay_duration(uint8_t(std::max(8u,unsigned(s.mid))),0,effective);
        if(s.peak_input!=32 && time>=120) s.period=20;
        if(--s.divider==0) {
            s.divider=s.period;
            if(!s.stopped) {if(s.amount) {--s.amount;update_feedback();}}
            else if(s.amount<(s.peak_input==32?20:12)) {++s.amount;update_feedback();}
        }
        if(--s.peak_divider==0) {
            s.peak_divider=8;
            if(s.peak_input!=32) {
                const unsigned denominator=uint16_t(time<<4);
                const unsigned release=std::max(1u,denominator?unsigned(uint16_t((16u*65536)/denominator)):65535u);
                s.held=uint16_t(s.held>=release?s.held-release:0);
                s.trigger_peak=uint8_t(s.trigger_peak>=3?s.trigger_peak-3:0);
            }
        }
        return changed(before);
    }
private:
    void update_feedback() noexcept {state_.feedback_mid=state_.mid;state_.feedback_amount=state_.amount;}
    bool changed(const DynamicsState& before) const noexcept {
        return before.low!=state_.low || before.mid!=state_.mid || before.feedback_mid!=state_.feedback_mid ||
            before.feedback_amount!=state_.feedback_amount;
    }
    DynamicsState state_{};
    uint8_t detectors_=0,fast_detectors_=0;
    uint16_t transfer_peak_=0;
};
}
