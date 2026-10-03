// Shared offline preparation. Never called by the audio processor callback.
#include "bank_import.hpp"
#include "../core/hall.hpp"
#include <juce-plugin/source/roms/sha256.hpp>
#include <emulator/host.hpp>
#include <isa-level-cpp/wcs_disassembler.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <stdexcept>
using namespace lexicon224x;
using namespace native_hall;
using Coefficients=std::array<int8_t,100>;
using Offsets=std::array<uint16_t,100>;
static void check(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
template<unsigned V,unsigned C,unsigned R>
void compact(CoefficientTable<V,C,R>& table,const std::vector<Coefficients>& samples) {
    check(samples.size()==V,"wrong table sample count");
    for(unsigned r=0;r<100;++r) {
        bool changed=false;
        for(unsigned i=1;i<V;++i) changed|=samples[i][r]!=samples[0][r];
        if(!changed) continue;
        check(table.count<R,"too many coefficient rows");
        unsigned column=0;
        for(;column<table.columns;++column) {
            bool equal=true;
            for(unsigned i=0;i<V;++i) if(table.values[i][column]!=samples[i][r]) {equal=false;break;}
            if(equal) break;
        }
        if(column==table.columns) {
            check(column<C,"too many coefficient columns in "+std::to_string(V)+"-entry table");++table.columns;
            for(unsigned i=0;i<V;++i) table.values[i][column]=samples[i][r];
        }
        table.rows[table.count]=uint8_t(r);table.column[table.count++]=uint8_t(column);
    }
}

namespace native_hall::import {
int rom_chip(const uint8_t* data,size_t size) {
    if(size!=2048 || !data) return -1;
    const auto digest=lexplug::roms::Sha256::of(data,size);
    for(unsigned chip=0;chip<rom_hashes.size();++chip)
        if(digest==rom_hashes[chip]) return int(chip);
    return -1;
}
std::unique_ptr<ProgramBank> prepare_bank(const RomSet& roms,const Callbacks& callbacks) {
    auto host=std::make_unique<cpu::Host>(Model::Lexicon224);auto& h=*host;
    for(unsigned chip=0;chip<roms.size();++chip) {
        check(rom_chip(roms[chip].data(),roms[chip].size())==int(chip),
            "Only original Lexicon 224 v4.4 ROM1-ROM5 are supported; 224X/224XL are not supported.");
        std::copy(roms[chip].begin(),roms[chip].end(),h.memory.begin()+(chip==4?0x8000:chip*2048));
    }
    double progress=0;
    const char* stage="Starting original 224 v4.4 firmware";
    auto report=[&] {
        if(callbacks.progress && !callbacks.progress(progress,stage))
            throw std::runtime_error("Import cancelled.");
    };
    auto wait=[&](double ms) {
        const auto deadline=h.cycles+uint64_t(ms*2048);
        while(h.cycles<deadline) {
            report();
            h.run_until(std::min(deadline,h.cycles+uint64_t(40960)));
        }
    };
    auto button=[&](unsigned b,unsigned mask){h.panel.switches[b]=uint8_t(~mask);wait(100);h.panel.switches[b]=255;wait(40);};
    auto word=[&](unsigned a){return unsigned(h.memory[a])|unsigned(h.memory[a+1])<<8;};
    auto capture=[&]{Coefficients c;for(unsigned r=0;r<100;++r) {auto m=decode(h.dsp->wcs[r],Model::Lexicon224);c[r]=int8_t(m.negative?-int(m.coefficient):int(m.coefficient));}return c;};
    auto offsets=[&]{Offsets o;for(unsigned r=0;r<100;++r) o[r]=uint16_t(~decode(h.dsp->wcs[r],Model::Lexicon224).low)&0x3fff;return o;};
    auto bank=std::make_unique<ProgramBank>();std::copy_n(h.memory.begin(),4096,bank->modulation_sequence.begin());
    wait(9000);button(1,4);
    for(unsigned index=0;index<program_count;++index) {
        stage=program_names[index];progress=double(index)/program_count;report();
        unsigned prepared_controls=0;
        auto& p=bank->programs[index];p.identity=program_identities[index];p.network=index==1 || index==4?1:index==3?2:index==5?3:0;
        button(0,p.identity);wait(400);check((h.memory[0x3f65]&63)==p.identity,"program selection failed");
        if(h.memory[0x3f65]&64) button(0,64);
        if(h.memory[0x3f65]&128) button(0,128);
        wait(300);
        auto compile=[&](const Controls& c,unsigned predelay_index=0) {
            progress=(index+std::min(0.98,double(++prepared_controls)/1600))/program_count;
            h.memory[0x3f66]=uint8_t(c.mid);h.memory[0x3f67]=uint8_t(c.bass);
            h.memory[0x3f68]=uint8_t(c.crossover);h.memory[0x3f69]=uint8_t(c.treble);h.memory[0x3f6a]=uint8_t(c.depth);
            unsigned raw=std::min(255,(c.depth*256+71)/72);
            if(c.depth==21 && (p.network==0 || p.network==3)) raw=78;
            h.memory[0x3f6b]=uint8_t(raw*3);h.memory[0x3f6c]=uint8_t(raw*3>>8);
            h.memory[0x3f6d]=uint8_t(predelay_index+predelay_minima[index]);h.memory[0x3f6e]=uint8_t(c.diffusion);
            h.memory[0x3e34]=h.memory[0x3e35]=0;std::fill(h.memory.begin()+0x3f56,h.memory.begin()+0x3f64,0xff);
            // Freeze the accepted ADC readings so later panel scans cannot
            // softly pick up another pot during the pre-delay retiming ramp.
            h.panel.pots[0]=uint8_t(c.bass*8+4);
            for(unsigned i=0;i<6;++i) h.memory[0x3f20+i]=h.panel.pots[i];
            std::fill(h.memory.begin()+0x3f29,h.memory.begin()+0x3f32,0);
            h.memory[0x3f20]=255;h.memory[0x3f2b]=0x80;
            wait(35);
            unsigned settle=0;
            while(h.memory[0x3e2c] || word(0x3e62)!=word(0x3e64)) {
                wait(10);check(++settle<600,"pre-delay failed to settle");
            }
            wait(5); // Let deferred WCS bus writes pass the protected DSP rows.
        };
        Controls base;base.mode_enhancement=base.decay_optimization=false;base.predelay_ms=predelay_minima[index];compile(base);
        p.coefficients=capture();p.offsets=offsets();
        if(!callbacks.capture_prefix.empty()) {
            std::ofstream listing(callbacks.capture_prefix+"."+std::to_string(index)+".txt");
            listing<<lens::listing(h.dsp->wcs,Model::Lexicon224);
            std::ofstream image(callbacks.capture_prefix+"."+std::to_string(index)+".wcs",std::ios::binary);
            for(unsigned a=0x4000;a<0x4200;++a) image.put(char(h.peek(uint16_t(a))));
        }
        p.loop_count=h.memory[0x3e23];check(p.loop_count<=4,"unsupported loop diffusion descriptors");
        for(unsigned i=0;i<p.loop_count;++i) {
            unsigned a=word(word(0x3e21)+i*3);p.loop_rows[i]=uint8_t(127-(a-0x4000)/4);
        }
        p.modulation_flags=h.memory[0x3e29];check((p.modulation_flags&15)<=2,"unsupported modulation descriptors");
        std::copy_n(h.memory.begin()+word(0x3e27),(p.modulation_flags&15)*5,p.modulation_descriptors.begin());
        p.modulation_index=uint16_t(word(0x3e69));p.modulation_period=h.memory[0x3f72];p.modulation_hold=h.memory[0x3f73];
        p.modulation_step=h.memory[0x3f74];p.modulation_mask=h.memory[0x3f75];p.decay_amount=h.memory[0x3f71];
        p.initial_mod_divider=h.memory[0x3e66];p.initial_random_divider=h.memory[0x3e67];p.initial_random_hold=h.memory[0x3e68];
        std::memcpy(&p.initial_decay,h.memory.data()+0x3e32,sizeof p.initial_decay);p.initial_decay.amount=0;
        std::vector<Coefficients> samples;samples.reserve(1024);
        for(int b=0;b<32;++b) for(int m=0;m<32;++m) {auto c=base;c.bass=b;c.mid=m;compile(c);samples.push_back(capture());}
        compact(p.tail,samples);
        auto sweep=[&](auto& table,int Controls::* member,int count) {
            samples.clear();for(int i=0;i<count;++i) {auto c=base;c.*member=member==&Controls::diffusion?std::max(1,i):i;compile(c);samples.push_back(capture());}compact(table,samples);
        };
        sweep(p.crossover,&Controls::crossover,32);sweep(p.treble,&Controls::treble,32);
        sweep(p.depth,&Controls::depth,72);sweep(p.diffusion,&Controls::diffusion,64);
        std::vector<Offsets> delays;
        for(unsigned i=0;i<129;++i) {compile(base,i);delays.push_back(offsets());}
        for(unsigned r=0;r<100;++r) {
            bool changed=false;for(unsigned i=1;i<129;++i) changed|=delays[i][r]!=delays[0][r];
            if(!changed) continue;check(p.predelay_count<4,"too many pre-delay rows");
            unsigned j=p.predelay_count++;p.predelay_rows[j]=uint8_t(r);
            for(unsigned i=0;i<129;++i) p.predelay[i][j]=delays[i][r];
        }
        // Combine all controls, then compare every coefficient and offset with
        // ROM. This catches hidden interactions missed by independent sweeps.
        auto native=std::make_unique<Hall>();native->prepare(*bank,index);
        for(unsigned n=0;n<96;++n) {
            auto c=base;c.bass=1+(n*7)%31;c.mid=(n*11)%32;c.crossover=(n*13)%32;c.treble=(n*17)%32;
            c.depth=(n*19)%72;c.diffusion=1+(n*23)%63;unsigned delay=(n*29)%129;
            c.predelay_ms=int(delay)+predelay_minima[index];compile(c,delay);native->set_controls(c);
            auto wanted=capture();auto wanted_offsets=offsets();
            for(unsigned r=0;r<100;++r) check(native->coefficients()[r]==wanted[r] && native->offsets()[r]==wanted_offsets[r],
                "control composition mismatch: program "+std::to_string(p.identity)+" set "+std::to_string(n)+" row "+std::to_string(r)+" coefficients="+std::to_string(native->coefficients()[r])+"/"+std::to_string(wanted[r])+" offsets="+std::to_string(native->offsets()[r])+"/"+std::to_string(wanted_offsets[r])+" pd rows="+std::to_string(p.predelay_count)+" RAM pd="+std::to_string(h.memory[0x3f6d])+" request="+std::to_string(c.predelay_ms)+" target="+std::to_string(word(0x3e62))+" cached="+std::to_string(word(0x3e64))+" meta="+std::to_string(h.peek(0x4003)));
        }
        if(callbacks.log) *callbacks.log<<program_names[index]<<": 96 composed controls exact; tail "<<unsigned(p.tail.count)<<" rows/"<<unsigned(p.tail.columns)<<" columns; predelay "<<unsigned(p.predelay_count)<<" rows"<<std::endl;
        compile(base);native->prepare(*bank,index);native->set_controls(base);
        // Compare the modulation step law at the actual firmware routine clock.
        unsigned comparisons=0;h.memory[0x3f65]=h.memory[0x3f55]=uint8_t(p.identity|64);
        h.pc_watches[0xc7c]=true;
        h.pc_observer=[&](uint64_t,cpu::CpuSnapshot cpu) {
            if(cpu.pc!=0xc7c) return;
            if(comparisons) for(unsigned i=0;i<(p.modulation_flags&15);++i) {
                unsigned a=unsigned(p.modulation_descriptors[i*5])|unsigned(p.modulation_descriptors[i*5+1])<<8;
                unsigned row=127-(a-0x4000)/4;auto current=capture();auto addresses=offsets();
                for(unsigned r:{row,row+1}) check(native->coefficients()[r]==current[r] && native->offsets()[r]==addresses[r],"modulation mismatch program "+std::to_string(p.identity)+" update "+std::to_string(comparisons)+" row "+std::to_string(r));
            }
            native->advance_modulation();++comparisons;h.pc_watches[0xc7c]=true;
        };
        wait(2000);h.pc_observer={};h.pc_watches[0xc7c]=false;
        if(callbacks.log) *callbacks.log<<"  Modulation: "<<comparisons-1<<" ROM-clocked updates exact"<<std::endl;
        compile(base);
        // Check the shared level controller and each program's own AP targets.
        unsigned decay_checks=0;bool first=true,pending=false;
        auto optimized=base;optimized.decay_optimization=true;native->set_controls(optimized);
        h.memory[0x3f65]=h.memory[0x3f55]=uint8_t(p.identity|128);
        h.pc_watches[0x0779]=h.pc_watches[0x0228]=true;
        h.pc_observer=[&](uint64_t,cpu::CpuSnapshot cpu) {
            h.pc_watches[cpu.pc]=true;
            if(cpu.pc==0x0779) {
                if(first) {DecayState state;std::memcpy(&state,h.memory.data()+0x3e32,sizeof state);native->restore_decay(state);first=false;}
                native->advance_decay(DecayController::level_from_word(cpu.hl));pending=true;
            }
            if(cpu.pc==0x0228 && pending) {
                pending=false;++decay_checks;
                check(!std::memcmp(&native->decay_state(),h.memory.data()+0x3e32,sizeof(DecayState)),"decay state differs program "+std::to_string(p.identity));
                auto current=capture();for(unsigned i=0;i<p.loop_count;++i) for(unsigned j=0;j<3;++j) {
                    unsigned r=p.loop_rows[i]+j;check(native->coefficients()[r]==current[r],"decay AP differs program "+std::to_string(p.identity)+" row "+std::to_string(r));
                }
            }
        };
        for(unsigned n=0;n<32;++n) {
            unsigned amplitude=n%16<4?800:n%16==9?2047:0;
            h.set_audio(amplitude&4095,0,unsigned(-int(amplitude))&4095,0);
            unsigned detectors=amplitude==2047?31:amplitude?7:0;
            h.set_level_detectors(0,detectors);h.set_level_detectors(1,detectors);wait(150);
        }
        h.pc_observer={};h.pc_watches[0x0779]=h.pc_watches[0x0228]=false;
        if(callbacks.log) *callbacks.log<<"  Decay: "<<decay_checks<<" ROM-clocked states and AP updates exact"<<std::endl;
        h.memory[0x3f65]=h.memory[0x3f55]=p.identity;compile(base);
        h.set_audio(0,0,0,0);h.set_level_detectors(0,0);h.set_level_detectors(1,0);
        for(unsigned mode=0;mode<4;++mode) {
            h.memory[0x3f65]=h.memory[0x3f55]=uint8_t(p.identity|((mode&1)?64:0)|((mode&2)?128:0));wait(300);
            unsigned count=0,mod_count=0;uint64_t first=0,last=0,mod_first=0,mod_last=0;
            h.pc_watches[0x0755]=h.pc_watches[0xc7c]=true;
            h.pc_observer=[&](uint64_t cycles,cpu::CpuSnapshot cpu) {
                h.pc_watches[cpu.pc]=true;
                if(cpu.pc==0x0755) {if(!count++)first=cycles;last=cycles;}
                if(cpu.pc==0xc7c) {if(!mod_count++)mod_first=cycles;mod_last=cycles;}
            };
            wait(1500);h.pc_observer={};h.pc_watches[0x0755]=h.pc_watches[0xc7c]=false;
            check(count>20 && last>first && mod_count>100 && mod_last>mod_first,"controller clock measurement failed");
            p.level_rate_tenths[mode]=uint16_t(std::lround((count-1)*20480000.0/double(last-first)));
            p.modulation_rate_tenths[mode]=uint16_t(std::lround((mod_count-1)*20480000.0/double(mod_last-mod_first)));
        }
        if(callbacks.log) *callbacks.log<<"  Nominal modulation Hz (Mod, Mod+Opt): "<<p.modulation_rate_tenths[1]/10.0<<", "<<p.modulation_rate_tenths[3]/10.0<<std::endl;
        check(p.valid(index),"invalid extracted profile "+std::to_string(p.identity));
    }
    if(callbacks.progress && !callbacks.progress(1.0,"Saving prepared bank"))
        throw std::runtime_error("Import cancelled.");
    return bank;
}
} // namespace native_hall::import
