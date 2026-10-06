#pragma once
#include "diffusion.hpp"
#include "control_timing_xl.hpp"
#include <bit>

namespace cineol::xl {
struct FeedbackBytesXL {uint8_t tag=128,outer=0,middle=32;};
struct TimedFeedbackXL {unsigned work_states=0;uint64_t finished_state=0;};
// Native byte-product work: eight fixed bit positions, plus an add for each
// set bit of the byte operand. The value uses arithmetic multiplication.
constexpr unsigned byte_product_work(uint8_t operand) noexcept {
    return 339+10*std::popcount(unsigned(operand));
}
// B000 feedback control law and work. low2 contains only each immutable
// coefficient lane's two structural bits in CPU bus polarity. The prepared
// profile/metadata and native indices are supplied outside this kernel.
template<class Write>
unsigned feedback_work_cycles(const DiffusionProfile& profile,const std::array<uint8_t,128>& low2,
    uint8_t normal_index,uint8_t stop_index,uint8_t reduction,FeedbackBytesXL& bytes,Write&& write) noexcept {
    if(!profile.count)return 31;
    unsigned at=88;uint8_t doubled=uint8_t(normal_index*2);bytes.tag=128;
    for(unsigned i=0;i<profile.count;++i) {
        const auto& target=profile.targets[i];at+=65;
        if(target.scale_cap!=bytes.tag) {
            bytes.tag=target.scale_cap;const uint8_t scale=target.scale_cap&224;
            at+=78+byte_product_work(scale)+52;
            unsigned scaled=(unsigned(scale)*doubled)>>8;
            if(profile.half_scale){scaled>>=1;at+=14;}
            at+=28;const unsigned cap=target.scale_cap&31;
            if(cap>=scaled)at+=5;
            const unsigned capped=std::min(cap,scaled);at+=60;
            uint8_t outer=uint8_t(capped-reduction);
            if(outer&128){outer=0;at+=7;}
            bytes.outer=outer;at+=68;
            const uint8_t operand=uint8_t(outer*2);
            at+=byte_product_work(operand)+79;
            const uint16_t product=uint16_t(unsigned(operand)*operand);
            bytes.middle=uint8_t(32-uint8_t(uint16_t(product*2+128)>>8));
        }
        for(unsigned j=0;j<3;++j) {
            at+=j?55:40;
            // Native coefficient-byte writer: template lookup and bit merge
            // take 125 states before STAX; POP/RET take twenty after it.
            at+=125;const unsigned row=target.row+j;
            const uint8_t magnitude=j==1?bytes.middle:bytes.outer;
            const uint8_t value=uint8_t(uint8_t(~magnitude)*4)|(low2[row]&3);
            at+=write(at,ControlByteXL{row,3,value});at+=20;
        }
        at+=40;
        if(profile.separate_stop) {
            at+=27;
            if(!profile.half_scale) {
                at+=21;
                if(profile.count-i<=3){doubled=uint8_t(stop_index*2);bytes.tag=128;at+=42;}
            }
        }
        at+=15;
    }
    return at+10;
}
template<class Resume,class Emit>
TimedFeedbackXL timed_feedback(const DiffusionProfile& profile,const std::array<uint8_t,128>& low2,
    uint8_t normal_index,uint8_t stop_index,uint8_t reduction,FeedbackBytesXL& bytes,
    WcsTimingXL& bus,Resume&& resume,Emit&& emit) noexcept {
    const unsigned work=feedback_work_cycles(profile,low2,normal_index,stop_index,reduction,bytes,
        [&](unsigned at,ControlByteXL payload) noexcept {
            const uint64_t start=resume(at);const auto access=bus.access(start+4,false);
            emit(at,start,payload,access);return unsigned(access.finished_cpu_state-start);
        });
    return {work,resume(work)};
}
} // namespace cineol::xl
