#include "pitch_engine.hpp"
#include <signalsmith-stretch/signalsmith-stretch.h>
#include <array>
#include <algorithm>

namespace cineol::pitch_lab {
class SignalsmithReference final:public PitchEngine {
public:
    void prepare(unsigned rate)override {
        maximum_=0;
        for(unsigned w=0;w<3;++w)for(unsigned o=0;o<3;++o) {
            const unsigned i=w*3+o,block=2048u<<w,hop=block/(4u<<o);
            engines_[i].configure(2,int(block),int(hop),true);
            delays_[i]=unsigned(engines_[i].inputLatency()+engines_[i].outputLatency());
            maximum_=std::max(maximum_,delays_[i]);
        }
        (void)rate; // This laboratory's windows are defined at the fixed 48 kHz boundary.
    }
    void select(unsigned window,unsigned overlap)noexcept override{selection_=std::min(window,2u)*3+std::min(overlap,2u);}
    void reset()noexcept override{for(auto& engine:engines_)engine.reset();}
    void settings(float shift,float formant,bool preserve)noexcept override {
        engines_[selection_].setTransposeSemitones(shift);
        engines_[selection_].setFormantSemitones(formant,preserve);
    }
    void process(const float*const* in,float*const* out,unsigned n)noexcept override{engines_[selection_].process(in,int(n),out,int(n));}
    unsigned latency()const noexcept override{return delays_[selection_];}
    unsigned maximum_latency()const noexcept override{return maximum_;}
    const char* name()const noexcept override{return "Signalsmith Stretch 1.3.2 / reference";}
private:
    std::array<signalsmith::stretch::SignalsmithStretch<float>,9> engines_;
    std::array<unsigned,9> delays_{};unsigned selection_=4,maximum_=0;
};
std::unique_ptr<PitchEngine> make_signalsmith_reference(){return std::make_unique<SignalsmithReference>();}
}
