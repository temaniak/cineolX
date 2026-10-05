#include "reference224.hpp"
#include <iostream>

int main(int argc,char** argv) try {
    using namespace native_hall;using namespace sound_validation;
    require(argc==4,"usage: native_224_modulation_check ROM_DIRECTORY BANK OUTPUT_CSV");
    auto bank=read_bank(argv[2]);auto reference=std::make_unique<lexplug::Engine>(1);
    reference->host().audio_observer={};load_roms(*reference,argv[1]);auto& h=reference->host();
    wait(h,9000);button(h,1,4);
    std::ofstream csv(argv[3]);require(bool(csv),"Cannot write CSV");
    csv<<"program,depth,mode,amplitude,steps,measured_hz,nominal_hz,rate_error_percent,min_interval_cycles,max_interval_cycles\n";
    unsigned total=0,table_intersections=0;
    // Isolate the program compiler from the enabled modulation routine.
    // These valid counter sentinels are private oracle RAM, not audio data.
    h.memory[0x3e66]=11;h.memory[0x3e67]=5;h.memory[0x3e68]=7;
    h.pc_watches[0xc7c]=true;
    h.pc_observer=[&](uint64_t,lexicon224x::cpu::CpuSnapshot cpu) {
        if(cpu.pc==0xc7c) {
            h.memory[0x3f65]&=63;h.memory[0x3f55]&=63;h.pc_watches[cpu.pc]=true;
        }
    };
    for(unsigned program=0;program<6;++program) {
        button(h,0,program_identities[program]);wait(h,400);
        require(h.memory[0x3e66]==11 && h.memory[0x3e67]==5 && h.memory[0x3e68]==7,
                "ROM program compiler changed global modulation counters");
        require(word(h,0x3e69)==4,"ROM program compiler did not reset random index to four");
    }
    h.pc_observer={};h.pc_watches[0xc7c]=false;
    std::cout<<"Six ROM program compilers: modulation dividers/hold retained; random index reset to four\n";
    for(unsigned program=0;program<6;++program) {
        button(h,0,program_identities[program]);wait(h,400);
        for(unsigned depth:{0u,7u,21u,35u,54u,71u}) {
            Controls c;c.depth=int(depth);c.predelay_ms=predelay_minima[program];
            c.bass=1+(depth*7)%31;c.mid=(depth*11)%32;
            c.mode_enhancement=c.decay_optimization=false;
            h.memory[0x3f65]=h.memory[0x3f55]=program_identities[program];compile_controls(h,program,c);
            auto native=std::make_unique<Hall>();native->prepare(*bank,program);native->set_controls(c);
            const auto cc=coefficients(h);const auto oo=offsets(h);
            const auto& p=bank->programs[program];
            std::array<bool,100> mod_rows{};
            for(unsigned i=0;i<(p.modulation_flags&15);++i) {
                unsigned a=p.modulation_descriptors[5*i]|unsigned(p.modulation_descriptors[5*i+1])<<8;
                unsigned row=127-(a-0x4000)/4;mod_rows[row]=mod_rows[row+1]=true;
            }
            // A program load preserves/freezes the tap phase reached during boot.
            for(unsigned row=0;row<100;++row)if(!mod_rows[row])require(native->coefficients()[row]==cc[row] && native->offsets()[row]==oo[row],
                "Static control composition mismatch program "+std::to_string(program)+" depth "+std::to_string(depth)+" row "+std::to_string(row)+
                " coefficient "+std::to_string(native->coefficients()[row])+"/"+std::to_string(cc[row])+" offset "+std::to_string(native->offsets()[row])+"/"+std::to_string(oo[row]));
            for(unsigned i=0;i<(p.modulation_flags&15);++i) {
                unsigned a=p.modulation_descriptors[5*i]|unsigned(p.modulation_descriptors[5*i+1])<<8;
                unsigned row=127-(a-0x4000)/4;
                for(unsigned j=0;j<p.depth.count;++j)if(p.depth.rows[j]==row || p.depth.rows[j]==row+1)++table_intersections;
            }
            for(unsigned mode:{1u,3u})for(unsigned amplitude:{0u,128u,1024u,2047u}) {
                c.mode_enhancement=true;c.decay_optimization=bool(mode&2);native->set_controls(c);
                h.memory[0x3f65]=h.memory[0x3f55]=uint8_t(program_identities[program]|64|((mode&2)?128:0));
                h.set_audio(amplitude,0,amplitude,0);
                unsigned detector=amplitude==2047?31:amplitude>=1024?7:amplitude?1:0;
                h.set_level_detectors(0,detector);h.set_level_detectors(1,detector);
                unsigned checks=0;uint64_t first=0,last=0,lo=~uint64_t(0),hi=0;
                h.pc_watches[0xc7c]=true;
                h.pc_observer=[&](uint64_t cycles,lexicon224x::cpu::CpuSnapshot cpu) {
                    if(cpu.pc!=0xc7c)return;
                    h.pc_watches[cpu.pc]=true;
                    auto state=modulation(h);
                    if(checks) {
                        require(equal(native->modulation_state(),state),"Full modulation state mismatch: program "+std::to_string(program)+" depth "+std::to_string(depth)+" step "+std::to_string(checks));
                        lo=std::min(lo,cycles-last);hi=std::max(hi,cycles-last);
                    } else {native->restore_modulation(state);first=cycles;}
                    last=cycles;native->advance_modulation();++checks;
                };
                wait(h,600);h.pc_observer={};h.pc_watches[0xc7c]=false;
                require(checks>100,"Insufficient modulation checks");total+=checks-1;
                const double measured=(checks-1)*2048000.0/double(last-first);
                const double nominal=p.modulation_rate_tenths[mode]/10.0;
                csv<<program<<','<<depth<<','<<mode<<','<<amplitude<<','<<checks-1<<','<<measured<<','<<nominal<<','<<100*(nominal/measured-1)<<','<<lo<<','<<hi<<'\n';csv.flush();
            }
        }
        h.set_audio(0,0,0,0);h.set_level_detectors(0,0);h.set_level_detectors(1,0);
        std::cout<<program_names[program]<<": full modulation state exact at ROM call boundaries\n";
    }
    std::cout<<total<<" exact steps; depth/modulation row intersections="<<table_intersections<<'\n';
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
