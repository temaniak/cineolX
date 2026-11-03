#pragma once
#include "../../core/program_bank.hpp"
#include <juce_audio_utils/juce_audio_utils.h>

// Read-only original-bank access. Construction happens outside processing;
// this prototype cannot import ROMs, write caches or load XL banks.
class DualHallBank {
public:
    DualHallBank() {
        const auto override=juce::SystemStats::getEnvironmentVariable("CINEOL224_CACHE_DIR",{});
        auto folder=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
       #if JUCE_MAC
        folder=folder.getChildFile("Application Support");
       #endif
        folder=override.isNotEmpty()?juce::File(override):folder.getChildFile("Cineol-X 224");
        const auto file=folder.getChildFile("programs-v44-import-v1.bank224");
        juce::MemoryBlock bytes;
        ready_=file.getSize()==int64_t(sizeof(native_hall::BankHeader)+sizeof(data)) &&
            file.loadFileAsData(bytes) && native_hall::read_bank(bytes.getData(),bytes.getSize(),data);
    }
    bool ready() const noexcept {return ready_;}
    native_hall::ProgramBank data;
private:
    bool ready_=false;
};
