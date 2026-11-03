#pragma once
#include "Processor.hpp"

class DualHallEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit DualHallEditor(DualHallProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    DualHallProcessor& processor_;
    std::array<juce::Slider,14> sliders_;
    std::array<juce::Label,14> labels_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,14> attachments_;
    juce::ToggleButton modulation_{"Mode Enhancement"},optimization_{"Decay Optimization"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> modulation_attachment_,optimization_attachment_;
    juce::Label status_;
    juce::TextButton clear_{"Clear tails"};
    float feedback_=-1;
};
