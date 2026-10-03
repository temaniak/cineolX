#pragma once
#include "../import/bank_import.hpp"
#include <juce_audio_utils/juce_audio_utils.h>

// Shared by all instances in one plugin binary. The bank is published once
// and remains immutable until the last processor has been destroyed.
class CineolRomBank final : private juce::Thread {
public:
    CineolRomBank();
    ~CineolRomBank() override;
    bool ready() const noexcept {return ready_.load(std::memory_order_acquire);}
    const native_hall::ProgramBank& bank() const noexcept {return bank_;}
    bool importing() const noexcept {return importing_.load(std::memory_order_acquire);}
    double progress() const noexcept {return progress_.load(std::memory_order_relaxed);}
    juce::String status() const;
    bool startImport(const juce::Array<juce::File>&);
    void cancelImport() {signalThreadShouldExit();}
    static juce::File cacheFile();
private:
    void run() override;
    void setStatus(const juce::String&);
    native_hall::ProgramBank bank_{};
    std::atomic<bool> ready_{false},importing_{false};
    std::atomic<double> progress_{0};
    mutable juce::CriticalSection status_lock_;
    juce::String status_;
    juce::Array<juce::File> sources_;
    static_assert(std::atomic<bool>::is_always_lock_free,"bank publication must not lock");
};
