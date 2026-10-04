#pragma once
#include "concert.hpp"
#include "analog_xl48.hpp"
#include "dynamics.hpp"
#include "../core/engine48.hpp"
#include <numeric>

namespace cineol::xl {
// Desktop-only native XL audio path. It accepts prepared control snapshots;
// it never loads ROMs, interprets opcodes, or executes an 8080 in audio.
template<Graph graph> class Native48 {
public:
    static constexpr Graph graph_id=graph;
    using Core=Network<graph>;
    using Settings=typename Core::Settings;
    static constexpr int sample_rate=48000;
    static constexpr unsigned divisor=std::gcd(640u,9*Core::rows);
    static constexpr unsigned down_num=640/divisor,down_den=9*Core::rows/divisor;
    // Existing 64/32-tap design: 32 host + 16 native samples. Align dry to
    // the rounded integer host delay for this graph's own internal clock.
    static constexpr int latency_samples=int(32+16.0*down_den/down_num+0.5);
    bool prepare(const Settings& settings) noexcept {
        if(!concert_.prepare(settings)) return false;
        down_.prepare();up_.prepare();
        for(auto& filter:input_) filter.prepare(true);
        for(auto& filter:output_) filter.prepare(false);
        fifo_.fill({});dry_.fill({});raw_delay_.fill({});raw_hold_.fill(0);
        control_profile_=nullptr;
        read_=write_=position_=0;gain_=gain_target_=mix_=mix_target_=analog_=analog_target_=1;
        left_=0;right_=2;wet_fade_=1;return true;
    }
    bool set_settings(const Settings& settings) noexcept {return concert_.set_settings(settings);}
    // Kernel/circuit setup happens once outside audio; program recall clears
    // only state and fixed delay storage, retaining the precomputed kernels.
    bool activate(const Settings& settings) noexcept {
        if(!concert_.prepare(settings)) return false;
        control_profile_=nullptr;
        down_.reset();up_.reset();for(auto& f:input_) f.reset();for(auto& f:output_) f.reset();
        fifo_.fill({});dry_.fill({});raw_delay_.fill({});raw_hold_.fill(0);read_=write_=position_=0;wet_fade_=0;
        return true;
    }
    void set_controls(const Settings& settings) noexcept {concert_.set_controls(settings);}
    void set_dynamics(const DynamicsProfile& profile,const DynamicsState& initial) noexcept {
        dynamics_profile_=profile;dynamics_.reset(initial);slow_clock_=fast_clock_=0;
    }
    void set_native_controls(const Settings& base,const ControlProfile& controls,const std::array<uint8_t,48>& raw,
        bool optimization) noexcept {
        const bool dynamic=(raw[42]&1)!=0;
        bool update_decay=control_profile_!=&controls || dynamic!=dynamic_enabled_;
        for(unsigned cell:{0u,1u,6u,7u,12u,13u,43u,44u}) update_decay|=raw[cell]!=control_values_[cell];
        control_profile_=&controls;control_base_=base;control_values_=raw;
        dynamic_enabled_=dynamic;optimization_enabled_=optimization;
        dynamics_.parameters(dynamics_profile_,controls,raw,dynamic_enabled_,optimization,update_decay);
        compile_dynamics();
    }
    const DynamicsState& dynamics_state() const noexcept {return dynamics_.state();}
    void set_chorus(uint8_t raw) noexcept {concert_.set_chorus(raw);}
    void enable_modulation(bool enabled) noexcept {concert_.enable_modulation(enabled);}
    void set_diffusion(const DiffusionProfile& profile,uint8_t index) noexcept {concert_.set_diffusion(profile,index);}
    bool set_modulation(const ModulationProfile& profile,const ModulationState& state,bool enabled) noexcept {
        return concert_.set_modulation(profile,state,enabled);
    }
    void set_global(float input_db,float mix,float clean,int left,int right) noexcept {
        gain_target_=std::pow(10.0f,std::clamp(input_db,-36.0f,12.0f)/20.0f);
        mix_target_=std::clamp(mix,0.0f,1.0f);analog_target_=std::clamp(clean,0.0f,1.0f);
        left_=std::clamp(left,0,3);right_=std::clamp(right,0,3);
    }
    void process(float left,float right,float& out_left,float& out_right,bool wet_only=false) noexcept {
        if(!std::isfinite(left)) left=0;if(!std::isfinite(right)) right=0;
        gain_+=0.002f*(gain_target_-gain_);mix_+=0.002f*(mix_target_-mix_);
        analog_+=0.002f*(analog_target_-analog_);
        if(std::abs(mix_-mix_target_)<1e-5f) mix_=mix_target_;
        if(std::abs(analog_-analog_target_)<1e-5f) analog_=analog_target_;
        const float raw[2]={left*gain_,right*gain_};float filtered[2];
        for(unsigned c=0;c<2;++c) filtered[c]=input_[c].process(raw[c]);
        down_.process(filtered,[&](const float* input) {
            int16_t words[2];
            for(unsigned c=0;c<2;++c) {
                const auto clean=native_hall::Engine48::adc(input[c]);
                const auto bare=native_hall::Engine48::adc(raw[c],false);
                words[c]=int16_t(std::lround(bare+analog_*(float(clean)-bare)));
            }
            int16_t output[4];concert_.process(words[0],words[1],output);
            if(control_profile_ && dynamics_profile_.enabled) {
                dynamics_.observe(input[0],input[1],concert_.control_output());
                bool changed=false;
                fast_clock_+=dynamics_profile_.fast_rate_tenths*Core::rate_denominator;
                if(fast_clock_>=Core::rate_numerator*10) {
                    fast_clock_-=Core::rate_numerator*10;
                    changed|=dynamics_.fast_poll(dynamics_profile_,*control_profile_,control_values_);
                }
                slow_clock_+=dynamics_profile_.slow_rate_tenths*Core::rate_denominator;
                if(slow_clock_>=Core::rate_numerator*10) {
                    slow_clock_-=Core::rate_numerator*10;
                    changed|=dynamics_.slow_poll(dynamics_profile_,*control_profile_,control_values_,dynamic_enabled_,optimization_enabled_);
                }
                if(changed) compile_dynamics();
            }
            float samples[4];
            for(unsigned c=0;c<4;++c) samples[c]=raw_hold_[c]=native_hall::Engine48::dac(output[c])/32768.0f;
            up_.process(samples,[&](const float* upsampled) {
                std::array<float,4> frame{};
                for(unsigned c=0;c<4;++c) frame[c]=output_[c].process(upsampled[c]);
                fifo_[write_++%fifo_.size()]=frame;
            });
        });
        std::array<float,4> wet{};if(read_<write_) wet=fifo_[read_++%fifo_.size()];
        raw_delay_[position_%raw_delay_.size()]=raw_hold_;
        const auto& bare=raw_delay_[(position_+raw_delay_.size()-latency_samples)%raw_delay_.size()];
        wet_fade_=std::min(1.0f,wet_fade_+1.0f/128);
        for(unsigned c=0;c<4;++c) wet[c]=(bare[c]+analog_*(wet[c]-bare[c]))*wet_fade_;
        dry_[position_%dry_.size()]={left,right};
        const auto dry=dry_[(position_+dry_.size()-latency_samples)%dry_.size()];++position_;
        out_left=wet_only?wet[left_]:dry[0]*(1-mix_)+wet[left_]*mix_;
        out_right=wet_only?wet[right_]:dry[1]*(1-mix_)+wet[right_]*mix_;
    }
    const Core& concert() const noexcept {return concert_;}
private:
    void compile_dynamics() noexcept {
        auto effective=control_values_;effective[0]=dynamics_.state().low;effective[1]=dynamics_.state().mid;
        auto settings=control_base_;control_profile_->apply_size(settings,control_values_);
        control_profile_->apply_static(settings,effective,dynamics_.state().amount);
        // A decay compile and a feedback compile have distinct controller
        // boundaries. Preserve the last committed feedback between them.
        effective[1]=dynamics_.state().feedback_mid;
        control_profile_->apply_feedback(settings,effective,dynamics_.state().feedback_amount);concert_.set_controls(settings);
    }
    DynamicsProfile dynamics_profile_{};
    Dynamics dynamics_;
    const ControlProfile* control_profile_=nullptr;
    Settings control_base_{};
    std::array<uint8_t,48> control_values_{};
    uint32_t slow_clock_=0,fast_clock_=0;
    bool dynamic_enabled_=false,optimization_enabled_=false;
    Core concert_;
    native_hall::RationalFilter<down_num,down_den,64,2> down_;
    native_hall::RationalFilter<down_den,down_num,32,4> up_;
    std::array<Analog48,2> input_;
    std::array<Analog48,4> output_;
    std::array<std::array<float,4>,16> fifo_{};
    std::array<std::array<float,2>,128> dry_{};
    std::array<std::array<float,4>,128> raw_delay_{};
    std::array<float,4> raw_hold_{};
    uint64_t read_=0,write_=0,position_=0;
    float wet_fade_=1;
    float gain_=1,gain_target_=1,mix_=1,mix_target_=1,analog_=1,analog_target_=1;
    int left_=0,right_=2;
};
using Concert48=Native48<Graph::concert>;
}
