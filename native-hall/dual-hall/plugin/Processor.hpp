#pragma once
#include "../dual_hall.hpp"
#include "Bank.hpp"
#include <juce-plugin/plugin/rate_bridge.hpp>

class DualHallProcessor final : public juce::AudioProcessor {
public:
    DualHallProcessor();
    void prepareToPlay(double,int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override {return true;}
    const juce::String getName() const override {return "Cineol Dual Hall B";}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    bool isMidiEffect() const override {return false;}
    double getTailLengthSeconds() const override {return 180;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return "Dual Large Hall B";}
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    bool ready() const noexcept {return prepared_.load(std::memory_order_acquire);}
    void request_clear_tails() noexcept {clear_requested_.store(true,std::memory_order_release);}
    uint64_t underruns() const noexcept {return bridge_.underruns();}
    juce::AudioProcessorValueTreeState state;
    static constexpr const char* ids[]={"bass","mid","crossover","treble","depth","predelay","diffusion",
        "mod_rate","mod_depth","opt_response","mode_enh","decay_opt","crossfeedback","input_db","mix","output_db"};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    static native_hall::Controls hall_controls(const std::array<float,std::size(ids)>&) noexcept;
    void controls(const std::array<float,std::size(ids)>&) noexcept;
    juce::SharedResourcePointer<DualHallBank> bank_;
    cineol::dual_hall::Tanks tanks_;
    lexplug::RateBridge bridge_;
    std::array<std::atomic<float>*,std::size(ids)> values_{};
    std::array<float,std::size(ids)> previous_{};
    std::array<std::array<float,2>,cineol::dual_hall::Tanks::latency> dry_{};
    unsigned dry_position_=0;
    float gain_=1,gain_target_=1,mix_=1,output_=1,output_target_=1;
    std::atomic<bool> prepared_{false};
    std::atomic<bool> clear_requested_{false};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DualHallProcessor)
};
