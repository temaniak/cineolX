#pragma once
#include <array>
#include <cstdint>
namespace cineol::xl {
// Native delay-memory layout. All anchors/ranges are prepared from the
// selected program, independently of the audio graph and hardware controls.
struct AddressLayout {
    std::array<uint16_t,4> regions{};
    std::array<uint16_t,2> lengths{};
    std::array<uint8_t,2> scales{};
    uint16_t scale_address(uint16_t base) const noexcept {
        if(base<=regions[2]) return base;
        auto scaled=[&](uint16_t anchor,unsigned scale) {return uint16_t(anchor+((unsigned(uint16_t(base-anchor))*scale)>>4));};
        if(base<regions[0]) return scaled(regions[2],scales[1]);
        if(base<=regions[1]) return scaled(regions[0],scales[0]);
        const uint16_t boundary=uint16_t(regions[1]+(uint16_t(-regions[1])>>1));
        return base<boundary?uint16_t(base-regions[1]+lengths[0]):uint16_t(base-boundary+lengths[1]);
    }
};
struct SizeProfile {
    bool enabled=false;
    uint8_t coupling=0;
    std::array<uint8_t,4> ranges{};
    std::array<uint16_t,128> offsets{};
    std::array<bool,128> scale{};
    bool valid() const noexcept {
        return !enabled || (ranges[0] && ranges[1]>=ranges[0] &&
            (coupling || (ranges[2] && ranges[3]>=ranges[2])));
    }
    static uint8_t factor(uint8_t raw,uint8_t minimum,uint8_t maximum) noexcept {
        const unsigned value=minimum+((unsigned(raw)*(maximum-minimum))>>8);
        return uint8_t(value*16/minimum);
    }
    AddressLayout layout(AddressLayout result,uint8_t left,uint8_t right) const noexcept {
        if(!enabled) return result;
        result.scales[0]=factor(left,ranges[0],ranges[1]);
        result.lengths[0]=uint16_t(result.regions[0]+((unsigned(uint16_t(result.regions[1]-result.regions[0]))*result.scales[0])>>4)+1);
        result.lengths[1]=uint16_t(result.lengths[0]+(uint16_t(-result.lengths[0])>>1));
        if(!coupling) result.scales[1]=factor(right,ranges[2],ranges[3]);
        else {
            result.scales[1]=uint8_t(16+((unsigned(coupling)*(result.scales[0]-16)*4)>>8));
            const unsigned gap=uint16_t(result.regions[0]-result.regions[2])>>2;
            const unsigned span=uint16_t(uint16_t(result.regions[3]-result.regions[2])*4);
            if(span) {
                const unsigned cap=uint8_t((gap*65536/span)>>8);
                if(result.scales[1]>=cap) result.scales[1]=uint8_t(cap);
            }
        }
        return result;
    }
    template<class Settings>
    void apply(Settings& settings,const AddressLayout& layout) const noexcept {
        if(!enabled) return;
        for(unsigned row=0;row<settings.rows;++row)
            settings.offsets[row]=scale[row]?layout.scale_address(offsets[row]):offsets[row];
    }
};
}
