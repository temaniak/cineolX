#pragma once
#include "../desktop/bank.hpp"
#include "bank_import.hpp"
#include <vector>

namespace cineol::xl::import {
using RomSet=std::array<std::vector<uint8_t>,11>;
int rom_chip(const uint8_t*,size_t);
std::unique_ptr<Bank> prepare_bank(const RomSet&,const native_hall::import::Callbacks& = {});
// Offline build diagnostics: allocate the actual importer/operator frames and
// cancel before emulation. No firmware or prepared bank is needed.
struct PreparationRuntimeStats {size_t largest_frame=0,peak_frames=0;};
PreparationRuntimeStats check_preparation_runtime(const native_hall::import::Callbacks& = {});
} // namespace cineol::xl::import
