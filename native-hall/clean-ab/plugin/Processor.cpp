#include "Processor.hpp"
#include "Editor.hpp"
#include "../../core/control_scales.hpp"
#include <limits>
// 0 selection, 1 A/B, 2 input, 3 mix, 4 B trim, 5 Mod, 6 Opt,
// 7..13 original faders, 14 Dynamic, 15 output gain, 16..63 XL cells.
juce::String CleanABProcessor::id(unsigned i) {
    static const char* ids[]={"algorithm","compare","input_db","mix","b_trim","mod","opt","bass","mid","crossover","treble","depth","predelay","diffusion","dynamic","output_db"};
    return i<16?juce::String(ids[i]):"xl"+juce::String(i-16);
}
juce::AudioProcessorValueTreeState::ParameterLayout CleanABProcessor::layout(){
    using namespace juce;AudioProcessorValueTreeState::ParameterLayout p;
    StringArray names;for(auto n:cineol::clean_ab::names)names.add(n);
    p.add(std::make_unique<AudioParameterChoice>(ParameterID{id(0),1},"Algorithm",names,0));
    p.add(std::make_unique<AudioParameterChoice>(ParameterID{id(1),1},"A / B",StringArray{"A - Original","B - Clean"},0));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{id(2),1},"Input",NormalisableRange<float>(-36,12,.1f),0,AudioParameterFloatAttributes{}.withLabel("dB")));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{id(3),1},"Dry / Wet",NormalisableRange<float>(0,1,.001f),1));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{id(4),1},"B Level Trim",NormalisableRange<float>(-12,12,.1f),0,AudioParameterFloatAttributes{}.withLabel("dB")));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{id(5),1},"Mode Enhancement",false));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{id(6),1},"Decay Optimization",false));
    const char* labels[]={"Bass Decay","Mid Decay","Crossover","Treble Decay","Depth","Pre-delay Offset (ms)","Diffusion"};
    const int lo[]={1,0,0,0,0,0,1},hi[]={31,31,31,31,71,128,63},def[]={17,14,5,23,21,0,1};
    for(unsigned j=0;j<7;++j)p.add(std::make_unique<AudioParameterInt>(ParameterID{id(j+7),1},labels[j],lo[j],hi[j],def[j]));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{id(14),1},"Dynamic Decay (XL)",false));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{id(15),1},"Output Gain",NormalisableRange<float>(-24,24,.1f),0));
    for(unsigned j=16;j<count;++j)p.add(std::make_unique<AudioParameterInt>(ParameterID{id(j),1},"XL Control "+String(j-16),-1,255,-1));
    return p;
}
CleanABProcessor::CleanABProcessor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"CineolCleanAB",layout()){
    for(unsigned i=0;i<count;++i)values_[i]=state.getRawParameterValue(id(i));previous_.fill(std::numeric_limits<float>::quiet_NaN());
}
bool CleanABProcessor::isBusesLayoutSupported(const BusesLayout& l)const{return l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo() && (l.getMainInputChannelSet()==juce::AudioChannelSet::mono() || l.getMainInputChannelSet()==juce::AudioChannelSet::stereo());}
void CleanABProcessor::prepareToPlay(double rate,int block){
    prepared_.store(false,std::memory_order_release);bridge_.setup(int(std::lround(rate)),std::clamp(block,1,8192));
    setLatencySamples(bridge_.latency()+int(std::lround(70*rate/48000)));engines_.prepare(bank_->ready()?&bank_->bank():nullptr,bank_->xlReady()?&bank_->xlBank():nullptr);
    previous_.fill(std::numeric_limits<float>::quiet_NaN());selection_=7;dry_.fill({});dryPos_=0;
    blend_=values_[1]->load();gain_=std::pow(10.f,values_[2]->load()/20);mix_=values_[3]->load();trim_=std::pow(10.f,values_[4]->load()/20);output_=std::pow(10.f,values_[15]->load()/20);
    prepared_.store(true,std::memory_order_release);
}
void CleanABProcessor::controls(const std::array<float,count>& v)noexcept{
    const unsigned program=unsigned(std::clamp(int(v[0]),0,6));bool changed=v!=previous_;
    if(program!=selection_){selection_=program;engines_.select(program);changed=true;}
    if(!changed || !ready(program))return;
    if(program<3){native_hall::Controls c;c.bass=int(v[7]);c.mid=int(v[8]);c.crossover=int(v[9]);c.treble=int(v[10]);c.depth=int(v[11]);c.predelay_ms=native_hall::predelay_minima[cineol::clean_ab::indices[program]]+int(v[12]);c.diffusion=int(v[13]);c.mode_enhancement=v[5]>=.5f;c.decay_optimization=v[6]>=.5f;c.modulation_period=2;c.modulation_step=1;c.optimization_response=5;engines_.original_controls(c);}
    else {auto raw=bank_->xlBank().programs[cineol::clean_ab::indices[program]].controls.factory;for(unsigned i=0;i<48;++i)if(v[i+16]>=0)raw[i]=uint8_t(v[i+16]);engines_.xl_controls(raw,v[5]>=.5f,v[6]>=.5f,v[14]>=.5f);}
    previous_=v;
}
void CleanABProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&){
    juce::ScopedNoDenormals noDenormals;if(!prepared_.load(std::memory_order_acquire)){b.clear();return;}
    std::array<float,count> v;for(unsigned i=0;i<count;++i)v[i]=values_[i]->load(std::memory_order_relaxed);controls(v);
    if(!ready(selection_)){b.clear();return;}
    double energyA=0,energyB=0;unsigned frames=0;const float gain=std::pow(10.f,v[2]/20),trim=std::pow(10.f,v[4]/20),output=std::pow(10.f,v[15]/20);
    bridge_.process(b.getReadPointer(0),b.getReadPointer(getTotalNumInputChannels()==1?0:1),b.getWritePointer(0),b.getWritePointer(1),b.getNumSamples(),[&](const float*l,const float*r,float*ol,float*ore,int n){
        for(int i=0;i<n;++i){float il=std::isfinite(l[i])?l[i]:0,ir=std::isfinite(r[i])?r[i]:0;gain_+=.002f*(gain-gain_);mix_+=.002f*(v[3]-mix_);trim_+=.002f*(trim-trim_);output_+=.002f*(output-output_);
            const auto wet=engines_.process(il*gain_,ir*gain_);const auto dry=dry_[dryPos_];dry_[dryPos_]={il,ir};if(++dryPos_==dry_.size())dryPos_=0;
            blend_+=std::clamp(v[1]-blend_,-1.f/480,1.f/480);
            ol[i]=dry[0]*(1-mix_)+((1-blend_)*wet[0]+blend_*wet[2]*trim_)*mix_;
            ore[i]=dry[1]*(1-mix_)+((1-blend_)*wet[1]+blend_*wet[3]*trim_)*mix_;
            ol[i]*=output_;ore[i]*=output_;
            energyA+=double(wet[0])*wet[0]+double(wet[1])*wet[1];energyB+=(double(wet[2])*wet[2]+double(wet[3])*wet[3])*double(trim_)*trim_;frames+=2;
        }});
    if(frames){meterA.store(float(10*std::log10(std::max(energyA/frames,1e-18))),std::memory_order_relaxed);meterB.store(float(10*std::log10(std::max(energyB/frames,1e-18))),std::memory_order_relaxed);}
}
void CleanABProcessor::getStateInformation(juce::MemoryBlock& b){if(auto xml=state.copyState().createXml())copyXmlToBinary(*xml,b);}
void CleanABProcessor::setStateInformation(const void*d,int n){if(auto xml=getXmlFromBinary(d,n);xml && xml->hasTagName(state.state.getType()))state.replaceState(juce::ValueTree::fromXml(*xml));}
juce::AudioProcessorEditor* CleanABProcessor::createEditor(){return new CleanABEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new CleanABProcessor;}
