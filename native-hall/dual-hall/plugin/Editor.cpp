#include "Editor.hpp"

DualHallEditor::DualHallEditor(DualHallProcessor& p):AudioProcessorEditor(p),processor_(p) {
    const char* ids[]={"bass","mid","crossover","treble","depth","predelay","diffusion",
        "crossfeedback","input_db","mix","output_db","mod_rate","mod_depth","opt_response"};
    const char* names[]={"Bass Decay","Mid Decay","Crossover","Treble Decay","Depth","Pre-delay","Diffusion",
        "Crossfeedback","Input Gain","Dry / Wet","Output Gain","Mod Rate","Mod Depth","Opt Response"};
    const char* tips[]={"Shared low-frequency decay for both Hall B tanks.","Shared mid-frequency decay for both tanks.",
        "Shared decay crossover frequency.","Shared high-frequency decay control.","Shared original Hall B output-tap depth.",
        "Shared original pre-delay. Live changes retain the original retap behavior.","Shared original diffusion control.",
        "Reciprocal wet return to the opposite tank. 0% gives independent tanks; 50% is -6.02 dB per link. Changes ramp over up to 20 ms.",
        "Gain of the new input signal; does not amplify the reciprocal return.","Final dry/wet balance; leaves both tanks and their coupling running.",
        "Final output gain; does not change the coupling.","Shared original modulation period setting.",
        "Shared original modulation step setting.","Shared original Decay Optimization response setting."};
    for(unsigned i=0;i<sliders_.size();++i) {
        auto& s=sliders_[i];s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,132,25);
        s.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(i==7?0xffe4af68:0xff79cfc0));
        s.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        s.setTooltip(tips[i]);addAndMakeVisible(s);
        labels_[i].setText(names[i],juce::dontSendNotification);
        labels_[i].setJustificationType(juce::Justification::centred);
        labels_[i].setColour(juce::Label::textColourId,juce::Colour(i==7?0xffffd49a:0xffdce8ec));
        addAndMakeVisible(labels_[i]);
        attachments_[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,ids[i],s);
    }
    sliders_[5].setTextValueSuffix(" ms");sliders_[8].setTextValueSuffix(" dB");sliders_[10].setTextValueSuffix(" dB");
    addAndMakeVisible(modulation_);addAndMakeVisible(optimization_);
    modulation_attachment_=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"mode_enh",modulation_);
    optimization_attachment_=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"decay_opt",optimization_);
    addAndMakeVisible(clear_);clear_.onClick=[this]{processor_.request_clear_tails();};
    clear_.setTooltip("Clear both tank histories and their reciprocal return. Parameters stay as set.");
    addAndMakeVisible(status_);status_.setColour(juce::Label::textColourId,juce::Colour(0xffa2b7bf));
    setSize(1120,650);startTimerHz(10);timerCallback();
}
void DualHallEditor::timerCallback() {
    const float feedback=processor_.state.getRawParameterValue("crossfeedback")->load(std::memory_order_relaxed);
    if(feedback!=feedback_){feedback_=feedback;repaint(430,90,260,85);}
    status_.setText(processor_.ready()?"Long decay + strong coupling can self-oscillate. Clear tails resets both tanks.":
        "Import original 224 v4.4 ROM1-ROM5 in Cineol-X 224, then reopen this plugin.",juce::dontSendNotification);
}
void DualHallEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff101b22));
    g.setColour(juce::Colour(0xffedf4f3));g.setFont(juce::FontOptions(29));
    g.drawText("Cineol / Dual Hall B",30,20,750,38,juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff9db5bc));g.setFont(juce::FontOptions(14));
    g.drawText("Two complete Hall B tanks. One channel each. Reciprocal wet coupling.",32,62,1000,24,juce::Justification::centredLeft);
    for(int side=0;side<2;++side) {
        const float x=side?700.f:30.f;
        g.setColour(juce::Colour(0xff1b3039));g.fillRoundedRectangle(x,100,390,78,9);
        g.setColour(juce::Colour(0xff79cfc0));g.setFont(juce::FontOptions(19));
        g.drawText(side?"RIGHT / Hall B":"LEFT / Hall B",int(x)+18,110,350,26,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffa2b7bf));g.setFont(juce::FontOptions(13));
        g.drawText(side?"R excitation  /  original output C":"L excitation  /  original output A",int(x)+18,142,350,22,juce::Justification::centredLeft);
    }
    g.setColour(juce::Colour(0xffe4af68));g.drawLine(440,119,680,119,1.5f);g.drawLine(440,159,680,159,1.5f);
    g.drawLine(680,119,672,114,1.5f);g.drawLine(680,119,672,124,1.5f);
    g.drawLine(440,159,448,154,1.5f);g.drawLine(440,159,448,164,1.5f);
    g.setFont(juce::FontOptions(14));g.drawText(juce::String(feedback_*100,1)+"% crossfeedback",440,123,240,30,juce::Justification::centred);
    g.setColour(juce::Colour(0xff29414a));g.drawLine(30,555,1090,555);
}
void DualHallEditor::resized() {
    for(unsigned i=0;i<sliders_.size();++i) {
        const int x=25+int(i%7)*154,y=200+int(i/7)*176;
        labels_[i].setBounds(x,y,146,25);sliders_[i].setBounds(x,y+25,146,134);
    }
    modulation_.setBounds(32,572,230,27);optimization_.setBounds(280,572,240,27);
    clear_.setBounds(890,572,195,28);
    status_.setBounds(27,611,1065,25);
}
