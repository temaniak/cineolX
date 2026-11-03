#include "Editor.hpp"
#include "../../core/control_scales.hpp"
CleanABEditor::CleanABEditor(CleanABProcessor& p):AudioProcessorEditor(p),p_(p){
    for(unsigned i=0;i<7;++i)algorithms_.addItem(cineol::clean_ab::names[i],int(i+1));addAndMakeVisible(algorithms_);
    algorithmAttachment_=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"algorithm",algorithms_);
    for(auto* x:{&a_,&b_,&previous_,&next_,&reset_})addAndMakeVisible(x);
    for(auto* button:{&a_,&b_}){button->setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff267c6b));button->setColour(juce::TextButton::buttonColourId,juce::Colour(0xff29323a));button->setColour(juce::TextButton::textColourOnId,juce::Colours::white);}
    a_.onClick=[this]{set(1,0);refresh();};b_.onClick=[this]{set(1,1);refresh();};
    previous_.onClick=[this]{if(page_)--page_;refresh();};next_.onClick=[this]{++page_;refresh();};
    reset_.onClick=[this]{for(unsigned i=7;i<14;++i){auto*q=p_.state.getParameter(CleanABProcessor::id(i));q->beginChangeGesture();q->setValueNotifyingHost(q->getDefaultValue());q->endChangeGesture();}for(unsigned i=16;i<64;++i)set(i,-1);set(5,0);set(6,0);set(14,0);refresh();};
    std::array<juce::ToggleButton*,3> toggles{&mod_,&opt_,&dynamic_};const unsigned toggleIds[]={5,6,14};
    for(unsigned i=0;i<3;++i){addAndMakeVisible(*toggles[i]);toggleAttachments_[i]=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,CleanABProcessor::id(toggleIds[i]),*toggles[i]);}
    for(unsigned i=0;i<7;++i){auto&s=faders_[i];s.setSliderStyle(juce::Slider::LinearVertical);s.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);s.setColour(juce::Slider::trackColourId,juce::Colour(0xff79c9ba));addAndMakeVisible(s);addAndMakeVisible(labels_[i]);addAndMakeVisible(values_[i]);labels_[i].setJustificationType(juce::Justification::centred);values_[i].setJustificationType(juce::Justification::centred);
        s.onDragStart=[this,i]{gestures_[i]=p_.state.getParameter(CleanABProcessor::id(binding_[i]));gestures_[i]->beginChangeGesture();};
        s.onDragEnd=[this,i]{if(gestures_[i])gestures_[i]->endChangeGesture();gestures_[i]=nullptr;};
        s.onValueChange=[this,i]{if(updating_)return;auto*q=gestures_[i]?gestures_[i]:p_.state.getParameter(CleanABProcessor::id(binding_[i]));if(!gestures_[i])q->beginChangeGesture();q->setValueNotifyingHost(q->convertTo0to1(float(faders_[i].getValue())));if(!gestures_[i])q->endChangeGesture();refresh();};
    }
    const char* names[]={"Input (dB)","Dry / Wet","B Level Trim (dB)","Output (dB)"};const unsigned ids[]={2,3,4,15};
    for(unsigned i=0;i<4;++i){auto&s=globals_[i];s.setSliderStyle(juce::Slider::LinearHorizontal);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,110,24);addAndMakeVisible(s);globalLabels_[i].setText(names[i],juce::dontSendNotification);addAndMakeVisible(globalLabels_[i]);globalAttachments_[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,CleanABProcessor::id(ids[i]),s);}
    for(auto*x:{&pageLabel_,&status_,&meters_})addAndMakeVisible(x);
    status_.setColour(juce::Label::textColourId,juce::Colour(0xffb5c0cb));
    setSize(980,650);startTimerHz(10);refresh();
}
void CleanABEditor::set(unsigned i,float value){auto*q=p_.state.getParameter(CleanABProcessor::id(i));q->beginChangeGesture();q->setValueNotifyingHost(q->convertTo0to1(value));q->endChangeGesture();}
void CleanABEditor::timerCallback(){refresh();}
void CleanABEditor::refresh(){
    juce::ScopedValueSetter<bool> update(updating_,true);const unsigned program=unsigned(p_.state.getRawParameterValue("algorithm")->load());
    if(program!=lastProgram_){page_=0;lastProgram_=program;}
    const bool clean=p_.state.getRawParameterValue("compare")->load()>=.5f;a_.setToggleState(!clean,juce::dontSendNotification);b_.setToggleState(clean,juce::dontSendNotification);
    a_.setButtonText(clean?"A  /  ORIGINAL":"A  /  ORIGINAL  -  ACTIVE");b_.setButtonText(clean?"B  /  CLEAN  -  ACTIVE":"B  /  CLEAN");
    const auto*data=p_.xlData(program);unsigned pages=data?data->page_count:1;page_=std::min(page_,pages?pages-1:0);previous_.setEnabled(page_>0);next_.setEnabled(page_+1<pages);dynamic_.setEnabled(program>=3);
    pageLabel_.setText(program<3?"224 controls":"XL control page "+juce::String(page_+1)+" / "+juce::String(pages),juce::dontSendNotification);
    std::array<uint8_t,48>raw{};if(data){raw=data->controls.factory;for(unsigned i=0;i<48;++i){const float v=p_.state.getRawParameterValue(CleanABProcessor::id(i+16))->load();if(v>=0)raw[i]=uint8_t(v);}data->resolve_controls(raw);}
    const char*names[]={"Bass Decay","Mid Decay","Crossover","Treble Decay","Depth","Pre-delay","Diffusion"};const int lo[]={1,0,0,0,0,0,1},hi[]={31,31,31,31,71,128,63};
    for(unsigned i=0;i<7;++i){bool visible=program<3 || (data && i<6 && data->pages[page_].names[i][0]);for(auto*x:std::array<juce::Component*,3>{&faders_[i],&labels_[i],&values_[i]})x->setVisible(visible);if(!visible)continue;
        juce::String label,value;double position;unsigned minimum,maximum;
        if(program<3){binding_[i]=i+7;minimum=unsigned(lo[i]);maximum=unsigned(hi[i]);position=p_.state.getRawParameterValue(CleanABProcessor::id(binding_[i]))->load();label=names[i];const int v=int(position);
            if(i<2)value=juce::String(native_hall::scales::times[v],1)+" s";else if(i<4)value=juce::String(native_hall::scales::frequencies[v])+" Hz";else if(i==5)value=juce::String(v+native_hall::predelay_minima[cineol::clean_ab::indices[program]])+" ms";else value=juce::String(v);
        }else {const auto&pg=data->pages[page_];const unsigned cell=pg.cells[i];binding_[i]=16+cell;minimum=0;maximum=data->control_maximum(page_,i,raw);position=std::min(unsigned(raw[cell]),maximum);label=juce::String(pg.names[i].data());value=juce::String(pg.values[i][unsigned(position)].data());}
        labels_[i].setText(label,juce::dontSendNotification);values_[i].setText(value,juce::dontSendNotification);if(!gestures_[i]){faders_[i].setRange(minimum,std::max(minimum+1,maximum),1);faders_[i].setValue(position,juce::dontSendNotification);}faders_[i].setEnabled(maximum>minimum);
    }
    meters_.setText("Wet RMS   A: "+juce::String(p_.meterA.load(),1)+" dBFS     B: "+juce::String(p_.meterB.load(),1)+" dBFS",juce::dontSendNotification);
    status_.setText(p_.ready(program)?"Both tails run continuously. Match loudness with B Level Trim; switch A/B during a tail.":"Bank missing: import this model in Cineol-X 224, then reopen Clean AB.",juce::dontSendNotification);
}
void CleanABEditor::paint(juce::Graphics&g){g.fillAll(juce::Colour(0xff1b2026));g.setColour(juce::Colours::white);g.setFont(juce::FontOptions(26));g.drawText("Cineol Clean AB",25,12,600,35,juce::Justification::centredLeft);g.setFont(juce::FontOptions(14));g.setColour(juce::Colour(0xffb5c0cb));g.drawText("Original clocks and filters / extended fixed-point precision",25,154,900,25,juce::Justification::centredLeft);}
void CleanABEditor::resized(){
    algorithms_.setBounds(25,58,700,35);reset_.setBounds(745,58,210,35);a_.setBounds(25,106,300,38);b_.setBounds(345,106,300,38);meters_.setBounds(660,102,300,48);
    for(unsigned i=0;i<7;++i){int x=25+int(i)*133;labels_[i].setBounds(x,192,128,28);faders_[i].setBounds(x+30,224,68,155);values_[i].setBounds(x,385,128,28);}
    previous_.setBounds(25,424,150,27);pageLabel_.setBounds(190,424,380,27);next_.setBounds(580,424,150,27);
    mod_.setBounds(25,464,220,30);opt_.setBounds(270,464,220,30);dynamic_.setBounds(520,464,220,30);
    for(unsigned i=0;i<4;++i){int x=25+int(i)*235;globalLabels_[i].setBounds(x,510,220,25);globals_[i].setBounds(x,539,215,55);}status_.setBounds(20,610,940,27);
}
