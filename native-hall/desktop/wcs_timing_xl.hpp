#pragma once
#include "graphs.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <algorithm>

namespace cineol::xl {
// Fixed graph properties, independently checked through physical Mod/Size
// controls. These are protection slots, not firmware words or coefficients.
inline constexpr std::array<std::array<uint8_t,2>,22> wcs_unprotected_rows{{
    {72,102},{72,102},{71,104},{62,97},{72,102},{82,105},{69,102},
    {63,97},{83,105},{82,105},{62,97},{71,97},{1,39},{83,106},
    {44,97},{26,47},{47,80},{61,99},{66,101},{62,101},{78,102},{80,101}}};

// Native T&C access clock. Stores only graph phase, protection flip-flops and
// a bounded displacement window. No CPU registers, ROM or opcode interpreter.
// The full firmware/controller clock is separate; this component accepts a
// data-bus T1 state and predicts the resulting grant and visibility boundaries.
class WcsTimingXL {
public:
    using Tick=uint64_t;
    static constexpr Tick cpu_period=281250,row_period=168750,master_period=18750;
    static constexpr Tick first_marker=207531,fetch_offset=58842,phi2_rise=62500;
    struct Access {
        Tick grant=0,ack=0,sample=0,commit=0;
        Tick displaced_begin=0,displaced_end=0,held_begin=0,held_end=0;
        Tick read_drive=0,read_release=0;
        uint64_t finished_cpu_state=0;
    };
    void reset(Graph graph,Tick origin=first_marker) noexcept {
        graph_=graph;rows_=graph_info(graph).rows;origin_=marker_=origin;pc_=0;
        fetch_pending_=false;pair_=resetd_=protected_=reset_=false;
        displaced_begin_=displaced_end_=0;last_operations_=0;
    }
    // Row-zero marker after a complete quiet pass: the previous flush row
    // is protected, the pair is clear and RESETD is low. It is not a general
    // phase/state import; the caller must establish that quiet boundary.
    void reset_settled(Graph graph,Tick row_zero_marker) noexcept {
        reset(graph,row_zero_marker);protected_=true;
    }
    Access access(uint64_t bus_t1_state,bool reading=false) noexcept {
        const Tick t1=bus_t1_state*cpu_period;
        const Tick request=t1+(reading?cpu_period+phi2_rise:2*cpu_period);
        last_operations_=0;advance(request);
        Access result;
        // Every supported graph has two slots and a protected RESET. With
        // sequential CPU accesses a grant occurs within two complete passes.
        for(unsigned attempt=0;attempt<4*rows_+16;++attempt) {
            if(fetch_pending_) {fetch();continue;}
            if(skip_stable(~Tick(0)))continue;
            const bool allowed=!pair_ && !protected_;
            fetch_pending_=true;++last_operations_;
            if(!allowed)continue;
            result.grant=marker_;
            const Tick latched=marker_+44352; // capture 4.5 ns + latch 72.5 ns
            result.ack=edge(phi2_rise,cpu_period,latched)+31680; // XACK +55 ns
            Tick ready=t1+cpu_period+93750;
            if(ready<=result.ack)ready+=(1+(result.ack-ready)/cpu_period)*cpu_period;
            result.sample=ready-93750+cpu_period+phi2_rise;
            result.finished_cpu_state=(result.sample-phi2_rise)/cpu_period+1;
            result.displaced_begin=marker_+row_period;
            result.held_begin=marker_+2*row_period+3*master_period;
            if(reading) {
                const Tick release=edge(marker_+51930,row_period,result.sample+24768);
                result.displaced_end=edge(marker_,row_period,release+1);
                result.held_end=release+15*master_period+4320;
                result.read_drive=marker_+103830;
                result.read_release=release+master_period+14400;
            } else {
                result.commit=marker_+226440;
                result.displaced_end=marker_+2*row_period;
                result.held_end=marker_+3*row_period;
            }
            displaced_begin_=result.displaced_begin;displaced_end_=result.displaced_end;
            return result;
        }
        assert(false && "unsupported XL WCS access sequence");return result;
    }
    unsigned last_operations()const noexcept{return last_operations_;}
private:
    static Tick edge(Tick first,Tick period,Tick minimum) noexcept {
        return minimum<=first?first:first+((minimum-first+period-1)/period)*period;
    }
    bool row_protected(unsigned pc)const noexcept {
        const auto& allowed=wcs_unprotected_rows[unsigned(graph_)];return pc!=allowed[0] && pc!=allowed[1];
    }
    void fetch() noexcept {
        const bool zero=marker_>=displaced_begin_ && marker_<displaced_end_;
        const bool next_protected=!zero && row_protected(pc_);
        const bool next_reset=!zero && pc_+2==rows_;
        if(!protected_ && next_protected)pair_=resetd_?!pair_:false;
        if(resetd_ && reset_)pair_=false;
        resetd_=!reset_;pc_=reset_?0:(pc_+1)&127;
        protected_=next_protected;reset_=next_reset;
        marker_+=row_period;fetch_pending_=false;++last_operations_;
    }
    bool skip_stable(Tick limit) noexcept {
        if(fetch_pending_ || !protected_ || reset_ || !resetd_ || marker_>=limit)return false;
        if(marker_>=displaced_begin_ && marker_<displaced_end_)return false;
        const auto& slots=wcs_unprotected_rows[unsigned(graph_)];
        unsigned boundary=rows_-2;
        if(pc_<=slots[0])boundary=slots[0];else if(pc_<=slots[1])boundary=slots[1];
        if(boundary<=pc_)return false;
        Tick target=marker_+(boundary-pc_)*row_period;
        if(displaced_begin_>marker_)target=std::min(target,displaced_begin_);
        target=std::min(target,limit);
        const unsigned count=unsigned((target-marker_)/row_period);
        if(!count)return false;
        pc_+=count;marker_+=count*row_period;++last_operations_;return true;
    }
    void advance(Tick time) noexcept {
        // A complete quiet pass clears pair state. Jump over long quiet gaps
        // without iterating over elapsed clock states or audio duration.
        const Tick pass=rows_*row_period;
        if(marker_+3*pass<time && displaced_end_+2*pass<time) {
            marker_=origin_+(time-origin_)/pass*pass;pc_=0;
            fetch_pending_=false;pair_=false;resetd_=false;protected_=true;reset_=false;
        }
        while((fetch_pending_?marker_+fetch_offset:marker_)<=time) {
            if(fetch_pending_)fetch();
            else if(skip_stable(time))continue;
            else {fetch_pending_=true;++last_operations_;}
        }
    }
    Graph graph_=Graph::concert;
    Tick origin_=first_marker,marker_=first_marker,displaced_begin_=0,displaced_end_=0;
    unsigned rows_=105,pc_=0,last_operations_=0;
    bool fetch_pending_=false,pair_=false,resetd_=false,protected_=false,reset_=false;
};
} // namespace cineol::xl
