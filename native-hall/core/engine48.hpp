#pragma once
#include "hall.hpp"
#include "rate48.hpp"
#include "analog48.hpp"
#include <cmath>

namespace native_hall {
struct Parameters {
    Controls hall{};
    float input_db=0,mix=1;
    bool analog=true;
    int output_left=0,output_right=2;
    unsigned program=2;
    float dirt=0; // Optional continuous desktop control; hardware defaults retain their sound.
};
// Default boundaries remain the portable/Daisy implementation. Desktop can
// supply separate circuit sampling without duplicating controls/integer DSP.
class LegacyInput48 {
public:
    static int16_t adc(float sample,bool gain_ranging=true) noexcept {
        if(!gain_ranging) return int16_t(int(std::lround(std::clamp(sample,-1.0f,1.0f)*2047.0f))*16);
        float a=std::abs(sample);unsigned range=a<0.112f?3:a<0.224f?2:a<0.448f?1:0;
        int code=int(std::lround(std::clamp(sample*float(1u<<range)*2047,-2048.0f,2047.0f)));
        return int16_t(code*int(1u<<(4-range)));
    }
    static unsigned detectors(float sample) noexcept {
        unsigned mask=0;float a=std::abs(sample);
        constexpr float thresholds[]={0.056f,0.112f,0.224f,0.448f,1.0f};
        for(unsigned k=0;k<5;++k) if(a>=thresholds[k]) mask|=1u<<k;
        return mask;
    }
    void prepare() noexcept {down_.prepare();for(auto& f:filters_)f.prepare(true);}
    void reset() noexcept {down_.reset();for(auto& f:filters_)f.reset();}
    template<class Emit> void process(const float* raw,Emit&& emit) noexcept {
        float filtered[2];for(unsigned c=0;c<2;++c)filtered[c]=filters_[c].process(raw[c]);
        down_.process(filtered,emit);
    }
private:
    RationalFilter<32,75,64,2> down_;
    std::array<Analog48,2> filters_;
};
class LegacyOutput48 {
public:
    void prepare() noexcept {up_.prepare();for(auto& f:filters_)f.prepare(false);}
    void reset() noexcept {up_.reset();for(auto& f:filters_)f.reset();fifo_.fill({});read_=write_=0;}
    void select_network(unsigned) noexcept {}
    void push(const float* samples) noexcept {
        up_.process(samples,[&](const float* host) {
            std::array<float,4> frame{};
            for(unsigned c=0;c<4;++c)frame[c]=filters_[c].process(host[c]);
            fifo_[write_%fifo_.size()]=frame;++write_;
        });
    }
    std::array<float,4> sample() noexcept {
        std::array<float,4> frame{};if(read_<write_)frame=fifo_[read_++%fifo_.size()];return frame;
    }
private:
    RationalFilter<75,32,32,4> up_;
    std::array<Analog48,4> filters_;
    std::array<std::array<float,4>,16> fifo_{};
    uint64_t read_=0,write_=0;
};
template<class Output,class Input=LegacyInput48,class Core=Hall> class Engine48WithOutput {
public:
    static constexpr int sample_rate=48000, latency_samples=70;
    bool prepare(const Profile& profile) noexcept {
        bank_=nullptr;hall_.prepare(profile);initialize();return true;
    }
    bool prepare(const ProgramBank& bank,unsigned program=2) noexcept {
        bank_=&bank;current_program_=std::min(program,program_count-1);
        hall_.prepare(bank,current_program_);initialize();params_.program=current_program_;return true;
    }
    // Desktop firmware switches reuse kernels prepared outside processing.
    // Existing prepare() behavior and the portable Daisy signal path are retained.
    void prepare_audio() noexcept {
        input_.prepare();output_.prepare();
    }
    bool activate(const ProgramBank& bank,unsigned program=2) noexcept {
        bank_=&bank;current_program_=std::min(program,program_count-1);
        hall_.prepare(bank,current_program_);reset_audio();params_.program=current_program_;return true;
    }
private:
    void initialize() noexcept {prepare_audio();reset_audio();}
    void reset_audio() noexcept {
        input_.reset();output_.reset();
        output_.select_network(bank_?program_networks[current_program_]:0);
        dry_.fill({});dry_position_=0;
        raw_delay_.fill({});raw_hold_.fill(0);
        controls_applied_=false;
        params_=Parameters{};gain_=gain_target_=1;gain_db_=0;mix_=1;analog_blend_=1;
    }
public:
    void set_parameters(const Parameters& p) noexcept {
        const bool controls_changed=!controls_applied_ || !same_controls(params_.hall,p.hall);
        const unsigned program=std::min(p.program,program_count-1);
        const bool program_changed=bank_ && program!=current_program_;
        params_=p; params_.mix=std::clamp(p.mix,0.0f,1.0f);
        params_.output_left=std::clamp(p.output_left,0,3);params_.output_right=std::clamp(p.output_right,0,3);
        float db=std::clamp(p.input_db,-36.0f,12.0f);
        if(db!=gain_db_) {gain_db_=db;gain_target_=std::pow(10.0f,db/20.0f);}
        if(bank_) {
            params_.program=program;
            if(program_changed) {
                current_program_=params_.program;hall_.select_program(current_program_);
                output_.select_network(program_networks[current_program_]);
            }
        }
        // Keep control polling and audio-rate smoothing unchanged. Only avoid
        // reapplying the same Hall tables; program resets always need a write.
        if(controls_changed || program_changed) hall_.set_controls(p.hall);
        controls_applied_=true;
    }
    // Desktop low-latency mode mixes the direct input at the host rate.
    // The default path (including Daisy) retains its original dry delay/mix.
    void process(float left,float right,float& out_left,float& out_right,bool wet_only=false) noexcept {
        if(!std::isfinite(left)) left=0;
        if(!std::isfinite(right)) right=0;
        gain_+=0.002f*(gain_target_-gain_);mix_+=0.002f*(params_.mix-mix_);
        const float clean=params_.analog?1-std::clamp(params_.dirt,0.0f,1.0f):0;
        analog_blend_+=0.002f*(clean-analog_blend_);
        if(std::abs(analog_blend_-clean)<1e-5f) analog_blend_=clean;
        float raw[]={left*gain_,right*gain_};
        input_.process(raw,[&](const auto* native) {
            // Analog OFF matches the reference's old digital boundary:
            // nearest input sample, fixed ADC gain, no anti-aliasing FIR.
            int16_t words[2];unsigned levels=0;
            for(unsigned c=0;c<2;++c) {
                int16_t clean=Input::adc(native[c]),bare=adc(raw[c],false);
                words[c]=int16_t(std::lround(bare+analog_blend_*(float(clean)-bare)));
                levels|=Input::detectors(raw[c]+analog_blend_*(native[c]-raw[c]));
            }
            int16_t four[4];hall_.process(words[0],words[1],four,levels);
            float samples[4];for(unsigned c=0;c<4;++c) samples[c]=raw_hold_[c]=dac(four[c])/32768.0f;
            output_.push(samples);
        });
        auto wet=output_.sample();
        raw_delay_[dry_position_%raw_delay_.size()]=raw_hold_;
        const auto& bare=raw_delay_[(dry_position_+raw_delay_.size()-latency_samples)%raw_delay_.size()];
        // Zero-order DAC holds preserve the images that make the reference's
        // bypass sound digital. Keep the advertised host delay in both modes.
        for(unsigned c=0;c<4;++c) wet[c]=bare[c]+analog_blend_*(wet[c]-bare[c]);
        dry_[dry_position_%dry_.size()]={left,right};
        // Preserve the dry signal exactly with the reported integer latency.
        // Averaging adjacent samples for fractional delay would dull dry HF.
        auto dry=dry_[(dry_position_+dry_.size()-latency_samples)%dry_.size()];
        ++dry_position_;
        out_left=wet_only?wet[params_.output_left]:dry[0]*(1-mix_)+wet[params_.output_left]*mix_;
        out_right=wet_only?wet[params_.output_right]:dry[1]*(1-mix_)+wet[params_.output_right]*mix_;
    }
    Core& hall() noexcept {return hall_;}
    unsigned active_program() const noexcept {return current_program_;}
    // Original comparator thresholds and 12-bit mantissa + 6 dB gain steps.
    static int16_t adc(float sample,bool gain_ranging=true) noexcept {
        return LegacyInput48::adc(sample,gain_ranging);
    }
    static unsigned detectors(float sample) noexcept {
        return LegacyInput48::detectors(sample);
    }
    static int16_t dac(int16_t sample) noexcept {
        // FPC compares bits 15 and 14; each shift inserts a low 1 bit.
        unsigned range=0;int value=sample;
        while(range<3 && value>=-16384 && value<=16383) {value=value*2+1;++range;}
        // Twelve-bit DAC truncates the normalized word's low four bits.
        int code=value>>4;
        return int16_t(code*int(1u<<(4-range)));
    }
private:
    static bool same_controls(const Controls& a,const Controls& b) noexcept {
        return a.bass==b.bass && a.mid==b.mid && a.crossover==b.crossover
            && a.treble==b.treble && a.depth==b.depth && a.predelay_ms==b.predelay_ms
            && a.diffusion==b.diffusion && a.mode_enhancement==b.mode_enhancement
            && a.decay_optimization==b.decay_optimization;
    }
    const ProgramBank* bank_=nullptr;
    unsigned current_program_=2;
    float gain_db_=0;
    Core hall_;
    Input input_;
    Output output_;
    std::array<std::array<float,2>,128> dry_{};
    std::array<std::array<float,4>,128> raw_delay_{};
    std::array<float,4> raw_hold_{};
    uint64_t dry_position_=0;
    Parameters params_{};
    bool controls_applied_=false;
    float gain_=1,gain_target_=1,mix_=1,analog_blend_=1;
};
using Engine48=Engine48WithOutput<LegacyOutput48>;
} // namespace native_hall
