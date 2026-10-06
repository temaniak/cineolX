#pragma once
#include "graphs.hpp"
#include "wcs_timing_xl.hpp"
#include <cstdint>

namespace cineol::xl {
namespace coefficient_structure_detail {
struct Builder {
    std::array<uint8_t,128>& low2;
    template<unsigned Row,unsigned Op,unsigned RA,unsigned WA,bool Transfer,bool Zero,unsigned Outputs,bool Shift,bool WriteX=false>
    constexpr void node(int16_t=0) noexcept {
        // The coefficient lane shares bits 25/24 with ZERO/XFER. CPU bus
        // polarity is inverted, so retain their complements in its low bits.
        low2[Row]=uint8_t((Zero?0:2)|(Transfer?0:1));
    }
    constexpr void run(Graph graph) noexcept {
        int16_t left=0,right=0;
        switch(graph) {
#include "graph_dispatch.inc"
        }
    }
};
constexpr auto make() noexcept {
    std::array<std::array<uint8_t,128>,22> result{};
    for(unsigned i=0;i<result.size();++i){result[i].fill(3);Builder{result[i]}.run(Graph(i));}
    return result;
}
} // namespace coefficient_structure_detail
// Derive immutable lane structure from the existing native graph, rather
// than requiring ROM words or another bank payload in audio processing.
inline constexpr auto coefficient_lane_low2=coefficient_structure_detail::make();
namespace coefficient_structure_detail {
struct SignBuilder {
    std::array<uint8_t,128>& low7;Graph graph;
    template<unsigned Row,unsigned Op,unsigned RA,unsigned WA,bool Transfer,bool Zero,unsigned Outputs,bool Shift,bool WriteX=false>
    constexpr void node(int16_t=0) noexcept {
        const auto slots=wcs_unprotected_rows[unsigned(graph)];
        // The source-less RESET row uses OPER; other Op=0 rows use NOP.
        const unsigned operation=Op==1?3:Op==2?2:Op || Row+2==graph_info(graph).rows?1:0;
        const unsigned protect=Row!=slots[0] && Row!=slots[1];
        low7[Row]=uint8_t(~((protect<<6)|(RA<<4)|(WA<<2)|operation))&127;
    }
    constexpr void run() noexcept {
        int16_t left=0,right=0;
        switch(graph) {
#include "graph_dispatch.inc"
        }
    }
};
constexpr auto make_sign() noexcept {
    std::array<std::array<uint8_t,128>,22> result{};
    for(unsigned i=0;i<result.size();++i){result[i].fill(127);SignBuilder{result[i],Graph(i)}.run();}
    return result;
}
}
inline constexpr auto coefficient_sign_lane_low7=coefficient_structure_detail::make_sign();
} // namespace cineol::xl
