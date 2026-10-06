#pragma once
#include <array>
#include <cstdint>

namespace cineol::xl {
// Prepared data for the X/XL interpolation controller. Sequence bytes
// come from the user's firmware during offline import and stay out of Git.
struct ModulationProfile {
    static constexpr unsigned max_taps=15; // four-bit firmware descriptor count
    std::array<uint8_t,4096> sequence{};
    std::array<uint8_t,max_taps> rows{},caps{};
    std::array<std::array<bool,2>,max_taps> negative{};
    uint8_t flags=0,period=1,hold=32,step=4,mask=128;
    // AB52 starts with a pointer into SBC ROM, not the NVS sequence. The
    // first index advance at AD77..AD81 moves the pointer to 8000..8FFF.
    uint8_t startup_byte=0;
    // Nominal controller calls per second, in tenths; measured offline.
    uint32_t rate_tenths=0;
    // The native v8.21 Chorus compiler: its lower half slows a four-unit
    // fractional step; its upper half increases the step at every call.
    void set_chorus(uint8_t raw) noexcept {
        const unsigned index=raw>>3;
        period=uint8_t(index<16?17-index:1);
        step=uint8_t(index<16?4:(index-15)*4);
    }
    bool valid(unsigned row_count) const noexcept {
        const unsigned count=flags&15;
        if(count>rows.size() || !period || !hold || !step ||
           !rate_tenths || rate_tenths>100000) return false;
        for(unsigned i=0;i<count;++i) if(unsigned(rows[i])+1>=row_count) return false;
        return true;
    }
};
struct ModulationState {
    uint16_t index=0;
    uint8_t divider=1,random_divider=8,random_hold=1;
    std::array<uint8_t,ModulationProfile::max_taps> address_low{},phase{};
    bool startup_lookup=false;
    bool valid() const noexcept {return index<4096 && divider && random_divider && random_hold &&
        (!startup_lookup || index==59);}
};
// Native integer control law corresponding to the v8.21 AD5C..AE9B work loop.
// Call at a control boundary, independently of the fixed DSP graph. No CPU,
// opcode dispatch, allocation or mutable ROM memory is used here.
class Modulator {
public:
    void reset(const ModulationState& state) noexcept {state_=state;}
    const ModulationState& state() const noexcept {return state_;}
    template<class Settings>
    void step(const ModulationProfile& profile,Settings& settings) noexcept {
        auto& s=state_;
        if(--s.random_divider==0) {
            s.random_divider=8;
            if(--s.random_hold==0) {s.random_hold=profile.hold;s.index=(s.index+1)&4095;s.startup_lookup=false;}
        }
        if(--s.divider!=0) return;
        s.divider=profile.period;
        uint8_t random=s.startup_lookup?profile.startup_byte:profile.sequence[s.index];
        const unsigned count=profile.flags&15;
        for(unsigned i=0;i<count;++i) {
            const unsigned flags=profile.flags-i;
            if((flags&128) && (flags&1)) random^=1;
            else if(!(flags&64)) random=uint8_t((random>>1)|(random<<7));
            const unsigned row=profile.rows[i];
            const uint8_t cap=profile.caps[i],cached=s.address_low[i];
            uint8_t phase=s.phase[i];
            const bool up=(random&1)!=0;
            int value=int(phase),delta=0;
            bool advance=false;
            if(up && !(phase&1)) {value+=profile.step;advance=value>255;delta=2;}
            if(up && (phase&1)) {value-=profile.step;advance=uint8_t(value)<cap;delta=1;}
            if(!up && (phase&1)) {value+=profile.step;advance=value>255;delta=-2;}
            if(!up && !(phase&1)) {value-=profile.step;advance=uint8_t(value)<cap;delta=-1;}
            if(advance) {
                const int moved=int(cached)+delta;
                const uint8_t candidate=uint8_t(moved);
                if(moved<0 || moved>255 || ((candidate^cached)&profile.mask)) {
                    s.random_hold=s.random_divider=1;continue;
                }
                const bool first=delta==2 || delta==-2;
                if(first) s.address_low[i]=candidate;
                const unsigned target=row+(first?0:1);
                settings.offsets[target]=uint16_t((settings.offsets[target]&0xff00)|candidate);
                phase^=1;
                value=int(phase)+(((up && !(phase&1)) || (!up && (phase&1)))?profile.step:-int(profile.step));
            }
            phase=uint8_t(value);s.phase[i]=phase;
            const uint8_t first=uint8_t((phase&0xfc)|(cap&3));
            const uint8_t second=uint8_t(((cap|3)-4-first)|3);
            const int magnitudes[]={int((~first&255)>>2),int((~second&255)>>2)};
            for(unsigned pair=0;pair<2;++pair) settings.coefficients[row+pair]=
                int8_t(profile.negative[i][pair]?-magnitudes[pair]:magnitudes[pair]);
        }
    }
private:
    ModulationState state_{};
};
// Both additions occupy previous padding: preserve the v5 payload layout.
static_assert(sizeof(ModulationProfile)==4168 && sizeof(ModulationState)==36);
}
