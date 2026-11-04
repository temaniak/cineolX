#pragma once
#include "../core.hpp"
#include "../../dual-hall/plugin/Bank.hpp"
#include <juce-plugin/plugin/rate_bridge.hpp>

class PitchLabProcessor final : public juce::AudioProcessor {
public:
    PitchLabProcessor();
    void prepareToPlay(double,int)override;
    void releaseResources()override{}
    bool isBusesLayoutSupported(const BusesLayout&)const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
    juce::AudioProcessorEditor* createEditor()override;
    bool hasEditor()const override{return true;}
    const juce::String getName()const override{return "Cineol Pitch Lab";}
    bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
    double getTailLengthSeconds()const override{return 180;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return "Laboratory";}void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock&)override;
    void setStateInformation(const void*,int)override;
    void applyPreset(unsigned);
    void setValues(const cineol::pitch_lab::Values&);
    void requestPanic()noexcept{panic_.store(true,std::memory_order_release);}
    bool ready()const noexcept{return ready_.load(std::memory_order_acquire);}
    bool hallReady()const noexcept{return hall_ready_.load(std::memory_order_acquire);}
    std::atomic<bool> guardTripped{false};
    std::atomic<float> callbackLoad{0},pitchLatencyMs{0};
    std::atomic<uint64_t> hallSaturations{0};
    struct AtomicMeter {std::atomic<float> peak{0},power{0};std::atomic<uint64_t> clips{0};};
    std::array<AtomicMeter,cineol::pitch_lab::node_count> meters;
    // Audio producer / GUI consumer. Overflow drops visual data, never audio.
    juce::AbstractFifo analyzerFifo{8192};
    std::array<std::array<float,8192>,2> analyzerSamples{};
    juce::AudioProcessorValueTreeState state;
    juce::ValueTree snapshotA{"SnapshotA"},snapshotB{"SnapshotB"};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    juce::SharedResourcePointer<DualHallBank> bank_;
    cineol::pitch_lab::Core core_;
    lexplug::RateBridge bridge_;
    std::array<std::atomic<float>*,cineol::pitch_lab::count> values_{};
    cineol::pitch_lab::Values last_=cineol::pitch_lab::defaults();
    std::atomic<bool> ready_{false},hall_ready_{false},panic_{false};
    std::atomic<unsigned> edit_revision_{0};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchLabProcessor)
};
