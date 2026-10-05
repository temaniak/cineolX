#include "../plugin/Processor.hpp"
#include <cstdlib>
#include <exception>
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
static void preparation_runtime() {
    // Match the plugin's background JUCE thread, including Windows stack size.
    struct Probe final:juce::Thread {
        Probe():Thread("XL import runtime check") {}
        std::exception_ptr error;
        void run() override {
            try {
                native_hall::import::Callbacks callbacks;
                callbacks.progress=[](double,const char* stage) {
                    std::cout<<"XL runtime: "<<stage<<std::endl;return true;
                };
                const auto stats=cineol::xl::import::check_preparation_runtime(callbacks);
                std::cout<<"XL runtime: largest frame="<<stats.largest_frame
                    <<", peak frames="<<stats.peak_frames<<"; pass"<<std::endl;
            } catch(...) {error=std::current_exception();}
        }
    } probe;
    require(probe.startThread(juce::Thread::Priority::low),"could not start XL runtime check");
    require(probe.waitForThreadToExit(10000),"XL runtime check timed out");
    if(probe.error) std::rethrow_exception(probe.error);
}
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
    auto reference=std::make_unique<native_hall::DesktopEngine48>();reference->prepare(*bank);
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
static void compare_xl(NativeHallProcessor& processor) {
    juce::MemoryBlock bytes;require(CineolRomBank::xlCacheFile().loadFileAsData(bytes),"missing XL bank");
    auto bank=std::make_unique<cineol::xl::Bank>();
    require(cineol::xl::read_bank(bytes.getData(),bytes.getSize(),*bank),"invalid XL bank");
    auto reference=std::make_unique<cineol::xl::Runtime>();reference->select(*bank,0);
    reference->controls(bank->programs[0].controls.factory,true,0,1,1,0,2);
    std::array<std::array<float,2>,native_hall::Engine48::latency_samples-cineol::xl::Runtime::latency_samples> delay{};unsigned position=0;
    juce::AudioBuffer<float> block(2,256);juce::MidiBuffer midi;
    for(int repeat=0;repeat<100;++repeat) {
        std::array<std::array<float,2>,256> wanted{};
        for(int i=0;i<256;++i) {
            const float l=0.12f*std::sin(float(i+repeat*256)*0.1f),r=(i==0 && repeat==0)?0.3f:0;
            block.setSample(0,i,l);block.setSample(1,i,r);float a,b;reference->process(l,r,a,b,false);
            wanted[i]=delay[position];delay[position]={a,b};position=(position+1)%delay.size();
        }
        audio=true;processor.processBlock(block,midi);audio=false;
        for(int i=0;i<256;++i) for(int c=0;c<2;++c)
            require(block.getSample(c,i)==wanted[i][c],"published XL differs from native runtime");
    }
    auto corrupted=bytes;static_cast<uint8_t*>(corrupted.getData())[corrupted.getSize()-1]^=1;
    require(!cineol::xl::read_bank(corrupted.getData(),corrupted.getSize(),*bank),"XL checksum did not reject corruption");
    corrupted=bytes;cineol::xl::BankHeader bad;std::memcpy(&bad,bytes.getData(),sizeof bad);bad.version=99;
    std::memcpy(corrupted.getData(),&bad,sizeof bad);
    require(!cineol::xl::read_bank(corrupted.getData(),corrupted.getSize(),*bank),"XL version not validated");
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
static void invalid_imports() {
    auto refresh_ui=[] {juce::Thread::sleep(120);juce::Timer::callPendingTimersSynchronously();};
    const auto cache=CineolRomBank::cacheFile();
    require(!cache.existsAsFile() && !CineolRomBank::xlCacheFile().existsAsFile(),"invalid-import check requires an empty isolated cache");
    const auto folder=cache.getParentDirectory().getChildFile("invalid-rom-fixtures");
    require(folder.createDirectory().wasOk(),"could not create ROM fixtures");
    juce::MemoryBlock unknown(2048,true);
    const auto chip=folder.getChildFile(juce::String::charToString(0x0420)+"OM-unknown.bin");
    require(chip.replaceWithData(unknown.getData(),unknown.getSize()),"could not write unknown ROM");
    const auto wrong_size=folder.getChildFile("truncated.bin"),broken_zip=folder.getChildFile("broken.zip"),zip=folder.getChildFile("unknown.zip");
    require(wrong_size.replaceWithText("truncated") && broken_zip.replaceWithText("invalid ZIP"),"could not write malformed fixtures");
    {
        juce::ZipFile::Builder builder;
        builder.addEntry(std::make_unique<juce::MemoryInputStream>(unknown,true),6,"ROM1.bin",juce::Time::getCurrentTime());
        builder.addEntry(std::make_unique<juce::MemoryInputStream>(unknown,true),6,"nested/duplicate.bin",juce::Time::getCurrentTime());
        juce::FileOutputStream stream(zip);require(stream.openedOk() && builder.writeToStream(stream,nullptr),"could not write ZIP fixture");
    }
    for(bool cached:{false,true}) {
        if(cached) {
            // Synthetic coefficients exercise cached-224/add-XL UI without
            // firmware or private bank data on public Windows/macOS runners.
            auto bank=std::make_unique<native_hall::ProgramBank>();
            for(unsigned i=0;i<native_hall::program_count;++i) {
                auto& p=bank->programs[i];p.identity=native_hall::program_identities[i];p.network=native_hall::program_networks[i];
                p.level_rate_tenths.fill(100);p.modulation_rate_tenths.fill(1000);
                require(p.valid(i),"invalid synthetic profile");
            }
            native_hall::BankHeader header;header.checksum=native_hall::profile_checksum(bank.get(),sizeof(*bank));
            juce::FileOutputStream stream(cache);
            require(stream.openedOk() && stream.write(&header,sizeof header) && stream.write(bank.get(),sizeof(*bank)),"could not seed synthetic cache");
        }
        juce::MemoryBlock before;if(cached) require(cache.loadFileAsData(before),"missing synthetic cache");
        auto processor=std::make_unique<NativeHallProcessor>();
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
        auto* setup=find(*editor,"rom_setup");
        auto* model=dynamic_cast<juce::Button*>(find(*editor,"firmware"));
        auto* close=dynamic_cast<juce::Button*>(find(*editor,"close_rom_setup"));
        auto* status=dynamic_cast<juce::Label*>(find(*editor,"rom_status"));
        require(setup && model && close && status && find(*editor,"choose_rom_folder"),"ROM setup controls missing");
        require(setup->isVisible()!=cached,"incorrect initial setup visibility");
        model->onClick();refresh_ui();
        require(setup->isVisible(),"reopening ROM setup did not survive panel refresh");
        for(const auto& source:juce::Array<juce::File>{chip,wrong_size,broken_zip,zip,folder,folder.getChildFile("missing.bin")}) {
            bool started=false;
            for(int retry=0;retry<100 && !started;++retry) {
                started=processor->importRoms({source});if(!started) juce::Thread::sleep(10);
            }
            require(started,"repeated invalid import did not start");wait(*processor);
            refresh_ui();
            require(processor->programAvailable(0)==cached && !processor->programAvailable(6),"invalid input changed available banks");
            for(int retry=0;retry<10 && status->getText()!=processor->romStatus();++retry) refresh_ui();
            if(!setup->isVisible() || status->getText()!=processor->romStatus() || !status->getText().contains("11 224XL v8.21"))
                std::cerr<<"ROM setup visible="<<setup->isVisible()<<" label="<<status->getText()<<" status="<<processor->romStatus()<<'\n';
            require(setup->isVisible() && status->getText()==processor->romStatus() && status->getText().contains("11 224XL v8.21"),"ROM rejection status hidden or unclear");
            require(!CineolRomBank::xlCacheFile().existsAsFile(),"invalid input saved XL bank");
            if(cached) {juce::MemoryBlock after;require(cache.loadFileAsData(after) && before==after,"invalid XL import changed cached 224");}
            else {require(!cache.existsAsFile(),"invalid input saved 224 bank");dry(*processor);}
        }
        require(close->isEnabled()==cached,"setup close availability incorrect");
        if(cached) {close->onClick();refresh_ui();require(!setup->isVisible(),"ROM setup did not close");}
    }
    require(cache.deleteFile() && folder.deleteRecursively(),"could not remove synthetic fixtures");
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    require(argc>=2,"usage: rom_import_check --runtime-check|--empty|--cached|--reject SOURCE|--cancel SOURCE|--import SOURCE BANK");
    const std::string mode=argv[1];
    if(mode=="--empty" || mode=="--runtime-check") preparation_runtime();
    if(mode=="--runtime-check") return 0;
    if(mode=="--invalid-check") {
        invalid_imports();require(allocations==0 && releases==0,"invalid-import audio allocated/released memory");
        std::cout<<"invalid ROM/ZIP/folder/retry and cached-224 setup: pass; audio new=0, delete=0\n";return 0;
    }
    {
        NativeHallProcessor processor;
        if(mode=="--cached" || mode=="--cached-xl") {
            if(mode=="--cached-xl") processor.setCurrentProgram(6);
            require(processor.ready() && !processor.importingRoms(),"restart did not use cached bank");
            processor.prepareToPlay(48000,256);if(mode=="--cached-xl") compare_xl(processor);else compare(processor);
        } else if(mode=="--add-xl") {
            require(argc==4 && processor.ready() && !processor.programAvailable(6),"add-XL test requires only original cache");
            juce::MemoryBlock original;require(CineolRomBank::cacheFile().loadFileAsData(original),"original bank missing");
            NativeHallProcessor second;processor.setCurrentProgram(6);second.setCurrentProgram(6);dry(processor);
            processor.setPlayConfigDetails(2,2,48000,256);processor.prepareToPlay(48000,256);
            std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
            require(!find(*editor,"rom_setup")->isVisible() && find(*editor,"algorithm")->isEnabled(),"missing XL trapped existing 224 UI");
            require(processor.importRoms({juce::File(juce::String::fromUTF8(argv[2]))}),"adding XL did not start");
            wait(processor);require(processor.ready() && second.ready(),"added XL not published to instances");
            juce::MemoryBlock got,wanted,unchanged;
            require(CineolRomBank::xlCacheFile().loadFileAsData(got) && juce::File(juce::String::fromUTF8(argv[3])).loadFileAsData(wanted) && got==wanted,"added XL differs from offline preparation");
            require(CineolRomBank::cacheFile().loadFileAsData(unchanged) && original==unchanged,"adding XL changed original bank");
            compare_xl(processor);
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
                if(mode=="--import-xl") {processor.setCurrentProgram(6);second.setCurrentProgram(6);}
                if(mode=="--reject" || mode=="--cancel") {
                    require(!processor.ready() && !second.ready(),"unsupported/cancelled import activated processor");
                    if(mode=="--reject") require(processor.romStatus().contains("224 v4.4"),"missing compatibility error");
                    require(!CineolRomBank::cacheFile().existsAsFile(),"failed import saved a bank");
                    dry(processor);
                } else {
                    require((mode=="--import" || mode=="--import-xl") && argc==4,"invalid test mode");
                    require(processor.ready() && second.ready(),"completed bank not shared with second instance");
                    juce::Timer::callPendingTimersSynchronously();
                    require(!find(*editor,"rom_setup")->isVisible() && find(*editor,"algorithm")->isEnabled(),
                            "completed import did not reveal the plugin controls");
                    juce::MemoryBlock got,wanted;
                    require((mode=="--import-xl"?CineolRomBank::xlCacheFile():CineolRomBank::cacheFile()).loadFileAsData(got) &&
                        juce::File(juce::String::fromUTF8(argv[3])).loadFileAsData(wanted) && got==wanted,"runtime bank differs from build-time bank");
                    if(mode=="--import-xl") compare_xl(processor);else compare(processor);
                }
            }
        }
    }
    // The existing public build workflow already runs --empty on both hosts.
    // Exercise import rejection after its processors have been destroyed.
    if(mode=="--empty" && !CineolRomBank::cacheFile().existsAsFile() && !CineolRomBank::xlCacheFile().existsAsFile()) invalid_imports();
    require(allocations==0 && releases==0,"audio callback allocated/released memory");
    std::cout<<mode<<": pass; audio new=0, delete=0\n";
}
