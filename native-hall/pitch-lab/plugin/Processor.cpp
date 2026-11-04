#include "Processor.hpp"
#include "Editor.hpp"
#include "../../core/control_scales.hpp"
#include <chrono>

using namespace cineol::pitch_lab;
juce::AudioProcessorValueTreeState::ParameterLayout PitchLabProcessor::layout() {
    using namespace juce;AudioProcessorValueTreeState::ParameterLayout result;
    for(unsigned i=0;i<count;++i) {
        const auto d=definitions[i];const ParameterID id{d.id,1};
        if(d.kind==Kind::toggle)result.add(std::make_unique<AudioParameterBool>(id,d.name,d.initial>.5f));
        else if(d.kind==Kind::choice)result.add(std::make_unique<AudioParameterChoice>(id,d.name,StringArray::fromTokens(d.choices,"|",""),int(d.initial)));
        else if(d.kind==Kind::integer) {
            auto a=AudioParameterIntAttributes{}.withStringFromValueFunction([i,d](int v,int) {
                if(i==bass || i==mid)return String(native_hall::scales::times[v],1)+" s";
                if(i==crossover || i==treble)return String(native_hall::scales::frequencies[v])+" Hz";
                return String(v)+(String(d.unit).isEmpty()?"":" "+String(d.unit));
            }).withValueFromStringFunction([i](const String& text) {
                if(i==bass || i==mid || i==crossover || i==treble) {
                    const float value=text.getFloatValue();int best=0;
                    for(int n=1;n<32;++n) {
                        const auto at=[i](int x){return i==bass || i==mid?native_hall::scales::times[x]:float(native_hall::scales::frequencies[x]);};
                        if(std::abs(at(n)-value)<std::abs(at(best)-value))best=n;
                    }
                    return best;
                }
                return text.getIntValue();
            });
            result.add(std::make_unique<AudioParameterInt>(id,d.name,int(d.lo),int(d.hi),int(d.initial),a));
        } else {
            auto a=AudioParameterFloatAttributes{}.withStringFromValueFunction([i,d](float v,int) {
                if(i==feedback)return String(v*100,1)+" %";
                if(String(d.unit)=="dB" && v<=-79.99f)return String("-inf dB");
                return String(v,String(d.unit)=="Hz"?0:1)+(String(d.unit).isEmpty()?"":" "+String(d.unit));
            }).withValueFromStringFunction([i](const String& text) {
                if(text.containsIgnoreCase("inf"))return -80.f;
                return text.getFloatValue()/(i==feedback?100.f:1.f);
            });
            result.add(std::make_unique<AudioParameterFloat>(id,d.name,NormalisableRange<float>(d.lo,d.hi,d.step,d.skew),d.initial,a));
        }
    }
    return result;
}
PitchLabProcessor::PitchLabProcessor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
    .withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"CineolPitchLab",layout()) {
    for(unsigned i=0;i<count;++i)values_[i]=state.getRawParameterValue(definitions[i].id);
}
bool PitchLabProcessor::isBusesLayoutSupported(const BusesLayout& b)const {
    return b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo() &&
        (b.getMainInputChannelSet()==juce::AudioChannelSet::mono() || b.getMainInputChannelSet()==juce::AudioChannelSet::stereo());
}
void PitchLabProcessor::prepareToPlay(double rate,int block) {
    ready_.store(false,std::memory_order_release);
    bridge_.setup(std::max(1,int(std::lround(rate))),std::clamp(block,1,8192));
    core_.prepare(bank_->ready()?&bank_->data:nullptr);
    for(unsigned i=0;i<count;++i)last_[i]=values_[i]->load(std::memory_order_relaxed);
    core_.set_values(last_);core_.clear();
    setLatencySamples(bridge_.latency()+int(std::lround(core_.latency()*rate/48000)));
    pitchLatencyMs.store(core_.pitch_latency()/48.f);hall_ready_.store(core_.hall_available());
    analyzerFifo.reset();panic_.store(false);ready_.store(true,std::memory_order_release);
}
void PitchLabProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) {
    juce::ScopedNoDenormals denormals;
    if(!ready()){b.clear();return;}
    const auto begin=std::chrono::steady_clock::now();
    const unsigned revision=edit_revision_.load(std::memory_order_acquire);
    if(!(revision&1u)) {
        Values next;for(unsigned i=0;i<count;++i)next[i]=values_[i]->load(std::memory_order_relaxed);
        if(revision==edit_revision_.load(std::memory_order_acquire) && next!=last_){core_.set_values(next);last_=next;}
    }
    if(panic_.exchange(false,std::memory_order_acq_rel))core_.clear();
    bridge_.process(b.getReadPointer(0),b.getReadPointer(getTotalNumInputChannels()==1?0:1),b.getWritePointer(0),b.getWritePointer(1),b.getNumSamples(),
        [&](const float* l,const float* r,float* ol,float* ore,int frames) {
            int start1,size1,start2,size2;analyzerFifo.prepareToWrite(frames,start1,size1,start2,size2);
            for(int n=0;n<frames;++n) {
                const auto v=core_.process(l[n],r[n]);ol[n]=v[0];ore[n]=v[1];
                if(n<size1){analyzerSamples[0][size_t(start1+n)]=v[0];analyzerSamples[1][size_t(start1+n)]=v[1];}
                else if(n-size1<size2){analyzerSamples[0][size_t(start2+n-size1)]=v[0];analyzerSamples[1][size_t(start2+n-size1)]=v[1];}
            }
            analyzerFifo.finishedWrite(size1+size2);
        });
    for(unsigned n=0;n<node_count;++n) {
        const auto& m=core_.meters()[n];meters[n].peak.store(m.peak,std::memory_order_relaxed);meters[n].power.store(m.power,std::memory_order_relaxed);meters[n].clips.store(m.clips,std::memory_order_relaxed);
    }
    guardTripped.store(core_.guard_tripped(),std::memory_order_relaxed);hallSaturations.store(core_.hall_saturations(),std::memory_order_relaxed);
    pitchLatencyMs.store(core_.pitch_latency()/48.f,std::memory_order_relaxed);
    const float load=b.getNumSamples()?float(std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()*getSampleRate()/b.getNumSamples()*100):0;
    callbackLoad.store(load,std::memory_order_relaxed);
}
void PitchLabProcessor::setValues(const Values& values) {
    edit_revision_.fetch_add(1,std::memory_order_acq_rel);
    for(unsigned i=0;i<count;++i) {auto* p=state.getParameter(definitions[i].id);p->setValueNotifyingHost(p->convertTo0to1(values[i]));}
    edit_revision_.fetch_add(1,std::memory_order_release);requestPanic();
}
void PitchLabProcessor::applyPreset(unsigned which) {
    Values v;for(unsigned i=0;i<count;++i)v[i]=values_[i]->load(std::memory_order_relaxed);
    setValues(preset(which,v));
}
void PitchLabProcessor::getStateInformation(juce::MemoryBlock& out) {
    auto tree=state.copyState();tree.addChild(snapshotA.createCopy(),-1,nullptr);tree.addChild(snapshotB.createCopy(),-1,nullptr);
    if(auto xml=tree.createXml())copyXmlToBinary(*xml,out);
}
void PitchLabProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size);xml && xml->hasTagName(state.state.getType())) {
        edit_revision_.fetch_add(1,std::memory_order_acq_rel);auto tree=juce::ValueTree::fromXml(*xml);
        if(auto a=tree.getChildWithName("SnapshotA");a.isValid()){snapshotA=a.createCopy();tree.removeChild(a,nullptr);}
        if(auto b=tree.getChildWithName("SnapshotB");b.isValid()){snapshotB=b.createCopy();tree.removeChild(b,nullptr);}
        state.replaceState(tree);edit_revision_.fetch_add(1,std::memory_order_release);requestPanic();
    }
}
juce::AudioProcessorEditor* PitchLabProcessor::createEditor(){return new PitchLabEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new PitchLabProcessor;}
