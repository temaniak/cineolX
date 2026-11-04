#pragma once
#include "Processor.hpp"
#include <juce_dsp/juce_dsp.h>

class PitchLabEditor final:public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit PitchLabEditor(PitchLabProcessor&);
    ~PitchLabEditor()override;
    void paint(juce::Graphics&)override;
    void resized()override;
private:
    struct Row;
    void timerCallback()override;
    void snapshot(bool a,bool store);
    PitchLabProcessor& processor_;
    std::vector<std::unique_ptr<Row>> rows_;
    juce::ComboBox preset_;
    juce::TextButton panic_{"PANIC / CLEAR"},storeA_{"Store A"},recallA_{"Recall A"},storeB_{"Store B"},recallB_{"Recall B"};
    juce::Label status_;
    juce::dsp::FFT fft_{11};
    std::array<std::array<float,4096>,2> fft_work_{};
    std::array<float,1024> spectrum_{};
    unsigned spectrum_frames_=0;
    juce::TooltipWindow tooltips_{this,600};
};
