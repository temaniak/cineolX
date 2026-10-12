#pragma once
#include "Processor.hpp"

class FloatHallABEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit FloatHallABEditor(FloatHallABProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    FloatHallABProcessor& processor_;
    std::array<juce::Slider,10> sliders_;
    std::array<juce::Label,10> labels_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,10> slider_attachments_;
    juce::TextButton a_{"A  /  Original 224"},b_{"B  /  Float 48 kHz"};
    juce::ComboBox left_,right_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>,2> combo_attachments_;
    juce::ToggleButton modulation_{"Mode Enhancement"},optimization_{"Decay Optimization"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> modulation_attachment_,optimization_attachment_;
    juce::Label status_;
};
