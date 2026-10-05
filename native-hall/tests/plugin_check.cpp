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
static void set_cache_directory(const juce::File& folder) {
#ifdef _WIN32
    require(_putenv_s("CINEOL224_CACHE_DIR",folder.getFullPathName().toRawUTF8())==0,"could not set test cache directory");
#else
    require(setenv("CINEOL224_CACHE_DIR",folder.getFullPathName().toRawUTF8(),1)==0,"could not set test cache directory");
#endif
}
static void set(NativeHallProcessor& p,const char* id,float value) {
    auto* param=p.state.getParameter(id);param->setValueNotifyingHost(param->convertTo0to1(value));
}
static juce::Component* find(juce::Component& root,const char* id) {
    if(root.getComponentID()==id) return &root;
    for(auto* child:root.getChildren()) if(auto* found=find(*child,id)) return found;
    return nullptr;
}
static void next_page(juce::Component& editor) {
    auto* display=find(editor,"display");unsigned count=0;
    for(unsigned i=1;i<=9;++i) if(auto* button=find(editor,("parameter_page_"+juce::String(i)).toRawUTF8()))
        if(button->isVisible()) ++count;
    require(display && count,"numbered page controls missing");
    const unsigned next=(unsigned(int(display->getProperties()["page_index"]))+1)%count+1;
    auto* button=dynamic_cast<juce::Button*>(find(editor,("parameter_page_"+juce::String(next)).toRawUTF8()));
    require(button && bool(button->onClick),"numbered page action missing");button->onClick();
}

