#pragma once
#include "../engines.hpp"
#include "../../plugin/RomBank.hpp"
#include <juce-plugin/plugin/rate_bridge.hpp>
class CleanABProcessor final : public juce::AudioProcessor {
public:
    CleanABProcessor();
    void prepareToPlay(double,int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&)const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
    juce::AudioProcessorEditor* createEditor()override;
    bool hasEditor()const override{return true;}
    const juce::String getName()const override{return "Cineol Clean AB";}
    bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
    double getTailLengthSeconds()const override{return 70;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return "";}void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock&)override;
    void setStateInformation(const void*,int)override;
    bool ready(unsigned p)const noexcept{return prepared_.load(std::memory_order_acquire) && engines_.available(p);}
    const cineol::xl::ProgramData* xlData(unsigned p)const noexcept {
        return p>=3 && p<7 && bank_->xlReady()?&bank_->xlBank().programs[cineol::clean_ab::indices[p]]:nullptr;
    }
    juce::AudioProcessorValueTreeState state;
    static constexpr unsigned count=64;
    static juce::String id(unsigned i);
    std::atomic<float> meterA{-120},meterB{-120};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void controls(const std::array<float,count>&)noexcept;
    juce::SharedResourcePointer<CineolRomBank> bank_;
    cineol::clean_ab::Engines engines_;
    lexplug::RateBridge bridge_;
    std::array<std::atomic<float>*,count> values_{};
    std::array<float,count> previous_{};
    std::array<std::array<float,2>,70> dry_{};unsigned dryPos_=0,selection_=7;
    float blend_=0,gain_=1,mix_=1,trim_=1,output_=1;
    std::atomic<bool> prepared_{false};
};
