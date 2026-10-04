#pragma once
// Offline display preparation only. These short formatter probes use a private
// ROM image during import; the resulting bank contains text and logical bindings.
// No CPU, executable image or firmware address is published to the audio path.
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace cineol::xl::import {
inline std::array<char,16> display_value(const std::array<uint8_t,65536>& source,
                                       unsigned slot,unsigned cell,unsigned raw,
                                       std::array<uint8_t,65536>& memory) {
    memory=source;
    memory[0x3ca3+cell]=uint8_t(raw);
    i8080_t cpu;uint64_t pins=i8080_init(&cpu);
    cpu.pc=0x8887;cpu.sp=0x3f00;cpu.b=uint8_t(slot);
    unsigned ticks=0;
    for(;ticks<100000;++ticks) {
        if(i8080_opdone(&cpu) && cpu.pc==0x8c8a) break;
        pins=i8080_tick(&cpu,pins|I8080_READY);
        if(pins&I8080_DBIN) I8080_SET_DATA(pins,memory[I8080_GET_ADDR(pins)]);
        if(pins&I8080_WR) memory[I8080_GET_ADDR(pins)]=I8080_GET_DATA(pins);
    }
    if(ticks==100000) throw std::runtime_error("XL display formatter did not finish.");
    std::array<char,16> result{};unsigned n=0;
    for(int address=0x3f88;address>=0x3f86;--address) {
        const unsigned byte=memory[unsigned(address)],character=byte&127;
        if(character) result[n++]=char(character==64?'-':character);
        if(byte&128) result[n++]='.';
    }
    const char* units[]={"",""," s"," ms"," Hz"," kHz"," %"," m"};
    if(cpu.a>=std::size(units)) throw std::runtime_error("Unknown XL display unit.");
    std::snprintf(result.data()+n,result.size()-n,"%s",units[cpu.a]);
    return result;
}
inline std::array<char,16> display_value(const std::array<uint8_t,65536>& source,
                                       unsigned slot,unsigned cell,unsigned raw) {
    auto memory=std::make_unique<std::array<uint8_t,65536>>();
    return display_value(source,slot,cell,raw,*memory);
}
}
