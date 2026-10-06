#pragma once
// Offline/private-ROM fixture support. Never include this header in plugin
// audio processing or the portable Daisy core.
#include <juce-plugin/source/operator/larc_operator.hpp>
#include <juce-plugin/source/roms/firmware_sets_data.hpp>
#include <juce-plugin/source/roms/sha256.hpp>
#include "../desktop/graphs.hpp"
#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

namespace xl_test {
using namespace lexplug;using namespace lexplug::op;
inline void require(bool ok,const char* message) {if(!ok) {std::cerr<<message<<'\n';std::exit(1);}}
inline void load(Engine& engine,const char* path) {
    const auto& set=lexplug::roms::data::known_sets[7];
    require(std::string(set.name)=="224XL v8.21","firmware catalog changed");
    unsigned mask=0;
    for(const auto& file:std::filesystem::directory_iterator(path)) {
        if(!file.is_regular_file() || file.file_size()>4096) continue;
        std::ifstream input(file.path(),std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
        const auto hash=lexplug::roms::Sha256::of(bytes.data(),bytes.size());
        for(int i=0;i<set.chip_count;++i) if(hash==set.chips[i].sha256) {
            engine.load(bytes.data(),bytes.size(),set.chips[i].base);mask|=1u<<i;
        }
    }
    require(mask==(1u<<set.chip_count)-1,"complete original 224XL v8.21 ROM set required");
}
inline Task<void> select(Machine& machine,LarcOperator& op,unsigned bank,unsigned program) {
    if(!co_await op.selectProgram(int(bank),int(program))) co_await fail("XL program selection failed");
    co_await op.setToggle(0,false);co_await op.setToggle(1,false);co_await op.setToggle(2,false);
    co_await machine.sleep(0.5);
}
inline Task<void> boot(Machine& machine,LarcOperator& op) {
    co_await machine.sleep(16);co_await select(machine,op,1,1);
}
// run_task's factory must call a coroutine function. A capturing coroutine
// lambda would retain the destroyed factory closure after its first wait.
inline Task<void> read_pages(LarcOperator& op,PagesReading& pages) {co_await op.readPages(pages);}
inline Task<void> move_slider(Machine& machine,LarcOperator& op,unsigned page,unsigned slot,unsigned raw,double settle) {
    co_await op.moveSlider(int(page),slot,raw);co_await machine.sleep(settle);
}
struct ShapeCheck {
    const lexicon224x::Machine& source;
    cineol::xl::Graph graph=cineol::xl::Graph::concert;
    bool valid=true;
    template<unsigned Row,unsigned Op,unsigned RA,unsigned WA,bool Transfer,bool Zero,unsigned Outputs,bool Shift,bool WriteX=false>
    void node(int16_t=0) {
        const auto mi=lexicon224x::decode(source.wcs[Row]);
        const unsigned operation=mi.op==lexicon224x::MEMR?1:mi.op==lexicon224x::MEMW?2:
            mi.op==lexicon224x::OPER && mi.source==lexicon224x::FromADC?3:
            mi.op==lexicon224x::OPER && mi.source==lexicon224x::FromRR?4:
            mi.op==lexicon224x::OPER && mi.source==lexicon224x::FromXREG?5:0;
        valid &= operation==Op && mi.ra==RA && mi.wa==WA && mi.xfer==Transfer && mi.zero==Zero &&
            (mi.wr_da?mi.channels:0)==Outputs && mi.keep_shifting==Shift &&
            mi.wr_xreg==WriteX &&
            mi.reset==(Row+2==cineol::xl::graph_info(graph).rows);
    }
    void run() {int16_t left=0,right=0;
        using cineol::xl::Graph;
        switch(graph) {
#include "../desktop/graph_dispatch.inc"
        }
    }
};
}
