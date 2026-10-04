#pragma once
#include "../core/engine48.hpp"
#include "RomBank.hpp"
#include "Firmware.hpp"
#include "../desktop/runtime.hpp"
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
    int getNumPrograms() override {return int(program_count);}
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    static constexpr unsigned program_count=native_hall::program_count+cineol::xl::graphs.size();
    static bool isXL(int program) noexcept {return program>=int(native_hall::program_count);}
    bool usesXL() const noexcept;
    const cineol::xl::ProgramData* xlProgram() const noexcept;
    unsigned parameterPages() const noexcept;
    uint8_t xlControl(unsigned cell) const noexcept;
    juce::String firmwareId() const;
    bool programAvailable(int program) const noexcept {return isXL(program)?rom_bank_->xlReady():rom_bank_->ready();}
    bool ready() const noexcept {return programAvailable(algorithm_parameter_->getIndex());}
    bool importingRoms() const noexcept {return rom_bank_->importing();}
    double importProgress() const noexcept {return rom_bank_->progress();}
    juce::String romStatus() const {return rom_bank_->status();}
    bool importRoms(const juce::Array<juce::File>& files) {return rom_bank_->startImport(files);}
    void cancelRomImport() {rom_bank_->cancelImport();}
    static juce::File presetDirectory();
    juce::Result savePreset(const juce::File&);
    juce::Result loadPreset(const juce::File&);
    struct PresetInfo {juce::String name;int algorithm=0;juce::String firmware="224-v4.4";};
    static juce::File presetBankFile();
    juce::Result presetBankEntries(juce::Array<PresetInfo>&,const juce::File& bank=presetBankFile());
    juce::Result presetNames(juce::StringArray&,const juce::File& bank=presetBankFile());
    juce::Result saveBankPreset(const juce::String& name,bool replace=false,const juce::File& bank=presetBankFile());
    juce::Result loadBankPreset(const juce::String& name,const juce::File& bank=presetBankFile());
    juce::String presetName();
    bool presetModified();
    int editorPage() {const juce::ScopedLock lock(preset_write_lock_);return std::clamp(int(state.state.getProperty("editor_page",0)),0,8);}
    void setEditorPage(int page) {const juce::ScopedLock lock(preset_write_lock_);state.state.setProperty("editor_page",std::clamp(page,0,8),nullptr);}
    unsigned presetRecallRevision() const noexcept {return preset_recall_revision_.load(std::memory_order_acquire);}
    bool presetRecallInProgress() const noexcept {return (parameter_transaction_.load(std::memory_order_acquire)&1u)!=0;}
    juce::AudioProcessorValueTreeState state;
    static constexpr const char* ids[]={"bass","mid","crossover","treble","depth","predelay","diffusion",
        "input_db","mix","analog","mode_enh","decay_opt","output_l","output_r","algorithm","xl_chorus","xl_diffusion",
        "xl_00", "xl_01", "xl_02", "xl_03", "xl_04", "xl_05", "xl_06", "xl_07", "xl_08", "xl_09", "xl_10", "xl_11", "xl_12", "xl_13", "xl_14", "xl_15", "xl_16", "xl_17", "xl_18", "xl_19", "xl_20", "xl_21", "xl_22", "xl_23", "xl_24", "xl_25", "xl_26", "xl_27", "xl_28", "xl_29", "xl_30", "xl_31", "xl_32", "xl_33", "xl_34", "xl_35", "xl_36", "xl_37", "xl_38", "xl_39", "xl_40", "xl_41", "xl_42", "xl_43", "xl_44", "xl_45", "xl_46", "xl_47"};
    static constexpr unsigned xl_parameter_begin=17;
    static constexpr unsigned parameter_count=std::size(ids);
private:
    juce::Result applyPreset(const juce::ValueTree&,const juce::String&);
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void updateLatency();
    void timerCallback() override {updateLatency();}
    juce::SharedResourcePointer<CineolRomBank> rom_bank_;
    native_hall::Engine48 engine_;
    cineol::xl::Runtime xl_engine_;
    std::array<std::array<float,2>,native_hall::Engine48::latency_samples-cineol::xl::Runtime::latency_samples> xl_alignment_{};
    unsigned xl_alignment_position_=0;
    int active_program_=-1;
    lexplug::RateBridge bridge_;
    juce::AudioParameterChoice* algorithm_parameter_=nullptr;
    std::array<std::atomic<float>*,parameter_count> values_{};
    std::array<float,parameter_count> previous_{};
    std::array<float,parameter_count> stable_values_{}; // Audio-thread snapshot; never follows a partial preset recall.
    juce::CriticalSection preset_write_lock_; // Preset/session writers only; never used by processBlock.
    std::atomic<unsigned> parameter_transaction_{0},preset_recall_revision_{0};
    std::atomic<float>* low_latency_value_=nullptr;
    std::atomic<bool> low_latency_active_{false};
    std::atomic<int> normal_latency_{native_hall::Engine48::latency_samples};
    juce::AudioBuffer<float> direct_input_;
    float host_mix_=1,host_mix_coefficient_=0.002f;
    bool engine_initialized_=false; // Owned only by prepareToPlay/processBlock.
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativeHallProcessor)
};
