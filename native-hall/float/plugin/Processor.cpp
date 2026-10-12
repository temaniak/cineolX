#include "Processor.hpp"
#include "Editor.hpp"
#include "../../core/control_scales.hpp"
#include <limits>

juce::AudioProcessorValueTreeState::ParameterLayout FloatHallABProcessor::layout() {
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
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"bass",1},"Bass Decay",1,31,17,times));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"mid",1},"Mid Decay",0,31,14,times));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"crossover",1},"Crossover",0,31,5,frequencies));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"treble",1},"Treble Decay",0,31,23,frequencies));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"depth",1},"Depth",0,71,21));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"predelay",1},"Pre-delay",24,152,24,AudioParameterIntAttributes{}.withLabel("ms")));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"diffusion",1},"Diffusion",1,63,1));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{"mode_enh",1},"Mode Enhancement",false));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{"decay_opt",1},"Decay Optimization",false));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"input_db",1},"Input Gain",NormalisableRange<float>(-36,12,.1f),0,AudioParameterFloatAttributes{}.withLabel("dB")));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"mix",1},"Dry / Wet",NormalisableRange<float>(0,1,.001f),1,
        AudioParameterFloatAttributes{}.withStringFromValueFunction([](float v,int){return String(v*100,1)+" %";})
            .withValueFromStringFunction([](const String& s){return s.getFloatValue()/100;})));
    p.add(std::make_unique<AudioParameterChoice>(ParameterID{"output_l",1},"Left Output",StringArray{"A","B","C","D"},0));
    p.add(std::make_unique<AudioParameterChoice>(ParameterID{"output_r",1},"Right Output",StringArray{"A","B","C","D"},2));
    p.add(std::make_unique<AudioParameterChoice>(ParameterID{"compare",1},"A / B",StringArray{"A - Original","B - Float 48 kHz"},0));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"float_trim",1},"B Level Trim",NormalisableRange<float>(-12,12,.1f),0,AudioParameterFloatAttributes{}.withLabel("dB")));
    return p;
}
FloatHallABProcessor::FloatHallABProcessor():AudioProcessor(BusesProperties()
    .withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
    state(*this,nullptr,"CineolFloatHallAB",layout()) {
    for(unsigned i=0;i<values_.size();++i)values_[i]=state.getRawParameterValue(ids[i]);
    previous_.fill(std::numeric_limits<float>::quiet_NaN());
}
bool FloatHallABProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo() &&
        (l.getMainInputChannelSet()==juce::AudioChannelSet::stereo() || l.getMainInputChannelSet()==juce::AudioChannelSet::mono());
}
void FloatHallABProcessor::prepareToPlay(double rate,int block) {
    prepared_.store(false,std::memory_order_release);
    bridge_.setup(int(std::lround(rate)),std::clamp(block,1,8192));
    setLatencySamples(bridge_.latency()+int(std::lround(70*rate/48000)));
    float_delay_.fill({});dry_delay_.fill({});delay_position_=0;
    previous_.fill(std::numeric_limits<float>::quiet_NaN());
    blend_=values_[13]->load(std::memory_order_relaxed);
    gain_=std::pow(10.0f,values_[9]->load(std::memory_order_relaxed)/20);
    mix_=values_[10]->load(std::memory_order_relaxed);
    trim_=std::pow(10.0f,values_[14]->load(std::memory_order_relaxed)/20);
    if(bank_->ready() && floating_.prepare(bank_->bank())) {
        original_.prepare(bank_->bank(),2);
        prepared_.store(true,std::memory_order_release);
    }
}
void FloatHallABProcessor::controls(const std::array<float,std::size(ids)>& v) noexcept {
    bool changed=false;for(unsigned i=0;i<v.size();++i)changed|=v[i]!=previous_[i];
    if(!changed)return;
    native_hall::Parameters p;p.program=2;p.mix=1;p.analog=true;p.dirt=0;
    p.hall.bass=int(v[0]);p.hall.mid=int(v[1]);p.hall.crossover=int(v[2]);p.hall.treble=int(v[3]);
    p.hall.depth=int(v[4]);p.hall.predelay_ms=int(v[5]);p.hall.diffusion=int(v[6]);
    p.hall.mode_enhancement=v[7]>=.5f;p.hall.decay_optimization=v[8]>=.5f;
    p.output_left=int(v[11]);p.output_right=int(v[12]);
    original_.set_parameters(p);
    bool hall_changed=false;for(unsigned i=0;i<9;++i)hall_changed|=v[i]!=previous_[i];
    if(hall_changed)floating_.set_controls(p.hall);
    gain_target_=std::pow(10.0f,v[9]/20);trim_target_=std::pow(10.0f,v[14]/20);
    previous_=v;
}
void FloatHallABProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    if(!ready()) {buffer.clear();return;}
    std::array<float,std::size(ids)> v;
    for(unsigned i=0;i<v.size();++i)v[i]=values_[i]->load(std::memory_order_relaxed);
    controls(v);
    auto* left=buffer.getWritePointer(0);auto* right=buffer.getWritePointer(1);
    const int input_right=getTotalNumInputChannels()==1?0:1;
    const unsigned output_left=unsigned(v[11]),output_right=unsigned(v[12]);
    bridge_.process(left,buffer.getReadPointer(input_right),left,right,buffer.getNumSamples(),
        [&](const float* l,const float* r,float* ol,float* ore,int n) {
            for(int i=0;i<n;++i) {
                const float il=std::isfinite(l[i])?l[i]:0,ir=std::isfinite(r[i])?r[i]:0;
                gain_+=.002f*(gain_target_-gain_);mix_+=.002f*(v[10]-mix_);trim_+=.002f*(trim_target_-trim_);
                const float x=il*gain_,y=ir*gain_;
                float al,ar;original_.process(x,y,al,ar,true);
                const auto b=float_delay_[delay_position_];float_delay_[delay_position_]=floating_.process(x,y);
                const auto dry=dry_delay_[delay_position_];dry_delay_[delay_position_]={il,ir};
                if(++delay_position_==float_delay_.size())delay_position_=0;
                // Both engines run continuously; a 10 ms bounded switch
                // crossfade preserves each history without resetting its tail.
                blend_+=std::clamp(v[13]-blend_,-1.0f/480,1.0f/480);
                ol[i]=dry[0]*(1-mix_)+((1-blend_)*al+blend_*b[output_left]*trim_)*mix_;
                ore[i]=dry[1]*(1-mix_)+((1-blend_)*ar+blend_*b[output_right]*trim_)*mix_;
            }
        });
}
void FloatHallABProcessor::getStateInformation(juce::MemoryBlock& result) {
    if(auto xml=state.copyState().createXml())copyXmlToBinary(*xml,result);
}
void FloatHallABProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size);xml && xml->hasTagName(state.state.getType()))
        state.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorEditor* FloatHallABProcessor::createEditor() {return new FloatHallABEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new FloatHallABProcessor;}
