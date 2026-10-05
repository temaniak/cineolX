#pragma once
// Offline oracle only. Never include this header in the audio processor.
#include "../desktop/engine22448.hpp"
#include "../import/bank_import.hpp"
#include <juce-plugin/source/engine.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace sound_validation {
inline void require(bool value,const std::string& message) {
    if(!value)throw std::runtime_error(message);
}
inline std::unique_ptr<native_hall::ProgramBank> read_bank(const char* path) {
    std::ifstream file(path,std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<native_hall::ProgramBank>();
    require(native_hall::read_bank(bytes.data(),bytes.size(),*bank),"Invalid original-224 prepared bank");
    return bank;
}
inline void load_roms(lexplug::Engine& engine,const char* directory) {
    unsigned loaded=0;
    for(const auto& entry:std::filesystem::directory_iterator(directory)) {
        if(!entry.is_regular_file() || entry.file_size()!=2048)continue;
        std::array<uint8_t,2048> bytes{};std::ifstream file(entry.path(),std::ios::binary);
        require(bool(file.read(reinterpret_cast<char*>(bytes.data()),bytes.size())),"Cannot read ROM");
        const int chip=native_hall::import::rom_chip(bytes.data(),bytes.size());
        if(chip<0)continue;
        engine.load(bytes.data(),bytes.size(),chip==4?0x8000:unsigned(chip)*2048);loaded|=1u<<chip;
    }
    require(loaded==31,"All five original Lexicon 224 v4.4 ROMs are required (validated by SHA-256)");
}
inline unsigned word(const lexicon224x::cpu::Host& host,unsigned at) {
    return unsigned(host.memory[at])|unsigned(host.memory[at+1])<<8;
}
inline void wait(lexicon224x::cpu::Host& host,double ms) {
    host.run_until(host.cycles+uint64_t(ms*2048));
}
inline void button(lexicon224x::cpu::Host& host,unsigned bank,unsigned mask) {
    host.panel.switches[bank]=uint8_t(~mask);wait(host,100);
    host.panel.switches[bank]=255;wait(host,40);
}
inline void compile_controls(lexicon224x::cpu::Host& h,unsigned program,const native_hall::Controls& c) {
    auto& ram=h.memory;
    ram[0x3f66]=uint8_t(c.mid);ram[0x3f67]=uint8_t(c.bass);
    ram[0x3f68]=uint8_t(c.crossover);ram[0x3f69]=uint8_t(c.treble);ram[0x3f6a]=uint8_t(c.depth);
    unsigned raw=std::min(255,(c.depth*256+71)/72);
    if(c.depth==21 && (native_hall::program_networks[program]==0 || native_hall::program_networks[program]==3))raw=78;
    ram[0x3f6b]=uint8_t(raw*3);ram[0x3f6c]=uint8_t(raw*3>>8);
    ram[0x3f6d]=uint8_t(c.predelay_ms);ram[0x3f6e]=uint8_t(c.diffusion);
    ram[0x3e34]=ram[0x3e35]=0;
    std::fill(ram.begin()+0x3f56,ram.begin()+0x3f64,255);
    h.panel.pots[0]=uint8_t(c.bass*8+4);
    for(unsigned i=0;i<6;++i)ram[0x3f20+i]=h.panel.pots[i];
    std::fill(ram.begin()+0x3f29,ram.begin()+0x3f32,0);
    ram[0x3f20]=255;ram[0x3f2b]=128;
    wait(h,35);unsigned attempts=0;
    while(ram[0x3e2c] || word(h,0x3e62)!=word(h,0x3e64)) {
        require(++attempts<600,"ROM predelay did not settle");wait(h,10);
    }
    wait(h,5);
}
inline std::array<int8_t,100> coefficients(const lexicon224x::cpu::Host& h) {
    std::array<int8_t,100> result{};
    for(unsigned row=0;row<100;++row) {
        const auto m=lexicon224x::decode(h.dsp->wcs[row],lexicon224x::Model::Lexicon224);
        result[row]=int8_t(m.negative?-int(m.coefficient):int(m.coefficient));
    }
    return result;
}
inline std::array<uint16_t,100> offsets(const lexicon224x::cpu::Host& h) {
    std::array<uint16_t,100> result{};
    for(unsigned row=0;row<100;++row)
        result[row]=uint16_t(~lexicon224x::decode(h.dsp->wcs[row],lexicon224x::Model::Lexicon224).low)&0x3fff;
    return result;
}
inline native_hall::ModulationState modulation(const lexicon224x::cpu::Host& h) {
    native_hall::ModulationState state;
    const unsigned count=h.memory[0x3e29]&15;
    require(count<=2,"Unsupported modulation descriptor count");
    std::copy_n(h.memory.begin()+word(h,0x3e27),5*count,state.descriptors.begin());
    state.index=uint16_t(word(h,0x3e69));state.divider=h.memory[0x3e66];
    state.random_divider=h.memory[0x3e67];state.hold=h.memory[0x3e68];
    auto c=coefficients(h);auto o=offsets(h);
    for(unsigned i=0;i<count;++i) {
        unsigned a=state.descriptors[5*i]|unsigned(state.descriptors[5*i+1])<<8;
        const unsigned row=127-(a-0x4000)/4;
        for(unsigned j=0;j<2;++j) {state.coefficients[2*i+j]=c[row+j];state.offsets[2*i+j]=o[row+j];}
    }
    return state;
}
inline bool equal(const native_hall::ModulationState& a,const native_hall::ModulationState& b) {
    return a.descriptors==b.descriptors && a.coefficients==b.coefficients && a.offsets==b.offsets &&
           a.index==b.index && a.divider==b.divider && a.random_divider==b.random_divider && a.hold==b.hold;
}
} // namespace sound_validation
