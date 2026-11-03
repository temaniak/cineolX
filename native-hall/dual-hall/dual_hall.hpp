#pragma once
#include "../desktop/engine22448.hpp"

namespace cineol::dual_hall {
// Two complete, unchanged original-224 Hall B engines. The wrapper owns only
// the external mono feeds and reciprocal wet return. It never edits delay RAM,
// graph coefficients, converter models or controller/modulation state.
class Tanks {
public:
    static constexpr unsigned rate=48000,latency=70;
    static constexpr float maximum_feedback=.5f,feedback_step=maximum_feedback/960;
    void prepare(const native_hall::ProgramBank& bank,const native_hall::Controls& controls,float feedback) noexcept {
        bank_=&bank;
        for(auto& engine:engines_)engine.prepare(bank,2);
        wet_={};feedback_=feedback_target_=bounded_feedback(feedback);
        set_controls(controls);ready_=true;
    }
    void set_controls(const native_hall::Controls& controls) noexcept {
        controls_=controls;
        native_hall::Parameters p;p.program=2;p.hall=controls;
        p.input_db=0;p.mix=1;p.analog=true;p.dirt=0;p.output_left=0;p.output_right=2;
        for(auto& engine:engines_)engine.set_parameters(p);
    }
    void clear_tails() noexcept {
        if(!ready_)return;
        for(auto& engine:engines_)engine.activate(*bank_,2);
        set_controls(controls_);wet_={};feedback_=feedback_target_;
    }
    void set_crossfeedback(float value) noexcept {feedback_target_=bounded_feedback(value);}
    float crossfeedback() const noexcept {return feedback_;}
    std::array<float,2> process(float left,float right) noexcept {
        if(!ready_)return {};
        if(!std::isfinite(left))left=0;
        if(!std::isfinite(right))right=0;
        feedback_+=std::clamp(feedback_target_-feedback_,-feedback_step,feedback_step);
        // Snapshot BOTH previous wet outputs before either engine advances.
        // The extra link delay is exactly one 48 kHz frame, never one host
        // block. Complete converter/filter/transport delays also remain.
        const float input_l=left+feedback_*wet_[1],input_r=right+feedback_*wet_[0];
        float discard;
        engines_[0].process(input_l,input_l,wet_[0],discard,true);
        engines_[1].process(input_r,input_r,discard,wet_[1],true);
        return wet_;
    }
    uint64_t saturations() noexcept {
        return engines_[0].hall().saturation_count()+engines_[1].hall().saturation_count();
    }
private:
    static float bounded_feedback(float value) noexcept {
        return std::isfinite(value)?std::clamp(value,0.f,maximum_feedback):0.f;
    }
    std::array<native_hall::DesktopEngine48,2> engines_;
    const native_hall::ProgramBank* bank_=nullptr;
    native_hall::Controls controls_{};
    std::array<float,2> wet_{};
    float feedback_=0,feedback_target_=0;
    bool ready_=false;
};
} // namespace cineol::dual_hall
