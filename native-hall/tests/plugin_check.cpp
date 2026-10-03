#include "../plugin/Processor.hpp"
#include <iostream>
#include <new>
#include <cstdlib>
#ifdef _WIN32
#include <malloc.h>
#endif
static thread_local bool audio=false;
static unsigned allocations=0,releases=0;
void* operator new(size_t n) {if(audio) ++allocations;if(void* p=std::malloc(n?n:1)) return p;throw std::bad_alloc();}
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
static void require(bool ok,const char* s) {if(!ok) {std::cerr<<s<<'\n';std::exit(1);}}
static void set(NativeHallProcessor& p,const char* id,float value) {
    auto* param=p.state.getParameter(id);param->setValueNotifyingHost(param->convertTo0to1(value));
}
static juce::Component* find(juce::Component& root,const char* id) {
    if(root.getComponentID()==id) return &root;
    for(auto* child:root.getChildren()) if(auto* found=find(*child,id)) return found;
    return nullptr;
}
// Count distinct bright LED cores in a rendered digit. Adjacent segments must
// remain separate even at the editor's smallest supported scale.
static int lit_segments(const juce::Image& image,juce::Rectangle<int> area) {
    std::vector<bool> visited(size_t(area.getWidth()*area.getHeight()));
    std::vector<juce::Point<int>> pending;int count=0;
    auto lit=[&](int x,int y) {
        auto c=image.getPixelAt(x,y);
        return c.getAlpha()>240 && c.getRed()>240 && c.getGreen()<100 && c.getBlue()<100;
    };
    auto index=[&](int x,int y){return size_t((y-area.getY())*area.getWidth()+x-area.getX());};
    for(int y=area.getY();y<area.getBottom();++y) for(int x=area.getX();x<area.getRight();++x) {
        if(visited[index(x,y)] || !lit(x,y)) continue;
        ++count;pending.push_back({x,y});visited[index(x,y)]=true;
        while(!pending.empty()) {
            auto point=pending.back();pending.pop_back();
            for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                int nx=point.x+dx,ny=point.y+dy;
                if(area.contains(nx,ny) && !visited[index(nx,ny)] && lit(nx,ny)) {
                    visited[index(nx,ny)]=true;pending.push_back({nx,ny});
                }
            }
        }
    }
    return count;
}
static void check_editor() {
    NativeHallProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* dirt=dynamic_cast<juce::ToggleButton*>(find(*editor,"digital_dirt"));
    auto* bass=dynamic_cast<juce::Slider*>(find(*editor,"bass"));
    auto* delay=dynamic_cast<juce::Slider*>(find(*editor,"predelay"));
    auto* algorithm=dynamic_cast<juce::ComboBox*>(find(*editor,"algorithm"));
    auto* output=dynamic_cast<juce::ComboBox*>(find(*editor,"output_r"));
    auto* diffusion=find(*editor,"diffusion");auto* display=find(*editor,"display");
    require(dirt && bass && delay && algorithm && output && diffusion && display,"editor controls missing");
    require(!dirt->getToggleState() && p.state.getRawParameterValue("analog")->load()==1,"Digital Dirt default inversion");
    dirt->setToggleState(true,juce::sendNotificationSync);
    require(p.state.getRawParameterValue("analog")->load()==0,"Digital Dirt ON did not bypass filters");
    dirt->setToggleState(false,juce::sendNotificationSync);
    require(p.state.getRawParameterValue("analog")->load()==1,"Digital Dirt OFF did not enable filters");
    set(p,"analog",0);require(dirt->getToggleState(),"legacy automation did not update Digital Dirt");
    set(p,"analog",1);require(!dirt->getToggleState(),"legacy clean state did not clear Digital Dirt");
    bass->setValue(20,juce::sendNotificationSync);
    require(p.state.getRawParameterValue("bass")->load()==20 && display->getName().contains("BASS 4.6 s"),
            "fader/display did not update together");
    algorithm->setSelectedId(4,juce::sendNotificationSync);
    require(p.getCurrentProgram()==3 && !diffusion->isEnabled() && display->getName().startsWith("04 | Acoustic Chamber"),
            "algorithm display/Chamber state did not update");
    delay->setValue(88,juce::sendNotificationSync);
    require(display->getName().contains("PRE-DELAY 89 ms"),"Chamber pre-delay display is wrong");
    algorithm->setSelectedId(2,juce::sendNotificationSync);
    require(diffusion->isEnabled() && display->getName().startsWith("02 | Vocal Plate") &&
            display->getName().contains("PRE-DELAY 64 ms"),"Plate display/minimum did not update with unchanged fader");
    output->setSelectedId(2,juce::sendNotificationSync);
    require(p.state.getRawParameterValue("output_r")->load()==1,"output selection attachment failed");
    set(p,"analog",0);juce::MemoryBlock saved;p.getStateInformation(saved);set(p,"analog",1);
    p.setStateInformation(saved.getData(),int(saved.getSize()));
    require(dirt->getToggleState(),"Digital Dirt saved-state restoration failed");
    for(int program=0;program<6;++program) {
        set(p,"algorithm",float(program));
        require(display->getName().startsWith(juce::String(program+1).paddedLeft('0',2)+" | "),"host algorithm display failed");
        constexpr int segments[]={2,5,5,4,5,6};
        for(float scale:{0.6f,0.7f,1.0f}) {
            auto shot=display->createComponentSnapshot(display->getLocalBounds(),true,scale);
            auto area=[&](float x){return juce::Rectangle<float>(x,5,72,116).transformedBy(
                juce::AffineTransform::scale(scale)).getSmallestIntegerContainer();};
            require(lit_segments(shot,area(62))==6 && lit_segments(shot,area(152))==segments[program],
                    "LED digit segments touch, overlap, or disappear at an editor size");
        }
    }
    for(int width:{882,1029,1470}) {
        editor->setSize(width,int(std::lround(width*1070.0/1470)));
        auto shot=editor->createComponentSnapshot(editor->getLocalBounds());
        require(shot.isValid() && shot.getWidth()==width,"resized editor snapshot failed");
    }
    std::cout<<"Editor: fader/display, six algorithms, separated LED segments, outputs, inverted Digital Dirt + legacy automation/state, three sizes pass\n";
}
static std::vector<float> run(int rate,int block,bool offline,int program,bool mono=false) {
    auto p=std::make_unique<NativeHallProcessor>();require(p->ready(),"missing imported bank");
    p->setCurrentProgram(program);
    p->setPlayConfigDetails(mono?1:2,2,rate,2048);p->setNonRealtime(offline);p->prepareToPlay(rate,2048);
    juce::AudioBuffer<float> b(2,20000);juce::MidiBuffer midi;
    const int frames=rate*2;std::vector<float> out(size_t(frames)*2);
    for(int pos=0;pos<frames;) {
        int n=std::min(block,frames-pos);
        // Change controls at a shared sample position independent of block size.
        if(pos<rate && pos+n>rate) n=rate-pos;
        if(pos==rate) {
            set(*p,"bass",20);set(*p,"mid",16);set(*p,"diffusion",30);set(*p,"analog",0);
            set(*p,"algorithm",float((program+1)%p->getNumPrograms()));
        }
        b.setSize(2,n,false,false,true);
        for(int i=0;i<n;++i) {
            int at=pos+i;
            b.setSample(0,i,at<rate/4?0.1f*std::sin(at*0.117f):0);
            b.setSample(1,i,at==0?0.4f:0);
        }
        audio=true;p->processBlock(b,midi);audio=false;
        for(int i=0;i<n;++i) for(int c=0;c<2;++c) {
            float v=b.getSample(c,i);require(std::isfinite(v),"nonfinite plugin output");out[size_t(pos+i)*2+c]=v;
        }
        pos+=n;
    }
    juce::MemoryBlock state;p->getStateInformation(state);
    auto restored=std::make_unique<NativeHallProcessor>();restored->setStateInformation(state.getData(),int(state.getSize()));
    require(restored->state.getRawParameterValue("bass")->load()==20 &&
            restored->state.getRawParameterValue("diffusion")->load()==30 &&
            restored->getCurrentProgram()==(program+1)%p->getNumPrograms(),"parameter/algorithm state round trip");
    return out;
}
static void check_state_and_ranges() {
    auto p=std::make_unique<NativeHallProcessor>();
    require(p->getNumPrograms()==6 && p->getCurrentProgram()==2,"program count/default");
    for(unsigned i=0;i<15;++i)
        require(p->state.getParameter(NativeHallProcessor::ids[i])->getParameterIndex()==int(i),"legacy parameter order changed");
    auto* delay=p->state.getParameter("predelay");
    for(int program=0;program<6;++program) {
        p->setCurrentProgram(program);
        require(p->getCurrentProgram()==program && p->getProgramName(program).isNotEmpty(),"host program selection");
        const int minimum=native_hall::predelay_minima[program];
        for(int offset:{0,64,128}) {
            const float value=delay->convertTo0to1(float(24+offset));
            require(delay->getText(value,0).getIntValue()==minimum+offset,"pre-delay display range");
            require(delay->getValueForText(juce::String(minimum+offset)+" ms")==value,"pre-delay text entry");
        }
    }
    p->setCurrentProgram(-1);p->setCurrentProgram(6);require(p->getCurrentProgram()==5,"invalid host program accepted");
    set(*p,"bass",20);set(*p,"predelay",88);
    auto legacy=p->state.copyState();legacy.removeChild(legacy.getChildWithProperty("id","algorithm"),nullptr);
    auto xml=legacy.createXml();juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*xml,old);
    p->setStateInformation(old.getData(),int(old.getSize()));
    require(p->getCurrentProgram()==2 && p->state.getRawParameterValue("bass")->load()==20 &&
            p->state.getRawParameterValue("predelay")->load()==88,"v0.2 state migration");
    require(delay->getText(delay->getValue(),0)=="88","legacy pre-delay changed");
    std::cout<<"6 host programs, parameter indices, pre-delay ranges/text entry, v0.2 migration pass\n";
}
static void check_daisy_engine(const char* path) {
    juce::MemoryBlock data;
    require(juce::File(juce::String::fromUTF8(path)).loadFileAsData(data),"missing Daisy bank fixture");
    auto bank=std::make_unique<native_hall::ProgramBank>();
    require(native_hall::read_bank(data.getData(),data.getSize(),*bank),"invalid Daisy bank fixture");
    auto p=std::make_unique<NativeHallProcessor>();p->prepareToPlay(48000,511);
    auto engine=std::make_unique<native_hall::Engine48>();engine->prepare(*bank);
    juce::AudioBuffer<float> block(2,511);juce::MidiBuffer midi;
    unsigned position=0;
    // Directed switches plus simultaneous delay/mode changes. Compare the
    // shipped processor with the same public Engine48 used by Daisy.
    for(int from=0;from<6;++from) for(int to=0;to<6;++to) for(int program:{from,to}) {
        p->setCurrentProgram(program);
        set(*p,"predelay",float(24+(position%3)*64));
        set(*p,"analog",float(position%2));set(*p,"mode_enh",float((position/2)%2));
        set(*p,"decay_opt",float((position/4)%2));
        native_hall::Parameters params;params.program=unsigned(program);
        params.hall.predelay_ms=native_hall::predelay_minima[program]+int((position%3)*64);
        params.analog=position%2;params.hall.mode_enhancement=(position/2)%2;
        params.hall.decay_optimization=(position/4)%2;engine->set_parameters(params);
        for(int repeat=0;repeat<8;++repeat) {
            std::array<std::array<float,2>,511> expected{};
            for(int i=0;i<511;++i) {
                float l=0.08f*std::sin(float(position+i)*0.11f),r=i==0?0.2f:0;
                block.setSample(0,i,l);block.setSample(1,i,r);
                engine->process(l,r,expected[i][0],expected[i][1]);
            }
            audio=true;p->processBlock(block,midi);audio=false;
            for(int i=0;i<511;++i) for(int c=0;c<2;++c)
                require(block.getSample(c,i)==expected[i][c],"plugin differs from Daisy engine");
            // Keep control positions fixed until the next switch.
        }
        ++position;
    }
    std::cout<<"36 directed switches + all pre-delay ranges/modes: plugin == Daisy Engine48 exactly at 48k\n";
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    // Offline fixtures belong only to an isolated test cache, never to the
    // shipped plugin or the user's first-run data directory.
    struct TestCache {
        juce::File folder;
        ~TestCache() {if(folder.isDirectory()) folder.deleteRecursively();}
    } test_cache;
    if(argc==3 && std::string(argv[1])=="--bank") {
        test_cache.folder=juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("cineol-plugin-check",{},false);
        require(test_cache.folder.createDirectory().wasOk(),"could not create test cache");
        setenv("CINEOL224_CACHE_DIR",test_cache.folder.getFullPathName().toRawUTF8(),1);
        require(juce::File(juce::String::fromUTF8(argv[2])).copyFileTo(CineolRomBank::cacheFile()),"could not seed test bank");
    }
    const bool previewBundle=juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getFileNameWithoutExtension()=="CineolEditorPreview";
    if((argc==2 && std::string(argv[1])=="--ui") || (argc==1 && previewBundle)) {
        NativeHallProcessor p;
        class Window final : public juce::DocumentWindow {
        public:
            explicit Window(NativeHallProcessor& processor):DocumentWindow("Cineol-X 224",juce::Colours::black,allButtons) {
                setUsingNativeTitleBar(true);setContentOwned(processor.createEditor(),true);
                setResizable(true,false);centreWithSize(getWidth(),getHeight());setVisible(true);
            }
            void closeButtonPressed() override {juce::MessageManager::getInstance()->stopDispatchLoop();}
        } window(p);
        juce::MessageManager::getInstance()->runDispatchLoop();return 0;
    }
    if((argc==3 || argc==4 || argc==6) && std::string(argv[1])=="--editor") {
        NativeHallProcessor p;
        if(argc>=4) p.setCurrentProgram(std::atoi(argv[3]));
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        if(argc==6) set(p,argv[4],std::strtof(argv[5],nullptr));
        juce::File file(juce::String::fromUTF8(argv[2]));
        file.getParentDirectory().createDirectory();
        juce::FileOutputStream output(file);juce::PNGImageFormat png;
        require(output.openedOk() && output.setPosition(0) && output.truncate().wasOk() && png.writeImageToStream(
            editor->createComponentSnapshot(editor->getLocalBounds()),output),"editor screenshot failed");
        return 0;
    }
    check_state_and_ranges();check_editor();
    if(argc==3 && std::string(argv[1])=="--bank") check_daisy_engine(argv[2]);
    for(int program=0;program<6;++program) for(int rate:{44100,48000,96000}) {
        auto a=run(rate,128,false,program), b=run(rate,511,true,program),c=run(rate,20000,false,program);
        require(a==b && b==c,"algorithm switching/block-size or offline/realtime output differs");
        double energy=0;for(float v:a) energy+=double(v)*v;require(energy>0.01,"silent plugin algorithm");
        auto mono=run(rate,333,false,program,true);require(!mono.empty(),"mono bus failed");
        std::cout<<"Program "<<program+1<<", "<<rate<<" Hz: switch + 128/511/20000 blocks exact, state + mono pass\n";
    }
    require(allocations==0 && releases==0,"audio callback allocated or released memory");
    std::cout<<"AU/VST processor callback: new=0, delete=0\n";
}
