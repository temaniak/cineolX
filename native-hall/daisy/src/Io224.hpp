#pragma once
#include "../../core/engine48.hpp"
#include "Io224Tables.hpp"
#include <cassert>

namespace native_hall {
// Original 224 only. The desktop's 33-tap input kernel/modal tail and separate
// ADC holds, reduced to float. Immutable tables live in AXI SRAM, not DTCM.
class DaisyInput224 {
public:
    static int16_t adc(float sample) noexcept {
        return LegacyInput48::adc(sample);
    }
    static unsigned detectors(float sample) noexcept {
        return LegacyInput48::detectors(sample);
    }
    void prepare() noexcept {reset();}
    void reset() noexcept {raw_={};states_={};raw_slot_=state_slot_=seen_=0;next_pass_=0;}
    template<class Emit> void process(const float* input,Emit&& emit) noexcept {
        using namespace daisy_io224;
        const auto delayed=raw_[(raw_slot_+32)&63];
        raw_[raw_slot_]=raw_[raw_slot_+64]={input[0],input[1]};
        const auto& previous=states_[(state_slot_+19)%20];
        auto& current=states_[state_slot_];
        for(unsigned c=0;c<2;++c) {
            for(unsigned k=0;k<4;++k) {
                const auto p=previous[c][k],a=input_whole[k],b=input_tail[k];
                current[c][k]={a.re*p.re-a.im*p.im+b.re*delayed[c],
                               a.im*p.re+a.re*p.im+b.im*delayed[c]};
            }
            for(unsigned k=4;k<6;++k)
                current[c][k]={input_whole[k].re*previous[c][k].re+input_tail[k].re*delayed[c],0};
            if(seen_==0)for(unsigned k=0;k<6;++k) {
                current[c][k].re-=input_startup[k].re*input[c];
                current[c][k].im-=input_startup[k].im*input[c];
            }
        }
        if(next_pass_<frame_ticks) {
            float converted[2];
            for(unsigned c=0;c<2;++c)
                converted[c]=sample(c,next_pass_+(c?4:-46)*281250+585805-16*frame_ticks);
            emit(converted);next_pass_+=pass_ticks;
        }
        next_pass_-=frame_ticks;
        raw_slot_=(raw_slot_+1)&63;state_slot_=(state_slot_+1)%20;
        seen_=std::min(seen_+1,64u);
    }
private:
    static constexpr int32_t frame_ticks=12000000,pass_ticks=28125000;
    float sample(unsigned c,int32_t time) const noexcept {
        using namespace daisy_io224;
        assert(time<=0);
        const unsigned ago=unsigned((-time+frame_ticks-1)/frame_ticks);
        assert(ago<20);
        if(ago>seen_)return 0;
        const int32_t fraction=time+int32_t(ago)*frame_ticks;
        assert(fraction>=23305 && (fraction-23305)%187500==0);
        const unsigned phase=unsigned((fraction-23305)/187500);
        assert(phase<64);
        float result=0;
        // Mirrored 64-frame history makes all 33 tap reads contiguous.
        const auto* history=&raw_[raw_slot_+64-ago+1];
        for(unsigned j=0;j<33;++j)result+=input_fir[phase][j]*history[-int(j)][c];
        const auto& state=states_[(state_slot_+20-ago)%20][c];
        for(unsigned k=0;k<4;++k) {
            const auto a=input_fractional[phase][k],p=state[k];
            result+=a.re*p.re-a.im*p.im;
        }
        for(unsigned k=4;k<6;++k)result+=input_fractional[phase][k].re*state[k].re;
        return result;
    }
    std::array<std::array<float,2>,128> raw_{};
    std::array<std::array<std::array<daisy_io224::Complex,6>,2>,20> states_{};
    unsigned raw_slot_=0,state_slot_=0,seen_=0;
    int32_t next_pass_=0;
};

// Event-timed DAC/AOUT without an output SRC FIR. Keep the desktop's channel
// capture phases and transport; omit only its inaudible -500 kHz circuit mode.
class DaisyOutput224 {
public:
    void prepare() noexcept {reset();}
    void reset() noexcept {next_pass_=0;clear();}
    void select_network(unsigned n) noexcept {network_=std::min(n,3u);clear();}
    void push(const float* values) noexcept {
        for(unsigned c=0;c<4;++c) {
            auto& channel=channels_[c];assert(channel.count<2);
            channel.events[(channel.read+channel.count)&1]={
                next_pass_+int32_t(capture_rows[network_][c])*281250+643405,values[c]};
            ++channel.count;
        }
        next_pass_+=pass_ticks;
    }
    std::array<float,4> sample() noexcept {
        using namespace daisy_io224;
        auto output=delay_[position_];auto& frame=delay_[position_];frame={};
        for(unsigned c=0;c<4;++c) {
            auto& channel=channels_[c];
            if(channel.count && channel.events[channel.read].time<=0) {
                const auto event=channel.events[channel.read];advance(channel);
                channel.held=event.value;channel.age=-event.time;channel.started=true;
                channel.read=(channel.read+1)&1;--channel.count;
            }
            if(channel.started) {
                const unsigned phase=unsigned(channel.age/93750);assert(phase<300);
                float sum=0;
                for(unsigned k=0;k<3;++k) {
                    const auto a=output_fractional[phase][k],s=channel.state[k];
                    const float re=s.re+a.re*(s.re+channel.held)-a.im*s.im;
                    const float im=s.im+a.im*(s.re+channel.held)+a.re*s.im;
                    sum+=output_weights[k].re*re-output_weights[k].im*im;
                }
                for(unsigned k=3;k<6;++k) {
                    const float s=channel.state[k].re;
                    sum+=output_weights[k].re*(s+output_fractional[phase][k].re*(s+channel.held));
                }
                frame[c]=sum*1.5975062688498582f;channel.age+=frame_ticks;
            }
            for(unsigned n=0;n<channel.count;++n)channel.events[(channel.read+n)&1].time-=frame_ticks;
        }
        next_pass_-=frame_ticks;position_=(position_+1)%38;
        return output;
    }
private:
    static constexpr int32_t frame_ticks=12000000,pass_ticks=28125000;
    inline static constexpr unsigned capture_rows[4][4]={
        {62,39,112,89},{57,109,109,57},{78,32,109,55},{62,39,112,89}};
    struct Event {int32_t time=0;float value=0;};
    struct Channel {
        std::array<daisy_io224::Complex,6> state{};
        std::array<Event,2> events{};
        float held=0;int32_t age=0;unsigned read=0,count=0;bool started=false;
    };
    void clear() noexcept {channels_={};delay_={};position_=0;}
    static void advance(Channel& channel) noexcept {
        using namespace daisy_io224;
        for(unsigned k=0;k<3;++k) {
            const auto a=output_whole[k],s=channel.state[k];
            channel.state[k]={s.re+a.re*(s.re+channel.held)-a.im*s.im,
                              s.im+a.im*(s.re+channel.held)+a.re*s.im};
        }
        for(unsigned k=3;k<6;++k) {
            auto& s=channel.state[k].re;s+=output_whole[k].re*(s+channel.held);
        }
    }
    std::array<Channel,4> channels_{};
    std::array<std::array<float,4>,38> delay_{};
    int32_t next_pass_=0;unsigned network_=0,position_=0;
};
// Preserve Hall, control timing, parameters, ADC/DAC coding and dry/mix law.
using DaisyEngine224=Engine48WithOutput<DaisyOutput224,DaisyInput224>;
} // namespace native_hall
