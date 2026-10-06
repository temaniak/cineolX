#pragma once
#include "concert.hpp"
#include "diffusion.hpp"
#include "controls.hpp"
#include "dynamics.hpp"
#include "../core/profile.hpp"
#include <cstring>
#include <cstdio>

namespace cineol::xl {
// Local, immutable coefficients, offsets, control metadata and modulation data.
// Version 4 adds prepared level-following control clocks and initial state.
// Runtime control integration is validated separately from bank preparation.
struct ProgramData {
    ControlProfile controls{};
    struct Page {
        uint8_t column=0,type=0;
        std::array<uint8_t,6> cells{},maximum{};
        std::array<bool,6> variable_predelay{};
        // Complete logical-position display scales, prepared offline. UI only;
        // the native audio compiler uses the semantic ControlProfile below.
        std::array<std::array<std::array<char,16>,256>,6> values{};
        std::array<std::array<char,13>,6> names{};
        std::array<std::array<char,25>,6> factory_values{};
        std::array<uint8_t,6> raw{};
    };
    std::array<Page,9> pages{};
    uint8_t page_count=0,chorus_page=0,chorus_slot=0,diffusion_page=0,diffusion_slot=0;
    std::array<int8_t,128> coefficients{};
    std::array<uint16_t,128> offsets{};
    ModulationProfile modulation{};
    ModulationState initial_modulation{};
    DiffusionProfile diffusion{};
    uint8_t chorus=16,diffusion_index=16;
    uint8_t predelay_base=0;
    DynamicsProfile dynamics{};
    DynamicsState initial_dynamics{};
    unsigned predelay_maximum(const std::array<uint8_t,48>& raw) const noexcept {
        const unsigned length=controls.layout(raw).lengths[1];
        const unsigned available=uint16_t(0xfff6-length);
        const unsigned ms=((available*65536+length)/0x8800)>>6;
        if(ms>=1180) return 255;
        return ms<50?ms:ms<150?50+(ms-50)/2:ms<350?100+(ms-150)/4:150+(ms-350)/8;
    }
    void resolve_controls(std::array<uint8_t,48>& raw) const noexcept {
        const unsigned variable_limit=predelay_maximum(raw);
        for(unsigned p=0;p<page_count;++p) for(unsigned slot=0;slot<6;++slot) {
            const auto& page=pages[p];const unsigned cell=page.cells[slot];if(cell>=48) continue;
            const unsigned maximum=page.variable_predelay[slot]?variable_limit:page.maximum[slot];
            raw[cell]=uint8_t(std::min(unsigned(raw[cell]),maximum));
        }
    }
    // UI-only value formatting. Size changes the decay-time scale and the
    // variable pre-delay minimum, so those values use the current layout.
    std::array<char,16> display_value(unsigned page_index,unsigned slot,const std::array<uint8_t,48>& raw) const noexcept {
        const auto& page=pages[page_index];const unsigned cell=page.cells[slot];
        if(cell>=48) return {};
        auto result=page.values[slot][raw[cell]];
        const unsigned stop=dynamics.shared_stop?6:12;
        if(dynamics.enabled && (cell==stop || cell==stop+1)) {
            if(raw[cell]>=253) std::snprintf(result.data(),result.size(),"-- s");
            else std::snprintf(result.data(),result.size(),"%.1f s",controls.decay_duration(raw[cell],0,raw)*0.1);
            return result;
        }
        for(const auto& control:controls.slots) if(control.cell==cell &&
            (control.kind==ControlKind::low_decay || control.kind==ControlKind::mid_decay)) {
            if(raw[cell]>=253) std::snprintf(result.data(),result.size(),"-- s");
            else std::snprintf(result.data(),result.size(),"%.1f s",controls.decay_duration(raw[cell],page.type==2?1:0,raw)*0.1);
            return result;
        }
        if(page.variable_predelay[slot]) {
            const unsigned minimum=std::min(255u,(unsigned(controls.layout(raw).scales[1])*predelay_base)>>4);
            const unsigned amount=std::min(255u,minimum+raw[cell]);
            std::snprintf(result.data(),result.size(),"%u ms",std::min(999u,ControlProfile::milliseconds(amount)));
        }
        return result;
    }
    bool control_active(unsigned cell) const noexcept {
        if(cell>=48) return false;
        if(cell==45) return dynamics.enabled;
        if((dynamics.shared_stop && (cell==6 || cell==7)) ||
            (!dynamics.shared_stop && (cell==12 || cell==13))) return dynamics.enabled;
        if(size_cell(cell)) return controls.size.enabled;
        for(const auto& slot:controls.slots) if(slot.cell==cell) {
            if(slot.kind==ControlKind::chorus) return (modulation.flags&15)!=0;
            if(slot.kind==ControlKind::definition && controls.feedback.count) return true;
            for(unsigned g=0;g<slot.groups;++g) if(slot.group[g].count) return true;
        }
        return false;
    }
    static bool size_cell(unsigned cell) noexcept {return cell==43 || cell==44;}
    bool valid(Graph graph) const noexcept {
        const unsigned rows=graph_info(graph).rows;
        if((modulation.flags&15) && (initial_modulation.index!=59 || !initial_modulation.startup_lookup)) return false;
        for(unsigned r=0;r<rows;++r) if(coefficients[r]<-63 || coefficients[r]>63) return false;
        if(!page_count || page_count>pages.size()) return false;
        for(unsigned p=0;p<page_count;++p) for(unsigned slot=0;slot<6;++slot) {
            const auto& page=pages[p];
            if(page.cells[slot]!=255 && page.cells[slot]>=48) return false;
            if(page.names[slot].back() || page.factory_values[slot].back()) return false;
            for(const auto& value:page.values[slot]) if(value.back()) return false;
        }
        return dynamics.valid() && controls.valid(rows) && modulation.valid(rows) && initial_modulation.valid() && diffusion.valid(rows) &&
            chorus<32 && diffusion_index<64 && diffusion.half_scale && !diffusion.separate_stop;
    }
    template<Graph graph> typename Network<graph>::Settings settings() const noexcept {
        typename Network<graph>::Settings result;
        std::copy_n(coefficients.begin(),result.rows,result.coefficients.begin());
        std::copy_n(offsets.begin(),result.rows,result.offsets.begin());return result;
    }
};
struct Bank {
    std::array<ProgramData,graphs.size()> programs{};
    bool valid() const noexcept {
        for(unsigned i=0;i<programs.size();++i) if(!programs[i].valid(Graph(i))) return false;
        return true;
    }
};
struct BankHeader {
    char magic[8]={'B','X','L','8','2','1',0,0};
    // v5 retains the payload layout but stores the physical compiler's
    // interpolation seed/taps. A v4 running-phase snapshot cannot reconstruct
    // the program's original descriptors; re-import it outside audio.
    uint32_t version=5,size=sizeof(Bank),checksum=0;
};
inline bool read_bank(const void* data,size_t size,Bank& result) noexcept {
    if(!data || size!=sizeof(BankHeader)+sizeof(Bank)) return false;
    BankHeader header;std::memcpy(&header,data,sizeof header);const BankHeader expected;
    const auto* payload=static_cast<const uint8_t*>(data)+sizeof header;
    if(std::memcmp(header.magic,expected.magic,8) || header.version!=expected.version || header.size!=sizeof(Bank) ||
        header.checksum!=native_hall::profile_checksum(payload,sizeof(Bank))) return false;
    std::memcpy(&result,payload,sizeof result);return result.valid();
}
} // namespace cineol::xl
