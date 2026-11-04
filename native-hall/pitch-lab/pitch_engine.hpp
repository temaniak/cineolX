#pragma once
#include <memory>

namespace cineol::pitch_lab {
// Host-owned laboratory interface. Future independently developed pitch DSP
// plugs in here; routing, Hall, filters, meters and DAW tests do not depend on
// any vendor's implementation. All allocation belongs to prepare()/factory.
class PitchEngine {
public:
    virtual ~PitchEngine()=default;
    virtual void prepare(unsigned sample_rate)=0;
    virtual void select(unsigned window,unsigned overlap)noexcept=0;
    virtual void reset()noexcept=0;
    virtual void settings(float semitones,float formant_semitones,bool preserve)noexcept=0;
    virtual void process(const float* const* input,float* const* output,unsigned frames)noexcept=0;
    virtual unsigned latency()const noexcept=0;
    virtual unsigned maximum_latency()const noexcept=0;
    virtual const char* name()const noexcept=0;
};
std::unique_ptr<PitchEngine> make_signalsmith_reference();
}
