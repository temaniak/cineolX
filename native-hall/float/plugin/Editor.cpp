#include "Editor.hpp"

FloatHallABEditor::FloatHallABEditor(FloatHallABProcessor& p):AudioProcessorEditor(p),processor_(p) {
    const char* ids[]={"bass","mid","crossover","treble","depth","predelay","diffusion","input_db","mix","float_trim"};
    const char* names[]={"Bass Decay","Mid Decay","Crossover","Treble Decay","Depth","Pre-delay","Diffusion","Input Gain","Dry / Wet","B Level Trim"};
    for(unsigned i=0;i<sliders_.size();++i) {
        auto& s=sliders_[i];s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,100,25);
        s.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xff63d6c6));
        addAndMakeVisible(s);labels_[i].setText(names[i],juce::dontSendNotification);
        labels_[i].setJustificationType(juce::Justification::centred);addAndMakeVisible(labels_[i]);
        slider_attachments_[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,ids[i],s);
    }
    sliders_[5].setTextValueSuffix(" ms");sliders_[7].setTextValueSuffix(" dB");sliders_[9].setTextValueSuffix(" dB");
    for(unsigned i=0;i<4;++i) {left_.addItem("Left: "+juce::String::charToString(juce::juce_wchar('A'+i)),int(i+1));right_.addItem("Right: "+juce::String::charToString(juce::juce_wchar('A'+i)),int(i+1));}
    addAndMakeVisible(a_);addAndMakeVisible(b_);addAndMakeVisible(left_);addAndMakeVisible(right_);
    const auto choose=[this](float value) {
        auto* parameter=processor_.state.getParameter("compare");
        parameter->beginChangeGesture();parameter->setValueNotifyingHost(value);parameter->endChangeGesture();timerCallback();
    };
    a_.onClick=[choose]{choose(0);};b_.onClick=[choose]{choose(1);};
    for(auto* button:{&a_,&b_}) {
        button->setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff276e68));
        button->setColour(juce::TextButton::textColourOnId,juce::Colours::white);
    }
    combo_attachments_[0]=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"output_l",left_);
    combo_attachments_[1]=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"output_r",right_);
    addAndMakeVisible(modulation_);addAndMakeVisible(optimization_);
    modulation_attachment_=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"mode_enh",modulation_);
    optimization_attachment_=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"decay_opt",optimization_);
    addAndMakeVisible(status_);status_.setColour(juce::Label::textColourId,juce::Colour(0xffa9b9c9));
    setSize(850,540);startTimerHz(20);timerCallback();
}
void FloatHallABEditor::timerCallback() {
    const bool floating=processor_.state.getParameter("compare")->getValue()>=.5f;
    a_.setToggleState(!floating,juce::dontSendNotification);b_.setToggleState(floating,juce::dontSendNotification);
    status_.setText(processor_.ready()?"Both tails stay active. Use B Level Trim to compare at equal loudness.":
        "Import 224 v4.4 ROMs in Cineol-X 224, then reopen this plugin.",juce::dontSendNotification);
}
void FloatHallABEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff101923));g.setColour(juce::Colour(0xffe8f0f7));
    g.setFont(juce::FontOptions(27));g.drawText("Cineol Float / Hall A-B",30,20,790,36,juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(15));g.setColour(juce::Colour(0xffa9b9c9));
    g.drawText("Large Concert Hall B  |  224 v4.4",30,62,790,24,juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(14));
    g.drawText("A: current engine, ADC / DAC and circuit filters.   B: float network, direct input / output.",30,144,790,26,juce::Justification::centredLeft);
}
void FloatHallABEditor::resized() {
    a_.setBounds(30,100,215,36);b_.setBounds(260,100,215,36);left_.setBounds(545,100,125,36);right_.setBounds(690,100,130,36);
    for(unsigned i=0;i<sliders_.size();++i) {
        const int x=30+int(i%5)*160,y=185+int(i/5)*145;
        labels_[i].setBounds(x,y,150,24);sliders_[i].setBounds(x,y+24,150,110);
    }
    modulation_.setBounds(30,475,205,26);optimization_.setBounds(250,475,205,26);
    status_.setBounds(25,507,800,23);
}
