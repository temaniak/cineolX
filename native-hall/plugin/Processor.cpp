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
    for(const auto& graph:cineol::xl::graphs) names.add(juce::String(graph.name));
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
                return String(v-24+native_hall::predelay_minima[std::min(selection->getIndex(),5)]);
            }).withValueFromStringFunction([selection](const String& v) {
                return v.getIntValue()+24-native_hall::predelay_minima[std::min(selection->getIndex(),5)];
            })));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"diffusion",1},"Diffusion",1,63,1));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"input_db",1},"Input Gain",NormalisableRange<float>(-36,12,0.1f),0,
        AudioParameterFloatAttributes{}.withLabel("dB")));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"mix",1},"Dry / Wet",NormalisableRange<float>(0,1,0.001f),1,
        AudioParameterFloatAttributes{}.withStringFromValueFunction([](float v,int){return String(v*100,1)+" %";})
        .withValueFromStringFunction([](const String& v){return v.getFloatValue()/100;})));
    // Keep the original clean=1 polarity and parameter index. Dirt's UI
    // presents the inverse, with continuous intermediate positions.
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"analog",1},"Clean Amount",NormalisableRange<float>(0,1,0.001f),1));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{"mode_enh",1},"Mode Enhancement",true));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{"decay_opt",1},"Decay Optimization",true));
    p.add(std::make_unique<AudioParameterChoice>(ParameterID{"output_l",1},"Left Output",StringArray{"A","B","C","D"},0));
    p.add(std::make_unique<AudioParameterChoice>(ParameterID{"output_r",1},"Right Output",StringArray{"A","B","C","D"},2));
    // Append so existing parameter indices/IDs continue to address v0.2 controls.
    p.add(std::move(algorithm));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{"low_latency",3},"Low latency",false,
        AudioParameterBoolAttributes{}.withAutomatable(false)));
    auto percent=AudioParameterIntAttributes{}.withStringFromValueFunction([](int v,int){return String(v*100/63)+" %";});
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"xl_chorus",4},"Chorus",0,31,16,
        AudioParameterIntAttributes{}.withStringFromValueFunction([](int v,int){return String(v*100/31)+" %";})));
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"xl_diffusion",4},"XL Diffusion",0,63,16,percent));
    for(unsigned cell=0;cell<48;++cell) p.add(std::make_unique<AudioParameterInt>(
        ParameterID{ids[xl_parameter_begin+cell],5},"XL Control "+String(cell+1),-1,255,-1));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{"spillover",6},"Spillover",false,
        AudioParameterBoolAttributes{}.withAutomatable(false)));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"spillover_time",6},"Spillover time",
        NormalisableRange<float>(1.0f,10.0f,1.0f),5.0f,
        AudioParameterFloatAttributes{}.withLabel("s").withAutomatable(false)));
    return p;
}
NativeHallProcessor::NativeHallProcessor():AudioProcessor(BusesProperties()
    .withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
    state(*this,nullptr,"NativeHall224",layout()) {
    state.state.setProperty("firmware_id",juce::String(cineol::current_firmware.id.data()),nullptr);
    algorithm_parameter_=static_cast<juce::AudioParameterChoice*>(state.getParameter("algorithm"));
    for(unsigned i=0;i<values_.size();++i) {
        values_[i]=state.getRawParameterValue(ids[i]);stable_values_[i]=values_[i]->load();
    }
    low_latency_value_=state.getRawParameterValue("low_latency");
    spillover_value_=state.getRawParameterValue("spillover");
    spillover_time_value_=state.getRawParameterValue("spillover_time");
    previous_.fill(std::numeric_limits<float>::quiet_NaN());
    // Poll outside the audio callback, even with the editor closed. Host
    // latency notifications can invoke locks/listeners and must stay here.
    startTimerHz(30);
}
NativeHallProcessor::~NativeHallProcessor() {stopTimer();}
void NativeHallProcessor::updateLatency() {
    const bool low=low_latency_value_->load(std::memory_order_relaxed)>=0.5f;
    setLatencySamples(low?0:normal_latency_.load(std::memory_order_relaxed));
    low_latency_active_.store(low,std::memory_order_relaxed);
}
bool NativeHallProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo() &&
        (l.getMainInputChannelSet()==juce::AudioChannelSet::stereo() || l.getMainInputChannelSet()==juce::AudioChannelSet::mono());
}
void NativeHallProcessor::prepareToPlay(double rate,int block) {
    for(auto& slot:slots_) {slot.engine.prepare_audio();slot.program=-1;slot.alignment.fill({});slot.alignment_position=0;}
    engine_initialized_=false;active_program_=-1;active_slot_=0;tail_slot_=-1;
    tail_length_=tail_position_=tail_input_remaining_=0;tail_shortened_=false;
    transition_dry_.fill({});transition_dry_position_=0;
    transition_mix_=values_[8]->load(std::memory_order_relaxed);
    transition_mixing_=false;
    bridge_.setup(int(std::lround(rate)),std::clamp(block,1,8192));
    direct_input_.setSize(2,std::clamp(block,1,8192));
    host_mix_=values_[8]->load(std::memory_order_relaxed);
    host_mix_coefficient_=float(1-std::pow(0.998,48000/rate));
    normal_latency_.store(bridge_.latency()+int(std::lround(native_hall::Engine48::latency_samples*rate/48000)),
        std::memory_order_relaxed);
    updateLatency();
    previous_.fill(std::numeric_limits<float>::quiet_NaN());
}
void NativeHallProcessor::AudioSlot::process(float l,float r,float& ol,float& ore,bool wet) noexcept {
    if(!NativeHallProcessor::isXL(program)) {engine.process(l,r,ol,ore,wet);return;}
    float a,c;xl_engine.process(l,r,a,c,wet);
    auto& delayed=alignment[alignment_position];
    ol=delayed[0];ore=delayed[1];delayed={a,c};
    alignment_position=(alignment_position+1)%alignment.size();
}
float NativeHallProcessor::tailGain() const noexcept {
    if(tail_slot_<0 || tail_position_>=tail_length_) return 0;
    const float x=1-float(tail_position_)/float(tail_length_);
    return tail_start_gain_*x*x*(3-2*x); // Smooth amplitude fade with rounded endpoints.
}
void NativeHallProcessor::shortenTail() noexcept {
    if(tail_slot_<0 || tail_shortened_) return;
    tail_start_gain_=tailGain();
    tail_length_=std::min(retire_samples,tail_length_-tail_position_);tail_position_=0;
    tail_shortened_=true;
}
void NativeHallProcessor::selectProgram(int requested,const std::array<float,parameter_count>& v,bool spill) {
    if(spill && engine_initialized_ && requested!=active_program_) {
        // The caller retires an older tail before recycling its slot.
        tail_slot_=int(active_slot_);active_slot_=1-active_slot_;
        tail_length_=unsigned(std::lround(std::clamp(spillover_time_value_->load(std::memory_order_relaxed),1.0f,10.0f)*48000));
        tail_position_=0;tail_input_remaining_=input_fade_samples;tail_start_gain_=1;tail_shortened_=false;
        engine_initialized_=false;
    }
    auto& slot=slots_[active_slot_];
    if(!engine_initialized_ || (requested!=active_program_ && (isXL(requested) || isXL(active_program_)))) {
        if(isXL(requested)) slot.xl_engine.select(rom_bank_->xlBank(),unsigned(requested-native_hall::program_count));
        else slot.engine.activate(rom_bank_->bank(),unsigned(requested));
        slot.alignment.fill({});slot.alignment_position=0;
        previous_.fill(std::numeric_limits<float>::quiet_NaN());engine_initialized_=true;
    }
    active_program_=slot.program=requested;
    bool changed=false;for(unsigned i=0;i<v.size();++i) changed|=v[i]!=previous_[i];
    if(changed) {previous_=v;applyControls(v);}
}
void NativeHallProcessor::applyControls(const std::array<float,parameter_count>& v) {
    const float dirt=1-v[9],clean=1-dirt*dirt;
    if(isXL(active_program_)) {
        const auto& data=rom_bank_->xlBank().programs[unsigned(active_program_)-native_hall::program_count];
        auto controls=data.controls.factory;
        for(unsigned cell=0;cell<controls.size();++cell) if(v[xl_parameter_begin+cell]>=0)
            controls[cell]=uint8_t(std::clamp(int(v[xl_parameter_begin+cell]),0,255));
        if(data.chorus_page) controls[data.pages[data.chorus_page-1].cells[data.chorus_slot]]=uint8_t(unsigned(v[15])*8);
        if(data.diffusion_page) controls[data.pages[data.diffusion_page-1].cells[data.diffusion_slot]]=uint8_t(unsigned(v[16])*4);
        data.resolve_controls(controls);
        slots_[active_slot_].xl_engine.controls(controls,v[10]>=0.5f,v[7],v[8],clean,int(v[12]),int(v[13]),v[11]>=0.5f);
    }
    else {
        native_hall::Parameters p;
        p.hall.bass=int(v[0]);p.hall.mid=int(v[1]);p.hall.crossover=int(v[2]);p.hall.treble=int(v[3]);
        p.program=unsigned(std::clamp(active_program_,0,int(native_hall::program_count)-1));
        p.hall.depth=int(v[4]);p.hall.predelay_ms=int(v[5])-24+native_hall::predelay_minima[p.program];
        p.hall.diffusion=int(v[6]);
        p.input_db=v[7];p.mix=v[8];p.analog=true;p.dirt=dirt*dirt;p.hall.mode_enhancement=v[10]>=0.5f;
        p.hall.decay_optimization=v[11]>=0.5f;p.output_left=int(v[12]);p.output_right=int(v[13]);
        slots_[active_slot_].engine.set_parameters(p);
    }
}
void NativeHallProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    // One bounded attempt. A writer in progress leaves the previous complete
    // settings in force; audio never waits, retries, or reads preset files.
    auto v=stable_values_;
    const unsigned transaction=parameter_transaction_.load(std::memory_order_acquire);
    if((transaction&1u)==0) {
        std::array<float,parameter_count> candidate;
        for(unsigned i=0;i<candidate.size();++i) candidate[i]=values_[i]->load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        if(parameter_transaction_.load(std::memory_order_acquire)==transaction) stable_values_=v=candidate;
    }
    const int requested=std::clamp(int(v[14]),0,int(program_count)-1);
    if(!programAvailable(requested)) {
        if(getTotalNumInputChannels()==1) b.copyFrom(1,0,b,0,0,b.getNumSamples());
        engine_initialized_=false;active_program_=-1;tail_slot_=-1;return;
    }
    const bool spill=spillover_value_->load(std::memory_order_relaxed)>=0.5f;
    // Keep the common mixer after its first use until the stream is prepared
    // again. Newly activated kernels start their private mix smoothing at 1;
    // returning to that mixer during an early disable would jump in level.
    transition_mixing_|=spill;
    if(!spill && tail_slot_>=0) shortenTail();
    const bool pending=spill && engine_initialized_ && requested!=active_program_ && tail_slot_>=0;
    if(pending) shortenTail();
    else selectProgram(requested,v,spill);
    const bool low=low_latency_active_.load(std::memory_order_relaxed);
    const int right_channel=getTotalNumInputChannels()==1?0:1;
    for(int offset=0;offset<b.getNumSamples();) {
        const int count=std::min(direct_input_.getNumSamples(),b.getNumSamples()-offset);
        // Preserve both inputs before in-place rendering, including mono.
        if(low) {
            direct_input_.copyFrom(0,0,b,0,offset,count);
            direct_input_.copyFrom(1,0,b,right_channel,offset,count);
        }
        auto* left=b.getWritePointer(0,offset);auto* right=b.getWritePointer(1,offset);
        bridge_.process(left,b.getReadPointer(right_channel,offset),left,right,count,
            [&](const float* l,const float* r,float* ol,float* ore,int n) {
                for(int i=0;i<n;++i) {
                    // A rapid third selection waits only for the oldest tail's
                    // bounded 20 ms retirement, then switches at an internal sample.
                    if(pending && tail_slot_<0 && active_program_!=requested) selectProgram(requested,v,true);
                    const float il=std::isfinite(l[i])?l[i]:0,ir=std::isfinite(r[i])?r[i]:0;
                    const auto dry=transition_dry_[transition_dry_position_];
                    transition_dry_[transition_dry_position_]={il,ir};
                    transition_dry_position_=(transition_dry_position_+1)%transition_dry_.size();
                    transition_mix_+=0.002f*(v[8]-transition_mix_);
                    const bool separate_wet=transition_mixing_;
                    const float old_input=tail_slot_>=0?float(tail_input_remaining_)/input_fade_samples:0;
                    slots_[active_slot_].process(il*(1-old_input),ir*(1-old_input),ol[i],ore[i],low || separate_wet);
                    if(tail_slot_>=0) {
                        float tl,tr;slots_[unsigned(tail_slot_)].process(il*old_input,ir*old_input,tl,tr,true);
                        const float gain=tailGain();ol[i]+=tl*gain;ore[i]+=tr*gain;
                        if(tail_input_remaining_) --tail_input_remaining_;
                        if(++tail_position_>=tail_length_) {tail_slot_=-1;tail_input_remaining_=0;}
                    }
                    if(separate_wet && !low) {
                        ol[i]=dry[0]*(1-transition_mix_)+ol[i]*transition_mix_;
                        ore[i]=dry[1]*(1-transition_mix_)+ore[i]*transition_mix_;
                    }
                }
            });
        for(int i=0;i<count;++i) {
            host_mix_+=host_mix_coefficient_*(v[8]-host_mix_);
            if(std::abs(host_mix_-v[8])<1e-5f) host_mix_=v[8];
            if(low) {
                const float dl=direct_input_.getSample(0,i),dr=direct_input_.getSample(1,i);
                left[i]=(std::isfinite(dl)?dl:0)*(1-host_mix_)+left[i]*host_mix_;
                right[i]=(std::isfinite(dr)?dr:0)*(1-host_mix_)+right[i]*host_mix_;
            }
        }
        offset+=count;
    }
}
int NativeHallProcessor::getCurrentProgram() {
    // Parameter listeners can run before APVTS publishes its raw mirror.
    // Read the choice's own atomic value so the display changes immediately.
    return algorithm_parameter_->getIndex();
}
void NativeHallProcessor::setCurrentProgram(int program) {
    if(program<0 || program>=getNumPrograms() || program==getCurrentProgram()) return;
    const juce::ScopedLock lock(preset_write_lock_);
    parameter_transaction_.fetch_add(1,std::memory_order_acq_rel);
    preset_recall_revision_.fetch_add(1,std::memory_order_release);
    if(isXL(program) && rom_bank_->xlReady()) {
        const auto& data=rom_bank_->xlBank().programs[unsigned(program)-native_hall::program_count];
        for(unsigned alias=0;alias<2;++alias) {
            auto* parameter=state.getParameter(ids[15+alias]);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(float(alias?data.diffusion_index:data.chorus)));
        }
        for(unsigned cell=0;cell<48;++cell) {
            auto* parameter=state.getParameter(ids[xl_parameter_begin+cell]);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(float(data.controls.factory[cell])));
        }
    }
    int left=0,right=2;
    if(isXL(program)) {
        const auto graph=cineol::xl::Graph(unsigned(program)-native_hall::program_count);
        if(graph==cineol::xl::Graph::chorus_echo) {left=2;right=0;}
        else if(unsigned(graph)>=unsigned(cineol::xl::Graph::hall_hall)) right=1;
    }
    for(unsigned channel=0;channel<2;++channel) {
        auto* route=state.getParameter(ids[12+channel]);
        route->setValueNotifyingHost(route->convertTo0to1(float(channel?right:left)));
    }
    auto* parameter=state.getParameter("algorithm");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(float(program)));
    parameter_transaction_.fetch_add(1,std::memory_order_release);
}
const juce::String NativeHallProcessor::getProgramName(int program) {
    if(program<0 || program>=getNumPrograms()) return {};
    if(isXL(program)) return juce::String(cineol::xl::graphs[unsigned(program)-native_hall::program_count].name);
    return algorithm_names[program];
}
juce::AudioProcessorEditor* NativeHallProcessor::createEditor() {return new CineolEditor(*this);}
bool NativeHallProcessor::usesXL() const noexcept {return isXL(algorithm_parameter_->getIndex());}
const cineol::xl::ProgramData* NativeHallProcessor::xlProgram() const noexcept {
    const int program=algorithm_parameter_->getIndex();
    return isXL(program) && rom_bank_->xlReady()?&rom_bank_->xlBank().programs[unsigned(program)-native_hall::program_count]:nullptr;
}
unsigned NativeHallProcessor::parameterPages() const noexcept {if(const auto* data=xlProgram()) return data->page_count;return 2;}
uint8_t NativeHallProcessor::xlControl(unsigned cell) const noexcept {
    if(cell>=48) return 0;
    if(const auto* data=xlProgram()) {
        if(data->chorus_page && data->pages[data->chorus_page-1].cells[data->chorus_slot]==cell)
            return uint8_t(values_[15]->load(std::memory_order_relaxed)*8);
        if(data->diffusion_page && data->pages[data->diffusion_page-1].cells[data->diffusion_slot]==cell)
            return uint8_t(values_[16]->load(std::memory_order_relaxed)*4);
    }
    const auto value=values_[xl_parameter_begin+cell]->load(std::memory_order_relaxed);
    if(value<0) {if(const auto* data=xlProgram()) return data->controls.factory[cell];return 0;}
    return uint8_t(std::clamp(int(value),0,255));
}
juce::String NativeHallProcessor::firmwareId() const {return usesXL()?"224xl-v8.21":"224-v4.4";}
void NativeHallProcessor::getStateInformation(juce::MemoryBlock& block) {
    const juce::ScopedLock lock(preset_write_lock_);
    state.state.setProperty("firmware_id",firmwareId(),nullptr);
    if(auto xml=state.copyState().createXml()) copyXmlToBinary(*xml,block);
}
void NativeHallProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size)) if(xml->hasTagName(state.state.getType())) {
        auto restored=juce::ValueTree::fromXml(*xml);
        const auto identity=restored.getProperty("firmware_id",juce::String(cineol::current_firmware.id.data())).toString();
        const auto* firmware=cineol::find_firmware(identity.toStdString());
        if(!firmware || !firmware->selectable) return;
        restored.setProperty("firmware_id",identity,nullptr);
        // A v0.2 session always used Large Concert Hall B, even if another
        // algorithm is currently selected in this instance.
        if(!restored.getChildWithProperty("id","algorithm").isValid()) {
            juce::ValueTree algorithm("PARAM");algorithm.setProperty("id","algorithm",nullptr);
            algorithm.setProperty("value",2.0f,nullptr);restored.appendChild(algorithm,nullptr);
        }
        // Loading an older session into an already-enabled instance must
        // restore the original compensated mode, not keep the new setting.
        if(!restored.getChildWithProperty("id","low_latency").isValid()) {
            juce::ValueTree low("PARAM");low.setProperty("id","low_latency",nullptr);
            low.setProperty("value",0.0f,nullptr);restored.appendChild(low,nullptr);
        }
        for(const char* id:{"spillover","spillover_time"}) if(!restored.getChildWithProperty("id",id).isValid()) {
            juce::ValueTree value("PARAM");value.setProperty("id",id,nullptr);
            value.setProperty("value",juce::String(id)=="spillover"?0.0f:5.0f,nullptr);restored.appendChild(value,nullptr);
        }
        for(const char* id:{"xl_chorus","xl_diffusion"}) if(!restored.getChildWithProperty("id",id).isValid()) {
            juce::ValueTree value("PARAM");value.setProperty("id",id,nullptr);value.setProperty("value",16.0f,nullptr);restored.appendChild(value,nullptr);
        }
        for(unsigned cell=0;cell<48;++cell) if(!restored.getChildWithProperty("id",ids[xl_parameter_begin+cell]).isValid()) {
            juce::ValueTree value("PARAM");value.setProperty("id",ids[xl_parameter_begin+cell],nullptr);
            value.setProperty("value",-1.0f,nullptr);restored.appendChild(value,nullptr);
        }
        const juce::ScopedLock lock(preset_write_lock_);
        parameter_transaction_.fetch_add(1,std::memory_order_acq_rel);
        preset_recall_revision_.fetch_add(1,std::memory_order_release);
        state.replaceState(restored);
        parameter_transaction_.fetch_add(1,std::memory_order_release);
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {return new NativeHallProcessor;}
