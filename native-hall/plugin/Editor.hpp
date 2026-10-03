#pragma once
#include "Processor.hpp"

class CineolEditor final : public juce::AudioProcessorEditor {
public:
    explicit CineolEditor(NativeHallProcessor&);
    ~CineolEditor() override;
    void resized() override;
private:
    struct Panel;
    std::unique_ptr<Panel> panel_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CineolEditor)
};
