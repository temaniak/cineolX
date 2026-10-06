#pragma once
#include "dynamics.hpp"
#include "feedback_timing_xl.hpp"
#include <cassert>

namespace cineol::xl {
struct DecayCompilerMemoryXL {
    std::array<uint8_t,6> cached{};
    uint16_t decay_time=0;
    uint8_t special=1,feedback_index=1;
    bool extended_mask=false,main_enabled=true;
};
namespace decay_compiler_detail {
constexpr unsigned index_work(uint8_t raw) noexcept {return (raw&248)?30:39;}
constexpr unsigned sign_work(int value) noexcept {return value<0?45:29;}
constexpr unsigned wide_product_work(uint8_t operand,uint16_t multiplicand) noexcept {
    unsigned work=419+10*std::popcount(unsigned(operand)),partial=0;
    for(unsigned bit=8;bit--;) {
        partial=(partial*2)&65535;
        if((operand>>bit)&1) {const unsigned sum=partial+multiplicand;work+=sum>65535?5:0;partial=sum&65535;}
    }
    return work;
}
struct Division {uint16_t quotient;unsigned work;};
constexpr Division divide(uint16_t numerator,uint16_t denominator) noexcept {
    unsigned remainder=numerator,quotient=0,work=30;
    for(unsigned i=0;i<16;++i) {
        remainder*=2;const bool subtract=remainder>65535 || remainder>=denominator;
        remainder=(subtract?remainder-denominator:remainder)&65535;
        quotient=((quotient<<1)|unsigned(subtract))&65535;work+=120+(subtract?28:0);
    }
    return {uint16_t(quotient),work};
}
inline unsigned curve_work(const ControlProfile& profile,uint8_t type,unsigned index) noexcept {
    const auto& curve=profile.decay_curves[(type-1)&15];const unsigned position=index*32;
    const unsigned next=(position>>8)+1;
    // At the exact last knot, interpolation still reads the adjacent curve
    // byte for its work cost; its zero fraction leaves the value unchanged.
    const unsigned last=next<curve.size()?curve[next]:profile.decay_curves[type&15][0];
    const int difference=int(last)-int(curve[position>>8]);
    // Curve selection/indexing followed by the signed byte interpolation.
    constexpr unsigned select=11+11+7+5+4+4+4+5+7+5+5+5+50+11+5+10+10+10+10+17;
    constexpr unsigned interpolation=11+7+11+5+7+4+11+10+11+7+17+10+10+5+10+10+7+4+10+10;
    return select+interpolation+byte_product_work(uint8_t(std::abs(difference)))+
        (difference<0?18:0)+7+4+10+10+10;
}
template<class Write>
unsigned magnitude_write(unsigned at,unsigned row,unsigned magnitude,const std::array<uint8_t,128>& low2,Write&& write) noexcept {
    const uint8_t value=uint8_t(uint8_t(uint8_t(~magnitude)*4)|(low2[row]&3));
    return 125+write(at+125,ControlByteXL{row,3,value})+20;
}
template<class Write>
unsigned sign_write(unsigned at,unsigned row,int value,const std::array<uint8_t,128>& low7,Write&& write) noexcept {
    const uint8_t payload=uint8_t(low7[row]|(value<0?0:128));
    return 129+write(at+129,ControlByteXL{row,2,payload})+25;
}
constexpr unsigned descriptor_work=63;
template<class Write>
unsigned low_work(const ControlProfile& p,DecayCompilerMemoryXL& m,bool changed,uint8_t low,uint8_t mid,
    unsigned at,const std::array<uint8_t,128>& low2,const std::array<uint8_t,128>& low7,Write&& write) noexcept {
    const unsigned begin=at;at+=10;if(!changed)return 35;
    at+=11+4+17+index_work(low)+5+5+7+11+17+index_work(mid)+5+5+4+17;
    const unsigned lf=std::max(1u,unsigned(low)>>3),mf=std::max(1u,unsigned(mid)>>3);
    int difference=int(lf)-int(mf);at+=sign_work(difference);
    const auto& group=p.slots[0].group[0];const unsigned count=group.count;
    at+=13+7+10;
    if(count) {
        at+=16+5+5+7+4+10;
        if(group.targets[0].meta) {
            at+=11+13+7+5+7+13+7+5+5+17;
            const auto ratio=divide(uint16_t(p.slots[1].group[0].count),uint16_t(count*256));
            const unsigned exponent=ratio.quotient>>8;at+=ratio.work+10+5+7+10;
            if(exponent>=2) {
                at+=11+5+4+4+4+4+5+5+10;
                unsigned power=mf*8;
                if(exponent>2) {
                    at+=5+5+7+17;
                    at+=byte_product_work(uint8_t(power));unsigned next=(power*power)>>8;
                    at+=5+5+10;
                    for(unsigned i=3;i<exponent;++i) {at+=17+byte_product_work(uint8_t(next))+5+5+10;next=(next*power)>>8;}
                    power=next;
                }
                at+=10+11+7+10;if(power<64)at+=7;power=std::max(64u,power);
                at+=5+5+7+7+5+5+17;
                const auto scaled=divide(uint16_t(unsigned(std::abs(difference))&63),uint16_t(power));
                at+=scaled.work+10+5+7+5+5+7+10;
                unsigned value=scaled.quotient>>8;if(value>=32){value=31;at+=7;}
                difference=difference<0?-int(value):int(value);at+=4+5;
            }
        }
    }
    at+=5+13+10+10+11+17+descriptor_work+10+10;
    if(!count) {m.cached[1]=255;at+=11+10+10+10+10+10;return at-begin;}
    at+=11+5+13+5;
    for(unsigned i=0;i<count;++i) {
        const unsigned row=group.targets[i].row;at+=7+5+7+5+5+17;
        at+=sign_write(at,row,difference,low7,write);at+=17;
        at+=magnitude_write(at,row,unsigned(std::abs(difference)),low2,write);at+=5+10;
    }
    return at+10+10-begin;
}
template<class Write>
unsigned mid_work(const ControlProfile& p,DecayCompilerMemoryXL& m,DynamicsState& state,bool changed,uint8_t low,uint8_t mid,
    const std::array<uint8_t,48>& raw,unsigned at,const std::array<uint8_t,128>& low2,
    const std::array<uint8_t,128>& low7,Write&& write) noexcept {
    const unsigned begin=at;at+=10;if(!changed)return 35;
    at+=11+11+11+11+17+index_work(mid)+5+13+5+5+17;
    const unsigned normal=std::max(1u,unsigned(mid)>>3),scale=p.layout(raw).scales[0];
    at+=11+11+5+7+10+10+7+7+7+10;if(normal>=30)at+=5+5+7;
    at+=4+10+4+11+16+7+7+5+17+byte_product_work(uint8_t(scale))+40+5+10;
    const unsigned product=scale*p.time_scale;uint8_t time_scale=uint8_t((product>>4)&255);
    if(product&4096){time_scale=255;at+=7;}
    at+=10+17+wide_product_work(time_scale,uint16_t(p.decay_times[normal]*2));
    const unsigned duration=p.decay_times[normal]*2*time_scale;
    at+=5+5+5+5+4+10;if(duration&128)at+=5;
    at+=10+10+10;
    m.decay_time=uint16_t((duration+128)>>8);
    at+=16+10+10+10+10+10+7+10;
    if(mid>=253){m.special=65;state.peak_input=32;at+=7+13+7+10;}
    else {
        at+=11+13+7+10;
        if(m.special&64){at+=13+7+10;if(state.peak_input==32){m.special=129;at+=7+13;}}
        at+=10+17+index_work(mid);state.peak_input=uint8_t(normal);
    }
    at+=13+17+descriptor_work;
    const auto& group=p.slots[1].group[0];if(!group.count)return at+11-begin;
    at+=5+11+5+13+5;
    for(unsigned i=0;i<group.count;++i) {
        const auto& target=group.targets[i];at+=11+7+5+7+5+7+4+10;
        unsigned magnitude=state.peak_input;
        if(target.meta) {
            at+=5+7+17+curve_work(p,target.meta,state.peak_input);
            magnitude=p.decay_coefficient(target.meta,state.peak_input);at+=5+7+7+10;
            if(target.meta&64) {
                at+=5+17;at+=magnitude_write(at,target.row,magnitude,low2,write);
                at+=7+5+7+5+5+10+11+5+11+13+17+index_work(low)+5+7+5+17;
                const unsigned lf=std::max(1u,unsigned(low)>>3);
                at+=curve_work(p,target.second_meta,lf);
                const int value=int(p.decay_coefficient(target.second_meta,lf))-int(magnitude);
                at+=10+4+17+sign_work(value)+17;at+=sign_write(at,target.second_row,value,low7,write);
                at+=5+17;at+=magnitude_write(at,target.second_row,unsigned(std::abs(value)),low2,write);
                at+=10+5+10;continue;
            }
        }
        at+=5+17;at+=magnitude_write(at,target.row,magnitude,low2,write);at+=10+5+10;
    }
    return at+10+10-begin;
}
constexpr unsigned unchanged_slot_work(ControlKind kind) noexcept {
    // Ordinary parameter transactions prepare the non-decay first-page
    // controls coherently. Decay recompiles must not change their snapshots.
    switch(kind) {
        case ControlKind::filter:return 10+10+10+5+5+10;
        case ControlKind::depth:return 10+10+10+5+5+10;
        case ControlKind::predelay:return 35;
        default:return 35;
    }
}
}
// Prepared main LF/MID compiler. Non-decay controls must already belong to
// the current coherent parameter snapshot. The enclosing runtime owns their
// transaction boundary, serial IRQs and the graph write clock.
template<class Write>
unsigned decay_compile_work(const DynamicsProfile& dynamics,const ControlProfile& profile,
    const std::array<uint8_t,48>& raw,DecayCompilerMemoryXL& memory,DynamicsState& state,
    const std::array<uint8_t,128>& low2,const std::array<uint8_t,128>& low7,Write&& write) noexcept {
    using namespace decay_compiler_detail;auto& m=memory;
    unsigned at=7+13+16+5+7+10;if(!m.extended_mask)at+=5+7+5;
    at+=10+16+10+10+10;
    if(!m.main_enabled)return at+11;
    at+=5+5+13+17+86+5+10;
    if(state.mid!=m.cached[1]){m.cached[0]=m.cached[1]=255;at+=7+13+13;}
    at+=13+13+11+16+16+16+16+10+7+13+17;
    for(unsigned slot=0;slot<6;++slot) {
        const auto& control=profile.slots[slot];const uint8_t value=slot==0?state.low:slot==1?state.mid:raw[control.cell];
        const bool changed=value!=m.cached[slot];m.cached[slot]=value;
        at+=17+93+11+11+17+58;
        if(slot==0)at+=low_work(profile,m,changed,state.low,state.mid,at,low2,low7,write);
        else if(slot==1)at+=mid_work(profile,m,state,changed,state.low,state.mid,raw,at,low2,low7,write);
        else {assert(!changed && "non-decay snapshot must be prepared before gradual compile");at+=unchanged_slot_work(control.kind);}
        at+=10+5+5+10+5+13+5+13+10;
    }
    at+=10+11+16+16+10+17;
    state.amount=1;const unsigned index=std::max(1u,unsigned(m.cached[1])>>3);
    const unsigned limit=profile.definition_cell==255?profile.feedback_limit:32-(unsigned(raw[profile.definition_cell])>>3);
    at+=7+13+11+7+17+index_work(m.cached[1])+5+13+4+10;
    if(limit){at+=5+4+10;if(index<=limit-1)at+=5;}else at+=5;
    m.feedback_index=uint8_t(limit?std::min(limit-1,index):index);at+=10+10;
    at+=13+11+11+16+10+10+7+7+10;
    if(!profile.size.enabled)at+=13+10;
    else {
        at+=13+5+7+13+7+10;
        if(state.peak_input==32)at+=20;
        at+=4+13+17;
        const unsigned scale=profile.layout(raw).scales[0]*(state.peak_input==32?4u:1u);
        at+=wide_product_work(dynamics.base_period,uint16_t(scale))+4+10;
        const unsigned product=scale*dynamics.base_period;
        if(product>65535)at+=7;
        at+=5+4+10;if(product<256)at+=5;
    }
    state.period=Dynamics::normal_period(dynamics,profile,raw,state.peak_input);
    return at+13+13+10+10+10+4+10;
}
} // namespace cineol::xl
