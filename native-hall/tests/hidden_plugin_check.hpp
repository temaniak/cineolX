// Included by plugin_check.cpp; exercises the host/UI contract against ROM-
// derived DSP values and independent Rate/Depth editing, including immediate saves.
static void check_hidden_controls() {
    NativeHallProcessor p;
    auto expect=[&](int rate,int depth,int opt) {
        require(p.originalHiddenValues()==std::array<int,3>{rate,depth,opt},"hidden effective values differ from independent control contract");
        for(unsigned f=0;f<3;++f) {
            auto* parameter=p.state.getParameter(NativeHallProcessor::ids[NativeHallProcessor::mod_rate_index+f]);
            require(parameter->convertFrom0to1(parameter->getValue())==float(std::array<int,3>{rate,depth,opt}[f]),"host parameter differs from effective hidden setting");
        }
    };
    for(unsigned f=0;f<3;++f) {
        auto* parameter=p.state.getParameter(NativeHallProcessor::ids[NativeHallProcessor::mod_rate_index+f]);
        require(parameter && parameter->isAutomatable() && parameter->isDiscrete() && parameter->getNumSteps()==16,"hidden parameter range/automation missing");
        require(parameter->getParameterIndex()==int(NativeHallProcessor::mod_rate_index+f+3),"hidden parameter displaced legacy host indices");
    }
    expect(2,1,5);
    for(int program=0;program<6;++program) {
        p.setCurrentProgram(program);set(p,"mode_enh",1);expect(2,1,5);
        set(p,"mod_depth",8);set(p,"mod_rate",2);expect(2,8,5);
        set(p,"mod_rate",2);set(p,"mod_depth",8);expect(2,8,5);
        set(p,"mod_rate",1);expect(1,8,5);set(p,"mod_depth",8);expect(1,8,5);
        for(int rate=1;rate<=16;++rate) {set(p,"mod_rate",float(rate));expect(rate,8,5);}
        for(int depth=1;depth<=16;++depth) {set(p,"mod_depth",float(depth));expect(16,depth,5);}
        for(int rate=1;rate<=16;++rate)for(int depth=1;depth<=16;++depth) {
            set(p,"mod_depth",float(depth));set(p,"mod_rate",float(rate));expect(rate,depth,5);
            set(p,"mod_rate",float(rate));set(p,"mod_depth",float(depth));expect(rate,depth,5);
        }
        set(p,"mod_rate",8);set(p,"mod_depth",6);set(p,"optimization_response",12);expect(8,6,12);
        p.setCurrentProgram(program);expect(8,6,12);
        set(p,"decay_opt",0);set(p,"decay_opt",1);expect(8,6,12);
        set(p,"mode_enh",0);expect(2,1,5);
        set(p,"mod_rate",8);set(p,"mod_depth",6);set(p,"optimization_response",12);expect(8,6,12);
        set(p,"mode_enh",1);expect(2,1,5);
        set(p,"mod_depth",8);set(p,"mod_rate",2);expect(2,8,5);
        // Saving before the message-thread mirrors update retains both knobs.
        juce::MemoryBlock saved;p.getStateInformation(saved);
        set(p,"mod_depth",1);p.setStateInformation(saved.getData(),int(saved.getSize()));expect(2,8,5);
        set(p,"mod_depth",8);expect(2,8,5);
        set(p,"mod_rate",1);set(p,"mod_depth",16);set(p,"optimization_response",1);
        p.getStateInformation(saved);NativeHallProcessor restored;
        restored.setStateInformation(saved.getData(),int(saved.getSize()));
        require(restored.originalHiddenValues()==std::array<int,3>{1,16,1},"session restore replayed Rate or Mod reset over saved hidden values");
        p.setCurrentProgram((program+1)%6);expect(2,1,5);
    }
    PresetTestFiles files;const auto file=files.folder.getChildFile("Hidden.cineol224");
    p.setCurrentProgram(2);set(p,"mode_enh",0);set(p,"mod_rate",1);set(p,"mod_depth",9);set(p,"optimization_response",16);
    require(p.savePreset(file).wasOk(),"hidden preset save failed");
    auto xml=juce::XmlDocument::parse(file);require(xml && xml->getIntAttribute("version")==7,"hidden preset schema missing");
    p.setCurrentProgram(3);set(p,"mode_enh",1);
    require(p.loadPreset(file).wasOk(),"hidden preset recall failed");expect(1,9,16);
    require(!p.presetModified(),"recalled hidden preset marked modified");
    // Every former v5 sound preset must load into a modified instance with
    // original hidden defaults; v5's complete count stays 69, not 72.
    auto legacy=juce::ValueTree::fromXml(*xml);legacy.setProperty("version",5,nullptr);
    for(unsigned i=NativeHallProcessor::mod_rate_index;i<NativeHallProcessor::parameter_count;++i)
        legacy.removeChild(legacy.getChildWithProperty("id",NativeHallProcessor::ids[i]),nullptr);
    require(legacy.createXml()->writeTo(file) && p.loadPreset(file).wasOk(),"v5 preset migration failed");expect(2,1,5);
    // Legacy sessions likewise reset hidden values in an already used instance.
    juce::MemoryBlock saved;p.getStateInformation(saved);auto session=juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize()));
    auto tree=juce::ValueTree::fromXml(*session);
    for(unsigned i=NativeHallProcessor::mod_rate_index;i<NativeHallProcessor::parameter_count;++i)
        tree.removeChild(tree.getChildWithProperty("id",NativeHallProcessor::ids[i]),nullptr);
    juce::AudioProcessor::copyXmlToBinary(*tree.createXml(),saved);set(p,"mod_depth",16);set(p,"optimization_response",1);
    p.setStateInformation(saved.getData(),int(saved.getSize()));expect(2,1,5);
    p.setEditorAppearance({1,.8f,false});p.setEditorPage(2);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto* page=dynamic_cast<juce::Button*>(find(*editor,"parameter_page_3"));require(page && page->isVisible(),"224 hidden control page missing");page->onClick();
    for(unsigned f=0;f<3;++f) {
        const auto* id=NativeHallProcessor::ids[NativeHallProcessor::mod_rate_index+f];
        auto* slider=dynamic_cast<juce::Slider*>(find(*editor,id));
        auto* value=dynamic_cast<juce::Slider*>(find(*editor,("screen_value_"+juce::String(f)).toRawUTF8()));
        require(slider && slider->isVisible() && slider->isEnabled() && value && value->isEnabled(),"hidden fader/screen inaccessible");
        require(value->getProperties()["parameter_id"].toString()==id && slider->getMinimum()==1 && slider->getMaximum()==16,"hidden fader/screen binding incorrect");
        value->setValue(11,juce::sendNotificationSync);
        require(p.originalHiddenValues()[f]==11,"screen value did not change hidden control");
    }
    auto* compact=dynamic_cast<juce::ToggleButton*>(find(*editor,"compact_mode"));compact->setToggleState(true,juce::dontSendNotification);compact->onClick();
    auto* value=dynamic_cast<juce::Slider*>(find(*editor,"screen_value_1"));value->setValue(15,juce::sendNotificationSync);expect(11,15,11);
    set(p,"mod_rate",2);juce::Timer::callPendingTimersSynchronously();
    require(value->getValue()==15 && p.originalHiddenValues()==std::array<int,3>{2,15,11},"Rate edit changed compact Mod Depth");
    // Direct host parameter setValue is realtime-safe and works without an
    // editor or deferred APVTS publication; DSP consumes the canonical state.
    editor.reset();p.prepareToPlay(48000,127);juce::AudioBuffer<float> buffer(2,127);juce::MidiBuffer midi;
    for(unsigned block=0;block<180;++block) {
        if(block%7==0) {
            auto* rate=p.state.getParameter("mod_rate");auto* depth=p.state.getParameter("mod_depth");auto* opt=p.state.getParameter("optimization_response");
            audio=true;static_cast<juce::AudioProcessorParameter*>(rate)->setValue(rate->convertTo0to1(float(1+block%16)));
            static_cast<juce::AudioProcessorParameter*>(depth)->setValue(depth->convertTo0to1(float(1+(block*3)%16)));
            static_cast<juce::AudioProcessorParameter*>(opt)->setValue(opt->convertTo0to1(float(1+(block*7)%16)));audio=false;
        }
        for(int i=0;i<127;++i) {const float v=block<90?.08f*std::sin(float(block*127+i)*.07f):0;buffer.setSample(0,i,v);buffer.setSample(1,i,v*.7f);}
        audio=true;p.processBlock(buffer,midi);audio=false;
        for(int c=0;c<2;++c)for(int i=0;i<127;++i)require(std::isfinite(buffer.getSample(c,i)),"nonfinite hidden-control audio");
    }
    require(!allocations && !releases,"hidden controls allocated/released in processing or host setter");
    std::cout<<"Hidden 224 controls: all six programs,1..16 Rate,ordering,toggles,canonical host values,immediate session/preset,v5 migration,full/compact UI,moving audio,no callback/setter new/delete: pass\n";
}
