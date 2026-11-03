// Included after the shared plugin-check fixtures; never linked into the plugin.
static void check_clean_plugin(const char* original_path,const char* xl_path,const juce::File& folder,bool audio_checks=true) {
    auto load=[](auto&bank,const char*path){juce::MemoryBlock bytes;require(juce::File(juce::String::fromUTF8(path)).loadFileAsData(bytes),"Clean fixture missing");require(read_bank(bytes.getData(),bytes.getSize(),*bank),"Clean fixture invalid");};
    auto original=std::make_unique<native_hall::ProgramBank>();auto xl=std::make_unique<cineol::xl::Bank>();load(original,original_path);load(xl,xl_path);
    auto clean=std::make_unique<clean_native_hall::ProgramBank>();auto cleanxl=std::make_unique<clean_cineol::xl::Bank>();std::memcpy(clean.get(),original.get(),sizeof(*clean));std::memcpy(cleanxl.get(),xl.get(),sizeof(*cleanxl));
    if(audio_checks) {
    for(unsigned program=0;program<28;++program) {
        auto p=std::make_unique<NativeHallProcessor>();p->setCurrentProgram(int(program));
        set(*p,"clean_mode",1);set(*p,"output_l",0);set(*p,"output_r",1);set(*p,"output_rear_l",2);set(*p,"output_rear_r",3);
        auto layout=p->getBusesLayout();layout.outputBuses.set(1,juce::AudioChannelSet::stereo());require(p->setBusesLayout(layout),"Clean quad layout");p->prepareToPlay(48000,128);
        auto engine=std::make_unique<clean_native_hall::DesktopEngine48>();engine->prepare_audio();auto runtime=std::make_unique<clean_cineol::xl::Runtime>();
        auto value=[&](const char*id){return p->state.getRawParameterValue(id)->load();};
        if(program<6) {
            engine->activate(*clean,program);clean_native_hall::Parameters v;v.program=program;
            v.hall.bass=int(value("bass"));v.hall.mid=int(value("mid"));v.hall.crossover=int(value("crossover"));v.hall.treble=int(value("treble"));v.hall.depth=int(value("depth"));v.hall.predelay_ms=int(value("predelay"))-24+native_hall::predelay_minima[program];v.hall.diffusion=int(value("diffusion"));v.hall.mode_enhancement=value("mode_enh")>=.5f;v.hall.decay_optimization=value("decay_opt")>=.5f;
            const auto hidden=p->originalHiddenValues();v.hall.modulation_period=uint8_t(std::max(1,hidden[0]-1));v.hall.modulation_step=uint8_t(hidden[1]*4);v.hall.optimization_response=uint8_t(hidden[2]);engine->set_parameters(v);
        } else {
            runtime->select(*cleanxl,program-6);auto raw=xl->programs[program-6].controls.factory;for(unsigned i=0;i<48;++i)raw[i]=p->xlControl(i);xl->programs[program-6].resolve_controls(raw);runtime->controls(raw,value("mode_enh")>=.5f,0,1,1,0,1,value("decay_opt")>=.5f);
        }
        cineol::SpaceClock clock;clock.prepare();std::array<std::array<float,4>,13> pad{};unsigned pos=0;
        juce::AudioBuffer<float> block(4,128);juce::MidiBuffer midi;
        for(unsigned start=0;start<96000;start+=128) {
            if(start==48000)set(*p,"clean_mode",0);
            if(start==60032)set(*p,"clean_mode",1);
            std::array<std::array<float,4>,128> expected{};
            for(unsigned i=0;i<128;++i){const unsigned at=start+i;const float l=at<24000?.08f*std::sin(float(at)*.117f):0,r=at<24000?.06f*std::cos(float(at)*.093f):0;block.setSample(0,int(i),l);block.setSample(1,int(i),r);
                expected[i]=clock.process(l,r,1,[&](float a,float b){std::array<float,4> out;float x,y;if(program<6)engine->process(a,b,x,y,true,&out);else{runtime->process(a,b,x,y,true,&out);const auto delayed=pad[pos];pad[pos]=out;pos=(pos+1)%pad.size();out=delayed;}return out;});
            }
            audio=true;p->processBlock(block,midi);audio=false;
            for(unsigned i=0;i<128;++i)for(unsigned c=0;c<4;++c){const auto actual=block.getSample(int(c),int(i));require(std::isfinite(actual),"Clean plugin nonfinite");if(start<48000 || start>=62080)require(actual==expected[i][c],"Clean plugin differs from independent precision engine or lost tail on toggle");}
        }
        require(p->getLatencySamples()==int(cineol::SpaceClock::latency),"Clean changed latency");
    }
    std::cout<<"Clean: all 28 programs, four outputs exactly match precision engine, warm-tail toggles, fixed latency passed\n";
    for(int rate:{44100,48000,96000})for(int program=0;program<28;++program){const auto a=run(rate,128,false,program,false,false,1,-1,true);require(a==run(rate,511,true,program,false,false,1,-1,true),"Clean block/offline variance");require(a==run(rate,128,false,program,false,true,1,-1,true),"Clean low latency changed wet DSP");}
    std::cout<<"Clean: all 28 programs x 44.1/48/96 kHz, block sizes/offline/low latency passed\n";
    for(int rate:{44100,48000,96000})require(spillover_catalog(rate,128,false,true)==spillover_catalog(rate,511,false,true),"Clean Spillover catalog depends on block size");
    for(bool low:{false,true})require(run(48000,128,false,8,false,low,0,-1,true)==run(48000,128,false,8,false,low,0,-1,false),"Clean changed dry path");
    std::cout<<"Clean: all-program Spillover at three rates and compensated/direct dry identity passed\n";
    for(int program:{2,8})require(run(48000,128,false,program,false,false,1,0,true)!=run(48000,128,false,program,false,false,1,1,true),"Dirt became inaudible in Clean");
    std::cout<<"Clean: Dirt retains its independent audible-band effect in both engines\n";
    }
    PresetTestFiles files;NativeHallProcessor p;require(p.state.getRawParameterValue("clean_mode")->load()==0&&p.state.getRawParameterValue("dark_stability")->load()==1,"new control defaults");
    // The logical value array excludes the pre-existing low-latency/spillover
    // preferences; they still occupy their original host parameter positions.
    for(unsigned i=0;i<NativeHallProcessor::parameter_count;++i)require(p.state.getParameter(NativeHallProcessor::ids[i])->getParameterIndex()==int(i+(i>=65?3:i>=15?1:0)),"parameter order changed");
    p.setCurrentProgram(8);set(p,"clean_mode",1);set(p,"dark_stability",.375f);juce::MemoryBlock session;p.getStateInformation(session);NativeHallProcessor restored;restored.setStateInformation(session.getData(),int(session.getSize()));require(restored.state.getRawParameterValue("clean_mode")->load()==1&&std::abs(restored.state.getRawParameterValue("dark_stability")->load()-.375f)<1e-6f,"Clean session roundtrip");
    const auto preset=files.folder.getChildFile("Clean.cineol224");require(p.savePreset(preset).wasOk(),"Clean preset save");set(p,"clean_mode",0);set(p,"dark_stability",1);require(p.loadPreset(preset).wasOk()&&p.state.getRawParameterValue("clean_mode")->load()==1&&std::abs(p.state.getRawParameterValue("dark_stability")->load()-.375f)<1e-6f,"Clean preset roundtrip");
    auto xml=juce::XmlDocument::parse(preset);auto tree=juce::ValueTree::fromXml(*xml);tree.setProperty("version",6,nullptr);for(const char* id:{"clean_mode","dark_stability"})tree.removeChild(tree.getChildWithProperty("id",id),nullptr);require(tree.createXml()->writeTo(preset),"v6 fixture save");set(p,"dark_stability",0);require(p.loadPreset(preset).wasOk()&&p.state.getRawParameterValue("clean_mode")->load()==0&&p.state.getRawParameterValue("dark_stability")->load()==1,"v6 preset retained new flags");
    auto legacy=p.state.copyState();for(const char*id:{"clean_mode","dark_stability"})legacy.removeChild(legacy.getChildWithProperty("id",id),nullptr);juce::MemoryBlock old;juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),old);set(p,"clean_mode",1);set(p,"dark_stability",0);p.setStateInformation(old.getData(),int(old.getSize()));require(p.state.getRawParameterValue("clean_mode")->load()==0&&p.state.getRawParameterValue("dark_stability")->load()==1,"old session retained new flags");
    require(folder.createDirectory().wasOk(),"Clean screenshot folder");p.setEditorAppearance({1,.8f,false});p.setCurrentProgram(8);p.setEditorPage(4);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());auto* panel=find(*editor,"cineol_panel");auto* slider=dynamic_cast<juce::Slider*>(find(*editor,"xl_slot_3"));auto* toggle=dynamic_cast<juce::ToggleButton*>(find(*editor,"clean_mode"));auto* compact=dynamic_cast<juce::ToggleButton*>(find(*editor,"compact_mode"));auto* settings=dynamic_cast<juce::Button*>(find(*editor,"settings"));require(panel&&slider&&toggle&&compact&&settings,"new UI controls missing");require(slider->isVisible()&&slider->getProperties()["parameter_id"]=="dark_stability"&&slider->getSliderStyle()==juce::Slider::LinearVertical,"Dark Hall variation is not a normal page fader");
    auto snap=[&](const juce::String&name){const auto image=editor->createComponentSnapshot(editor->getLocalBounds());auto stream=folder.getChildFile(name+".png").createOutputStream();juce::PNGImageFormat png;require(stream&&stream->setPosition(0)&&stream->truncate().wasOk()&&png.writeImageToStream(image,*stream),"Clean screenshot");};
    for(bool small:{false,true})for(bool clean_skin:{false,true}){
        compact->setToggleState(small,juce::dontSendNotification);compact->onClick();toggle->setToggleState(clean_skin,juce::sendNotificationSync);juce::Timer::callPendingTimersSynchronously();require(bool(panel->getProperties()["clean_skin"])==clean_skin,"Clean skin did not follow audio");require(small||panel->getLocalBounds().contains(slider->getBounds()),"stability control clipped");require(slider->isVisible()==!small&&find(*editor,"screen_value_2")->getProperties()["parameter_id"]=="dark_stability","variation missing from full/compact parameter page");
        const juce::String label=juce::String(small?"compact-":"full-")+(clean_skin?"clean":"original");snap(label);settings->setToggleState(true,juce::sendNotificationSync);auto* settings_panel=find(*editor,"settings_panel");require(settings_panel->getLocalBounds().contains(toggle->getBounds()),"Clean checkbox clipped");snap(label+"-settings");settings->setToggleState(false,juce::sendNotificationSync);
    }
    auto* number=dynamic_cast<juce::Slider*>(find(*editor,"screen_value_2"));number->setValue(.625,juce::sendNotificationSync);require(std::abs(p.state.getRawParameterValue("dark_stability")->load()-.625f)<1e-6f,"stability screen attachment");
    slider->setValue(0,juce::sendNotificationSync);require(find(*editor,"display")->getProperties()["slot_value_2"]=="Stable","zero does not show Stable");slider->setValue(1,juce::sendNotificationSync);require(find(*editor,"display")->getProperties()["slot_value_2"]=="Original","full does not show Original");
    editor.reset();editor.reset(p.createEditor());require(bool(find(*editor,"cineol_panel")->getProperties()["clean_skin"]),"reopen lost Clean skin");p.setCurrentProgram(6);juce::Timer::callPendingTimersSynchronously();editor.reset();editor.reset(p.createEditor());require(find(*editor,"xl_slot_3")->getProperties()["parameter_id"]!=juce::String("dark_stability")&&p.state.getRawParameterValue("clean_mode")->load()==1,"Clean not global / variation leaked to other algorithm");
    require(!allocations&&!releases,"Clean audio allocated/released memory");std::cout<<"Clean UI, v7 presets/sessions, v6 migration, unchanged parameter order; callback new/delete=0 passed\n";
}
static void benchmark_clean_plugin(){
    std::cout<<"program,clean,offline_cpu_percent,callback_p99_ms\n";
    for(int program=0;program<28;++program)for(bool clean:{false,true}){
        NativeHallProcessor p;p.setCurrentProgram(program);set(p,"clean_mode",clean?1.f:0.f);p.prepareToPlay(48000,128);juce::AudioBuffer<float>b(2,128);juce::MidiBuffer midi;std::array<double,1125> timings{};double total=0;
        for(unsigned block=0;block<timings.size()+100;++block){for(unsigned i=0;i<128;++i){b.setSample(0,int(i),.08f*std::sin(float(block*128+i)*.117f));b.setSample(1,int(i),.06f*std::cos(float(block*128+i)*.093f));}const auto start=std::chrono::steady_clock::now();audio=true;p.processBlock(b,midi);audio=false;const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();if(block>=100){timings[block-100]=seconds;total+=seconds;}}
        std::sort(timings.begin(),timings.end());std::cout<<program<<','<<clean<<','<<total/3*100<<','<<timings[timings.size()*99/100]*1000<<'\n';
    }
    require(!allocations&&!releases,"Clean benchmark allocations");
}
