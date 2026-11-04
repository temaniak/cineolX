#include "plugin/Processor.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <new>

using namespace cineol::pitch_lab;
static thread_local bool counting=false;
static size_t allocations=0,releases=0;
void* operator new(size_t n){if(counting)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(counting&&p)++releases;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,size_t)noexcept{::operator delete(p);}
static void require(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
static std::vector<float> render(double rate,const std::vector<unsigned>& blocks,bool mono=false) {
    auto p=std::make_unique<PitchLabProcessor>();
    if(mono){auto b=p->getBusesLayout();b.inputBuses.set(0,juce::AudioChannelSet::mono());require(p->setBusesLayout(b),"mono bus");}
    auto v=preset(0);v[output_db]=0;p->setValues(v);p->prepareToPlay(rate,128);
    require(p->ready(),"prepare");
    constexpr unsigned frames=48000;std::vector<float> result(frames*2);juce::AudioBuffer<float> audio(2,2048);juce::MidiBuffer midi;
    unsigned t=0,index=0;
    while(t<frames) {
        const unsigned n=std::min(blocks[index++%blocks.size()],frames-t);audio.setSize(2,int(n),false,false,true);
        for(unsigned i=0;i<n;++i){const float x=t+i<12000?.08f*std::sin((t+i)*.13f):0;audio.setSample(0,int(i),x);audio.setSample(1,int(i),mono?99:x);}
        counting=true;p->processBlock(audio,midi);counting=false;
        for(unsigned c=0;c<2;++c)for(unsigned i=0;i<n;++i){const float x=audio.getSample(int(c),int(i));require(std::isfinite(x),"nonfinite processor output");result[(t+i)*2+c]=x;}
        t+=n;
    }
    return result;
}
static void manifest(PitchLabProcessor& p,const char* destination) {
    juce::DynamicObject::Ptr root=new juce::DynamicObject;juce::Array<juce::var> params,presets;
    root->setProperty("plugin","Cineol Pitch Lab");root->setProperty("reference","Signalsmith Stretch 1.3.2");
    for(unsigned i=0;i<count;++i) {
        const auto& d=definitions[i];juce::DynamicObject::Ptr a=new juce::DynamicObject;
        a->setProperty("index",int(i));a->setProperty("id",d.id);a->setProperty("name",d.name);a->setProperty("min",d.lo);a->setProperty("max",d.hi);
        a->setProperty("step",d.step);a->setProperty("skew",d.skew);a->setProperty("default",d.initial);params.add(a.get());
    }
    for(unsigned n=0;n<6;++n) {
        const auto v=preset(n);juce::DynamicObject::Ptr a=new juce::DynamicObject,b=new juce::DynamicObject;
        for(unsigned i=0;i<count;++i)b->setProperty(definitions[i].id,p.state.getParameter(definitions[i].id)->convertTo0to1(v[i]));
        a->setProperty("name",preset_names[n]);a->setProperty("normalized",b.get());presets.add(a.get());
    }
    root->setProperty("parameters",params);root->setProperty("presets",presets);
    require(juce::File(juce::String(destination)).replaceWithText(juce::JSON::toString(root.get(),true)),"manifest write");
}
int main(int argc,char** argv)try {
    juce::ScopedJuceInitialiser_GUI gui;auto p=std::make_unique<PitchLabProcessor>();
    if(argc>2 && juce::String(argv[1])=="--manifest"){manifest(*p,argv[2]);return 0;}
    p->prepareToPlay(48000,128);require(p->ready(),"pitch must work without Hall bank");
    const int latency=p->getLatencySamples();require(latency>1000,"latency not reported");
    for(unsigned i=0;i<count;++i){auto* param=p->state.getParameter(definitions[i].id);param->setValueNotifyingHost(param->convertTo0to1(definitions[i].lo+(definitions[i].hi-definitions[i].lo)*.63f));}
    juce::MemoryBlock saved;p->getStateInformation(saved);auto restored=std::make_unique<PitchLabProcessor>();restored->setStateInformation(saved.getData(),int(saved.getSize()));
    for(const auto& d:definitions)require(std::abs(p->state.getParameter(d.id)->getValue()-restored->state.getParameter(d.id)->getValue())<1e-6f,"parameter state roundtrip");
    for(double rate:{44100.,48000.,96000.}) {
        const auto a=render(rate,{128}),b=render(rate,{1,127,511,2048,17});float error=0;
        for(size_t i=0;i<a.size();++i)error=std::max(error,std::abs(a[i]-b[i]));
        require(error<3e-7f,"host block partition changed audio");std::cout<<"rate="<<rate<<" partition error="<<error<<'\n';
    }
    const auto a=render(48000,{128}),b=render(48000,{128},true);require(a==b,"mono differs from duplicated stereo");
    auto v=preset(0);v[monitor]=1;v[output_db]=0;p->setValues(v);p->prepareToPlay(48000,128);
    juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;unsigned n=0;
    for(unsigned block=0;block<150;++block) {
        for(unsigned j=0;j<128;++j)for(unsigned ch=0;ch<2;++ch)audio.setSample(int(ch),int(j),n+j==0?.1f:0);
        counting=true;p->processBlock(audio,midi);counting=false;
        for(unsigned j=0;j<128;++j)for(unsigned ch=0;ch<2;++ch)require(std::abs(audio.getSample(int(ch),int(j))-(n+j==unsigned(latency)?.1f:0))<1e-8f,"input monitor latency");n+=128;
    }
    p->requestPanic();counting=true;p->processBlock(audio,midi);counting=false;
    require(allocations==0&&releases==0,"callback or Panic allocates/releases");
    auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p->createEditor());
    if(argc>1) {
        juce::File destination{juce::String(argv[1])};destination.getParentDirectory().createDirectory();auto stream=destination.createOutputStream();
        require(bool(stream),"editor image file");stream->setPosition(0);require(juce::PNGImageFormat{}.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),*stream),"editor image");stream->truncate();
    }
    std::cout<<"state, fixed monitor latency, mono, 44.1/48/96 kHz, variable blocks, editor and Panic pass; latency="<<latency<<" allocations="<<allocations<<" releases="<<releases<<'\n';
}catch(const std::exception& e){counting=false;std::cerr<<e.what()<<'\n';return 1;}
