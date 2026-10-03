#pragma once
#include "../core/program_bank.hpp"
#include <functional>
#include <iosfwd>
#include <memory>
#include <string>

namespace native_hall::import {
using RomSet=std::array<std::array<uint8_t,2048>,5>;
// Also read by the Daisy build preflight; keep one list for both platforms.
inline constexpr std::array<const char*,5> rom_hashes={
#include "rom_hashes.inc"
};
// Content recognition deliberately accepts only original 224 v4.4 chips.
int rom_chip(const uint8_t*,size_t);
struct Callbacks {
    // Return false to cancel; invoked between bounded emulation intervals.
    std::function<bool(double,const char*)> progress;
    std::ostream* log=nullptr;
    std::string capture_prefix; // CLI diagnostics only, never used by the plugin.
};
std::unique_ptr<ProgramBank> prepare_bank(const RomSet&,const Callbacks& = {});
}
