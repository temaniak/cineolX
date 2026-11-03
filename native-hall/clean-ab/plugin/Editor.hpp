#pragma once
#include "Processor.hpp"
class CleanABEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit CleanABEditor(CleanABProcessor&);
    void paint(juce::Graphics&)override;
    void resized()override;
private:
    void timerCallback()override;
    void set(unsigned,float);
    void refresh();
    CleanABProcessor& p_;
    juce::ComboBox algorithms_;
    juce::TextButton a_{"A  /  ORIGINAL"},b_{"B  /  CLEAN"},previous_{"Previous page"},next_{"Next page"},reset_{"Reset controls"};
    juce::ToggleButton mod_{"Mode Enhancement"},opt_{"Decay Optimization"},dynamic_{"Dynamic Decay (XL)"};
    std::array<juce::Slider,7> faders_;
    std::array<juce::Label,7> labels_;
    std::array<juce::Label,7> values_;
    std::array<unsigned,7> binding_{};
    std::array<juce::RangedAudioParameter*,7> gestures_{};
    std::array<juce::Slider,4> globals_;
    std::array<juce::Label,4> globalLabels_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,4> globalAttachments_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algorithmAttachment_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>,3> toggleAttachments_;
    juce::Label pageLabel_,status_,meters_;
    unsigned page_=0,lastProgram_=7;bool updating_=false;
};
