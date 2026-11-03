#pragma once
#include "Processor.hpp"

class CineolEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit CineolEditor(NativeHallProcessor&);
    ~CineolEditor() override;
    void resized() override;
private:
    void timerCallback() override;
    void setCompact(bool);
    NativeHallProcessor& processor_;
    bool initialising_=true;
    struct Panel;
    std::unique_ptr<Panel> panel_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CineolEditor)
};
