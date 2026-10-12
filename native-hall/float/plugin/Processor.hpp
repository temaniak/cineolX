#pragma once
#include "../hall.hpp"
#include "../../desktop/engine22448.hpp"
#include "../../plugin/RomBank.hpp"
#include <juce-plugin/plugin/rate_bridge.hpp>

class FloatHallABProcessor final : public juce::AudioProcessor {
public:
    FloatHallABProcessor();
    void prepareToPlay(double,int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override {return true;}
    const juce::String getName() const override {return "Cineol Float Hall AB";}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    bool isMidiEffect() const override {return false;}
    double getTailLengthSeconds() const override {return 70;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return "Large Concert Hall B";}
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    bool ready() const noexcept {return prepared_.load(std::memory_order_acquire);}
    juce::AudioProcessorValueTreeState state;
    static constexpr const char* ids[]={"bass","mid","crossover","treble","depth","predelay","diffusion",
        "mode_enh","decay_opt","input_db","mix","output_l","output_r","compare","float_trim"};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void controls(const std::array<float,std::size(ids)>&) noexcept;
    juce::SharedResourcePointer<CineolRomBank> bank_;
    native_hall::DesktopEngine48 original_;
    cineol::floating::Hall48 floating_;
    lexplug::RateBridge bridge_;
    std::array<std::atomic<float>*,std::size(ids)> values_{};
    std::array<float,std::size(ids)> previous_{};
    std::array<std::array<float,4>,70> float_delay_{};
    std::array<std::array<float,2>,70> dry_delay_{};
    unsigned delay_position_=0;
    float blend_=0,gain_=1,mix_=1,trim_=1,gain_target_=1,trim_target_=1;
    std::atomic<bool> prepared_{false};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FloatHallABProcessor)
};
