#pragma once
#include "graphs.hpp"
#include <algorithm>
#include <cstdint>

namespace cineol::xl {
inline constexpr bool rich_chorus_graph(Graph graph) noexcept {
    return graph==Graph::rich_plate || graph==Graph::rich_split;
}
// These v8.21 graphs have no interpolation destinations. AD5C still runs
// twice per monitor pass (18 times per slow scan), including its long delay
// when the Chorus divider has not expired. Disabling Mod selects a different,
// fixed delay. The upper half changes step only, so has no timing effect here.
inline double rich_chorus_work(uint8_t raw,bool enabled,uint8_t hold) noexcept {
    if(!enabled) return 1127;
    const unsigned index=raw>>3,period=index<16?17-index:1;
    return 1197.0-1006.0/period+35.0/8+75.0/(8*std::max(1u,unsigned(hold)));
}
} // namespace cineol::xl
