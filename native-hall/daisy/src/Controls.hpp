#pragma once
#include "../../core/engine48.hpp"
#include <array>

namespace cineol::controls {
inline constexpr unsigned controller_count=10;
enum class Target {Bass,Mid,Crossover,Treble,Depth,Predelay,Diffusion,InputGain,Mix,Program};
struct Rgb {float red,green,blue;};
struct ControlConfig {
    Target target;
    float minimum,maximum;
    bool inverted;
    float (*curve)(float)=nullptr;
};
#if __has_include("ControlConfig.local.hpp")
#include "ControlConfig.local.hpp"
#else
#include "ControlConfig.example.hpp"
#endif
constexpr bool valid_config() {
    unsigned seen=0;
    for(const auto& config:control_config) {
        const unsigned target=unsigned(config.target);
        if(target>=controller_count || !(config.maximum>config.minimum) || (seen&(1u<<target))) return false;
        seen|=1u<<target;
    }
    return seen==((1u<<controller_count)-1);
}
static_assert(valid_config(),"Assign each target once and use an increasing input range");
inline float normalize(float raw,const ControlConfig& config) noexcept {
    if(!std::isfinite(raw)) raw=config.minimum;
    float value=std::clamp((raw-config.minimum)/(config.maximum-config.minimum),0.0f,1.0f);
    if(config.inverted) value=1-value;
    if(config.curve) value=config.curve(value);
    return std::isfinite(value)?std::clamp(value,0.0f,1.0f):0.0f;
}
struct InputFrame {
    std::array<float,controller_count> values{};
    // Logical pressed states; electrical pull-ups/polarity belong in the adapter.
    bool button1=false,button2=false;
};
// A useful fixed preset for the pin-free reference adapter.
inline InputFrame default_frame() noexcept {
    InputFrame result;
    constexpr float positions[]={16.0f/30,14.0f/31,5.0f/31,23.0f/31,21.0f/71,
                                  0,0,0.5f,1,2.5f/6};
    for(unsigned i=0;i<controller_count;++i) {
        const auto& config=control_config[i];
        float p=positions[unsigned(config.target)];
        if(config.inverted) p=1-p;
        result.values[i]=config.minimum+p*(config.maximum-config.minimum);
    }
    return result;
}
class Quantizer {
public:
    int update(float coordinate,int low,int high) noexcept {
        coordinate=std::clamp(coordinate,float(low),float(high));
        if(!initialized_ || coordinate>value_+0.65f || coordinate<value_-0.65f) {
            value_=std::clamp(int(std::lround(coordinate)),low,high);initialized_=true;
        }
        return value_;
    }
private:
    int value_=0;bool initialized_=false;
};
class Debouncer {
public:
    bool update(bool down) noexcept {
        if(down!=candidate_) {candidate_=down;ticks_=1;}
        else if(ticks_<8) ++ticks_;
        bool released=false;
        if(ticks_>=8 && down!=stable_) {released=stable_ && !down;stable_=down;}
        return released;
    }
    bool down() const noexcept {return stable_;}
private:
    bool candidate_=false,stable_=false;unsigned ticks_=0;
};
class Mapper {
public:
    // Call at 500 Hz. Single releases toggle Mod/Opt; both buttons toggle Analog.
    void update(const InputFrame& input) noexcept {
        const bool released1=button1_.update(input.button1),released2=button2_.update(input.button2);
        if(button1_.down() && button2_.down()) chord_=true;
        if(!chord_ && released1) params_.hall.mode_enhancement=!params_.hall.mode_enhancement;
        if(!chord_ && released2) params_.hall.decay_optimization=!params_.hall.decay_optimization;
        if(chord_ && !button1_.down() && !button2_.down()) {params_.analog=!params_.analog;chord_=false;}
        std::array<float,controller_count> values{};
        for(unsigned i=0;i<controller_count;++i)
            values[unsigned(control_config[i].target)]=normalize(input.values[i],control_config[i]);
        params_.program=unsigned(q_[9].update(values[9]*6-0.5f,0,5));
        params_.hall.bass=q_[0].update(1+values[0]*30,1,31);
        params_.hall.mid=q_[1].update(values[1]*31,0,31);
        params_.hall.crossover=q_[2].update(values[2]*31,0,31);
        params_.hall.treble=q_[3].update(values[3]*31,0,31);
        params_.hall.depth=q_[4].update(values[4]*71,0,71);
        params_.hall.predelay_ms=q_[5].update(values[5]*128,0,128)+native_hall::predelay_minima[params_.program];
        params_.hall.diffusion=q_[6].update(1+values[6]*62,1,63);
        params_.input_db=q_[7].update(values[7]<=0.5f?-360+720*values[7]:240*(values[7]-0.5f),-360,120)*0.1f;
        params_.mix=q_[8].update(values[8]*1000,0,1000)*0.001f;
    }
    const native_hall::Parameters& parameters() const noexcept {return params_;}
private:
    native_hall::Parameters params_{};
    std::array<Quantizer,controller_count> q_{};
    Debouncer button1_,button2_;
    bool chord_=false;
};
}
