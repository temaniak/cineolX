#include "plugin/Processor.hpp"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <new>
#include <chrono>
static bool tracking=false;static unsigned allocations=0;
void* operator new(size_t n){if(tracking)++allocations;if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
static void require(bool b,const char*s){if(!b)throw std::runtime_error(s);}
static void set(CleanABProcessor&p,unsigned i,float v){auto*q=p.state.getParameter(CleanABProcessor::id(i));q->setValueNotifyingHost(q->convertTo0to1(v));}
int main(int argc,char**argv)try{
 juce::ScopedJuceInitialiser_GUI gui;juce::SharedResourcePointer<CineolRomBank> bank;require(bank->ready()&&bank->xlReady(),"Set CINEOL224_CACHE_DIR to a cache containing both models");
 auto p=std::make_unique<CleanABProcessor>();
 if(argc==2 && std::string(argv[1])=="--cpu"){
  std::cout<<"program,median_callback_percent,max_observed_block_ms\n";
  juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
  for(unsigned program=0;program<7;++program){set(*p,0,float(program));p->prepareToPlay(48000,128);std::array<double,5>samples{};double maximum=0;
   for(unsigned repeat=0;repeat<5;++repeat){double elapsed=0;for(unsigned block=0;block<250;++block){for(int n=0;n<128;++n){audio.setSample(0,n,.06f*std::sin((block*128+n)*.17f));audio.setSample(1,n,.04f*std::sin((block*128+n)*.11f));}const auto begin=std::chrono::steady_clock::now();p->processBlock(audio,midi);const double dt=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();if(block>=50)elapsed+=dt;maximum=std::max(maximum,dt*1000);}samples[repeat]=100*elapsed/(200*128/48000.);}std::sort(samples.begin(),samples.end());std::cout<<program<<','<<samples[2]<<','<<maximum<<'\n';}
  return 0;
 }
 auto e=std::make_unique<cineol::clean_ab::Engines>();juce::AudioBuffer<float>b(2,128);juce::MidiBuffer midi;
 for(unsigned program=0;program<7;++program)for(unsigned ab=0;ab<2;++ab){set(*p,0,float(program));set(*p,1,float(ab));p->prepareToPlay(48000,128);require(p->ready(program)&&p->getLatencySamples()==70,"prepare/latency failed");e->prepare(&bank->bank(),&bank->xlBank());e->select(program);
  if(program<3){native_hall::Controls c;c.mode_enhancement=c.decay_optimization=false;c.predelay_ms=native_hall::predelay_minima[cineol::clean_ab::indices[program]];c.modulation_period=2;c.modulation_step=1;c.optimization_response=5;e->original_controls(c);}else e->xl_controls(bank->xlBank().programs[cineol::clean_ab::indices[program]].controls.factory,false,false);
  for(unsigned block=0;block<100;++block){std::array<std::array<float,2>,128>expected;
   for(unsigned n=0;n<128;++n){float l=.06f*std::sin((block*128+n)*.17f),r=.04f*std::sin((block*128+n)*.11f);b.setSample(0,int(n),l);b.setSample(1,int(n),r);const auto v=e->process(l,r);expected[n]={v[2*ab],v[2*ab+1]};}
   tracking=true;p->processBlock(b,midi);tracking=false;
   for(unsigned n=0;n<128;++n)for(unsigned c=0;c<2;++c)require(std::abs(b.getSample(int(c),int(n))-expected[n][c])<2e-8,"A/B processor routing differs from independent engine");
  }
  // A/B switching while silence enters does not reset either engine.
  float blend=float(ab);
  for(unsigned block=0;block<30;++block){float target=float((block/5)&1);set(*p,1,target);b.clear();std::array<std::array<float,2>,128>expected;
   for(unsigned n=0;n<128;++n){auto v=e->process(0,0);blend+=std::clamp(target-blend,-1.f/480,1.f/480);for(unsigned c=0;c<2;++c)expected[n][c]=(1-blend)*v[c]+blend*v[c+2];}
   tracking=true;p->processBlock(b,midi);tracking=false;for(unsigned n=0;n<128;++n)for(unsigned c=0;c<2;++c)require(std::abs(b.getSample(int(c),int(n))-expected[n][c])<2e-8,"A/B switch resets or alters tails");
  }
 }
 set(*p,0,6);set(*p,1,1);set(*p,4,-2.3f);set(*p,16,123);set(*p,5,1);juce::MemoryBlock saved;p->getStateInformation(saved);auto restored=std::make_unique<CleanABProcessor>();restored->setStateInformation(saved.getData(),int(saved.getSize()));
 for(unsigned id:{0u,1u,4u,5u,16u})require(restored->state.getRawParameterValue(CleanABProcessor::id(id))->load()==p->state.getRawParameterValue(CleanABProcessor::id(id))->load(),"saved state changed");
 for(double rate:{44100.,48000.,96000.})for(int block:{1,128,511}){
  restored->prepareToPlay(rate,block);b.setSize(2,block);
  for(unsigned program=0;program<7;++program){set(*restored,0,float(program));set(*restored,1,float(program&1));for(unsigned i=0;i<10;++i){b.clear();b.setSample(0,0,.1f);tracking=true;restored->processBlock(b,midi);tracking=false;for(int c=0;c<2;++c)for(int n=0;n<block;++n)require(std::isfinite(b.getSample(c,n)),"rate/control switch nonfinite");}}
 }
 // Dry alignment, including mono input duplicated to stereo output.
 require(restored->setBusesLayout(juce::AudioProcessor::BusesLayout{{juce::AudioChannelSet::mono()},{juce::AudioChannelSet::stereo()}}),"mono layout rejected");
 set(*restored,3,0);set(*restored,15,0);restored->prepareToPlay(48000,128);b.setSize(2,128);std::array<float,70>dry{};unsigned pos=0;
 for(unsigned block=0;block<20;++block){std::array<float,128>expected;for(unsigned n=0;n<128;++n){float x=.1f*std::sin((block*128+n)*.13f);b.setSample(0,int(n),x);expected[n]=dry[pos];dry[pos]=x;pos=(pos+1)%70;}tracking=true;restored->processBlock(b,midi);tracking=false;for(int n=0;n<128;++n)for(int c=0;c<2;++c)require(b.getSample(c,n)==expected[unsigned(n)],"dry/mono alignment changed");}
 auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p->createEditor());require(editor->getWidth()>0,"editor missing");for(unsigned program=0;program<7;++program){set(*p,0,float(program));juce::MessageManager::getInstance()->runDispatchLoopUntil(120);
 if(argc==2){auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds());juce::File path=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]).getChildFile("ui-"+juce::String(program)+".png");auto stream=path.createOutputStream();require(stream!=nullptr,"cannot write UI snapshot");juce::PNGImageFormat png;require(png.writeImageToStream(snapshot,*stream),"cannot encode UI snapshot");}
 }
 require(allocations==0,"callback or algorithm switch allocated");std::cout<<"All 7 A/B routes and retained tails; 44.1/48/96 kHz; state; mono/dry; editor; callback allocations="<<allocations<<'\n';
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}
