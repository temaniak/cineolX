#include "Processor.hpp"
#include "Editor.hpp"
#include "../core/control_scales.hpp"
#include <limits>

namespace {
using native_hall::scales::times;
using native_hall::scales::frequencies;
constexpr const char* algorithm_names[native_hall::program_count]={"Small Concert Hall B","Vocal Plate",
    "Large Concert Hall B","Acoustic Chamber","Percussion Plate A","Small Concert Hall A"};
template<class T> int nearest(const T* table,float value,int start=0) {
    int best=start;for(int i=start;i<32;++i) if(std::abs(float(table[i])-value)<std::abs(float(table[best])-value)) best=i;
    return best;
}

}
juce::AudioProcessorValueTreeState::ParameterLayout NativeHallProcessor::layout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout p;
    StringArray names;for(const char* name:algorithm_names) names.add(name);
    auto algorithm=std::make_unique<AudioParameterChoice>(ParameterID{"algorithm",2},"Algorithm",names,2);
    auto* selection=algorithm.get();
    auto timeAttributes=AudioParameterIntAttributes{}.withStringFromValueFunction([](int v,int){return String(times[v],1)+" s";});
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"bass",1},"Bass",1,31,17,
        timeAttributes.withValueFromStringFunction([](const String& v){return nearest(times,v.getFloatValue(),1);})));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"mid",1},"Mid",0,31,14,
        timeAttributes.withValueFromStringFunction([](const String& v){return nearest(times,v.getFloatValue());})));
    auto hz=AudioParameterIntAttributes{}.withStringFromValueFunction([](int v,int){return String(frequencies[v])+" Hz";})
        .withValueFromStringFunction([](const String& v){return nearest(frequencies,v.getFloatValue());});
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"crossover",1},"Crossover",0,31,5,hz));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"treble",1},"Treble Decay",0,31,23,hz));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"depth",1},"Depth",0,71,21));
    // Keep the v0.2 raw range and normalized automation. As on Daisy, the
    // position selects 0..128 ms above the chosen program's minimum.
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"predelay",1},"Pre-delay",24,152,24,
        AudioParameterIntAttributes{}.withLabel("ms")
            .withStringFromValueFunction([selection](int v,int) {
                return String(v-24+native_hall::predelay_minima[selection->getIndex()]);
            }).withValueFromStringFunction([selection](const String& v) {
                return v.getIntValue()+24-native_hall::predelay_minima[selection->getIndex()];
            })));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"diffusion",1},"Diffusion",1,63,1));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"input_db",1},"Input Gain",NormalisableRange<float>(-36,12,0.1f),0,
        AudioParameterFloatAttributes{}.withLabel("dB")));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"mix",1},"Dry / Wet",NormalisableRange<float>(0,1,0.001f),1,
        AudioParameterFloatAttributes{}.withStringFromValueFunction([](float v,int){return String(v*100,1)+" %";})
        .withValueFromStringFunction([](const String& v){return v.getFloatValue()/100;})));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{"analog",1},"Analog Filters",true));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{"mode_enh",1},"Mode Enhancement",true));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{"decay_opt",1},"Decay Optimization",true));
    p.add(std::make_unique<AudioParameterChoice>(ParameterID{"output_l",1},"Left Output",StringArray{"A","B","C","D"},0));
    p.add(std::make_unique<AudioParameterChoice>(ParameterID{"output_r",1},"Right Output",StringArray{"A","B","C","D"},2));
    // Append so existing parameter indices/IDs continue to address v0.2 controls.
    p.add(std::move(algorithm));
    return p;
}
NativeHallProcessor::NativeHallProcessor():AudioProcessor(BusesProperties()
    .withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
    state(*this,nullptr,"NativeHall224",layout()) {
    algorithm_parameter_=static_cast<juce::AudioParameterChoice*>(state.getParameter("algorithm"));
    for(unsigned i=0;i<values_.size();++i) values_[i]=state.getRawParameterValue(ids[i]);
    previous_.fill(std::numeric_limits<float>::quiet_NaN());
}
bool NativeHallProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo() &&
        (l.getMainInputChannelSet()==juce::AudioChannelSet::stereo() || l.getMainInputChannelSet()==juce::AudioChannelSet::mono());
}
void NativeHallProcessor::prepareToPlay(double rate,int block) {
    engine_initialized_=ready();
    if(engine_initialized_) engine_.prepare(rom_bank_->bank(),unsigned(getCurrentProgram()));
    bridge_.setup(int(std::lround(rate)),std::clamp(block,1,8192));
    setLatencySamples(bridge_.latency()+int(std::lround(native_hall::Engine48::latency_samples*rate/48000)));
    previous_.fill(std::numeric_limits<float>::quiet_NaN());
}
void NativeHallProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    if(!ready()) {if(getTotalNumInputChannels()==1) b.copyFrom(1,0,b,0,0,b.getNumSamples());return;}
    if(!engine_initialized_) {
        // Import publishes immutable data. Only the audio thread touches its
        // engine; initialization uses fixed storage, with no I/O or allocation.
        engine_.prepare(rom_bank_->bank(),unsigned(getCurrentProgram()));
        previous_.fill(std::numeric_limits<float>::quiet_NaN());engine_initialized_=true;
    }
    std::array<float,15> v;bool changed=false;
    for(unsigned i=0;i<v.size();++i) {v[i]=values_[i]->load(std::memory_order_relaxed);changed|=v[i]!=previous_[i];}
    if(changed) {
        previous_=v;native_hall::Parameters p;
        p.hall.bass=int(v[0]);p.hall.mid=int(v[1]);p.hall.crossover=int(v[2]);p.hall.treble=int(v[3]);
        p.program=unsigned(std::clamp(int(v[14]),0,int(native_hall::program_count)-1));
        p.hall.depth=int(v[4]);p.hall.predelay_ms=int(v[5])-24+native_hall::predelay_minima[p.program];
        p.hall.diffusion=int(v[6]);
        p.input_db=v[7];p.mix=v[8];p.analog=v[9]>=0.5f;p.hall.mode_enhancement=v[10]>=0.5f;
        p.hall.decay_optimization=v[11]>=0.5f;p.output_left=int(v[12]);p.output_right=int(v[13]);
        engine_.set_parameters(p);
    }
    const float* right=b.getReadPointer(getTotalNumInputChannels()==1?0:1);
    bridge_.process(b.getReadPointer(0),right,b.getWritePointer(0),b.getWritePointer(1),b.getNumSamples(),
        [&](const float* l,const float* r,float* ol,float* ore,int n) {
            for(int i=0;i<n;++i) engine_.process(l[i],r[i],ol[i],ore[i]);
        });
}
int NativeHallProcessor::getCurrentProgram() {
    // Parameter listeners can run before APVTS publishes its raw mirror.
    // Read the choice's own atomic value so the display changes immediately.
    return algorithm_parameter_->getIndex();
}
void NativeHallProcessor::setCurrentProgram(int program) {
    if(program<0 || program>=getNumPrograms()) return;
    auto* parameter=state.getParameter("algorithm");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(float(program)));
}
const juce::String NativeHallProcessor::getProgramName(int program) {
    return program>=0 && program<getNumPrograms()?algorithm_names[program]:juce::String{};
}
juce::AudioProcessorEditor* NativeHallProcessor::createEditor() {return new CineolEditor(*this);}
void NativeHallProcessor::getStateInformation(juce::MemoryBlock& block) {
    if(auto xml=state.copyState().createXml()) copyXmlToBinary(*xml,block);
}
void NativeHallProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(state.state.getType())) {
        auto restored=juce::ValueTree::fromXml(*xml);
        // A v0.2 session always used Large Concert Hall B, even if another
        // algorithm is currently selected in this instance.
        if(!restored.getChildWithProperty("id","algorithm").isValid()) {
            juce::ValueTree algorithm("PARAM");algorithm.setProperty("id","algorithm",nullptr);
            algorithm.setProperty("value",2.0f,nullptr);restored.appendChild(algorithm,nullptr);
        }
        state.replaceState(restored);
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new NativeHallProcessor;}