static juce::Component* findNamed(juce::Component& root,const juce::String& name) {
    if(root.getName()==name) return &root;
    for(auto* child:root.getChildren()) if(auto* found=findNamed(*child,name)) return found;
    return nullptr;
}
static void check_editor() {
    NativeHallProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* dirt=dynamic_cast<juce::Slider*>(find(*editor,"dirt"));
    auto* bass=dynamic_cast<juce::Slider*>(find(*editor,"bass"));
    auto* delay=dynamic_cast<juce::Slider*>(find(*editor,"predelay"));
    auto* algorithm=dynamic_cast<juce::Button*>(find(*editor,"algorithm"));
    auto* output=dynamic_cast<juce::Button*>(find(*editor,"output_r"));
    auto* diffusion=find(*editor,"diffusion");auto* display=find(*editor,"display");
    auto* settings=dynamic_cast<juce::Button*>(find(*editor,"settings"));
    auto* settings_panel=find(*editor,"settings_panel");
    auto* low_latency=dynamic_cast<juce::ToggleButton*>(find(*editor,"low_latency"));
    auto* spillover=dynamic_cast<juce::ToggleButton*>(find(*editor,"spillover"));
    auto* duration=dynamic_cast<juce::ComboBox*>(find(*editor,"spillover_time"));
    require(dirt && bass && delay && algorithm && output && diffusion && display,"editor controls missing");
    require(settings && settings_panel && low_latency && !settings_panel->isVisible(),"Settings controls/default missing");
    require(spillover && duration && !spillover->getToggleState() && !duration->isEnabled() &&
            duration->getSelectedId()==5,"Spillover UI/default missing");
    require(spillover->getBounds().getCentreY()==duration->getBounds().getCentreY() &&
            duration->getX()>spillover->getRight(),"Spillover checkbox and time selector are not on one row");
    spillover->setToggleState(true,juce::sendNotificationSync);duration->setSelectedId(10,juce::sendNotificationSync);
    require(duration->isEnabled() && p.state.getRawParameterValue("spillover")->load()==1 &&
            p.state.getRawParameterValue("spillover_time")->load()==10,"Spillover options are not attached");
    set(p,"spillover",0);set(p,"spillover_time",5);
    require(!duration->isEnabled(),"Spillover duration did not follow host state");
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
    require(dirt->getValue()==0 && p.state.getRawParameterValue("analog")->load()==1,"Digital Dirt default inversion");
    dirt->setValue(1,juce::sendNotificationSync);
    require(p.state.getRawParameterValue("analog")->load()==0,"Digital Dirt ON did not bypass filters");
    dirt->setValue(0,juce::sendNotificationSync);
    require(p.state.getRawParameterValue("analog")->load()==1,"Digital Dirt OFF did not enable filters");
    set(p,"analog",0);require(dirt->getValue()==1,"legacy automation did not update Digital Dirt");
    set(p,"analog",1);require(dirt->getValue()==0,"legacy clean state did not clear Digital Dirt");
    dirt->setValue(0.35,juce::sendNotificationSync);
    require(std::abs(p.state.getRawParameterValue("analog")->load()-0.65f)<1e-5f,"continuous Dirt control failed");
    require(algorithm->getParentComponent()==display && output->getParentComponent()==display && bool(algorithm->onClick) && bool(output->onClick),"algorithm/output not on red display");
    for(const char* id:{"mode_enh","decay_opt"}) require(find(*editor,id)->getParentComponent()==display,"mode outside red display");
    require(!find(*editor,"digital_dirt") && dirt->getBounds().getHeight()>500,"old bottom controls or short fader retained");
    set(p,"analog",1);
    bass->setValue(20,juce::sendNotificationSync);
    require(p.state.getRawParameterValue("bass")->load()==20 && display->getName().contains("BASS 4.6 s"),
            "fader/display did not update together");
    p.setCurrentProgram(3);
    require(p.getCurrentProgram()==3 && !diffusion->isEnabled() && display->getName().startsWith("04 | Acoustic Chamber"),
            "algorithm display/Chamber state did not update");
    delay->setValue(88,juce::sendNotificationSync);
    require(display->getName().contains("PRE-DELAY 89 ms"),"Chamber pre-delay display is wrong");
    p.setCurrentProgram(1);
    require(diffusion->isEnabled() && display->getName().startsWith("02 | Vocal Plate") &&
            display->getName().contains("PRE-DELAY 64 ms"),"Plate display/minimum did not update with unchanged fader");
    set(p,"output_r",1);
    require(p.state.getRawParameterValue("output_r")->load()==1,"output selection attachment failed");
    set(p,"analog",0);juce::MemoryBlock saved;p.getStateInformation(saved);set(p,"analog",1);
    p.setStateInformation(saved.getData(),int(saved.getSize()));
    require(dirt->getValue()==1,"Digital Dirt saved-state restoration failed");
    for(int program=0;program<6;++program) {
        set(p,"algorithm",float(program));
        require(display->getName().startsWith(juce::String(program+1).paddedLeft('0',2)+" | "),"host algorithm display failed");
    }
    for(int width:{882,1029,1470}) {
        editor->setSize(width,int(std::lround(width*1240.0/1640)));
        auto shot=editor->createComponentSnapshot(editor->getLocalBounds());
        require(shot.isValid() && shot.getWidth()==width,"resized editor snapshot failed");
    }
    auto* page=dynamic_cast<juce::Button*>(find(*editor,"parameter_page_1"));
    require(page && find(*editor,"firmware"),"page/firmware controls missing");
    set(p,"algorithm",2);set(p,"diffusion",31);
    const float saved_depth=p.state.getRawParameterValue("depth")->load();
    const float saved_diffusion=p.state.getRawParameterValue("diffusion")->load();
    struct PageEvents final : juce::AudioProcessorParameter::Listener {
        unsigned changes=0;
        void parameterValueChanged(int,float) override {++changes;}
        void parameterGestureChanged(int,bool) override {}
    } page_events;
    for(const auto* id:NativeHallProcessor::ids) p.state.getParameter(id)->addListener(&page_events);
    auto* first_page=dynamic_cast<juce::Button*>(find(*editor,"parameter_page_1"));
    auto* second_page=dynamic_cast<juce::Button*>(find(*editor,"parameter_page_2"));
    require(first_page && second_page && first_page->isVisible() && second_page->isVisible() && first_page->getToggleState(),"direct page selectors missing or unselected");
    second_page->onClick();require(int(display->getProperties()["page_index"])==1 && second_page->getToggleState() && !first_page->getToggleState(),"direct detail-page selection failed");
    second_page->onClick();first_page->onClick();
    require(int(display->getProperties()["page_index"])==0 && first_page->getToggleState() && page_events.changes==0,"direct page selection changed parameters");
    next_page(*editor);
    require(int(display->getProperties()["page_index"])==1 && !bass->isVisible() && diffusion->isVisible(),"detail page binding failed");
    for(unsigned slot=0;slot<6;++slot) if(slot!=4) {
        auto* inactive=dynamic_cast<juce::Slider*>(find(*editor,("inactive_"+juce::String(slot+1)).toRawUTF8()));
        require(inactive && inactive->isVisible() && !inactive->isEnabled() && inactive->getValue()==0,"inactive fader parking failed");
    }
    next_page(*editor);
    require(int(display->getProperties()["page_index"])==0 && bass->isVisible() && !diffusion->isVisible(),"main page binding failed");
    require(page_events.changes==0 && p.state.getRawParameterValue("depth")->load()==saved_depth &&
        p.state.getRawParameterValue("diffusion")->load()==saved_diffusion,"page navigation changed DSP/automation");
    next_page(*editor);set(p,"algorithm",3);
    const auto parking_deadline=juce::Time::getMillisecondCounterHiRes()+400;
    while(juce::Time::getMillisecondCounterHiRes()<parking_deadline) {juce::Thread::sleep(10);juce::Timer::callPendingTimersSynchronously();}
    require(!diffusion->isEnabled() && diffusion->getProperties()["motor_position"]==juce::var(0.0) &&
        p.state.getRawParameterValue("diffusion")->load()==saved_diffusion,"Chamber parking wrote the parameter");
    for(const auto* id:NativeHallProcessor::ids) p.state.getParameter(id)->removeListener(&page_events);
    next_page(*editor);
    set(p,"algorithm",2);
    juce::MemoryBlock page_state;p.getStateInformation(page_state);NativeHallProcessor reference;
    reference.setStateInformation(page_state.getData(),int(page_state.getSize()));
    p.prepareToPlay(48000,256);reference.prepareToPlay(48000,256);
    juce::AudioBuffer<float> actual(2,256),expected(2,256);juce::MidiBuffer midi;
    double tail_energy=0;
    for(unsigned block=0;block<200;++block) {
        actual.clear();expected.clear();
        if(block==0) {actual.setSample(0,0,0.5f);expected.setSample(0,0,0.5f);}
        if(block%2) next_page(*editor);
        else (int(display->getProperties()["page_index"])==0?second_page:first_page)->onClick();
        p.processBlock(actual,midi);reference.processBlock(expected,midi);
        for(int c=0;c<2;++c) for(int i=0;i<256;++i) {
            require(actual.getSample(c,i)==expected.getSample(c,i),"page navigation altered audio or cleared the tail");
            if(block>100) tail_energy+=double(actual.getSample(c,i))*actual.getSample(c,i);
        }
    }
    require(tail_energy>1e-9,"page-navigation tail fixture was silent");
    std::cout<<"Editor: fader/display, six algorithms, expanded preset display, outputs, inverted Digital Dirt + legacy automation/state, three sizes pass\n";
}
static void check_fader_limits() {
    NativeHallProcessor p;std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* display=find(*editor,"display");auto* page=dynamic_cast<juce::Button*>(find(*editor,"parameter_page_1"));
    require(display && page,"limit-check display/page missing");unsigned checked=0;
    for(int program=0;program<p.getNumPrograms();++program) {
        p.setCurrentProgram(program);
        while(int(display->getProperties()["page_index"])!=0) next_page(*editor);
        const unsigned pages=p.parameterPages();
        for(unsigned i=0;i<9;++i) {
            auto* button=dynamic_cast<juce::Button*>(find(*editor,("parameter_page_"+juce::String(i+1)).toRawUTF8()));
            require(button && button->isVisible()==(i<pages),"direct page selector count differs from algorithm pages");
        }
        auto* last=dynamic_cast<juce::Button*>(find(*editor,("parameter_page_"+juce::String(pages)).toRawUTF8()));
        auto* first=dynamic_cast<juce::Button*>(find(*editor,"parameter_page_1"));
        last->onClick();require(int(display->getProperties()["page_index"])==int(pages-1) && last->getToggleState(),"direct jump to last parameter page failed");
        require(!find(*editor,"parameter_page"),"duplicate cyclic page control remains");
        first->onClick();
        for(unsigned current=0;current<pages;++current) {
            const unsigned count=p.usesXL()?6:9;
            auto* active=dynamic_cast<juce::Button*>(find(*editor,("parameter_page_"+juce::String(current+1)).toRawUTF8()));
            require(active && active->getToggleState() && int(display->getProperties()["page_index"])==int(current),"numbered page selection disagrees with bindings");
            for(unsigned i=0;i<pages;++i) {
                auto* button=dynamic_cast<juce::Button*>(find(*editor,("parameter_page_"+juce::String(i+1)).toRawUTF8()));
                require(button->getToggleState()==(i==current),"more than one page selector is active");
                require(find(*editor,"algorithm")->getRight()<=button->getX(),"algorithm name overlaps page selectors");
            }
            for(unsigned slot=0;slot<count;++slot) {
                const auto id=p.usesXL()?"xl_slot_"+juce::String(slot+1):juce::String(NativeHallProcessor::ids[slot]);
                auto* slider=dynamic_cast<juce::Slider*>(find(*editor,id.toRawUTF8()));
                if(!slider || !slider->isVisible() || !slider->isEnabled()) continue;
                slider->setValue(slider->getMinimum(),juce::sendNotificationSync);
                slider->setValue(slider->getMaximum(),juce::sendNotificationSync);
                if(display->getName().contains("--")) {
                    std::cerr<<"Endpoint "<<p.getProgramName(program)<<" page "<<current+1<<" slot "<<slot+1<<": "<<display->getName()<<'\n';
                    require(false,"active fader maximum displayed as inactive dashes");
                }
                ++checked;
            }
            if(current+1<pages) next_page(*editor);
        }
    }
    p.setCurrentProgram(6);while(int(display->getProperties()["page_index"])!=0) next_page(*editor);
    auto* crossover=dynamic_cast<juce::Slider*>(find(*editor,"xl_slot_3"));require(crossover && crossover->isEnabled(),"Crossover limit fixture missing");
    crossover->setValue(0,juce::sendNotificationSync);crossover->setValue(252,juce::sendNotificationSync);
    require(display->getName().endsWith("19.0 kHz"),"last finite XL Crossover value was lost");
    for(int value:{253,254,255}) {
        crossover->setValue(value,juce::sendNotificationSync);
        require(display->getName().endsWith("INF kHz") && p.state.getRawParameterValue("xl_02")->load()==value,
            "XL infinite frequency display changed control value or remained dashes");
    }
    p.setCurrentProgram(11);while(int(display->getProperties()["page_index"])!=0) next_page(*editor);
    auto* decay=dynamic_cast<juce::Slider*>(find(*editor,"xl_slot_1"));require(decay && decay->isEnabled(),"decay limit fixture missing");
    decay->setValue(0,juce::sendNotificationSync);decay->setValue(decay->getMaximum(),juce::sendNotificationSync);
    require(display->getName().endsWith("INF s"),"XL infinite decay was displayed as an unavailable parameter");
    require(checked>200,"insufficient active fader endpoint coverage");
    std::cout<<"Fader limits: "<<checked<<" active endpoints across all 224/XL pages, finite-to-INF frequency boundary and infinite decay pass\n";
}
struct PresetTestFiles {
    juce::File folder=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("cineol-preset-check-"+juce::Uuid().toString());
    PresetTestFiles() {require(folder.createDirectory().wasOk(),"could not create isolated preset fixtures");}
    ~PresetTestFiles() {folder.deleteRecursively();}
};
static void check_presets() {
    PresetTestFiles files;NativeHallProcessor p;
    const auto first=files.folder.getChildFile("Warm Concert Hall.cineol224");
    const auto second=files.folder.getChildFile("Short Hall.cineol224");
    set(p,"analog",0.8f);
    require(p.savePreset(first).wasOk() && p.presetName()=="Warm Concert Hall" && !p.presetModified(),"preset save/name failed");
    set(p,"bass",5);set(p,"depth",40);set(p,"mix",0.5f);set(p,"low_latency",1);set(p,"analog",0.15f);
    require(p.presetModified(),"edited preset was not marked modified");
    require(p.savePreset(second).wasOk(),"second preset save failed");
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* bass=dynamic_cast<juce::Slider*>(find(*editor,"bass"));
    auto* dirt=dynamic_cast<juce::Slider*>(find(*editor,"dirt"));
    auto* preset=dynamic_cast<juce::Button*>(find(*editor,"preset"));
    require(bass && dirt && preset,"preset UI missing");
    struct Counter final : juce::AudioProcessorParameter::Listener {
        int changes=0;
        void parameterValueChanged(int,float) override {++changes;}
        void parameterGestureChanged(int,bool) override {}
    } counter,dirt_counter;
    auto* parameter=p.state.getParameter("bass");parameter->addListener(&counter);
    auto* dirt_parameter=p.state.getParameter("analog");dirt_parameter->addListener(&dirt_counter);
    const double start=double(bass->getProperties()["motor_position"]);
    const double dirt_start=double(dirt->getProperties()["motor_position"]);
    require(p.loadPreset(first).wasOk() && p.getCurrentProgram()==2,"preset load failed");
    const double target=bass->valueToProportionOfLength(17);
    require(bass->getValue()==17 && counter.changes==1 && bool(bass->getProperties()["motor_moving"]) &&
        std::abs(double(bass->getProperties()["motor_position"])-start)<0.0001,"preset did not apply immediately with visual-only motion");
    require(std::abs(dirt->getValue()-0.2)<0.0001 && dirt_counter.changes==1 && bool(dirt->getProperties()["motor_moving"]) &&
        std::abs(double(dirt->getProperties()["motor_position"])-dirt_start)<0.0001,"Dirt preset recall did not animate from its previous position");
    auto tick=[](int ms) {
        const auto end=juce::Time::getMillisecondCounterHiRes()+ms;
        while(juce::Time::getMillisecondCounterHiRes()<end) {juce::Thread::sleep(10);juce::Timer::callPendingTimersSynchronously();}
    };
    tick(100);const double middle=double(bass->getProperties()["motor_position"]);
    const double dirt_middle=double(dirt->getProperties()["motor_position"]);
    require(middle>start && middle<target && counter.changes==1 && bass->getValue()==17,"fader animation changed audio/automation or skipped motion");
    require(dirt_middle<dirt_start && dirt_middle>0.2 && dirt_counter.changes==1 &&
        std::abs(p.state.getRawParameterValue("analog")->load()-0.8f)<0.0001,"Dirt animation changed the parameter or skipped motion");
    // A second recall starts at the currently drawn cap, not either endpoint.
    require(p.loadPreset(second).wasOk(),"rapid preset recall failed");
    require(std::abs(double(bass->getProperties()["motor_position"])-middle)<0.03,"rapid recall jumped its visual starting point");
    // The old trajectory advances while the second file is parsed. Allow a
    // small fraction of this larger travel, but reject a jump to either end.
    require(std::abs(double(dirt->getProperties()["motor_position"])-dirt_middle)<0.08,"rapid Dirt recall jumped its visual starting point");
    tick(400);
    require(!bool(bass->getProperties()["motor_moving"]) &&
        std::abs(double(bass->getProperties()["motor_position"])-start)<0.0001 && counter.changes==2,
        "animation did not settle or generated extra parameter events");
    require(!bool(dirt->getProperties()["motor_moving"]) &&
        std::abs(double(dirt->getProperties()["motor_position"])-dirt_start)<0.0001 && dirt_counter.changes==2,
        "Dirt animation did not settle or generated extra parameter events");
    require(p.loadPreset(first).wasOk(),"repeat recall failed");
    bass->setValue(22,juce::sendNotificationSync);
    dirt->setValue(0.45,juce::sendNotificationSync);
    require(!bool(bass->getProperties()["motor_moving"]) && p.presetModified() &&
        std::abs(double(bass->getProperties()["motor_position"])-bass->valueToProportionOfLength(22))<0.0001,
        "manual edit did not interrupt visual motion");
    require(!bool(dirt->getProperties()["motor_moving"]) &&
        std::abs(double(dirt->getProperties()["motor_position"])-0.45)<0.0001 &&
        std::abs(p.state.getRawParameterValue("analog")->load()-0.55f)<0.0001,"manual Dirt edit did not interrupt recall animation");
    parameter->removeListener(&counter);
    dirt_parameter->removeListener(&dirt_counter);
    require(p.state.getRawParameterValue("low_latency")->load()==1,"preset changed instance Low latency");
    require(p.loadPreset(first).wasOk() && !p.presetModified(),"preset baseline restoration failed");
    juce::MemoryBlock session;p.getStateInformation(session);NativeHallProcessor restored;
    restored.setStateInformation(session.getData(),int(session.getSize()));
    require(restored.presetName()=="Warm Concert Hall" && !restored.presetModified(),"DAW state lost preset identity/baseline");
    auto firmware_xml=juce::XmlDocument::parse(first);
    require(firmware_xml && firmware_xml->getStringAttribute("firmware")=="224-v4.4" &&
        firmware_xml->getIntAttribute("version")==4,"preset firmware identity missing");
    const auto foreign=files.folder.getChildFile("Foreign.cineol224");
    for(const auto& identity:juce::StringArray{"224xl-v8.21","unknown"}) {
        firmware_xml->setAttribute("firmware",identity);firmware_xml->writeTo(foreign);
        require(p.loadPreset(foreign).failed() && !p.presetModified(),"unavailable firmware preset changed current sound");
    }
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
    set(p,"analog",0.8f);
    require(p.savePreset(legacy).wasOk(),"legacy bank fixture failed");
    juce::StringArray names;
    require(p.presetNames(names,bank).wasOk() && names.contains("Legacy Hall"),"legacy preset was not adopted");
    set(p,"bass",5);set(p,"mix",0.5f);set(p,"low_latency",1);set(p,"analog",0.15f);
    const auto unicode=juce::String::fromUTF8("\u0417\u0430\u043b / \u0442\u0451\u043f\u043b\u044b\u0439");
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
    auto* dirt=dynamic_cast<juce::Slider*>(find(*editor,"dirt"));
    require(p.loadBankPreset("Legacy Hall",bank).wasOk() && bass && bass->getValue()==17 &&
        bool(bass->getProperties()["motor_moving"]),"bank recall did not animate the fader");
    require(dirt && std::abs(dirt->getValue()-0.2)<0.0001 && bool(dirt->getProperties()["motor_moving"]) &&
        std::abs(double(dirt->getProperties()["motor_position"])-0.85)<0.0001,
        "bank recall did not restore and animate fractional Dirt");
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
        dialog->getTextEditor("preset_name")->setText("UI Hall");dialog->getButton("Save")->onClick();
        juce::MessageManager::callAsync([&] {
            require(p.presetName()=="UI Hall" && !p.presetModified() &&
                p.presetBankEntries(entries).wasOk() && entries.size()==1 && entries[0].algorithm==2,
                "name-only UI save failed or used the wrong algorithm");
            require(p.loadBankPreset("Legacy Hall",bank).wasOk(),"UI test baseline restoration failed");
            save->onClick();
            auto* again=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
            require(again && again->getTextEditor("preset_name"),"second save dialog missing");
            again->getTextEditor("preset_name")->setText("UI Hall");again->getButton("Save")->onClick();
            juce::MessageManager::callAsync([&] {
                auto* replace=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
                require(replace && replace->getName()=="Replace preset?","replacement confirmation missing");
                require(&replace->getLookAndFeel()==&save->getLookAndFeel(),"replacement dialog lost the editor theme");
                replace->getButton("Cancel")->onClick();
                juce::MessageManager::callAsync([&] {
                    require(p.presetName()=="Legacy Hall" && !p.presetModified(),"cancelling replacement changed the preset");
                    save->onClick();editor.reset();
                    juce::MessageManager::callAsync([] {
                        require(!juce::Component::getCurrentlyModalComponent(),"closing the editor left a preset dialog open");
                        juce::MessageManager::getInstance()->stopDispatchLoop();
                    });
                });
            });
        });
        juce::MessageManager::getInstance()->runDispatchLoop();
        std::cout<<"Preset dialog: name-only bank save, themed replacement/cancel and safe editor close pass\n";
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
    // Listing an unavailable firmware must not make the shared library
    // unreadable. Availability is checked only when applying that preset.
    const auto mixed_bank=files.folder.getChildFile("Mixed").getChildFile("User Presets.cineolbank");
    require(p.saveBankPreset("Current",false,mixed_bank).wasOk(),"mixed bank preparation failed");
    auto mixed=juce::XmlDocument::parse(mixed_bank);auto foreign=std::make_unique<juce::XmlElement>(*mixed->getFirstChildElement());
    foreign->setAttribute("name","XL fixture");foreign->setAttribute("firmware","224xl-v8.1a");
    mixed->addChildElement(foreign.release());mixed->writeTo(mixed_bank);
    require(p.presetBankEntries(entries,mixed_bank).wasOk() && entries.size()==2,
        "unavailable firmware made the shared library unreadable");
    require(p.loadBankPreset("XL fixture",mixed_bank).failed() && p.presetName()=="Current" && !p.presetModified(),
        "unavailable firmware recall changed current sound");
    std::cout<<"Preset bank: named save/replace, persistent shared listing, legacy adoption, algorithm grouping/recall, Unicode names, visual recall, validation pass\n";
}
static void check_editor_window_focus() {
    require(juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()!=nullptr,"Focus check requires the macOS desktop");
    NativeHallProcessor first,second;
    class HostWindow final : public juce::DocumentWindow {
    public:
        explicit HostWindow(NativeHallProcessor& processor):DocumentWindow("Cineol focus regression",juce::Colours::black,allButtons) {
            setUsingNativeTitleBar(true);setContentOwned(processor.createEditor(),true);centreWithSize(getWidth(),getHeight());setVisible(true);
        }
        void closeButtonPressed() override {}
    } first_host(first),second_host(second);
    auto* first_editor=first_host.getContentComponent();auto* second_editor=second_host.getContentComponent();
    auto* save=dynamic_cast<juce::Button*>(find(*first_editor,"save_preset"));
    auto* other_bass=dynamic_cast<juce::Slider*>(find(*second_editor,"bass"));
    require(save && other_bass,"focus fixture controls missing");
    first_host.toFront(true);save->onClick();
    auto* dialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    require(dialog!=nullptr,"focus fixture dialog missing");
    require(first_editor->isParentOf(dialog) && !dialog->isOnDesktop(),"preset dialog is not owned by its editor");
    dialog->getTextEditor("preset_name")->setText("Cancelled on window hide");
    require(!other_bass->isCurrentlyBlockedByAnotherModalComponent(),"Visible preset dialog blocks another plugin instance");
    first_host.setVisible(false);
    require(!other_bass->isCurrentlyBlockedByAnotherModalComponent(),"A hidden plugin's preset dialog blocks another editor's faders");
    juce::Timer::callAfterDelay(100,[&] {
        require(!juce::Component::getCurrentlyModalComponent(),"Hiding the editor left an active preset dialog");
        require(!NativeHallProcessor::presetBankFile().loadFileAsString().contains("Cancelled on window hide"),"window-hide cancellation saved an unfinished preset");
        first_host.setVisible(true);first_host.toFront(true);save->onClick();
        require(juce::Component::getCurrentlyModalComponent()!=nullptr,"cancelled dialog prevented reopening Save");
        second_host.toFront(true);other_bass->grabKeyboardFocus();
        juce::Timer::callAfterDelay(100,[&] {
            require(!juce::Component::getCurrentlyModalComponent(),"Changing plugin windows left an active preset dialog");
            auto* output=dynamic_cast<juce::Button*>(find(*second_editor,"output_l"));
            require(output!=nullptr,"focus fixture output missing");output->onClick();
            auto* menu=findNamed(*second_editor,"menu");
            require(menu && second_editor->isParentOf(menu) && !menu->isOnDesktop(),"popup is a detached native window");
            first_host.clearContentComponent();
            require(menu->isVisible() && menu->isCurrentlyModal(),"closing one editor dismissed another instance's menu");
            menu->keyPressed(juce::KeyPress(juce::KeyPress::downKey));menu->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
            juce::Timer::callAfterDelay(100,[&] {
                require(!juce::Component::getCurrentlyModalComponent() && !other_bass->isCurrentlyBlockedByAnotherModalComponent(),
                    "selecting a menu item left the faders blocked");
                require(second.saveBankPreset("Focus Hall").wasOk(),"focus preset fixture failed");
                second.setCurrentProgram(6);
                require(second.loadBankPreset("Focus Hall").wasOk(),"focus fixture preset recall failed");
                const double before=other_bass->getValue();
                const auto layout=other_bass->getLookAndFeel().getSliderLayout(*other_bass).sliderBounds;
                const juce::Point<float> start(float(layout.getCentreX()),float(layout.getY()+layout.getHeight()*(1-other_bass->valueToProportionOfLength(before))));
                const auto now=juce::Time::getCurrentTime();
                const auto source=juce::Desktop::getInstance().getMainMouseSource();
                const juce::MouseEvent down(source,start,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,
                    other_bass,other_bass,now,start,now,1,false);
                other_bass->mouseDown(down);other_bass->mouseDrag(down.withNewPosition(start.translated(0,-50)));
                other_bass->mouseUp(down.withNewPosition(start.translated(0,-50)));
                require(other_bass->getValue()!=before && second.state.getRawParameterValue("bass")->load()==float(other_bass->getValue()),
                    "fader drag after focus changes/preset recall did not reach the processor");
                juce::MessageManager::getInstance()->stopDispatchLoop();
            });
        });
    });
    juce::MessageManager::getInstance()->runDispatchLoop();
    std::cout<<"Editor focus: scoped dialogs, hide/refocus cancellation, safe reopen, parented menus, instance isolation and real fader drag after preset recall pass\n";
}
static void check_quick_presets() {
    PresetTestFiles files;NativeHallProcessor p;
    const auto bank=files.folder.getChildFile("Quick.cineolbank");
    p.setCurrentProgram(2);set(p,"bass",9);require(p.saveBankPreset("Quick Hall",false,bank).wasOk(),"quick Hall fixture failed");
    p.setCurrentProgram(6);set(p,"analog",0.32f);require(p.saveBankPreset("Quick XL",false,bank).wasOk(),"quick XL fixture failed");
    require(p.loadBankPreset("Quick Hall",bank).wasOk(),"quick baseline failed");
    juce::MemoryBlock old_state;p.getStateInformation(old_state);
    const auto before=bank.loadFileAsString();
    struct HostEvents final : juce::AudioProcessorListener {
        unsigned parameters=0,metadata=0;
        void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {++parameters;}
        void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails& details) override {if(details.nonParameterStateChanged) ++metadata;}
    } host;
    p.addListener(&host);
    for(unsigned slot=0;slot<8;++slot) require(p.assignQuickPreset(slot,slot%2?"quick xl":"quick hall",bank).wasOk(),"eight-slot assignment failed");
    p.removeListener(&host);
    require(p.quickPresetNames()[0]=="Quick Hall" && p.quickPresetNames()[7]=="Quick XL" &&
        bank.loadFileAsString()==before && p.getCurrentProgram()==2 && !p.presetModified() && host.parameters==0 && host.metadata==8,
        "assignment changed sound/bank, lost canonical names or failed to notify host state");
    set(p,"spillover",1);set(p,"spillover_time",10);set(p,"low_latency",1);
    require(p.loadQuickPreset(7,bank).wasOk() && p.usesXL() && p.activeQuickPreset()==7 &&
        std::abs(p.state.getRawParameterValue("analog")->load()-0.32f)<1e-5f &&
        p.state.getRawParameterValue("spillover_time")->load()==10 && p.state.getRawParameterValue("spillover")->load()==1 &&
        p.state.getRawParameterValue("low_latency")->load()==1,"quick XL recall or instance options failed");
    set(p,"mix",0.3f);require(p.activeQuickPreset()==-1,"edited quick preset kept its active light");
    require(p.loadQuickPreset(0,bank).wasOk() && !p.usesXL() && p.activeQuickPreset()==0 &&
        p.state.getRawParameterValue("bass")->load()==9,"quick recall back to 224 failed");
    require(p.assignQuickPreset(0,"Quick XL",bank).wasOk() && !p.usesXL() && p.quickPresetNames()[0]=="Quick XL" &&
        p.loadQuickPreset(0,bank).wasOk() && p.activeQuickPreset()==0,"quick slot replacement failed");
    juce::MemoryBlock saved;p.getStateInformation(saved);NativeHallProcessor restored;
    restored.setStateInformation(saved.getData(),int(saved.getSize()));
    require(restored.quickPresetNames()==p.quickPresetNames() && restored.activeQuickPreset()==0 &&
        restored.loadQuickPreset(6,bank).wasOk() && !restored.usesXL(),"quick session round trip failed");
    restored.setStateInformation(old_state.getData(),int(old_state.getSize()));
    for(const auto& name:restored.quickPresetNames()) require(name.isEmpty(),"older session retained newer assignments");
    require(restored.loadQuickPreset(0,bank).failed() && p.assignQuickPreset(8,"Quick Hall",bank).failed() &&
        p.loadQuickPreset(8,bank).failed() && p.assignQuickPreset(0,"missing",bank).failed() &&
        p.quickPresetNames()[0]=="Quick XL","invalid quick action altered assignments");
    require(p.loadQuickPreset(2,bank).wasOk(),"quick replacement baseline failed");
    set(p,"bass",11);require(p.saveBankPreset("Quick Hall",true,bank).wasOk() && p.loadQuickPreset(2,bank).wasOk() &&
        p.state.getRawParameterValue("bass")->load()==11,"quick key did not follow a replaced bank preset");
    require(p.saveBankPreset("Portable",false,bank).wasOk() && !bank.loadFileAsString().contains("quick_preset"),"instance mappings leaked into sound presets");
    require(bank.deleteFile() && p.loadQuickPreset(0,bank).failed() && p.getCurrentProgram()==2 &&
        p.presetName()=="Portable" && !p.presetModified(),"missing quick preset changed current sound");
    std::cout<<"Quick presets: eight assignments, replacement, 224/XL recall, instance options, session/legacy state, latest bank contents and failed-load guards pass\n";
}
static void check_preset_browser() {
    NativeHallProcessor p;const auto bank=NativeHallProcessor::presetBankFile();
    const bool existed=bank.existsAsFile();const auto previous=bank.loadFileAsString();
    if(existed) require(bank.deleteFile(),"could not isolate browser fixture bank");
    p.setCurrentProgram(2);set(p,"analog",0.82f);require(p.saveBankPreset("Browser Hall").wasOk(),"browser Hall fixture failed");
    p.setCurrentProgram(1);require(p.saveBankPreset("Browser Plate").wasOk(),"browser Plate fixture failed");
    p.setCurrentProgram(6);set(p,"analog",0.23f);require(p.saveBankPreset("Browser XL").wasOk(),"browser XL fixture failed");
    require(p.loadBankPreset("Browser Hall").wasOk(),"browser baseline failed");
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* open=dynamic_cast<juce::Button*>(find(*editor,"preset"));require(open && bool(open->onClick),"browser launcher missing");open->onClick();
    auto* browser=find(*editor,"preset_browser");
    auto* filters=dynamic_cast<juce::ListBox*>(find(*editor,"preset_algorithm_filter"));
    auto* presets=dynamic_cast<juce::ListBox*>(find(*editor,"preset_list"));
    auto* search=dynamic_cast<juce::TextEditor*>(find(*editor,"preset_search"));
    auto* all=dynamic_cast<juce::Button*>(find(*editor,"preset_all_algorithms"));
    auto* load=dynamic_cast<juce::Button*>(find(*editor,"load_browser_preset"));
    auto* close=dynamic_cast<juce::Button*>(find(*editor,"close_preset_browser"));
    auto* assign=dynamic_cast<juce::Button*>(find(*editor,"assign_quick_preset"));
    require(browser && browser->isVisible() && filters && presets && search && all && load && close && assign,"preset browser controls missing");
    auto query=[&](const char* text) {
        // Drive the same callback as typed text without posting delayed
        // notifications after earlier modal tests have stopped their loop.
        search->setText(text,false);search->onTextChange();
    };
    require(filters->getListBoxModel()->getNumRows()==28 && presets->getListBoxModel()->getNumRows()==3,"browser did not show one unified preset list");
    require(presets->getListBoxModel()->getTooltipForRow(0).contains("Large Concert Hall B") &&
            presets->getListBoxModel()->getTooltipForRow(2).contains("224 XL"),"browser algorithm/model annotation missing");
    struct Events final : juce::AudioProcessorParameter::Listener {
        unsigned changes=0;
        void parameterValueChanged(int,float) override {++changes;}
        void parameterGestureChanged(int,bool) override {}
    } events;
    for(const auto* id:NativeHallProcessor::ids) p.state.getParameter(id)->addListener(&events);
    filters->getListBoxModel()->returnKeyPressed(2);require(presets->getListBoxModel()->getNumRows()==1,"single algorithm filter failed");
    filters->getListBoxModel()->returnKeyPressed(1);require(presets->getListBoxModel()->getNumRows()==2,"two algorithm union failed");
    filters->getListBoxModel()->returnKeyPressed(6);require(presets->getListBoxModel()->getNumRows()==3,"three algorithm union failed");
    filters->getListBoxModel()->returnKeyPressed(2);require(presets->getListBoxModel()->getNumRows()==2,"algorithm filter removal failed");
    all->onClick();require(presets->getListBoxModel()->getNumRows()==3,"all algorithms reset failed");
    query("bRoWsEr xL");require(presets->getListBoxModel()->getNumRows()==1,"case-insensitive preset search failed");
    query("Concert");require(presets->getListBoxModel()->getNumRows()==2,"algorithm search failed");
    query("no matching fixture");require(presets->getListBoxModel()->getNumRows()==0 && !load->isEnabled() && !assign->isEnabled(),"empty search action guard failed");
    query("224 XL");require(presets->getListBoxModel()->getNumRows()==1,"model search failed");
    require(events.changes==0 && p.getCurrentProgram()==2 && p.presetName()=="Browser Hall","filtering/search changed sound or automation");
    presets->selectRow(0);require(assign->isEnabled(),"assignment did not enable for selected row");
    if(juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()!=nullptr &&
        !juce::MessageManager::getInstance()->hasStopMessageBeenSent()) {
        assign->onClick();auto* menu=findNamed(*editor,"menu");
        require(menu && &menu->getLookAndFeel()==&assign->getLookAndFeel(),"assignment menu missing or lost its theme");
        query("Browser Plate"); // The menu must keep the row selected when it was opened.
        for(unsigned i=0;i<8;++i) menu->keyPressed(juce::KeyPress(juce::KeyPress::downKey));
        menu->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
        juce::MessageManager::callAsync([&] {
            require(p.quickPresetNames()[7]=="Browser XL" && events.changes==0 && p.presetName()=="Browser Hall",
                "menu assignment loaded sound or used a later selected row");
            juce::MessageManager::getInstance()->stopDispatchLoop();
        });
        juce::MessageManager::getInstance()->runDispatchLoop();
        require(p.quickPresetNames()[7]=="Browser XL","assignment callback was not dispatched");
    } else require(p.assignQuickPreset(7,"Browser XL").wasOk(),"headless assignment failed");
    query("224 XL");
    for(const auto* id:NativeHallProcessor::ids) p.state.getParameter(id)->removeListener(&events);
    presets->selectRow(0);require(load->isEnabled(),"browser load did not enable for selected preset");load->onClick();
    require(p.usesXL() && p.presetName()=="Browser XL" && !p.presetModified() &&
            std::abs(p.state.getRawParameterValue("analog")->load()-0.23f)<1e-5f,"browser recall did not restore XL and fractional Dirt");
    query("");presets->selectRow(0);presets->getListBoxModel()->returnKeyPressed(0);
    require(!p.usesXL() && p.presetName()=="Browser Hall" && !p.presetModified(),"browser keyboard recall back to 224 failed");
    close->onClick();require(!browser->isVisible(),"browser close failed");
    for(unsigned slot=0;slot<8;++slot) {
        auto* key=dynamic_cast<juce::Button*>(find(*editor,("quick_preset_"+juce::String(slot+1)).toRawUTF8()));
        require(key && key->isVisible() && key->getY()>900 && key->getBottom()<1160 &&
            key->isEnabled()==(slot==7),"quick key layout/empty-slot guard failed");
        if(slot==7) {key->onClick();require(p.usesXL() && p.activeQuickPreset()==7 && key->getToggleState(),"quick key click/active light failed");}
    }
    open->onClick();require(browser->isVisible() && presets->getListBoxModel()->getNumRows()==3,"browser reopen failed");
    require(p.saveBankPreset("Added while open").wasOk(),"browser refresh fixture failed");
    close->onClick();open->onClick();require(presets->getListBoxModel()->getNumRows()==4,"browser did not refresh the shared bank on reopen");
    require(browser->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && !browser->isVisible(),"browser Escape close failed");
    editor.reset();
    if(existed) require(bank.replaceWithText(previous),"could not restore browser fixture bank");
    else require(bank.deleteFile(),"could not remove browser fixture bank");
    std::cout<<"Preset browser: unified list + algorithm/model, one/two/three-filter union, search without parameter events, 224/XL + Dirt recall, keyboard, refresh and close pass\n";
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
static std::vector<float> run(int rate,int block,bool offline,int program,bool mono=false,bool low=false,float mix=1,float dirt=-1) {
    auto p=std::make_unique<NativeHallProcessor>();require(p->ready(),"missing imported bank");
    p->setCurrentProgram(program);
    set(*p,"low_latency",low?1.0f:0.0f);set(*p,"mix",mix);if(dirt>=0) set(*p,"analog",1-dirt);
    p->setPlayConfigDetails(mono?1:2,2,rate,2048);p->setNonRealtime(offline);p->prepareToPlay(rate,2048);
    juce::AudioBuffer<float> b(2,20000);juce::MidiBuffer midi;
    const int frames=rate*2;std::vector<float> out(size_t(frames)*2);
    for(int pos=0;pos<frames;) {
        int n=std::min(block,frames-pos);
        // Change controls at a shared sample position independent of block size.
        if(pos<rate && pos+n>rate) n=rate-pos;
        if(pos==rate) {
            set(*p,"bass",20);set(*p,"mid",16);set(*p,"diffusion",30);set(*p,"analog",dirt>=0?1-dirt:0);
            set(*p,"algorithm",float((program<6?(program+1)%6:6+(program-6+1)%int(cineol::xl::graphs.size()))));
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
            restored->getCurrentProgram()==(program<6?(program+1)%6:6+(program-6+1)%int(cineol::xl::graphs.size())),"parameter/algorithm state round trip");
    require(restored->state.getRawParameterValue("low_latency")->load()==(low?1.0f:0.0f),"Low latency state round trip");
    return out;
}
static void check_state_and_ranges() {
    auto p=std::make_unique<NativeHallProcessor>();
    require(p->getNumPrograms()==int(NativeHallProcessor::program_count) && p->getCurrentProgram()==2,"program count/default");
    for(unsigned i=0;i<15;++i)
        require(p->state.getParameter(NativeHallProcessor::ids[i])->getParameterIndex()==int(i),"legacy parameter order changed");
    require(p->state.getParameter("low_latency")->getParameterIndex()==15 &&
            !p->state.getParameter("low_latency")->isAutomatable() &&
            p->state.getRawParameterValue("low_latency")->load()==0,"Low latency parameter compatibility/default");
    require(p->state.getParameter("spillover")->getParameterIndex()==int(NativeHallProcessor::parameter_count)+1 &&
            !p->state.getParameter("spillover")->isAutomatable() &&
            p->state.getRawParameterValue("spillover")->load()==0 &&
            p->state.getRawParameterValue("spillover_time")->load()==5,"Spillover parameter compatibility/default");
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
    p->setCurrentProgram(-1);p->setCurrentProgram(int(NativeHallProcessor::program_count));require(p->getCurrentProgram()==5,"invalid host program accepted");
    set(*p,"bass",20);set(*p,"predelay",88);
    auto legacy=p->state.copyState();legacy.removeChild(legacy.getChildWithProperty("id","algorithm"),nullptr);
    legacy.removeChild(legacy.getChildWithProperty("id","low_latency"),nullptr);
    legacy.removeChild(legacy.getChildWithProperty("id","spillover"),nullptr);
    legacy.removeChild(legacy.getChildWithProperty("id","spillover_time"),nullptr);
    auto xml=legacy.createXml();juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*xml,old);
    set(*p,"low_latency",1);set(*p,"spillover",1);set(*p,"spillover_time",10);
    p->setStateInformation(old.getData(),int(old.getSize()));
    require(p->getCurrentProgram()==2 && p->state.getRawParameterValue("bass")->load()==20 &&
            p->state.getRawParameterValue("predelay")->load()==88,"v0.2 state migration");
    require(delay->getText(delay->getValue(),0)=="88","legacy pre-delay changed");
    require(p->state.getRawParameterValue("low_latency")->load()==0,"old session retained enabled Low latency");
    require(p->state.getRawParameterValue("spillover")->load()==0 &&
            p->state.getRawParameterValue("spillover_time")->load()==5,"old session retained Spillover options");
    std::cout<<NativeHallProcessor::program_count<<" host programs, parameter indices, pre-delay ranges/text entry, v0.2 migration pass\n";
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
static void check_desktop_engine(const char* path) {
    juce::MemoryBlock data;
    require(juce::File(juce::String::fromUTF8(path)).loadFileAsData(data),"missing desktop bank fixture");
    auto bank=std::make_unique<native_hall::ProgramBank>();
    require(native_hall::read_bank(data.getData(),data.getSize(),*bank),"invalid desktop bank fixture");
    auto p=std::make_unique<NativeHallProcessor>();p->prepareToPlay(48000,511);
    auto engine=std::make_unique<native_hall::DesktopEngine48>();engine->prepare(*bank);
    juce::AudioBuffer<float> block(2,511);juce::MidiBuffer midi;
    unsigned position=0;
    // Directed switches plus simultaneous delay/mode changes. Compare the
    // shipped processor with the desktop event-DAC engine.
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
                require(block.getSample(c,i)==expected[i][c],"plugin differs from desktop engine");
            // Keep control positions fixed until the next switch.
        }
        ++position;
    }
    std::cout<<"36 directed switches + all pre-delay ranges/modes: plugin == desktop event-DAC engine exactly at 48k\n";
}
static void check_dirt() {
    for(int program:{2,6,11}) {
        const auto clean=run(48000,128,false,program,false,false,1,0);
        const auto dirty=run(48000,128,false,program,false,false,1,1);
        const auto half=run(48000,128,false,program,false,false,1,0.5f);
        require(half==run(48000,511,true,program,false,false,1,0.5f),"continuous Dirt depends on render blocks");
        double from_clean=0,from_dirty=0;for(unsigned i=0;i<half.size();++i) {
            from_clean+=std::abs(double(half[i])-clean[i]);from_dirty+=std::abs(double(half[i])-dirty[i]);
            require(std::isfinite(half[i]) && std::abs(half[i])<4,"continuous Dirt output is invalid");
        }
        require(from_clean>0.01 && from_dirty>0.01,"Dirt still behaves like a switch");
    }
    PresetTestFiles files;NativeHallProcessor p;set(p,"analog",0.63f);
    const auto file=files.folder.getChildFile("A Very Long Concert Hall Preset Name With Gentle Digital Dirt.cineol224");
    require(p.savePreset(file).wasOk(),"fractional Dirt preset save failed");set(p,"analog",1);
    require(p.loadPreset(file).wasOk() && std::abs(p.state.getRawParameterValue("analog")->load()-0.63f)<1e-5f,"fractional Dirt preset recall failed");
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());auto* name=find(*editor,"preset");
    editor->createComponentSnapshot(editor->getLocalBounds());
    require(bool(name->getProperties()["marquee_active"]) && double(name->getProperties()["marquee_offset"])==0,"long preset did not start with readable pause");
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+2200;
    while(juce::Time::getMillisecondCounterHiRes()<deadline) {juce::Thread::sleep(10);juce::Timer::callPendingTimersSynchronously();}
    editor->createComponentSnapshot(editor->getLocalBounds());
    require(double(name->getProperties()["marquee_offset"])>0 && !p.presetModified(),"preset marquee did not scroll or changed sound");
    const auto bank=files.folder.getChildFile("Dirt Across Engines.cineolbank");
    p.setCurrentProgram(2);set(p,"analog",0.8f);
    require(p.saveBankPreset("Clean 224",false,bank).wasOk(),"224 Dirt bank fixture failed");
    p.setCurrentProgram(6);set(p,"analog",0.15f);
    require(p.saveBankPreset("Dirty XL",false,bank).wasOk(),"XL Dirt bank fixture failed");
    auto* dirt=dynamic_cast<juce::Slider*>(find(*editor,"dirt"));
    require(p.loadBankPreset("Clean 224",bank).wasOk() && !p.usesXL() && dirt &&
        std::abs(dirt->getValue()-0.2)<0.0001 && bool(dirt->getProperties()["motor_moving"]) &&
        std::abs(double(dirt->getProperties()["motor_position"])-0.85)<0.0001,
        "cross-engine bank recall did not restore and animate Dirt");
    const auto settle=juce::Time::getMillisecondCounterHiRes()+400;
    while(juce::Time::getMillisecondCounterHiRes()<settle) {juce::Thread::sleep(10);juce::Timer::callPendingTimersSynchronously();}
    require(p.loadBankPreset("Dirty XL",bank).wasOk() && p.usesXL() &&
        std::abs(dirt->getValue()-0.85)<0.0001 && bool(dirt->getProperties()["motor_moving"]) &&
        std::abs(double(dirt->getProperties()["motor_position"])-0.2)<0.0001,
        "224-to-XL bank recall did not restore and animate Dirt");
    std::cout<<"Dirt: continuous native sound at intermediate amounts, bounded + block invariant, fractional presets, marquee pause/scroll pass\n";
}
static void check_xl() {
    PresetTestFiles files;auto p=std::make_unique<NativeHallProcessor>();
    const auto bank=files.folder.getChildFile("Cross Engine.cineolbank");
    require(p->saveBankPreset("Original",false,bank).wasOk(),"original fixture failed");
    p->setCurrentProgram(6);set(*p,"xl_chorus",9);set(*p,"xl_diffusion",42);
    require(p->ready() && p->usesXL() && p->firmwareId()=="224xl-v8.21","XL engine unavailable");
    require(p->saveBankPreset("XL",false,bank).wasOk(),"XL fixture failed");
    juce::Array<NativeHallProcessor::PresetInfo> entries;
    require(p->presetBankEntries(entries,bank).wasOk() && entries.size()==2,"cross engine preset listing failed");
    auto reference=std::make_unique<NativeHallProcessor>();
    // Split programs need one output from each independent engine. A/C would
    // select two outputs of the left engine and silently discard the right input.
    for(int program=23;program<28;++program) {
        auto split=std::make_unique<NativeHallProcessor>();split->setCurrentProgram(program);
        require(split->state.getRawParameterValue("output_l")->load()==0 &&
            split->state.getRawParameterValue("output_r")->load()==1,"split factory routing does not cover both engines");
        split->prepareToPlay(48000,128);juce::AudioBuffer<float> signal(2,128);juce::MidiBuffer events;
        double right_energy=0;
        for(unsigned block=0;block<400;++block) {
            signal.clear();for(int i=0;i<128;++i) signal.setSample(1,i,0.12f*std::sin(float(block*128+i)*0.117f));
            audio=true;split->processBlock(signal,events);audio=false;
            for(int i=0;i<128;++i) right_energy+=double(signal.getSample(1,i))*signal.getSample(1,i);
        }
        require(right_energy>0.01,"split factory routing loses the right input");
    }
    auto echo=std::make_unique<NativeHallProcessor>();echo->setCurrentProgram(20);
    require(echo->state.getRawParameterValue("output_l")->load()==2 && echo->state.getRawParameterValue("output_r")->load()==0,
        "Chorus/Echo factory routing reverses left and right");
    std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditor());
    auto* algorithms=dynamic_cast<juce::Button*>(find(*editor,"algorithm"));
    auto* badge=dynamic_cast<juce::Button*>(find(*editor,"firmware"));
    require(algorithms && bool(algorithms->onClick) && algorithms->getParentComponent()==find(*editor,"display") && badge && badge->getButtonText()=="224 XL","unified XL UI missing");
    require(find(*editor,"xl_slot_1")->isVisible() && !find(*editor,"bass")->isVisible(),"XL fader binding failed");
    auto* dynamic=dynamic_cast<juce::Button*>(find(*editor,"xl_dynamic_decay"));
    auto* optimization=dynamic_cast<juce::Button*>(find(*editor,"decay_opt"));
    require(dynamic && dynamic->isVisible() && dynamic->isEnabled() && optimization && optimization->isEnabled(),"XL dynamics switches unavailable");
    dynamic->onClick();require((p->xlControl(42)&1)!=0,"dynamic switch did not update logical control");
    set(*p,"decay_opt",0);require(p->saveBankPreset("Dynamics",false,bank).wasOk(),"dynamic preset save failed");
    dynamic->onClick();set(*p,"decay_opt",1);
    require(p->loadBankPreset("Dynamics",bank).wasOk() && (p->xlControl(42)&1)!=0 &&
        p->state.getRawParameterValue("decay_opt")->load()==0 && !p->presetModified(),"dynamic switches did not round trip through preset");
    juce::MemoryBlock dynamic_session;p->getStateInformation(dynamic_session);NativeHallProcessor dynamic_restored;
    dynamic_restored.setStateInformation(dynamic_session.getData(),int(dynamic_session.getSize()));
    require((dynamic_restored.xlControl(42)&1)!=0 && dynamic_restored.state.getRawParameterValue("decay_opt")->load()==0,"DAW session lost dynamic switches");
    require(p->loadBankPreset("XL",bank).wasOk(),"dynamic fixture restoration failed");
    auto* page=dynamic_cast<juce::Button*>(find(*editor,"parameter_page_1"));
    require(page && page->isEnabled() && p->parameterPages()==5,"XL pages unavailable");
    const float saved_lf=p->state.getRawParameterValue("xl_00")->load();
    next_page(*editor);
    require(int(find(*editor,"display")->getProperties()["page_index"])==1 &&
        find(*editor,"xl_slot_3")->getProperties()["parameter_id"]=="xl_chorus","XL page control rebinding failed");
    auto* chorus=dynamic_cast<juce::Slider*>(find(*editor,"xl_slot_3"));
    chorus->setValue(72,juce::sendNotificationSync);
    require(p->state.getRawParameterValue("xl_chorus")->load()==9 && p->state.getRawParameterValue("xl_00")->load()==saved_lf,"XL fader wrote the wrong page parameter");
    next_page(*editor);next_page(*editor);next_page(*editor);
    auto* size=dynamic_cast<juce::Slider*>(find(*editor,"xl_slot_1"));
    require(size->getProperties()["parameter_id"]=="xl_43","XL Size binding targets marker rather than logical Size");
    require(!find(*editor,"xl_slot_2")->isEnabled(),"XL inactive slot is enabled");
    require(find(*editor,"xl_slot_3")->isEnabled(),"XL reverb stop delay unavailable");
    next_page(*editor);
    require(p->loadBankPreset("XL",bank).wasOk(),"XL page fixture reset failed");
    for(int rate:{44100,48000,96000}) {
        p->setCurrentProgram(2);reference->setCurrentProgram(2);
        p->setPlayConfigDetails(2,2,rate,128);reference->setPlayConfigDetails(2,2,rate,128);
        p->prepareToPlay(rate,128);reference->prepareToPlay(rate,128);
        juce::AudioBuffer<float> actual(2,128),expected(2,128);juce::MidiBuffer midi;
        for(int program=0;program<int(NativeHallProcessor::program_count);++program) {
            p->setCurrentProgram(program);reference->setCurrentProgram(program);
            if(program==6) {
                require(p->loadBankPreset("Original",bank).wasOk() && !p->usesXL(),"XL to 224 recall failed");
                require(p->loadBankPreset("XL",bank).wasOk() && p->usesXL() && !p->presetModified(),"224 to XL recall failed");
                set(*reference,"xl_chorus",9);set(*reference,"xl_diffusion",42);
            }
            double tail=0;
            for(unsigned block=0;block<200;++block) {
                for(unsigned i=0;i<128;++i) {
                    const float value=block<80?0.08f*std::sin(float(block*128+i)*0.117f):0;
                    for(int c=0;c<2;++c) {actual.setSample(c,int(i),value);expected.setSample(c,int(i),value);}
                }
                audio=true;p->processBlock(actual,midi);reference->processBlock(expected,midi);audio=false;
                for(int c=0;c<2;++c) for(int i=0;i<128;++i) {
                    require(std::isfinite(actual.getSample(c,i)) && actual.getSample(c,i)==expected.getSample(c,i),"cross-engine recall differs from manual selection");
                    if(block>100) tail+=double(actual.getSample(c,i))*actual.getSample(c,i);
                }
            }
            const bool finite_effect=NativeHallProcessor::isXL(program) &&
                (cineol::xl::graphs[program-native_hall::program_count].bank==4 ||
                 cineol::xl::Graph(program-native_hall::program_count)==cineol::xl::Graph::inverse_room);
            if(!finite_effect) require(tail>1e-8,"native algorithm has no reverb tail");
            require(p->getLatencySamples()==reference->getLatencySamples(),"engine switch changed latency");
        }
    }
    juce::MemoryBlock session;p->getStateInformation(session);auto restored=std::make_unique<NativeHallProcessor>();
    restored->setStateInformation(session.getData(),int(session.getSize()));
    require(restored->getCurrentProgram()==int(NativeHallProcessor::program_count)-1 && restored->usesXL() &&
        restored->state.getRawParameterValue("xl_diffusion")->load()==p->state.getRawParameterValue("xl_diffusion")->load(),"XL session round trip failed");
    std::cout<<"Unified selection + cross-engine presets/session/tails: "<<NativeHallProcessor::program_count<<" algorithms, 44.1/48/96k pass\n";
    for(int program=6;program<int(NativeHallProcessor::program_count);++program) for(int rate:{44100,48000,96000}) {
        auto a=run(rate,128,false,program),b=run(rate,511,true,program);
        require(a==b && a==run(rate,20000,false,program),"XL output depends on block size/offline mode");
        require(a==run(rate,128,false,program,false,true),"XL low-latency wet changed DSP");
        run(rate,333,false,program,true);
        std::cout<<"XL "<<program-5<<", "<<rate<<" Hz: native audio, block invariance, wet-only + mono pass\n";
    }
    for(int program=6;program<int(NativeHallProcessor::program_count);++program) {
        p->setCurrentProgram(program);set(*p,"mix",0);set(*p,"low_latency",0);p->prepareToPlay(48000,128);
        juce::AudioBuffer<float> dry(2,128);juce::MidiBuffer events;
        for(unsigned n=0;n<100;++n) {dry.clear();audio=true;p->processBlock(dry,events);audio=false;}
        dry.clear();dry.setSample(0,0,0.25f);dry.setSample(1,0,-0.125f);
        audio=true;p->processBlock(dry,events);audio=false;
        for(int i=0;i<128;++i) require(dry.getSample(0,i)==(i==70?0.25f:0) && dry.getSample(1,i)==(i==70?-0.125f:0),"XL common physical dry alignment failed");
    }
    p->setCurrentProgram(10);set(*p,"mix",0);set(*p,"low_latency",1);p->prepareToPlay(48000,128);
    juce::AudioBuffer<float> block(2,128);juce::MidiBuffer midi;block.clear();block.setSample(0,0,0.25f);
    audio=true;p->processBlock(block,midi);audio=false;
    require(block.getSample(0,0)==0.25f,"XL low latency dry not immediate");
    for(int i=1;i<128;++i) require(block.getSample(0,i)==0,"XL dry path modified input");
}
// Exercise the complete plugin path; all event positions are independent of host block size.
static std::vector<float> spillover_render(int rate,int block,int from,int to,bool low,bool rapid=false,bool disable=false,bool mono=false,float mix=1) {
    NativeHallProcessor p;set(p,"spillover",1);set(p,"spillover_time",1);set(p,"mix",mix);
    set(p,"low_latency",low?1.0f:0.0f);p.setCurrentProgram(from);
    p.setPlayConfigDetails(mono?1:2,2,rate,128);p.prepareToPlay(rate,128);
    const int change=rate/4,second=change+rate/100,third=second+rate/100,end=rate*3/2;
    juce::AudioBuffer<float> b(2,20000);juce::MidiBuffer midi;std::vector<float> result(size_t(end)*2);
    for(int pos=0;pos<end;) {
        if(pos==change) p.setCurrentProgram(to);
        if(rapid && pos==second) p.setCurrentProgram(1);
        if(rapid && pos==third) p.setCurrentProgram(27);
        if(disable && pos==rate/2) set(p,"spillover",0);
        int n=std::min(block,end-pos);
        for(int event:{change,second,third,rate/2}) if(pos<event && pos+n>event) n=event-pos;
        b.setSize(2,n,false,false,true);
        for(int i=0;i<n;++i) {
            const int at=pos+i;
            const float input=at<rate/8 || (rapid && at>=change && at<rate/2)?0.12f*std::sin(at*0.117f):0;
            b.setSample(0,i,input);b.setSample(1,i,mono?input:input*0.7f);
        }
        audio=true;p.processBlock(b,midi);audio=false;
        for(int i=0;i<n;++i) for(int c=0;c<2;++c) {
            const float value=b.getSample(c,i);require(std::isfinite(value) && std::abs(value)<4,"Spillover nonfinite/runaway output");
            result[size_t(pos+i)*2+c]=value;
        }
        pos+=n;
    }
    return result;
}
static std::vector<float> spillover_catalog(int rate,int block) {
    NativeHallProcessor p;set(p,"spillover",1);set(p,"spillover_time",10);
    p.setPlayConfigDetails(2,2,rate,128);p.prepareToPlay(rate,128);
    const int section=rate/10,end=section*int(NativeHallProcessor::program_count);
    juce::AudioBuffer<float> b(2,20000);juce::MidiBuffer midi;std::vector<float> result(size_t(end)*2);
    for(int pos=0;pos<end;) {
        if(pos%section==0) p.setCurrentProgram(pos/section);
        int n=std::min({block,end-pos,section-pos%section});b.setSize(2,n,false,false,true);
        for(int i=0;i<n;++i) {b.setSample(0,i,0.08f*std::sin((pos+i)*0.117f));b.setSample(1,i,0.06f*std::cos((pos+i)*0.093f));}
        audio=true;p.processBlock(b,midi);audio=false;
        for(int i=0;i<n;++i) for(int c=0;c<2;++c) {
            const float value=b.getSample(c,i);require(std::isfinite(value) && std::abs(value)<4,"Spillover catalog produced nonfinite/runaway output");
            result[size_t(pos+i)*2+c]=value;
        }
        pos+=n;
    }
    return result;
}
static void check_spillover_early_disable() {
    NativeHallProcessor p,old,fresh;
    set(p,"spillover",1);set(p,"spillover_time",1);set(p,"mix",0.25f);
    p.setCurrentProgram(6);old.setCurrentProgram(6);fresh.setCurrentProgram(2);
    for(auto* processor:{&p,&old,&fresh}) {processor->setPlayConfigDetails(2,2,48000,128);processor->prepareToPlay(48000,128);}
    constexpr int change=12000,disable=change+240,end=change+2400;
    juce::AudioBuffer<float> actual(2,128),previous(2,128),next(2,128);juce::MidiBuffer midi;
    auto signal=[](int at) {return at>=0?0.12f*std::sin(at*0.117f):0.0f;};
    auto smooth=[](float x) {return x*x*(3-2*x);};
    float worst=0;
    for(int pos=0;pos<end;) {
        if(pos==change) p.setCurrentProgram(2);
        if(pos==disable) set(p,"spillover",0);
        int n=std::min(128,end-pos);
        for(int event:{change,disable}) if(pos<event && pos+n>event) n=event-pos;
        for(auto* b:{&actual,&previous,&next}) b->setSize(2,n,false,false,true);
        for(int i=0;i<n;++i) for(int c=0;c<2;++c) {
            const int at=pos+i,elapsed=at-change;
            const float old_input=elapsed<0?1:std::max(0.0f,1-float(elapsed)/240);
            actual.setSample(c,i,signal(at));previous.setSample(c,i,signal(at)*old_input);
            next.setSample(c,i,signal(at)*(1-old_input));
        }
        audio=true;p.processBlock(actual,midi);old.processBlock(previous,midi);
        if(pos>=change) fresh.processBlock(next,midi);audio=false;
        for(int i=0;i<n;++i) for(int c=0;c<2;++c) {
            const int at=pos+i,elapsed=at-change;
            float wet=previous.getSample(c,i);
            if(elapsed>=0) {
                const float gain=at<disable?smooth(1-float(elapsed)/48000):
                    smooth(1-240.0f/48000)*smooth(std::max(0.0f,1-float(at-disable)/960));
                wet=wet*gain+next.getSample(c,i);
            }
            const float expected=signal(at-70)*0.75f+wet*0.25f;
            worst=std::max(worst,std::abs(actual.getSample(c,i)-expected));
        }
        pos+=n;
    }
    require(worst<2e-7f,"early Spillover disable jumped in mix level, duplicated dry or lost new input");
    std::cout<<"Spillover early disable: continuous new input, 25% wet, common dry, 20ms retirement match independent references; error="<<worst<<'\n';
}
static void check_spillover() {
    check_spillover_early_disable();
    PresetTestFiles files;NativeHallProcessor options;
    const auto preset=files.folder.getChildFile("Spillover.cineol224");
    require(options.savePreset(preset).wasOk(),"Spillover preset fixture failed");
    set(options,"spillover",1);set(options,"spillover_time",7);
    require(options.loadPreset(preset).wasOk() && options.state.getRawParameterValue("spillover")->load()==1 &&
            options.state.getRawParameterValue("spillover_time")->load()==7,"preset overwrote instance Spillover options");
    juce::MemoryBlock state;options.getStateInformation(state);NativeHallProcessor restored;
    restored.setStateInformation(state.getData(),int(state.getSize()));
    require(restored.state.getRawParameterValue("spillover")->load()==1 &&
            restored.state.getRawParameterValue("spillover_time")->load()==7,"session lost Spillover options");
    // Compare to two independent reference instances, preserving the old
    // settings, output pair and delay memory. No new input follows the switch.
    for(int seconds:{1,5,10}) {
        NativeHallProcessor p,old,fresh;set(p,"spillover",1);set(p,"spillover_time",float(seconds));
        p.setCurrentProgram(2);old.setCurrentProgram(2);fresh.setCurrentProgram(6);
        for(auto* processor:{&p,&old,&fresh}) {processor->setPlayConfigDetails(2,2,48000,128);processor->prepareToPlay(48000,128);}
        const int change=12000,length=seconds*48000,end=change+length+256;
        juce::AudioBuffer<float> actual(2,128),previous(2,128),next(2,128);juce::MidiBuffer midi;
        double tail_energy=0;float worst=0;
        for(int pos=0;pos<end;) {
            if(pos==change) p.setCurrentProgram(6);
            int n=std::min(128,end-pos);if(pos<change && pos+n>change) n=change-pos;
            for(auto* b:{&actual,&previous,&next}) b->setSize(2,n,false,false,true);
            next.clear();
            for(int i=0;i<n;++i) for(int c=0;c<2;++c) {
                float input=pos+i<6000?0.12f*std::sin((pos+i)*0.117f):0;
                actual.setSample(c,i,input);previous.setSample(c,i,input);
            }
            audio=true;p.processBlock(actual,midi);old.processBlock(previous,midi);
            if(pos>=change) fresh.processBlock(next,midi);audio=false;
            for(int i=0;i<n;++i) for(int c=0;c<2;++c) {
                const int elapsed=pos+i-change;
                float expected=previous.getSample(c,i);
                if(elapsed>=0) {
                    const float x=std::max(0.0f,1-float(elapsed)/length);
                    expected=expected*x*x*(3-2*x)+next.getSample(c,i);
                    if(elapsed<4800) tail_energy+=double(actual.getSample(c,i))*actual.getSample(c,i);
                }
                worst=std::max(worst,std::abs(actual.getSample(c,i)-expected));
            }
            pos+=n;
        }
        require(tail_energy>1e-6 && worst<2e-7f,"Spillover tail/fade differs from independent reference or exceeds duration");
        std::cout<<"Spillover "<<seconds<<"s: old tail + silent new algorithm match independent reference, maximum error="<<worst<<'\n';
    }
    for(int rate:{44100,48000,96000}) {
        const auto catalog=spillover_catalog(rate,128);
        require(catalog==spillover_catalog(rate,511) && catalog==spillover_catalog(rate,20000),
                "Spillover across all 28 programs depends on host block size");
        for(auto pair:{std::pair{2,1},std::pair{6,7},std::pair{2,6},std::pair{6,2},std::pair{27,2},std::pair{2,27}}) {
            auto a=spillover_render(rate,128,pair.first,pair.second,false);
            require(a==spillover_render(rate,511,pair.first,pair.second,false) &&
                    a==spillover_render(rate,20000,pair.first,pair.second,true),"Spillover depends on host blocks or changes low-latency wet audio");
        }
        auto rapid=spillover_render(rate,128,2,6,false,true);
        require(rapid==spillover_render(rate,511,2,6,false,true) &&
                rapid==spillover_render(rate,20000,2,6,false,true),"rapid Spillover retirement depends on block size");
        auto disabled=spillover_render(rate,128,6,2,false,false,true);
        require(disabled==spillover_render(rate,511,6,2,false,false,true),"disabling Spillover depends on block size");
        for(bool low:{false,true}) for(bool mono:{false,true}) {
            const auto dry=spillover_render(rate,128,2,6,low,true,false,mono,0);
            require(dry==spillover_render(rate,128,27,1,low,true,false,mono,0),"Spillover duplicated or reset the common dry path");
        }
        std::cout<<"Spillover "<<rate<<" Hz: 224/XL/split transitions, 128/511/20000 blocks, rapid retirement, disable, mono/stereo dry + low-latency pass\n";
    }
    require(allocations==0 && releases==0,"Spillover audio allocated or released memory");
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    // Offline fixtures belong only to an isolated test cache, never to the
    // shipped plugin or the user's first-run data directory.
    struct TestCache {
        juce::File folder;
        ~TestCache() {if(folder.isDirectory()) folder.deleteRecursively();}
    } test_cache;
    if((argc==3 && std::string(argv[1])=="--bank") || (argc==4 && (std::string(argv[1])=="--banks" || std::string(argv[1])=="--preset-check" || std::string(argv[1])=="--limits-check" || std::string(argv[1])=="--spillover-check" || std::string(argv[1])=="--quick-check" || std::string(argv[1])=="--focus-check"))) {
        test_cache.folder=juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("cineol-plugin-check",{},false);
        require(test_cache.folder.createDirectory().wasOk(),"could not create test cache");
        set_cache_directory(test_cache.folder);
        require(juce::File(juce::String::fromUTF8(argv[2])).copyFileTo(CineolRomBank::cacheFile()),"could not seed test bank");
        if(argc==4) require(juce::File(juce::String::fromUTF8(argv[3])).copyFileTo(CineolRomBank::xlCacheFile()),"could not seed XL bank");
    }
    if(argc==4 && std::string(argv[1])=="--focus-check") {check_editor_window_focus();return 0;}
    if(argc==4 && std::string(argv[1])=="--quick-check") {check_quick_presets();check_preset_browser();return 0;}
    if(argc==4 && std::string(argv[1])=="--spillover-check") {
        check_state_and_ranges();check_editor();check_spillover();return 0;
    }
    if(argc==4 && std::string(argv[1])=="--preset-check") {
        check_presets();check_preset_bank();check_quick_presets();check_preset_browser();check_preset_audio();return 0;
    }
    if(argc==4 && std::string(argv[1])=="--limits-check") {check_fader_limits();return 0;}
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
    if(argc==5 && std::string(argv[1])=="--editor" && (std::string(argv[4])=="presets" || std::string(argv[4])=="quick" || std::string(argv[4])=="assign")) {
        const auto original=CineolRomBank::cacheFile(),xl=CineolRomBank::xlCacheFile();
        test_cache.folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("cineol-browser-preview-"+juce::Uuid().toString());
        require(test_cache.folder.createDirectory().wasOk(),"browser preview cache failed");
        set_cache_directory(test_cache.folder);
        require(original.copyFileTo(CineolRomBank::cacheFile()) && xl.copyFileTo(CineolRomBank::xlCacheFile()),"browser preview banks failed");
    }
    if((argc==3 || argc==4 || argc==5 || argc==6) && std::string(argv[1])=="--editor") {
        NativeHallProcessor p;
        PresetTestFiles demo;
        if(argc==5 && (std::string(argv[4])=="presets" || std::string(argv[4])=="quick" || std::string(argv[4])=="assign")) {
            for(const auto& fixture:std::array<std::pair<int,const char*>,6>{{{2,"Warm Hall"},{1,"Soft Vocal Plate"},{6,"Airy Concert"},{7,"Bright Space"},{2,"Wide Hall"},{6,"Long Concert"}}}) {
                p.setCurrentProgram(fixture.first);require(p.saveBankPreset(fixture.second).wasOk(),"browser preview preset failed");
            }
        }
        if(argc>=4) p.setCurrentProgram(std::atoi(argv[3]));
        if(argc==5 && (std::string(argv[4])=="presets" || std::string(argv[4])=="quick" || std::string(argv[4])=="assign")) {
            const std::array<const char*,6> names{{"Warm Hall","Soft Vocal Plate","Airy Concert","Bright Space","Wide Hall","Long Concert"}};
            for(unsigned slot=0;slot<names.size();++slot) require(p.assignQuickPreset(slot,names[slot]).wasOk(),"preview quick assignment failed");
            require(p.loadQuickPreset(0).wasOk(),"preview quick recall failed");
        }
        if(argc==5 && std::string(argv[4])=="preset")
            require(p.savePreset(demo.folder.getChildFile("Warm Concert Hall.cineol224")).wasOk(),"preview preset failed");
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        juce::Component* snapshot=editor.get();
        if(argc==6) {
            set(p,argv[4],std::strtof(argv[5],nullptr));
            const auto end=juce::Time::getMillisecondCounterHiRes()+400;
            while(juce::Time::getMillisecondCounterHiRes()<end) {juce::Thread::sleep(10);juce::Timer::callPendingTimersSynchronously();}
        }
        if(argc==5 && (std::string(argv[4])=="detail" || std::string(argv[4])=="last")) {
            const auto id=std::string(argv[4])=="last" ? "parameter_page_"+std::to_string(p.parameterPages()) : "parameter_page_2";
            auto* button=dynamic_cast<juce::Button*>(find(*editor,id.c_str()));
            require(button!=nullptr,"Page button missing");button->onClick();
            const auto end=juce::Time::getMillisecondCounterHiRes()+400;
            while(juce::Time::getMillisecondCounterHiRes()<end) {juce::Thread::sleep(10);juce::Timer::callPendingTimersSynchronously();}
        }
        if(argc==5 && std::string(argv[4])=="settings") {
            auto* button=dynamic_cast<juce::Button*>(find(*editor,"settings"));
            require(button!=nullptr,"Settings button missing");button->setToggleState(true,juce::sendNotificationSync);
        }
        if(argc==5 && std::string(argv[4])=="save") {
            auto* button=dynamic_cast<juce::Button*>(find(*editor,"save_preset"));
            require(button!=nullptr,"Save button missing");button->onClick();
            snapshot=juce::Component::getCurrentlyModalComponent();
            require(dynamic_cast<juce::AlertWindow*>(snapshot)!=nullptr,"Save dialog missing");
        }
        if(argc==5 && (std::string(argv[4])=="presets" || std::string(argv[4])=="assign")) {
            auto* button=dynamic_cast<juce::Button*>(find(*editor,"preset"));
            require(button && bool(button->onClick),"Preset browser button missing");button->onClick();
            require(find(*editor,"preset_browser") && find(*editor,"preset_browser")->isVisible(),"Preset browser missing");
        }
        if(argc==5 && (std::string(argv[4])=="algorithms" || std::string(argv[4])=="outputs" || std::string(argv[4])=="assign")) {
            require(juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()!=nullptr,"Popup preview requires access to the macOS desktop");
            const auto mode=std::string(argv[4]);
            auto* button=dynamic_cast<juce::Button*>(find(*editor,mode=="outputs"?"output_l":mode=="assign"?"assign_quick_preset":"algorithm"));
            require(button!=nullptr,"Menu button missing");button->onClick();
            if(auto* menu=findNamed(*editor,"menu")) snapshot=menu;
            require(snapshot!=editor.get(),"Popup menu missing");
        }
        juce::File file(juce::String::fromUTF8(argv[2]));
        file.getParentDirectory().createDirectory();
        juce::FileOutputStream output(file);juce::PNGImageFormat png;
        require(output.openedOk() && output.setPosition(0) && output.truncate().wasOk() && png.writeImageToStream(
            snapshot->createComponentSnapshot(snapshot->getLocalBounds()),output),"editor screenshot failed");
        return 0;
    }
    check_state_and_ranges();check_editor();check_presets();check_preset_bank();
    // Mixed 224/XL quick recall requires the optional XL bank fixture.
    if(NativeHallProcessor().programAvailable(6)) check_quick_presets();
    else std::cout<<"Mixed 224/XL quick recall: skipped (224-only bank fixture)\n";
    check_preset_audio();
    if(std::string(argc>1?argv[1]:"")=="--bank" || std::string(argc>1?argv[1]:"")=="--banks") check_desktop_engine(argv[2]);
    check_low_latency();
    if(argc==4 && std::string(argv[1])=="--banks") {check_preset_browser();check_fader_limits();check_xl();check_dirt();check_spillover();}
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
