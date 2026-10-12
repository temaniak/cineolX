#include "plugin/Processor.hpp"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <new>

static bool count_allocations=false;
static size_t allocations=0;
void* operator new(size_t n) {if(count_allocations)++allocations;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,size_t) noexcept {std::free(p);}
void operator delete[](void* p,size_t) noexcept {std::free(p);}
static void require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
static void set(FloatHallABProcessor& p,const char* id,float value) {
    auto* parameter=p.state.getParameter(id);require(parameter,"missing parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
int main() try {
    juce::ScopedJuceInitialiser_GUI gui;
    auto plugin=std::make_unique<FloatHallABProcessor>();
    juce::SharedResourcePointer<CineolRomBank> bank;
    require(bank->ready(),"set CINEOL224_CACHE_DIR to an existing valid bank cache");
    auto original=std::make_unique<native_hall::DesktopEngine48>();original->prepare(bank->bank(),2);
    native_hall::Parameters params;params.hall.mode_enhancement=params.hall.decay_optimization=false;
    original->set_parameters(params);
    plugin->prepareToPlay(48000,128);require(plugin->ready(),"plugin not ready");
    require(plugin->getLatencySamples()==70,"48 kHz latency changed");
    juce::AudioBuffer<float> buffer(2,128);juce::MidiBuffer midi;
    double error=0;
    for(unsigned block=0;block<400;++block) {
        std::array<std::array<float,2>,128> expected;
        for(unsigned n=0;n<128;++n) {
            const float x=.07f*std::sin((block*128+n)*.17f),y=.04f*std::sin((block*128+n)*.11f);
            buffer.setSample(0,int(n),x);buffer.setSample(1,int(n),y);
            original->process(x,y,expected[n][0],expected[n][1],true);
        }
        count_allocations=true;plugin->processBlock(buffer,midi);count_allocations=false;
        for(unsigned n=0;n<128;++n)for(unsigned c=0;c<2;++c)
            error=std::max(error,double(std::abs(buffer.getSample(int(c),int(n))-expected[n][c])));
    }
    require(error==0,"A differs from current DesktopEngine48");require(allocations==0,"A/B audio allocates");
    std::cout<<"A bit-identical to current DesktopEngine48; processing allocations="<<allocations<<'\n';
    // B is checked against the standalone core, including its 70-frame pad.
    set(*plugin,"compare",1);plugin->prepareToPlay(48000,128);
    original->prepare(bank->bank(),2);original->set_parameters(params);
    auto floating=std::make_unique<cineol::floating::Hall48>();require(floating->prepare(bank->bank()),"float core prepare");floating->set_controls(params.hall);
    std::array<std::array<float,4>,70> delay{};unsigned position=0;error=0;
    for(unsigned block=0;block<400;++block) {
        std::array<std::array<float,2>,128> expected;
        for(unsigned n=0;n<128;++n) {
            const float x=.07f*std::sin((block*128+n)*.17f),y=.04f*std::sin((block*128+n)*.11f);
            buffer.setSample(0,int(n),x);buffer.setSample(1,int(n),y);
            expected[n]={delay[position][0],delay[position][2]};delay[position]=floating->process(x,y);
            float discard_l,discard_r;original->process(x,y,discard_l,discard_r,true);
            if(++position==delay.size())position=0;
        }
        count_allocations=true;plugin->processBlock(buffer,midi);count_allocations=false;
        for(unsigned n=0;n<128;++n)for(unsigned c=0;c<2;++c)
            error=std::max(error,double(std::abs(buffer.getSample(int(c),int(n))-expected[n][c])));
    }
    require(error<2e-8,"B routing differs from independent core + delay");require(allocations==0,"B audio allocates");
    // Switch in input-silent intervals: the continuously running A tail survives.
    for(unsigned block=0;block<40;++block) {
        buffer.clear();set(*plugin,"compare",float((block/8)&1));
        std::array<std::array<float,2>,128> expected;
        for(unsigned n=0;n<128;++n) {
            float al,ar;original->process(0,0,al,ar,true);
            expected[n]=((block/8)&1)?std::array<float,2>{delay[position][0],delay[position][2]}:std::array<float,2>{al,ar};
            delay[position]=floating->process(0,0);if(++position==delay.size())position=0;
        }
        count_allocations=true;plugin->processBlock(buffer,midi);count_allocations=false;
        for(unsigned c=0;c<2;++c)for(unsigned n=0;n<128;++n) {
            const float actual=buffer.getSample(int(c),int(n));require(std::isfinite(actual),"A/B switch nonfinite");
            if(block%8>=4)require(std::abs(actual-expected[n][c])<2e-8f,"A/B switching resets or changes a tail");
        }
    }
    require(allocations==0,"switching allocates");
    set(*plugin,"compare",1);set(*plugin,"float_trim",-2.3f);set(*plugin,"mix",.37f);
    juce::MemoryBlock saved;plugin->getStateInformation(saved);
    auto restored=std::make_unique<FloatHallABProcessor>();restored->setStateInformation(saved.getData(),int(saved.getSize()));
    require(restored->state.getRawParameterValue("compare")->load()==1,"A/B choice not restored");
    require(std::abs(restored->state.getRawParameterValue("float_trim")->load()+2.3f)<1e-5,"trim not restored");
    require(std::abs(restored->state.getRawParameterValue("mix")->load()-.37f)<1e-5,"mix not restored");
    for(double rate:{44100.,48000.,96000.})for(int block:{1,64,257,1024}) {
        restored->prepareToPlay(rate,block);require(restored->ready(),"rate prepare");buffer.setSize(2,block);
        for(unsigned iteration=0;iteration<20;++iteration) {
            buffer.clear();buffer.setSample(0,0,.2f);
            count_allocations=true;restored->processBlock(buffer,midi);count_allocations=false;
            for(unsigned c=0;c<2;++c)for(int n=0;n<block;++n)require(std::isfinite(buffer.getSample(int(c),n)),"host-rate rendering nonfinite");
        }
    }
    require(allocations==0,"host-rate rendering allocates");
    // Dry is sample-exact with the common delay, independent of A/B selection.
    set(*restored,"mix",0);restored->prepareToPlay(48000,128);buffer.setSize(2,128);
    std::array<std::array<float,2>,70> dry{};unsigned dry_position=0;
    for(unsigned block=0;block<30;++block) {
        std::array<std::array<float,2>,128> expected;
        for(unsigned n=0;n<128;++n) {
            const float x=.1f*std::sin((block*128+n)*.17f),y=.03f*std::sin((block*128+n)*.11f);
            expected[n]=dry[dry_position];dry[dry_position]={x,y};if(++dry_position==dry.size())dry_position=0;
            buffer.setSample(0,int(n),x);buffer.setSample(1,int(n),y);
        }
        restored->processBlock(buffer,midi);
        for(unsigned c=0;c<2;++c)for(unsigned n=0;n<128;++n)
            require(buffer.getSample(int(c),int(n))==expected[n][c],"dry signal or latency changed");
    }
    set(*restored,"mix",1);set(*restored,"float_trim",0);
    auto mono=restored->getBusesLayout();mono.inputBuses.set(0,juce::AudioChannelSet::mono());
    require(restored->setBusesLayout(mono),"mono input rejected");restored->prepareToPlay(48000,128);
    set(*plugin,"mix",1);set(*plugin,"float_trim",0);
    plugin->prepareToPlay(48000,128);
    juce::AudioBuffer<float> stereo(2,128);
    for(unsigned block=0;block<20;++block) {
        for(unsigned n=0;n<128;++n) {
            const float x=.1f*std::sin((block*128+n)*.17f);
            buffer.setSample(0,int(n),x);buffer.setSample(1,int(n),99);
            stereo.setSample(0,int(n),x);stereo.setSample(1,int(n),x);
        }
        restored->processBlock(buffer,midi);plugin->processBlock(stereo,midi);
        for(unsigned c=0;c<2;++c)for(unsigned n=0;n<128;++n)
            require(buffer.getSample(int(c),int(n))==stereo.getSample(int(c),int(n)),"mono channel indexing differs from dual-mono input");
    }
    auto editor=std::unique_ptr<juce::AudioProcessorEditor>(plugin->createEditor());
    require(editor->getWidth()==850 && editor->getHeight()==540,"editor layout");
    const auto screenshot=editor->createComponentSnapshot(editor->getLocalBounds());
    const auto destination=juce::File::getCurrentWorkingDirectory().getChildFile("build/validation/float-hall-20261012/editor.png");
    if(auto stream=destination.createOutputStream()) {
        stream->setPosition(0);juce::PNGImageFormat{}.writeImageToStream(screenshot,*stream);stream->truncate();
    }
    std::cout<<"B core routing, state recall, preserved tails, exact dry delay, mono, 44.1/48/96 kHz, variable blocks and editor passed\n";
} catch(const std::exception& error) {count_allocations=false;std::cerr<<error.what()<<'\n';return 1;}
