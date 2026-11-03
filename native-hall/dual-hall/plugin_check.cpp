#include "plugin/Processor.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <limits>
#include <new>
#include <stdexcept>
#include <cstdlib>
#include <vector>

static thread_local bool counting=false;
static thread_local size_t allocations=0,releases=0;
void* operator new(size_t n){if(counting)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(counting && p)++releases;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,size_t)noexcept{::operator delete(p);}
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void set(DualHallProcessor& p,const char* id,float value) {
    auto* parameter=p.state.getParameter(id);require(parameter,"missing parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
static void process(DualHallProcessor& p,juce::AudioBuffer<float>& b,juce::MidiBuffer& midi) {
    counting=true;p.processBlock(b,midi);counting=false;
}
static std::vector<float> render(double rate,const std::vector<int>& blocks) {
    auto p=std::make_unique<DualHallProcessor>();p->prepareToPlay(rate,128);
    require(p->ready(),"host-rate prepare failed");
    constexpr unsigned frames=48000;
    std::vector<float> output(2*frames);juce::AudioBuffer<float> b(2,4096);juce::MidiBuffer midi;
    unsigned position=0,index=0;
    while(position<frames) {
        const unsigned count=std::min(unsigned(blocks[index++%blocks.size()]),frames-position);
        b.setSize(2,int(count),false,false,true);
        for(unsigned n=0;n<count;++n) {
            const unsigned t=position+n;
            b.setSample(0,int(n),t<12000?.07f*std::sin(t*.13f):0);
            b.setSample(1,int(n),t<9000?.03f*std::sin(t*.19f):0);
        }
        process(*p,b,midi);
        for(unsigned c=0;c<2;++c)for(unsigned n=0;n<count;++n) {
            const float v=b.getSample(int(c),int(n));require(std::isfinite(v),"host-rate nonfinite output");
            output[(position+n)*2+c]=v;
        }
        position+=count;
    }
    require(p->underruns()==0,"host bridge underrun");return output;
}
int main(int argc,char** argv)try {
    juce::ScopedJuceInitialiser_GUI gui;
    auto p=std::make_unique<DualHallProcessor>();juce::AudioBuffer<float> buffer(2,128);juce::MidiBuffer midi;
    if(argc>1 && juce::String(argv[1])=="--missing-bank") {
        p->prepareToPlay(48000,128);require(!p->ready(),"unexpected bank in missing-bank fixture");
        for(int c=0;c<2;++c)for(int n=0;n<128;++n)buffer.setSample(c,n,.25f);
        process(*p,buffer,midi);require(buffer.getMagnitude(0,128)==0,"missing bank did not mute safely");
        require(allocations==0 && releases==0,"missing-bank process allocates/releases");
        std::cout<<"missing-bank silence passed\n";return 0;
    }
    juce::SharedResourcePointer<DualHallBank> bank;require(bank->ready(),"set CINEOL224_CACHE_DIR to an existing original bank cache");
    // Actual processor output against an independent wrapper, including live
    // common controls and source-silent crossfeedback automation.
    auto reference=std::make_unique<cineol::dual_hall::Tanks>();
    native_hall::Controls c;c.mode_enhancement=c.decay_optimization=false;
    c.modulation_period=1;c.modulation_step=4;c.optimization_response=5;
    set(*p,"mode_enh",0);set(*p,"decay_opt",0);set(*p,"crossfeedback",.25f);
    p->prepareToPlay(48000,128);reference->prepare(bank->data,c,.25f);
    require(p->getLatencySamples()==70,"incorrect 48 kHz latency");
    for(unsigned block=0;block<800;++block) {
        if(block==150){set(*p,"diffusion",37);set(*p,"mod_depth",3);c.diffusion=37;c.modulation_step=12;reference->set_controls(c);}
        if(block==400){set(*p,"crossfeedback",.5f);reference->set_crossfeedback(.5f);}
        if(block==600){set(*p,"crossfeedback",0);reference->set_crossfeedback(0);}
        std::array<std::array<float,2>,128> expected;
        for(unsigned n=0;n<128;++n) {
            const unsigned t=block*128+n;
            const float l=t<24000?.06f*std::sin(t*.17f):0,r=t<18000?.02f*std::sin(t*.11f):0;
            expected[n]=reference->process(l,r);buffer.setSample(0,int(n),l);buffer.setSample(1,int(n),r);
        }
        process(*p,buffer,midi);
        for(unsigned ch=0;ch<2;++ch)for(unsigned n=0;n<128;++n)
            require(buffer.getSample(int(ch),int(n))==expected[n][ch],"processor/core routing or tail automation differs");
    }
    std::cout<<"actual processor matches independent dual-tank core exactly, including live common controls and tail coupling changes\n";
    // Every parameter, including both mode switches, survives a fresh restore.
    for(unsigned i=0;i<std::size(DualHallProcessor::ids);++i) {
        auto* param=p->state.getParameter(DualHallProcessor::ids[i]);
        const float raw=param->convertFrom0to1(float((i*7)%19)/18);
        param->setValueNotifyingHost(param->convertTo0to1(raw));
    }
    juce::MemoryBlock saved;p->getStateInformation(saved);
    auto restored=std::make_unique<DualHallProcessor>();restored->setStateInformation(saved.getData(),int(saved.getSize()));
    for(const char* id:DualHallProcessor::ids) {
        const float before=p->state.getParameter(id)->getValue(),after=restored->state.getParameter(id)->getValue();
        if(std::abs(before-after)>=1e-6f)std::cerr<<"state mismatch "<<id<<" normalized before="<<before<<" after="<<after<<'\n';
        require(std::abs(before-after)<1e-6f,"parameter state not restored");
    }
    require(p->state.state.getType()==juce::Identifier("CineolDualHallB"),"non-isolated state identifier");
    // The explicit Clear tails action resets only histories, keeping the
    // current parameters; its result matches a freshly prepared processor.
    restored->prepareToPlay(48000,128);
    for(unsigned block=0;block<50;++block){buffer.clear();buffer.setSample(0,0,.08f);process(*restored,buffer,midi);}
    auto cleared=std::make_unique<DualHallProcessor>();cleared->setStateInformation(saved.getData(),int(saved.getSize()));
    cleared->prepareToPlay(48000,128);restored->request_clear_tails();
    juce::AudioBuffer<float> fresh(2,128);
    for(unsigned block=0;block<10;++block) {
        buffer.clear();fresh.clear();process(*restored,buffer,midi);process(*cleared,fresh,midi);
        for(int ch=0;ch<2;++ch)for(int n=0;n<128;++n)
            require(buffer.getSample(ch,n)==fresh.getSample(ch,n),"clear tails differs from fresh histories");
    }
    // The bridge must produce the same signal when the host partitions audio
    // differently, including blocks larger than the prepared capacity.
    for(double rate:{44100.,48000.,96000.}) {
        const auto regular=render(rate,{128}),single=render(rate,{1}),variable=render(rate,{1,127,511,4096,17});
        double error=0;
        for(size_t i=0;i<regular.size();++i)error=std::max({error,double(std::abs(regular[i]-single[i])),double(std::abs(regular[i]-variable[i]))});
        require(error<2e-7,"host block partition changed coupled audio");
        std::cout<<"host_rate="<<rate<<" block_partition_max_error="<<error<<'\n';
    }
    // Exact compensated dry path, independent of source excitation gain.
    set(*p,"mix",0);set(*p,"output_db",0);set(*p,"input_db",-12);p->prepareToPlay(48000,128);
    std::array<std::array<float,2>,70> delay{};unsigned position=0;
    for(unsigned block=0;block<30;++block) {
        std::array<std::array<float,2>,128> expected;
        for(unsigned n=0;n<128;++n) {
            const float l=.07f*std::sin((block*128+n)*.17f),r=.03f*std::sin((block*128+n)*.11f);
            expected[n]=delay[position];delay[position]={l,r};if(++position==delay.size())position=0;
            buffer.setSample(0,int(n),l);buffer.setSample(1,int(n),r);
        }
        process(*p,buffer,midi);
        for(unsigned ch=0;ch<2;++ch)for(unsigned n=0;n<128;++n)
            require(buffer.getSample(int(ch),int(n))==expected[n][ch],"dry path or latency differs");
    }
    // Mono input must be identical to duplicated stereo input, even when the
    // unused host channel contains unrelated data.
    auto mono=std::make_unique<DualHallProcessor>(),stereo=std::make_unique<DualHallProcessor>();
    auto layout=mono->getBusesLayout();layout.inputBuses.set(0,juce::AudioChannelSet::mono());
    require(mono->setBusesLayout(layout),"mono input rejected");
    mono->prepareToPlay(48000,128);stereo->prepareToPlay(48000,128);juce::AudioBuffer<float> other(2,128);
    for(unsigned block=0;block<100;++block) {
        for(unsigned n=0;n<128;++n) {
            const float x=.06f*std::sin((block*128+n)*.17f);
            buffer.setSample(0,int(n),x);buffer.setSample(1,int(n),99);
            other.setSample(0,int(n),x);other.setSample(1,int(n),x);
        }
        process(*mono,buffer,midi);process(*stereo,other,midi);
        for(unsigned ch=0;ch<2;++ch)for(unsigned n=0;n<128;++n)
            require(buffer.getSample(int(ch),int(n))==other.getSample(int(ch),int(n)),"mono channel indexing differs");
    }
    // NaN/Inf must not poison either return history.
    buffer.clear();buffer.setSample(0,0,std::numeric_limits<float>::quiet_NaN());
    buffer.setSample(1,1,std::numeric_limits<float>::infinity());process(*stereo,buffer,midi);
    for(unsigned ch=0;ch<2;++ch)for(unsigned n=0;n<128;++n)require(std::isfinite(buffer.getSample(int(ch),int(n))),"nonfinite input poisoned output");
    require(allocations==0 && releases==0,"audio callback allocated or released C++ heap memory");
    auto editor=std::unique_ptr<juce::AudioProcessorEditor>(stereo->createEditor());
    require(editor->getWidth()==1120 && editor->getHeight()==650,"editor dimensions");
    if(argc>1) {
        const juce::File destination{juce::String(argv[1])};require(destination.getParentDirectory().createDirectory().wasOk(),"screenshot folder");
        auto stream=destination.createOutputStream();require(bool(stream),"screenshot stream");stream->setPosition(0);
        require(juce::PNGImageFormat{}.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),*stream),"screenshot encode");stream->truncate();
    }
    std::cout<<"state, dry alignment, mono, nonfinite guards, GUI and allocation checks pass; allocations="<<allocations<<" releases="<<releases<<'\n';
    if(argc<3 || juce::String(argv[2])!="--cpu")return 0;
    // One assembled-build CPU observation, after correctness checks. Input,
    // allocation, preparation and UI rendering are outside timed callbacks.
    std::vector<double> loads;double worst_ms=0;
    stereo->prepareToPlay(48000,128);
    for(unsigned repeat=0;repeat<5;++repeat) {
        double seconds=0;
        for(unsigned block=0;block<250;++block) {
            for(unsigned n=0;n<128;++n){buffer.setSample(0,int(n),.05f*std::sin((block*128+n)*.13f));buffer.setSample(1,int(n),.04f*std::sin((block*128+n)*.19f));}
            const auto start=std::chrono::steady_clock::now();stereo->processBlock(buffer,midi);const auto end=std::chrono::steady_clock::now();
            if(block>=50){const double t=std::chrono::duration<double>(end-start).count();seconds+=t;worst_ms=std::max(worst_ms,t*1000);}
        }
        loads.push_back(seconds/(200.*128/48000)*100);
    }
    std::sort(loads.begin(),loads.end());
    std::cout<<"callback_cpu_median_percent="<<loads[2]<<" worst_observed_ms="<<worst_ms<<" budget_ms="<<128000./48000<<'\n';
}catch(const std::exception& e){counting=false;std::cerr<<e.what()<<'\n';return 1;}
