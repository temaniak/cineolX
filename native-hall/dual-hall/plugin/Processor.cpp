#include "Processor.hpp"
#include "Editor.hpp"
#include "../../core/control_scales.hpp"
#include <limits>

juce::AudioProcessorValueTreeState::ParameterLayout DualHallProcessor::layout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout p;
    auto times=AudioParameterIntAttributes{}.withStringFromValueFunction([](int v,int) {
        return String(native_hall::scales::times[v],1)+" s";
    }).withValueFromStringFunction([](const String& s) {
        int best=0;for(int i=1;i<32;++i)if(std::abs(native_hall::scales::times[i]-s.getFloatValue())<
            std::abs(native_hall::scales::times[best]-s.getFloatValue()))best=i;return best;
    });
    auto frequencies=AudioParameterIntAttributes{}.withStringFromValueFunction([](int v,int) {
        return String(native_hall::scales::frequencies[v])+" Hz";
    }).withValueFromStringFunction([](const String& s) {
        int best=0;for(int i=1;i<32;++i)if(std::abs(native_hall::scales::frequencies[i]-s.getFloatValue())<
            std::abs(native_hall::scales::frequencies[best]-s.getFloatValue()))best=i;return best;
    });
    const auto percent=AudioParameterFloatAttributes{}.withStringFromValueFunction([](float v,int) {
        return String(v*100,1)+" %";
    }).withValueFromStringFunction([](const String& s){return s.getFloatValue()/100;});
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"bass",1},"Bass Decay",1,31,17,times));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"mid",1},"Mid Decay",0,31,14,times));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"crossover",1},"Crossover",0,31,5,frequencies));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"treble",1},"Treble Decay",0,31,23,frequencies));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"depth",1},"Depth",0,71,21));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"predelay",1},"Pre-delay",24,152,24,AudioParameterIntAttributes{}.withLabel("ms")));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"diffusion",1},"Diffusion",1,63,1));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"mod_rate",1},"Modulation Rate",1,16,2));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"mod_depth",1},"Modulation Depth",1,16,1));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"opt_response",1},"Optimization Response",1,16,5));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{"mode_enh",1},"Mode Enhancement",true));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{"decay_opt",1},"Decay Optimization",true));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"crossfeedback",1},"Crossfeedback",
        NormalisableRange<float>(0,cineol::dual_hall::Tanks::maximum_feedback,.001f),.15f,percent));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"input_db",1},"Input Gain",NormalisableRange<float>(-36,12,.1f),0,
        AudioParameterFloatAttributes{}.withLabel("dB")));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"mix",1},"Dry / Wet",NormalisableRange<float>(0,1,.001f),1,percent));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"output_db",1},"Output Gain",NormalisableRange<float>(-24,6,.1f),0,
        AudioParameterFloatAttributes{}.withLabel("dB")));
    return p;
}
DualHallProcessor::DualHallProcessor():AudioProcessor(BusesProperties()
    .withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
    state(*this,nullptr,"CineolDualHallB",layout()) {
    for(unsigned i=0;i<values_.size();++i)values_[i]=state.getRawParameterValue(ids[i]);
    previous_.fill(std::numeric_limits<float>::quiet_NaN());
}
bool DualHallProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo() &&
        (l.getMainInputChannelSet()==juce::AudioChannelSet::stereo() || l.getMainInputChannelSet()==juce::AudioChannelSet::mono());
}
native_hall::Controls DualHallProcessor::hall_controls(const std::array<float,std::size(ids)>& v) noexcept {
    native_hall::Controls c;c.bass=int(v[0]);c.mid=int(v[1]);c.crossover=int(v[2]);c.treble=int(v[3]);
    c.depth=int(v[4]);c.predelay_ms=int(v[5]);c.diffusion=int(v[6]);
    c.modulation_period=uint8_t(std::max(1,int(v[7])-1));c.modulation_step=uint8_t(4*int(v[8]));
    c.optimization_response=uint8_t(v[9]);c.mode_enhancement=v[10]>=.5f;c.decay_optimization=v[11]>=.5f;
    return c;
}
void DualHallProcessor::prepareToPlay(double rate,int block) {
    prepared_.store(false,std::memory_order_release);
    clear_requested_.store(false,std::memory_order_relaxed);
    bridge_.setup(std::max(1,int(std::lround(rate))),std::clamp(block,1,8192));
    setLatencySamples(bridge_.latency()+int(std::lround(cineol::dual_hall::Tanks::latency*rate/48000)));
    dry_.fill({});dry_position_=0;
    std::array<float,std::size(ids)> v;
    for(unsigned i=0;i<v.size();++i)v[i]=values_[i]->load(std::memory_order_relaxed);
    previous_=v;gain_=gain_target_=std::pow(10.f,v[13]/20);mix_=v[14];output_=output_target_=std::pow(10.f,v[15]/20);
    if(bank_->ready()) {
        tanks_.prepare(bank_->data,hall_controls(v),v[12]);
        prepared_.store(true,std::memory_order_release);
    }
}
void DualHallProcessor::controls(const std::array<float,std::size(ids)>& v) noexcept {
    bool changed=false;for(unsigned i=0;i<12;++i)changed|=v[i]!=previous_[i];
    if(changed)tanks_.set_controls(hall_controls(v));
    tanks_.set_crossfeedback(v[12]);
    if(v[13]!=previous_[13])gain_target_=std::pow(10.f,v[13]/20);
    if(v[15]!=previous_[15])output_target_=std::pow(10.f,v[15]/20);
    previous_=v;
}
void DualHallProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    if(!ready()){b.clear();return;}
    std::array<float,std::size(ids)> v;
    for(unsigned i=0;i<v.size();++i)v[i]=values_[i]->load(std::memory_order_relaxed);
    controls(v);
    if(clear_requested_.exchange(false,std::memory_order_acq_rel)){tanks_.clear_tails();dry_.fill({});dry_position_=0;}
    bridge_.process(b.getReadPointer(0),b.getReadPointer(getTotalNumInputChannels()==1?0:1),
        b.getWritePointer(0),b.getWritePointer(1),b.getNumSamples(),
        [&](const float* l,const float* r,float* ol,float* ore,int n) {
            for(int i=0;i<n;++i) {
                const float il=std::isfinite(l[i])?l[i]:0,ir=std::isfinite(r[i])?r[i]:0;
                gain_+=.002f*(gain_target_-gain_);mix_+=.002f*(v[14]-mix_);output_+=.002f*(output_target_-output_);
                const auto wet=tanks_.process(il*gain_,ir*gain_);
                const auto dry=dry_[dry_position_];dry_[dry_position_]={il,ir};
                if(++dry_position_==dry_.size())dry_position_=0;
                // Input gain scales the new excitation only. The reciprocal
                // return is tapped before final mix/output gain, independently
                // of either knob. There is no dry signal in the return path.
                ol[i]=(dry[0]*(1-mix_)+wet[0]*mix_)*output_;
                ore[i]=(dry[1]*(1-mix_)+wet[1]*mix_)*output_;
            }
        });
}
void DualHallProcessor::getStateInformation(juce::MemoryBlock& out) {
    if(auto xml=state.copyState().createXml())copyXmlToBinary(*xml,out);
}
void DualHallProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size);xml && xml->hasTagName(state.state.getType()))
        state.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorEditor* DualHallProcessor::createEditor(){return new DualHallEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new DualHallProcessor;}
