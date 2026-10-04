#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include "size.hpp"
#include "diffusion.hpp"

namespace cineol::xl {
// Semantic compiler records prepared offline. No instruction stream, CPU
// registers, ROM addresses or opcode dispatch is retained by the controller.
enum class ControlKind : uint8_t {none,low_decay,mid_decay,filter,depth,predelay,
    chorus,diffusion,definition,level,delay,pan};
struct ControlTarget {
    uint8_t row=0,meta=0,second_row=0,second_meta=0;
    uint8_t signs=0;
    uint16_t offset=0;
};
struct ControlGroup {
    uint8_t count=0,flags=0,timing=0;
    std::array<ControlTarget,15> targets{};
};
struct ControlSlot {
    ControlKind kind=ControlKind::none;
    uint8_t cell=0,groups=0;
    std::array<ControlGroup,4> group{};
};
struct ControlProfile {
    std::array<ControlSlot,72> slots{};
    std::array<uint8_t,48> factory{};
    std::array<std::array<uint8_t,5>,16> decay_curves{};
    std::array<uint16_t,32> decay_times{};
    uint8_t time_scale=0;
    std::array<std::array<std::array<uint8_t,4>,4>,3> depth_curves{};
    AddressLayout addresses{};
    SizeProfile size{};
    DiffusionProfile feedback{};
    uint8_t definition_cell=255,feedback_limit=0,reduction=1;
    AddressLayout layout(const std::array<uint8_t,48>& values) const noexcept {
        return size.layout(addresses,values[43],values[44]);
    }
    unsigned decay_duration(uint8_t raw,unsigned side,const std::array<uint8_t,48>& values) const noexcept {
        const unsigned product=unsigned(layout(values).scales[side])*time_scale;
        const unsigned scale=(product&4096)?255:(product>>4)&255;
        return (unsigned(decay_times[raw>>3])*2*scale+128)>>8;
    }
    template<class Settings>
    void apply_size(Settings& settings,const std::array<uint8_t,48>& values) const noexcept {
        size.apply(settings,layout(values));
    }
    static unsigned milliseconds(unsigned raw) noexcept {
        if(raw<50) return raw;
        if(raw<100) return 50+(raw-50)*2;
        if(raw<150) return 150+(raw-100)*4;
        return 350+(raw-150)*8;
    }
    static unsigned timing(const AddressLayout& addresses,unsigned code,unsigned raw,unsigned fine) noexcept {
        if(code==255) return std::min(milliseconds(raw)*34,unsigned(uint16_t(addresses.lengths[1]-10)));
        const unsigned factors[]={4,8,17,34,68,136,136,136};
        unsigned value=(factors[code>>5]*(raw*256+fine)+128)>>8;
        if((code>>5)>=6) value<<=(code>>5)-5;
        return value&65535;
    }
    bool valid(unsigned rows) const noexcept {
        if(!size.valid() || !feedback.valid(rows) || (definition_cell!=255 && definition_cell>=factory.size())) return false;
        for(const auto& slot:slots) {
            if(slot.cell>=factory.size() || slot.groups>slot.group.size() || unsigned(slot.kind)>unsigned(ControlKind::pan)) return false;
            if((slot.kind==ControlKind::low_decay && slot.cell+1>=factory.size()) ||
               (slot.kind==ControlKind::mid_decay && slot.cell==0) ||
               (slot.kind==ControlKind::delay && slot.cell+6>=factory.size())) return false;
            for(unsigned g=0;g<slot.groups;++g) {
                const auto& group=slot.group[g];if(group.count>group.targets.size()) return false;
                for(unsigned t=0;t<group.count;++t) {
                    const auto& target=group.targets[t];
                    if(target.row>=rows || ((slot.kind==ControlKind::diffusion || slot.kind==ControlKind::definition) && unsigned(target.row)+2>=rows) || ((target.meta&64) && slot.kind==ControlKind::mid_decay && target.second_row>=rows)) return false;
                }
            }
        }
        return true;
    }
    static unsigned decay_index(uint8_t raw) noexcept {return raw>=253?32:std::max(1u,unsigned(raw)>>3);}
    static unsigned interpolate(const uint8_t* curve,unsigned raw) noexcept {
        const unsigned index=raw>>8,fraction=raw&255;
        const int first=curve[index];if(!fraction) return unsigned(first);
        const int difference=int(curve[index+1])-first;
        return unsigned(first+(difference<0?-1:1)*int((unsigned(std::abs(difference))*fraction)>>8));
    }
    unsigned decay_coefficient(uint8_t type,unsigned decay) const noexcept {
        return interpolate(decay_curves[(type-1)&15].data(),decay*32)>>1;
    }
    template<class Settings>
    static void set_magnitude(Settings& settings,const ControlTarget& target,unsigned magnitude) noexcept {
        settings.coefficients[target.row]=int8_t((target.signs&1)?-int(magnitude):int(magnitude));
    }
    template<class Settings>
    void apply_static(Settings& settings,const std::array<uint8_t,48>& values) const noexcept {
        const auto addresses=layout(values);
        const unsigned lf_count=slots[0].group[0].count,mid_count=slots[1].group[0].count;
        for(const auto& slot:slots) {
            const auto raw=values[slot.cell];
            switch(slot.kind) {
                case ControlKind::diffusion:case ControlKind::definition: {
                    const unsigned index=slot.kind==ControlKind::definition?63-(unsigned(raw)>>2):unsigned(raw)>>2;
                    const auto& group=slot.group[0];
                    for(unsigned t=0;t<group.count;++t) {
                        const auto& target=group.targets[t];
                        const unsigned outer=std::min(unsigned(target.meta&31),((unsigned(target.meta&224)*index*2)>>8)>>1);
                        const unsigned magnitudes[]={outer,32-((outer*outer*8+128)>>8),outer};
                        for(unsigned pair=0;pair<3;++pair) settings.coefficients[target.row+pair]=
                            int8_t((target.signs&(1u<<pair))?-int(magnitudes[pair]):int(magnitudes[pair]));
                    }
                    break;
                }
                case ControlKind::filter: {
                    const unsigned index=raw==255?0:raw>=253?32:unsigned(raw)>>3;
                    for(unsigned g=0;g<slot.groups;++g) {
                        const unsigned magnitude=raw==255?0:g==0?index:32-index;
                        for(unsigned t=0;t<slot.group[g].count;++t) set_magnitude(settings,slot.group[g].targets[t],magnitude);
                    }
                    break;
                }
                case ControlKind::depth: {
                    const auto flag=slot.group[0].flags;
                    const unsigned curve=(flag&128)?1:(flag&64)?2:0;
                    for(unsigned g=0;g<slot.groups;++g) {
                        const unsigned magnitude=raw==255?0:interpolate(depth_curves[curve][g].data(),unsigned(raw)*3);
                        for(unsigned t=0;t<slot.group[g].count;++t) set_magnitude(settings,slot.group[g].targets[t],magnitude);
                    }
                    break;
                }
                case ControlKind::predelay:case ControlKind::delay: {
                    const unsigned fine=slot.kind==ControlKind::delay?values[slot.cell+6]:0;
                    const auto& group=slot.group[0];const unsigned time=timing(addresses,group.timing,raw,fine);
                    for(unsigned t=0;t<group.count;++t) {
                        const auto& target=group.targets[t];
                        settings.offsets[target.row]=uint16_t(addresses.scale_address(target.offset)+time+1);
                    }
                    break;
                }
                case ControlKind::level: {
                    for(unsigned t=0;t<slot.group[0].count;++t) set_magnitude(settings,slot.group[0].targets[t],unsigned(raw)>>2);
                    break;
                }
                case ControlKind::pan: {
                    const int coefficient=int(std::max(1u,unsigned(raw)>>2))-32;
                    for(unsigned t=0;t<slot.group[0].count;++t) settings.coefficients[slot.group[0].targets[t].row]=int8_t(coefficient);
                    break;
                }
                case ControlKind::low_decay: {
                    const unsigned lf=std::max(1u,unsigned(raw)>>3),mid=std::max(1u,unsigned(values[slot.cell+1])>>3);
                    int difference=int(lf)-int(mid);unsigned magnitude=unsigned(std::abs(difference));
                    if(lf_count && mid_count/lf_count>=2 && slots[0].group[0].targets[0].meta) {
                        unsigned power=mid*8;
                        for(unsigned i=2;i<mid_count/lf_count;++i) power=(power*mid*8)>>8;
                        magnitude=std::min(31u,magnitude*256/std::max(64u,power));
                    }
                    const int coefficient=difference>=0?int(magnitude):-int(magnitude);
                    for(unsigned t=0;t<slot.group[0].count;++t) settings.coefficients[slot.group[0].targets[t].row]=int8_t(coefficient);
                    break;
                }
                case ControlKind::mid_decay: {
                    const unsigned mid=decay_index(raw),lf=std::max(1u,unsigned(values[slot.cell-1])>>3);
                    for(unsigned t=0;t<slot.group[0].count;++t) {
                        const auto& target=slot.group[0].targets[t];
                        if(!target.meta) {set_magnitude(settings,target,mid);continue;}
                        const auto magnitude=decay_coefficient(target.meta,mid);set_magnitude(settings,target,magnitude);
                        if(target.meta&64) {
                            const int coefficient=int(decay_coefficient(target.second_meta,lf))-int(magnitude);
                            settings.coefficients[target.second_row]=int8_t(coefficient);
                        }
                    }
                    break;
                }
                default:break;
            }
        }
        const unsigned limit=definition_cell==255?feedback_limit:32-(unsigned(values[definition_cell])>>3);
        auto index=[&](uint8_t raw) {const unsigned value=std::max(1u,unsigned(raw)>>3);return uint8_t(limit?std::min(limit-1,value):value);};
        feedback.apply(settings,index(values[1]),index(values[7]),reduction);
    }
};
}
