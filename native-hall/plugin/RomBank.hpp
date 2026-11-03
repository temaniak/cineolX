#pragma once
#include "../import/bank_import.hpp"
#include "../import/xl_import.hpp"
#include <juce_audio_utils/juce_audio_utils.h>
namespace clean_native_hall {struct ProgramBank;}
namespace clean_cineol::xl {struct Bank;}

// Shared by all instances in one plugin binary. The bank is published once
// and remains immutable until the last processor has been destroyed.
class CineolRomBank final : private juce::Thread {
public:
    CineolRomBank();
    ~CineolRomBank() override;
    bool ready() const noexcept {return ready_.load(std::memory_order_acquire);}
    const native_hall::ProgramBank& bank() const noexcept {return bank_;}
    bool xlReady() const noexcept {return xl_ready_.load(std::memory_order_acquire);}
    const cineol::xl::Bank& xlBank() const noexcept {return xl_bank_;}
    const clean_native_hall::ProgramBank& cleanBank() const noexcept {return *clean_bank_;}
    const clean_cineol::xl::Bank& cleanXLBank() const noexcept {return *clean_xl_bank_;}
    bool importing() const noexcept {return importing_.load(std::memory_order_acquire);}
    double progress() const noexcept {return progress_.load(std::memory_order_relaxed);}
    juce::String status() const;
    bool startImport(const juce::Array<juce::File>&);
    void cancelImport() {signalThreadShouldExit();}
    static juce::File cacheFile();
    static juce::File xlCacheFile() {return cacheFile().getParentDirectory().getChildFile("programs-v821-native-v5.bankxl");}
private:
    void run() override;
    void setStatus(const juce::String&);
    void publishOriginal();
    void publishXL();
    native_hall::ProgramBank bank_{};
    cineol::xl::Bank xl_bank_{};
    std::unique_ptr<clean_native_hall::ProgramBank> clean_bank_;
    std::unique_ptr<clean_cineol::xl::Bank> clean_xl_bank_;
    std::atomic<bool> xl_ready_{false};
    std::atomic<bool> ready_{false},importing_{false};
    std::atomic<double> progress_{0};
    mutable juce::CriticalSection status_lock_;
    juce::String status_;
    juce::Array<juce::File> sources_;
    static_assert(std::atomic<bool>::is_always_lock_free,"bank publication must not lock");
};
