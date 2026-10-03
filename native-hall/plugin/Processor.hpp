#pragma once
#include "../core/engine48.hpp"
#include "RomBank.hpp"
#include <juce-plugin/plugin/rate_bridge.hpp>
#include <juce_audio_utils/juce_audio_utils.h>

class NativeHallProcessor final : public juce::AudioProcessor,private juce::Timer {
public:
    NativeHallProcessor();
    ~NativeHallProcessor() override;
    void prepareToPlay(double,int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override {return true;}
    const juce::String getName() const override {return "Cineol-X 224";}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    bool isMidiEffect() const override {return false;}
    double getTailLengthSeconds() const override {return 70;}
    int getNumPrograms() override {return int(native_hall::program_count);}
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    bool ready() const noexcept {return rom_bank_->ready();}
    bool importingRoms() const noexcept {return rom_bank_->importing();}
    double importProgress() const noexcept {return rom_bank_->progress();}
    juce::String romStatus() const {return rom_bank_->status();}
    bool importRoms(const juce::Array<juce::File>& files) {return rom_bank_->startImport(files);}
    void cancelRomImport() {rom_bank_->cancelImport();}
    juce::AudioProcessorValueTreeState state;
    static constexpr const char* ids[]={"bass","mid","crossover","treble","depth","predelay","diffusion",
        "input_db","mix","analog","mode_enh","decay_opt","output_l","output_r","algorithm"};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void updateLatency();
    void timerCallback() override {updateLatency();}
    juce::SharedResourcePointer<CineolRomBank> rom_bank_;
    native_hall::Engine48 engine_;
    lexplug::RateBridge bridge_;
    juce::AudioParameterChoice* algorithm_parameter_=nullptr;
    std::array<std::atomic<float>*,15> values_{};
    std::array<float,15> previous_{};
    std::atomic<float>* low_latency_value_=nullptr;
    std::atomic<bool> low_latency_active_{false};
    std::atomic<int> normal_latency_{native_hall::Engine48::latency_samples};
    juce::AudioBuffer<float> direct_input_;
    float host_mix_=1,host_mix_coefficient_=0.002f;
    bool engine_initialized_=false; // Owned only by prepareToPlay/processBlock.
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativeHallProcessor)
};
