#include "../plugin/Processor.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#ifdef _WIN32
#include <malloc.h>
#endif

static thread_local bool audio=false;
static unsigned allocations=0,releases=0;
void* operator new(size_t n) {if(audio) ++allocations;if(auto* p=std::malloc(n?n:1)) return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {if(audio && p) ++releases;std::free(p);}
void operator delete(void* p,size_t) noexcept {::operator delete(p);}
void* operator new[](size_t n) {return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void* operator new(size_t n,std::align_val_t a) {
    if(audio) ++allocations;
#ifdef _WIN32
    void* p=_aligned_malloc(n?n:1,size_t(a));if(!p) throw std::bad_alloc();return p;
#else
    void* p=nullptr;if(posix_memalign(&p,size_t(a),n?n:1)) throw std::bad_alloc();return p;
#endif
}
void operator delete(void* p,std::align_val_t) noexcept {
#ifdef _WIN32
    if(audio && p) ++releases;_aligned_free(p);
#else
    ::operator delete(p);
#endif
}
void operator delete(void* p,size_t,std::align_val_t a) noexcept {::operator delete(p,a);}
static void require(bool ok,const char* text) {if(!ok) {std::cerr<<text<<'\n';std::exit(1);}}
static juce::Component* find(juce::Component& root,const char* id) {
    if(root.getComponentID()==id) return &root;
    for(auto* child:root.getChildren()) if(auto* found=find(*child,id)) return found;
    return nullptr;
}
static void dry(NativeHallProcessor& processor,bool mono=false) {
    processor.setPlayConfigDetails(mono?1:2,2,48000,256);processor.prepareToPlay(48000,256);
    juce::AudioBuffer<float> block(2,256);juce::MidiBuffer midi;
    for(int i=0;i<256;++i) {block.setSample(0,i,float(i)/512);block.setSample(1,i,-float(i)/1024);}
    audio=true;processor.processBlock(block,midi);audio=false;
    for(int i=0;i<256;++i) {
        require(block.getSample(0,i)==float(i)/512,"unimported left input changed");
        require(block.getSample(1,i)==(mono?float(i)/512:-float(i)/1024),"unimported right input changed");
    }
}
static void compare(NativeHallProcessor& processor) {
    juce::MemoryBlock bytes;require(CineolRomBank::cacheFile().loadFileAsData(bytes),"missing saved bank");
    auto bank=std::make_unique<native_hall::ProgramBank>();
    require(native_hall::read_bank(bytes.getData(),bytes.getSize(),*bank),"invalid saved bank");
    auto reference=std::make_unique<native_hall::Engine48>();reference->prepare(*bank);
    // Do not call prepareToPlay here: this checks publication to an already
    // running dry processor, including its first audio-block initialization.
    native_hall::Parameters params;reference->set_parameters(params);
    juce::AudioBuffer<float> block(2,256);juce::MidiBuffer midi;
    for(int repeat=0;repeat<24;++repeat) {
        std::array<std::array<float,2>,256> wanted{};
        for(int i=0;i<256;++i) {
            const float l=0.12f*std::sin(float(i+repeat*256)*0.1f),r=(i==0 && repeat==0)?0.3f:0;
            block.setSample(0,i,l);block.setSample(1,i,r);reference->process(l,r,wanted[i][0],wanted[i][1]);
        }
        audio=true;processor.processBlock(block,midi);audio=false;
        for(int i=0;i<256;++i) for(int c=0;c<2;++c)
            require(block.getSample(c,i)==wanted[i][c],"imported processor differs from native engine");
    }
}
static void wait(NativeHallProcessor& processor) {
    const double deadline=juce::Time::getMillisecondCounterHiRes()+1800000;
    juce::String last;
    while(processor.importingRoms()) {
        require(juce::Time::getMillisecondCounterHiRes()<deadline,"import timed out");
        const auto status=processor.romStatus();
        if(status!=last) {std::cout<<status<<std::endl;last=status;}
        juce::Thread::sleep(10);
    }
    std::cout<<processor.romStatus()<<'\n';
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    require(argc>=2,"usage: rom_import_check --empty|--cached|--reject SOURCE|--cancel SOURCE|--import SOURCE BANK");
    const std::string mode=argv[1];
    {
        NativeHallProcessor processor;
        if(mode=="--cached") {
            require(processor.ready() && !processor.importingRoms(),"restart did not use cached bank");
            processor.prepareToPlay(48000,256);compare(processor);
        } else {
            require(!processor.ready(),"test requires a fresh or invalid cache");
            dry(processor);dry(processor,true);processor.setPlayConfigDetails(2,2,48000,256);processor.prepareToPlay(48000,256);
            juce::MemoryBlock state;processor.getStateInformation(state);
            require(state.getSize()<10000,"session contains private bank bytes");
            processor.setStateInformation(state.getData(),int(state.getSize()));require(!processor.ready(),"session bypassed ROM setup");
            std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
            require(find(*editor,"rom_setup") && find(*editor,"rom_setup")->isVisible() &&
                    find(*editor,"choose_roms")->isEnabled(),"first-run ROM request missing");
            require(!find(*editor,"algorithm")->isEnabled(),"unimported controls enabled");
            auto* settings=dynamic_cast<juce::Button*>(find(*editor,"settings"));
            auto* panel=find(*editor,"settings_panel");
            auto* low=dynamic_cast<juce::ToggleButton*>(find(*editor,"low_latency"));
            require(settings && panel && low && !panel->isVisible() && !low->getToggleState(),"first-use Settings missing");
            settings->setToggleState(true,juce::sendNotificationSync);
            require(panel->isVisible(),"Settings did not open");
            low->setToggleState(true,juce::sendNotificationSync);
            juce::MemoryBlock low_state;processor.getStateInformation(low_state);
            low->setToggleState(false,juce::sendNotificationSync);
            processor.setStateInformation(low_state.getData(),int(low_state.getSize()));
            require(low->getToggleState(),"Low latency setting was not restored");
            for(int rate:{44100,48000,96000}) {
                processor.prepareToPlay(rate,256);
                require(processor.getLatencySamples()==0,"Low latency did not report zero");
            }
            dry(processor);dry(processor,true);
            low->setToggleState(false,juce::sendNotificationSync);
            settings->setToggleState(false,juce::sendNotificationSync);
            require(!panel->isVisible(),"Settings did not close");
            processor.setPlayConfigDetails(2,2,48000,256);processor.prepareToPlay(48000,256);
            require(processor.getLatencySamples()==70,"normal latency not restored");
            if(mode=="--empty") {
                if(argc==3) {
                    juce::File file(juce::String::fromUTF8(argv[2]));file.getParentDirectory().createDirectory();
                    juce::FileOutputStream stream(file);juce::PNGImageFormat png;
                    require(stream.openedOk() && stream.setPosition(0) && stream.truncate().wasOk() &&
                        png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),stream),"setup screenshot failed");
                }
            } else {
                require(argc>=3,"source required");
                NativeHallProcessor second;require(!second.ready(),"second instance unexpectedly ready");
                require(processor.importRoms({juce::File(juce::String::fromUTF8(argv[2]))}),"import did not start");
                require(!second.importRoms({juce::File(juce::String::fromUTF8(argv[2]))}),"parallel import started");
                if(mode=="--cancel") {
                    const auto deadline=juce::Time::getMillisecondCounterHiRes()+60000;
                    while(processor.importingRoms() && processor.importProgress()==0) {
                        require(juce::Time::getMillisecondCounterHiRes()<deadline,"preparation did not start");
                        juce::Thread::sleep(10);
                    }
                    processor.cancelRomImport();
                }
                wait(processor);
                if(mode=="--reject" || mode=="--cancel") {
                    require(!processor.ready() && !second.ready(),"unsupported/cancelled import activated processor");
                    if(mode=="--reject") require(processor.romStatus().contains("224 v4.4"),"missing compatibility error");
                    require(!CineolRomBank::cacheFile().existsAsFile(),"failed import saved a bank");
                    dry(processor);
                } else {
                    require(mode=="--import" && argc==4,"invalid test mode");
                    require(processor.ready() && second.ready(),"completed bank not shared with second instance");
                    juce::Timer::callPendingTimersSynchronously();
                    require(!find(*editor,"rom_setup")->isVisible() && find(*editor,"algorithm")->isEnabled(),
                            "completed import did not reveal the plugin controls");
                    juce::MemoryBlock got,wanted;
                    require(CineolRomBank::cacheFile().loadFileAsData(got) &&
                        juce::File(juce::String::fromUTF8(argv[3])).loadFileAsData(wanted) && got==wanted,"runtime bank differs from build-time bank");
                    compare(processor);
                }
            }
        }
    }
    require(allocations==0 && releases==0,"audio callback allocated/released memory");
    std::cout<<mode<<": pass; audio new=0, delete=0\n";
}
