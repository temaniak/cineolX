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
    auto* settings=dynamic_cast<juce::Button*>(find(*editor,"settings"));
    auto* settings_panel=find(*editor,"settings_panel");
    auto* low_latency=dynamic_cast<juce::ToggleButton*>(find(*editor,"low_latency"));
    require(dirt && bass && delay && algorithm && output && diffusion && display,"editor controls missing");
    require(settings && settings_panel && low_latency && !settings_panel->isVisible(),"Settings controls/default missing");
    require(find(*editor,"preset") && find(*editor,"save_preset"),"display preset controls missing");
    settings->setToggleState(true,juce::sendNotificationSync);
    require(settings_panel->isVisible(),"gear did not open Settings");
    low_latency->setToggleState(true,juce::sendNotificationSync);
    require(p.state.getRawParameterValue("low_latency")->load()==1,"Low latency checkbox attachment failed");
    juce::MemoryBlock settings_state;p.getStateInformation(settings_state);set(p,"low_latency",0);
    p.setStateInformation(settings_state.getData(),int(settings_state.getSize()));
    require(low_latency->getToggleState(),"Low latency checkbox restoration failed");
    settings->setToggleState(false,juce::sendNotificationSync);
    require(!settings_panel->isVisible(),"gear did not close Settings");
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
struct PresetTestFiles {
    juce::File folder=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("cineol-preset-check",{},false);
    ~PresetTestFiles() {folder.deleteRecursively();}
};
static void check_presets() {
    PresetTestFiles files;NativeHallProcessor p;
    const auto first=files.folder.getChildFile("Warm Concert Hall.cineol224");
    const auto second=files.folder.getChildFile("Short Hall.cineol224");
    require(p.savePreset(first).wasOk() && p.presetName()=="Warm Concert Hall" && !p.presetModified(),"preset save/name failed");
    set(p,"bass",5);set(p,"depth",40);set(p,"mix",0.5f);set(p,"low_latency",1);
    require(p.presetModified(),"edited preset was not marked modified");
    require(p.savePreset(second).wasOk(),"second preset save failed");
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* bass=dynamic_cast<juce::Slider*>(find(*editor,"bass"));
    auto* preset=dynamic_cast<juce::Button*>(find(*editor,"preset"));
    require(bass && preset,"preset UI missing");
    struct Counter final : juce::AudioProcessorParameter::Listener {
        int changes=0;
        void parameterValueChanged(int,float) override {++changes;}
        void parameterGestureChanged(int,bool) override {}
    } counter;
    auto* parameter=p.state.getParameter("bass");parameter->addListener(&counter);
    const double start=double(bass->getProperties()["motor_position"]);
    require(p.loadPreset(first).wasOk() && p.getCurrentProgram()==2,"preset load failed");
    const double target=bass->valueToProportionOfLength(17);
    require(bass->getValue()==17 && counter.changes==1 && bool(bass->getProperties()["motor_moving"]) &&
        std::abs(double(bass->getProperties()["motor_position"])-start)<0.0001,"preset did not apply immediately with visual-only motion");
    auto tick=[](int ms) {
        const auto end=juce::Time::getMillisecondCounterHiRes()+ms;
        while(juce::Time::getMillisecondCounterHiRes()<end) {juce::Thread::sleep(10);juce::Timer::callPendingTimersSynchronously();}
    };
    tick(100);const double middle=double(bass->getProperties()["motor_position"]);
    require(middle>start && middle<target && counter.changes==1 && bass->getValue()==17,"fader animation changed audio/automation or skipped motion");
    // A second recall starts at the currently drawn cap, not either endpoint.
    require(p.loadPreset(second).wasOk(),"rapid preset recall failed");
    require(std::abs(double(bass->getProperties()["motor_position"])-middle)<0.03,"rapid recall jumped its visual starting point");
    tick(400);
    require(!bool(bass->getProperties()["motor_moving"]) &&
        std::abs(double(bass->getProperties()["motor_position"])-start)<0.0001 && counter.changes==2,
        "animation did not settle or generated extra parameter events");
    require(p.loadPreset(first).wasOk(),"repeat recall failed");
    bass->setValue(22,juce::sendNotificationSync);
    require(!bool(bass->getProperties()["motor_moving"]) && p.presetModified() &&
        std::abs(double(bass->getProperties()["motor_position"])-bass->valueToProportionOfLength(22))<0.0001,
        "manual edit did not interrupt visual motion");
    parameter->removeListener(&counter);
    require(p.state.getRawParameterValue("low_latency")->load()==1,"preset changed instance Low latency");
    require(p.loadPreset(first).wasOk() && !p.presetModified(),"preset baseline restoration failed");
    juce::MemoryBlock session;p.getStateInformation(session);NativeHallProcessor restored;
    restored.setStateInformation(session.getData(),int(session.getSize()));
    require(restored.presetName()=="Warm Concert Hall" && !restored.presetModified(),"DAW state lost preset identity/baseline");
    // No partial mutation on broken, incomplete, nonnumeric or out-of-range files.
    auto original=juce::XmlDocument::parse(first);require(original!=nullptr,"saved preset XML missing");
    const auto broken=files.folder.getChildFile("Broken.cineol224");
    for(const auto& text:juce::StringArray{"not-a-number","9999","nan","12trailing"}) {
        original->getFirstChildElement()->setAttribute("value",text);require(original->writeTo(broken),"bad fixture write failed");
        require(p.loadPreset(broken).failed() && p.presetName()=="Warm Concert Hall" && !p.presetModified(),"invalid preset changed the instance");
    }
    original->removeChildElement(original->getFirstChildElement(),true);original->writeTo(broken);
    require(p.loadPreset(broken).failed() && !p.presetModified(),"incomplete preset was accepted");
    std::cout<<"Presets: file + session round trip, validation, Low latency isolation, 320ms visual-only/rapid/interrupted faders pass\n";
}
static void check_preset_bank() {
    PresetTestFiles files;NativeHallProcessor p;
    const auto bank=files.folder.getChildFile("User Presets.cineolbank");
    const auto legacy=files.folder.getChildFile("Legacy Hall.cineol224");
    require(p.savePreset(legacy).wasOk(),"legacy bank fixture failed");
    juce::StringArray names;
    require(p.presetNames(names,bank).wasOk() && names.contains("Legacy Hall"),"legacy preset was not adopted");
    set(p,"bass",5);set(p,"mix",0.5f);set(p,"low_latency",1);
    const auto unicode=juce::String::fromUTF8("Зал / тёплый");
    require(p.saveBankPreset(unicode,false,bank).wasOk() && p.presetName()==unicode && !p.presetModified(),"named bank save failed");
    require(bank.existsAsFile() && legacy.existsAsFile() &&
        files.folder.findChildFiles(juce::File::findFiles,false,"*.cineol224").size()==1,"bank save created per-preset files or deleted a legacy file");
    require(p.presetNames(names,bank).wasOk() && names.size()==2 && names.contains(unicode),"bank listing failed");
    set(p,"bass",9);
    const auto unchanged=bank.loadFileAsString();
    require(p.saveBankPreset(unicode,false,bank).failed() && p.presetModified() && bank.loadFileAsString()==unchanged,"duplicate save silently replaced a preset");
    require(p.saveBankPreset(unicode,true,bank).wasOk(),"explicit preset replacement failed");
    require(p.saveBankPreset("  ",false,bank).failed() && p.saveBankPreset(juce::String::repeatedString("x",81),false,bank).failed(),"invalid preset name accepted");
    NativeHallProcessor other;
    require(other.loadBankPreset(unicode,bank).wasOk() && other.state.getRawParameterValue("bass")->load()==9 &&
        other.state.getRawParameterValue("mix")->load()==0.5f && other.state.getRawParameterValue("low_latency")->load()==0,
        "bank did not persist between instances or changed instance latency");
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* bass=dynamic_cast<juce::Slider*>(find(*editor,"bass"));
    require(p.loadBankPreset("Legacy Hall",bank).wasOk() && bass && bass->getValue()==17 &&
        bool(bass->getProperties()["motor_moving"]),"bank recall did not animate the fader");
    require(p.loadBankPreset("missing",bank).failed() && p.presetName()=="Legacy Hall" && !p.presetModified(),"missing bank entry changed the instance");
    p.setCurrentProgram(1);require(p.saveBankPreset("Vocal Room",false,bank).wasOk(),"algorithm preset save failed");
    juce::Array<NativeHallProcessor::PresetInfo> entries;
    auto algorithmOf=[&](const juce::String& name) {for(const auto& entry:entries) if(entry.name==name) return entry.algorithm;return -1;};
    require(p.presetBankEntries(entries,bank).wasOk() && algorithmOf("Vocal Room")==1 && algorithmOf("Legacy Hall")==2,
        "presets were not assigned to their saved algorithms");
    p.setCurrentProgram(3);require(p.saveBankPreset("Vocal Room",true,bank).wasOk() &&
        p.presetBankEntries(entries,bank).wasOk() && entries.size()==3 && algorithmOf("Vocal Room")==3,
        "replaced preset did not move to its new algorithm group");
    p.setCurrentProgram(2);require(p.loadBankPreset("Vocal Room",bank).wasOk() && p.getCurrentProgram()==3,
        "bank recall did not restore the saved algorithm");
    require(p.loadBankPreset("Legacy Hall",bank).wasOk(),"bank baseline restoration failed");
    if(juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()!=nullptr) {
        // The disk button asks only for a name and saves directly into the managed bank.
        auto* save=dynamic_cast<juce::Button*>(find(*editor,"save_preset"));require(save && bool(save->onClick),"bank save button missing");
        save->onClick();
        auto* dialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        require(dialog && dialog->getTextEditor("preset_name"),"save opened a file chooser instead of a name dialog");
        dialog->getTextEditor("preset_name")->setText("UI Hall");dialog->exitModalState(1);
        juce::Timer::callAfterDelay(60,[&] {
            require(p.presetName()=="UI Hall" && !p.presetModified() &&
                p.presetBankEntries(entries).wasOk() && entries.size()==1 && entries[0].algorithm==2,
                "name-only UI save failed or used the wrong algorithm");
            require(p.loadBankPreset("Legacy Hall",bank).wasOk(),"UI test baseline restoration failed");
            save->onClick();editor.reset();
            juce::Timer::callAfterDelay(60,[] {
                require(!juce::Component::getCurrentlyModalComponent(),"closing the editor left a preset dialog open");
                juce::MessageManager::getInstance()->stopDispatchLoop();
            });
        });
        juce::MessageManager::getInstance()->runDispatchLoop();
        std::cout<<"Preset dialog: name-only bank save and safe editor close pass\n";
    }
    const auto before=bank.loadFileAsString();
    require(bank.replaceWithText("<broken/>"),"bank corruption fixture failed");
    require(p.saveBankPreset("New",false,bank).failed() && bank.loadFileAsString()=="<broken/>" &&
        p.loadBankPreset("Legacy Hall",bank).failed() && !p.presetModified(),"broken bank was overwritten or changed the instance");
    require(bank.replaceWithText(before),"bank fixture restoration failed");
    auto xml=juce::XmlDocument::parse(bank);require(xml!=nullptr,"bank XML missing");
    xml->getFirstChildElement()->getFirstChildElement()->setAttribute("value","nan");
    require(xml->writeTo(bank),"invalid bank parameter fixture failed");
    const auto invalidName=xml->getFirstChildElement()->getStringAttribute("name");
    require(p.loadBankPreset(invalidName,bank).failed() && !p.presetModified(),"invalid bank parameter changed the instance");
    std::cout<<"Preset bank: named save/replace, persistent shared listing, legacy adoption, algorithm grouping/recall, Unicode names, visual recall, validation pass\n";
}
static void check_preset_audio() {
    PresetTestFiles files;NativeHallProcessor p,reference;
    const auto bank=files.folder.getChildFile("Audio.cineolbank");
    require(p.saveBankPreset("First",true,bank).wasOk(),"audio preset fixture save failed");
    auto alternate=[](NativeHallProcessor& processor) {
        set(processor,"bass",20);set(processor,"mid",16);set(processor,"depth",40);
        set(processor,"diffusion",30);set(processor,"predelay",88);set(processor,"analog",0);
    };
    alternate(p);require(p.saveBankPreset("Second",true,bank).wasOk() && p.loadBankPreset("First",bank).wasOk(),"audio preset preparation failed");
    p.prepareToPlay(48000,128);reference.prepareToPlay(48000,128);
    juce::AudioBuffer<float> block(2,128),expected(2,128);juce::MidiBuffer midi;unsigned position=0;
    auto render=[&](bool input) {
        for(int i=0;i<128;++i) {
            const float value=input?0.1f*std::sin(float(position+i)*0.117f):0;
            block.setSample(0,i,value);block.setSample(1,i,0);
            expected.setSample(0,i,value);expected.setSample(1,i,0);
        }
        audio=true;p.processBlock(block,midi);reference.processBlock(expected,midi);audio=false;position+=128;
        double energy=0;
        for(int i=0;i<128;++i) for(int c=0;c<2;++c) {
            require(block.getSample(c,i)==expected.getSample(c,i),"preset audio differs from equivalent manual control updates");
            energy+=double(block.getSample(c,i))*block.getSample(c,i);
        }
        return energy;
    };
    for(int i=0;i<160;++i) render(i<120);
    struct DuringRecall final : juce::AudioProcessorParameter::Listener {
        std::function<void()> callback;
        void parameterValueChanged(int,float) override {callback();}
        void parameterGestureChanged(int,bool) override {}
    } during;
    bool probed=false;during.callback=[&]{probed=true;require(p.presetRecallInProgress(),"recall transaction was not active");render(false);};
    auto* bass=p.state.getParameter("bass");bass->addListener(&during);
    require(p.loadBankPreset("Second",bank).wasOk(),"same-algorithm audio recall failed");bass->removeListener(&during);
    require(probed,"mid-recall audio check was not exercised");alternate(reference);
    double tail=0;for(int i=0;i<100;++i) tail+=render(false);
    require(tail>1e-7,"same-algorithm preset recall cleared the tail");
    p.setCurrentProgram(1);require(p.saveBankPreset("Second",true,bank).wasOk(),"cross-algorithm fixture save failed");
    p.setCurrentProgram(2);render(false);require(p.loadBankPreset("Second",bank).wasOk(),"cross-algorithm recall failed");
    reference.setCurrentProgram(1);for(int i=0;i<30;++i) render(false);
    std::cout<<"Preset audio: same-algorithm tail retained, cross-algorithm switch == manual switch, partial recall keeps previous complete settings pass\n";
}
static std::vector<float> run(int rate,int block,bool offline,int program,bool mono=false,bool low=false,float mix=1) {
    auto p=std::make_unique<NativeHallProcessor>();require(p->ready(),"missing imported bank");
    p->setCurrentProgram(program);
    set(*p,"low_latency",low?1.0f:0.0f);set(*p,"mix",mix);
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
    require(restored->state.getRawParameterValue("low_latency")->load()==(low?1.0f:0.0f),"Low latency state round trip");
    return out;
}
static void check_state_and_ranges() {
    auto p=std::make_unique<NativeHallProcessor>();
    require(p->getNumPrograms()==6 && p->getCurrentProgram()==2,"program count/default");
    for(unsigned i=0;i<15;++i)
        require(p->state.getParameter(NativeHallProcessor::ids[i])->getParameterIndex()==int(i),"legacy parameter order changed");
    require(p->state.getParameter("low_latency")->getParameterIndex()==15 &&
            !p->state.getParameter("low_latency")->isAutomatable() &&
            p->state.getRawParameterValue("low_latency")->load()==0,"Low latency parameter compatibility/default");
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
    legacy.removeChild(legacy.getChildWithProperty("id","low_latency"),nullptr);
    auto xml=legacy.createXml();juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*xml,old);
    set(*p,"low_latency",1);
    p->setStateInformation(old.getData(),int(old.getSize()));
    require(p->getCurrentProgram()==2 && p->state.getRawParameterValue("bass")->load()==20 &&
            p->state.getRawParameterValue("predelay")->load()==88,"v0.2 state migration");
    require(delay->getText(delay->getValue(),0)=="88","legacy pre-delay changed");
    require(p->state.getRawParameterValue("low_latency")->load()==0,"old session retained enabled Low latency");
    std::cout<<"6 host programs, parameter indices, pre-delay ranges/text entry, v0.2 migration pass\n";
}
static void check_low_latency() {
    auto wait_for_latency=[](NativeHallProcessor& processor,int expected) {
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+1000;
        // This console check has no OS message loop. Drain pending timers
        // until the JUCE timer thread observes elapsed time and posts again.
        while(processor.getLatencySamples()!=expected && juce::Time::getMillisecondCounterHiRes()<deadline) {
            juce::Thread::sleep(10);juce::Timer::callPendingTimersSynchronously();
        }
        require(processor.getLatencySamples()==expected,"live latency update with closed editor failed");
    };
    for(int rate:{44100,48000,96000}) for(bool mono:{false,true}) {
        auto p=std::make_unique<NativeHallProcessor>();require(p->ready(),"missing imported bank");
        set(*p,"low_latency",1);set(*p,"mix",0);set(*p,"input_db",12);
        p->setPlayConfigDetails(mono?1:2,2,rate,128);p->prepareToPlay(rate,128);
        require(p->getLatencySamples()==0,"Low latency did not report zero delay");
        juce::AudioBuffer<float> block(2,20000);juce::MidiBuffer midi;
        for(int size:{1,127,511,20000}) {
            block.setSize(2,size,false,false,true);
            for(int i=0;i<size;++i) {
                block.setSample(0,i,i==0?0.4f:0.1f*std::sin(float(i)*0.73f));
                block.setSample(1,i,i==0?-0.3f:0.2f*std::cos(float(i)*0.31f));
            }
            audio=true;p->processBlock(block,midi);audio=false;
            for(int i=0;i<size;++i) {
                const float left=i==0?0.4f:0.1f*std::sin(float(i)*0.73f);
                const float right=mono?left:(i==0?-0.3f:0.2f*std::cos(float(i)*0.31f));
                require(block.getSample(0,i)==left && block.getSample(1,i)==right,
                        "Low latency dry signal delayed, filtered, gained or overwritten");
            }
        }
        // A running processor must update host compensation without an editor.
        set(*p,"low_latency",0);
        const int expected=rate==44100?227:rate==48000?70:316;
        wait_for_latency(*p,expected);
        set(*p,"low_latency",1);wait_for_latency(*p,0);
        audio=true;p->processBlock(block,midi);audio=false;
        auto wet=run(rate,128,false,2,mono,true);
        auto mixed=run(rate,128,false,2,mono,true,0.35f);
        set(*p,"mix",0.35f);
        const float mix=p->state.getRawParameterValue("mix")->load();
        require(mixed==run(rate,511,true,2,mono,true,0.35f) &&
                mixed==run(rate,20000,false,2,mono,true,0.35f),"Low latency mix depends on block size/offline mode");
        for(int i=0;i<rate*2;++i) {
            const float left=i<rate/4?0.1f*std::sin(i*0.117f):0;
            const float right=mono?left:(i==0?0.4f:0);
            require(mixed[size_t(i)*2]==left*(1-mix)+wet[size_t(i)*2]*mix &&
                    mixed[size_t(i)*2+1]==right*(1-mix)+wet[size_t(i)*2+1]*mix,
                    "Low latency dry/wet mixing is not at the host rate");
        }
    }
    std::cout<<"Low latency: zero-delay exact dry at 44.1/48/96k, mono/stereo, 1..20000 frames, host-rate mix, live latency + closed editor pass\n";
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
    if((argc==3 || argc==4 || argc==5 || argc==6) && std::string(argv[1])=="--editor") {
        NativeHallProcessor p;
        PresetTestFiles demo;
        if(argc>=4) p.setCurrentProgram(std::atoi(argv[3]));
        if(argc==5 && std::string(argv[4])=="preset")
            require(p.savePreset(demo.folder.getChildFile("Warm Concert Hall.cineol224")).wasOk(),"preview preset failed");
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        if(argc==6) set(p,argv[4],std::strtof(argv[5],nullptr));
        if(argc==5 && std::string(argv[4])=="settings") {
            auto* button=dynamic_cast<juce::Button*>(find(*editor,"settings"));
            require(button!=nullptr,"Settings button missing");button->setToggleState(true,juce::sendNotificationSync);
        }
        juce::File file(juce::String::fromUTF8(argv[2]));
        file.getParentDirectory().createDirectory();
        juce::FileOutputStream output(file);juce::PNGImageFormat png;
        require(output.openedOk() && output.setPosition(0) && output.truncate().wasOk() && png.writeImageToStream(
            editor->createComponentSnapshot(editor->getLocalBounds()),output),"editor screenshot failed");
        return 0;
    }
    check_state_and_ranges();check_editor();check_presets();check_preset_bank();check_preset_audio();
    if(argc==3 && std::string(argv[1])=="--bank") check_daisy_engine(argv[2]);
    check_low_latency();
    for(int program=0;program<6;++program) for(int rate:{44100,48000,96000}) {
        auto a=run(rate,128,false,program), b=run(rate,511,true,program),c=run(rate,20000,false,program);
        require(a==b && b==c,"algorithm switching/block-size or offline/realtime output differs");
        require(a==run(rate,128,false,program,false,true),"Low latency changed 100% wet DSP output");
        double energy=0;for(float v:a) energy+=double(v)*v;require(energy>0.01,"silent plugin algorithm");
        auto mono=run(rate,333,false,program,true);require(!mono.empty(),"mono bus failed");
        std::cout<<"Program "<<program+1<<", "<<rate<<" Hz: switch + 128/511/20000 blocks exact, state + mono pass\n";
    }
    require(allocations==0 && releases==0,"audio callback allocated or released memory");
    std::cout<<"AU/VST processor callback: new=0, delete=0\n";
}
