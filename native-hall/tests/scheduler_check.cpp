// Compare every clock/event callback with the clean pinned scheduler.
#include <emulator/scheduler.hpp>
#define lexicon224x reference_lexicon224x
#include "../../deps/reflexion/emulator/scheduler.hpp"
#undef lexicon224x
#include <array>
#include <iostream>
#include <vector>

using Entry=std::array<uint64_t,3>;
template<class Scheduler> static std::vector<Entry> trace(Scheduler& scheduler,uint64_t chunk,bool events) {
    std::vector<Entry> result;
    constexpr uint64_t period=281250;
    scheduler.set_cpu_time(62500);
    unsigned cpu=0;
    auto phase=[&](auto kind,uint64_t row) {
        result.push_back({scheduler.now(),unsigned(kind),row});
        if(events && unsigned(kind)==0 && row%47==0) {
            scheduler.at(scheduler.now()+scheduler.timing.fetch_offset,[&] {
                result.push_back({scheduler.now(),10,0});
            });
            scheduler.at(scheduler.now()+scheduler.timing.fetch_offset,[&] {
                result.push_back({scheduler.now(),11,0});
            },true);
        }
    };
    auto processor=[&] {
        result.push_back({scheduler.now(),20,cpu++});
        if(events && cpu%97==0) scheduler.at(scheduler.now()+19000,[&] {
            result.push_back({scheduler.now(),30,0});
            scheduler.at(scheduler.now()+6000,[&] {result.push_back({scheduler.now(),31,0});},true);
        });
        scheduler.set_cpu_time(scheduler.now()+period);
    };
    constexpr uint64_t end=20000*period;
    for(uint64_t deadline=chunk;deadline<end+chunk;deadline+=chunk) {
        const auto until=std::min(deadline,end);
        while(scheduler.step_one(until,phase,processor)) {}
        scheduler.stand_at(until);
    }
    return result;
}
int main() {
    uint64_t callbacks=0;
    for(bool original:{false,true}) for(bool events:{false,true})
        for(uint64_t chunk:{uint64_t(100003),uint64_t(28125000),uint64_t(5625000000)}) {
            lexicon224x::cpu::Scheduler actual;
            reference_lexicon224x::cpu::Scheduler reference;
            if(original) {actual.timing=lexicon224x::cpu::timing_224;reference.timing=reference_lexicon224x::cpu::timing_224;}
            const auto expected=trace(reference,chunk,events),got=trace(actual,chunk,events);
            if(got!=expected) {std::cerr<<"scheduler clock/event order differs\n";return 1;}
            callbacks+=got.size();
        }
    std::cout<<"Scheduler: "<<callbacks<<" callbacks match pinned reference across both models, deadlines and tied/nested events\n";
}
