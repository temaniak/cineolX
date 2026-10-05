#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>

namespace native_hall {
// Original 224 output: independently timed DAC holds through the continuous
// AOUT circuit, sampled at 48 kHz. No interpolation FIR or emulator is used.
// push() follows each 20,480 Hz network pass; sample() follows each host frame.
// All exponentials are prepared once. Processing has fixed storage and bounds.
class Dac48 {
public:
    static constexpr int32_t frame_ticks=12000000, pass_ticks=28125000;
    static constexpr int32_t row_ticks=281250, event_origin_ticks=643405;
    // Periodic FPC captures, including its waiting latch/normalization pipeline.
    // Close Hall/Chamber requests wait; captures are not simply WR_DA+10.
    // Firmware power-up and its transient converter phase are not emulated.
    inline static constexpr unsigned capture_rows[4][4]={
        {62,39,112,89}, {57,109,109,57}, {78,32,109,55}, {62,39,112,89}};

    void prepare() noexcept;
    void reset() noexcept {next_pass_=0;clear();}
    // A hard program switch clears the old output circuit/queued captures but
    // preserves the shared, free-running host/native clock.
    void select_network(unsigned network) noexcept {network_=std::min(network,3u);clear();}
    void push(const float* values) noexcept {
        for(unsigned c=0;c<4;++c) {
            auto& channel=channels_[c];
            assert(channel.count<channel.events.size());
            auto& event=channel.events[(channel.read+channel.count)%channel.events.size()];
            event={next_pass_+int32_t(capture_rows[network_][c])*row_ticks+event_origin_ticks,values[c]};
            ++channel.count;
        }
        next_pass_+=pass_ticks;
    }
    std::array<float,4> sample() noexcept {
        std::array<float,4> output{};
        for(unsigned c=0;c<4;++c) {
            auto& channel=channels_[c];
            if(channel.count && channel.events[channel.read].time<=0) {
                const auto event=channel.events[channel.read];
                advance(channel);
                channel.held=event.value;channel.age=-event.time;channel.started=true;
                channel.read=(channel.read+1)%channel.events.size();--channel.count;
            }
            if(channel.started) {
                // Every frame and row is on the 93,750-tick grid. The DAC's
                // observed propagation offset leaves the same fractional
                // remainder for every channel/network and every clock phase.
                const unsigned phase=unsigned(channel.age/grid_ticks);
                assert(phase<phases);
                float sum=0;
                for(unsigned k=0;k<modes;++k) {
                    const auto& a=fractional_[phase][k];const auto& state=channel.state[k];
                    float re=state.re+a.re*(state.re+channel.held)-a.im*state.im;
                    float im=state.im+a.im*(state.re+channel.held)+a.re*state.im;
                    sum+=weights_[k].re*re-weights_[k].im*im;
                }
                output[c]=(sum+float(3.5966000160032862e-12)*channel.held)*scale;
                channel.age+=frame_ticks;
            }
            for(unsigned n=0;n<channel.count;++n)
                channel.events[(channel.read+n)%channel.events.size()].time-=frame_ticks;
        }
        next_pass_-=frame_ticks;
        return output;
    }
private:
    static constexpr unsigned modes=7, phases=300;
    static constexpr int32_t grid_ticks=93750;
    static constexpr int32_t remainder_ticks=grid_ticks-event_origin_ticks%grid_ticks;
    static constexpr float scale=float(1.5975062688498582);
    struct Complex {float re=0,im=0;};
    struct Event {int32_t time=0;float value=0;};
    struct Channel {
        std::array<Complex,modes> state{};
        std::array<Event,2> events{};
        float held=0;int32_t age=0;unsigned read=0,count=0;bool started=false;
    };
    void clear() noexcept {channels_={};}
    void advance(Channel& channel) noexcept {
        // Store exp(p*T)-1, so the slow high-pass mode does not lose its small
        // step to subtraction of two floats close to one. w=p*z is normalized
        // circuit state: w' = p*w+p*u, w_next=w+(exp(p*T)-1)*(w+u).
        for(unsigned k=0;k<modes;++k) {
            const auto a=whole_[k];auto& state=channel.state[k];
            float re=state.re+a.re*(state.re+channel.held)-a.im*state.im;
            float im=state.im+a.im*(state.re+channel.held)+a.re*state.im;
            state={re,im};
        }
    }
    std::array<std::array<Complex,modes>,phases> fractional_{};
    std::array<Complex,modes> whole_{},weights_{};
    std::array<Channel,4> channels_{};
    int32_t next_pass_=0;unsigned network_=0;
};
inline void Dac48::prepare() noexcept {
    using C=std::complex<double>;
    // Circuit modes from the pinned original-224 AOUT, including its tiny fast
    // pole. Conjugate pairs are represented once; weights contain their factor 2.
    const double rows[modes][4]={
        {-499999.92763972917,0,-3.3357989152375994e-05,0},
        {-2078.9309743031404,52656.156156557365,-154.75983466442128,-521.1939205999234},
        {-7593.720389659135,47085.12790844678,1802.026900585217,1082.4614462112024},
        {-16514.177328627768,30674.64428732316,-4855.783307439754,1742.6454306668868},
        {-22720.857808301902,0,-9549.660499129033,0},
        {-2.0328722285876513,0,-1.274430980922891,0},
        {-12550.2093589853,0,16073.03221843225,0}};
    for(unsigned k=0;k<modes;++k) {
        C p(rows[k][0],rows[k][1]),r(rows[k][2],rows[k][3]);
        C weight=r/p*(p.imag()==0?1.0:2.0);
        weights_[k]={float(weight.real()),float(weight.imag())};
        const auto coefficient=[&](int32_t ticks) {
            C value=std::exp(p*(double(ticks)/576000000000.0))-C(1);
            return Complex{float(value.real()),float(value.imag())};
        };
        whole_[k]=coefficient(pass_ticks);
        for(unsigned phase=0;phase<phases;++phase)
            fractional_[phase][k]=coefficient(remainder_ticks+int32_t(phase)*grid_ticks);
    }
    reset();
}
} // namespace native_hall
