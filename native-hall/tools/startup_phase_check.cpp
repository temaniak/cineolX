#include "reference224.hpp"
#include <iostream>

int main(int argc,char** argv) try {
    using namespace native_hall;using namespace sound_validation;
    require(argc==4,"usage: native_224_startup_phase_check ROM_DIRECTORY BANK OUTPUT_CSV");
    auto bank=read_bank(argv[2]);auto reference=std::make_unique<lexplug::Engine>(1);
    reference->host().audio_observer={};load_roms(*reference,argv[1]);auto& h=reference->host();
    wait(h,9000);
    std::cout<<"Settled power-up: index="<<word(h,0x3e69)<<", counters="<<unsigned(h.memory[0x3e66])
        <<'/'<<unsigned(h.memory[0x3e67])<<'/'<<unsigned(h.memory[0x3e68])<<'\n';
    button(h,1,4);
    std::ofstream csv(argv[3]);require(bool(csv),"Cannot write CSV");
    csv<<"repeat,program,source,index,divider,random_divider,hold,tap,cap,cached,fraction,coefficient_first,coefficient_second,offset_first,offset_second\n";
    auto record=[&](unsigned repeat,unsigned program,const char* source,const ModulationState& s) {
        for(unsigned tap=0;tap<(bank->programs[program].modulation_flags&15);++tap)
            csv<<repeat<<','<<program<<','<<source<<','<<s.index<<','<<unsigned(s.divider)<<','
                <<unsigned(s.random_divider)<<','<<unsigned(s.hold)<<','<<tap<<','
                <<unsigned(s.descriptors[5*tap+2])<<','<<unsigned(s.descriptors[5*tap+3])<<','
                <<unsigned(s.descriptors[5*tap+4])<<','<<int(s.coefficients[2*tap])<<','
                <<int(s.coefficients[2*tap+1])<<','<<s.offsets[2*tap]<<','<<s.offsets[2*tap+1]<<'\n';
    };
    // Suppress enabled modulation before its first instruction. The program
    // compiler itself runs normally, including its WCS/descriptor writes.
    h.pc_watches[0xc7c]=true;
    h.pc_observer=[&](uint64_t,lexicon224x::cpu::CpuSnapshot cpu) {
        if(cpu.pc==0xc7c) {
            h.memory[0x3f65]&=63;h.memory[0x3f55]&=63;h.pc_watches[cpu.pc]=true;
        }
    };
    std::array<ModulationState,program_count> seeds{};unsigned control_checks=0,steps=0;
    for(unsigned repeat=0;repeat<2;++repeat)for(unsigned n=0;n<program_count;++n) {
        const unsigned program=repeat?program_count-1-n:n;
        const uint8_t divider=repeat?11:7,random_divider=repeat?5:3,hold=repeat?7:9;
        h.memory[0x3e66]=divider;h.memory[0x3e67]=random_divider;h.memory[0x3e68]=hold;
        h.memory[0x3e69]=210;h.memory[0x3e6a]=4;
        button(h,0,program_identities[program]);wait(h,400);
        require((h.memory[0x3f65]&63)==program_identities[program],"Program selection failed");
        Controls c;c.mode_enhancement=c.decay_optimization=false;c.predelay_ms=predelay_minima[program];
        compile_controls(h,program,c);
        const auto loaded=modulation(h);
        require(loaded.index==4 && loaded.divider==divider && loaded.random_divider==random_divider && loaded.hold==hold,
                "Program compiler index/counter retention mismatch");
        if(!repeat)seeds[program]=loaded;
        else require(loaded.descriptors==seeds[program].descriptors && loaded.coefficients==seeds[program].coefficients &&
                     loaded.offsets==seeds[program].offsets,"Program compiler tap seed depends on the preceding program");
        auto native=std::make_unique<DesktopHall>();native->prepare(*bank,program);native->set_controls(c);
        const auto captured=native->modulation_state();
        require(captured.index==loaded.index && captured.descriptors==loaded.descriptors &&
                captured.coefficients==loaded.coefficients && captured.offsets==loaded.offsets,
                "Prepared bank differs from compiler tap seed");
        record(repeat,program,"compiler",loaded);record(repeat,program,"bank",native->modulation_state());
        for(unsigned depth:{0u,7u,21u,35u,54u,71u}) {
            c.depth=int(depth);compile_controls(h,program,c);native->set_controls(c);
            const auto state=modulation(h),current=native->modulation_state();
            require(state.descriptors==current.descriptors && state.coefficients==current.coefficients && state.offsets==current.offsets,
                    "Control compilation changed the program's interpolation seed");++control_checks;
        }
        // Start both step laws from the compiler seed. Only the preserved
        // global counters come from the preceding program; no tap snapshot
        // is copied into the native engine. Compare every first-call state.
        auto start=native->modulation_state();start.divider=divider;start.random_divider=random_divider;start.hold=hold;
        native->restore_modulation(start);
        h.memory[0x3f65]=h.memory[0x3f55]=uint8_t(program_identities[program]|64);
        unsigned calls=0;
        h.pc_observer=[&](uint64_t,lexicon224x::cpu::CpuSnapshot cpu) {
            if(cpu.pc!=0xc7c)return;h.pc_watches[cpu.pc]=true;
            require(equal(native->modulation_state(),modulation(h)),"Modulation diverged from the compiler seed");
            native->advance_modulation();++calls;
        };
        wait(h,200);require(calls>100,"Too few startup modulation calls");steps+=calls;
        h.pc_observer=[&](uint64_t,lexicon224x::cpu::CpuSnapshot cpu) {
            if(cpu.pc==0xc7c){h.memory[0x3f65]&=63;h.memory[0x3f55]&=63;h.pc_watches[cpu.pc]=true;}
        };
        // Freeze now, so counter sentinels for the next load cannot be
        // consumed by a remaining routine from the preceding program.
        h.memory[0x3f65]&=63;h.memory[0x3f55]&=63;wait(h,5);
        std::cout<<program_names[program]<<": bank/compiler seed exact; "<<calls<<" initial calls exact\n";
    }
    unsigned switches=0;
    for(unsigned from=0;from<program_count;++from)for(unsigned to=0;to<program_count;++to) {
        if(from==to)continue;
        button(h,0,program_identities[from]);wait(h,400);
        auto native=std::make_unique<DesktopHall>();native->prepare(*bank,from);
        auto before=native->modulation_state();before.index=177;before.divider=11;before.random_divider=5;before.hold=7;
        native->restore_modulation(before);
        h.memory[0x3e66]=11;h.memory[0x3e67]=5;h.memory[0x3e68]=7;h.memory[0x3e69]=177;h.memory[0x3e6a]=0;
        button(h,0,program_identities[to]);wait(h,400);
        require((h.memory[0x3f65]&63)==program_identities[to],"Direct program selection failed");
        Controls controls;controls.mode_enhancement=controls.decay_optimization=false;controls.predelay_ms=predelay_minima[to];
        compile_controls(h,to,controls);native->select_program(to);native->set_controls(controls);
        require(equal(native->modulation_state(),modulation(h)),"Direct native program switch differs from compiler seed/counters");
        record(from,to,"direct_switch",native->modulation_state());++switches;
    }
    require(bool(csv),"Cannot write CSV");
    std::cout<<"12 program loads: tap seeds repeat exactly; index resets to four; all three counters retained\n";
    std::cout<<control_checks<<" depth compositions; "<<steps<<" startup modulation calls exact\n";
    std::cout<<switches<<" directed native program switches: complete load modulation state exact\n";
    h.pc_observer={};h.pc_watches.fill(false);
    unsigned mode_compilations=0;
    for(unsigned program=0;program<program_count;++program)for(unsigned mode=0;mode<4;++mode)for(unsigned flag:{64u,128u}) {
        button(h,0,program_identities[program]);wait(h,400);
        Controls controls;controls.predelay_ms=predelay_minima[program];controls.mode_enhancement=controls.decay_optimization=false;
        h.memory[0x3f65]=h.memory[0x3f55]=program_identities[program];compile_controls(h,program,controls);
        h.memory[0x3f65]=h.memory[0x3f55]=uint8_t(program_identities[program]|((mode&1)?64:0)|((mode&2)?128:0));
        wait(h,100);
        const auto previous=modulation(h);
        std::array<uint32_t,100> graph;std::copy_n(h.dsp->wcs,graph.size(),graph.begin());
        const uint8_t expected=uint8_t(h.memory[0x3f65]^flag);
        bool compiled=false,captured=false;std::array<uint8_t,3> counters{};ModulationState loaded;
        h.wcs_observer=[&](const lexicon224x::cpu::WcsWrite& write) {
            // Controller procedures never write the two graph instruction
            // lanes. Their first write identifies a real program recompile.
            if(!compiled && ((write.address&3)==1 || (write.address&3)==2)) {
                compiled=true;counters={{h.memory[0x3e66],h.memory[0x3e67],h.memory[0x3e68]}};
            }
        };
        h.pc_watches[0xc7c]=true;
        h.pc_observer=[&](uint64_t,lexicon224x::cpu::CpuSnapshot cpu) {
            if(cpu.pc!=0xc7c)return;h.pc_watches[cpu.pc]=true;
            if(!compiled || captured)return;
            require(h.memory[0x3f65]==expected,"Mode key did not produce the expected flags");
            loaded=modulation(h);captured=true;
            // Freeze subsequent modulation, after observing its first seed.
            h.memory[0x3f65]&=~64;h.memory[0x3f55]&=~64;
        };
        button(h,0,flag);wait(h,40);h.wcs_observer={};h.pc_observer={};h.pc_watches[0xc7c]=false;
        constexpr uint32_t control_data=0xfc000000u|0x800000u|0x3fffu;
        for(unsigned row=0;row<100;++row)require((graph[row]&~control_data)==(h.dsp->wcs[row]&~control_data),
                "Mode key changed the native graph's instruction fields");
        require(compiled==(flag==64),"Unexpected program recompilation policy for a mode key");
        if(!compiled) {
            require(h.memory[0x3f65]==expected,"Decay Opt key did not produce the expected flags");
            if(!(mode&1))require(equal(previous,modulation(h)),"Decay Opt changed a frozen modulation phase");
            auto native=std::make_unique<DesktopHall>();native->prepare(*bank,program);
            controls.mode_enhancement=bool(mode&1);controls.decay_optimization=bool(mode&2);native->set_controls(controls);
            native->restore_modulation(previous);controls.decay_optimization=!controls.decay_optimization;native->set_controls(controls);
            require(equal(previous,native->modulation_state()),"Native Decay Opt transition reset a modulation phase");
            continue;
        }
        require(captured,"Mode recompile had no following modulation entry");
        const auto& seed=seeds[program];
        require(loaded.index==4 && loaded.descriptors==seed.descriptors && loaded.coefficients==seed.coefficients && loaded.offsets==seed.offsets,
                "Mode compiler did not restore the program tap seed");
        require(loaded.divider==counters[0] && loaded.random_divider==counters[1] && loaded.hold==counters[2],
                "Mode compiler changed global modulation counters");
        auto native=std::make_unique<DesktopHall>();native->prepare(*bank,program);
        controls.mode_enhancement=bool(mode&1);controls.decay_optimization=bool(mode&2);native->set_controls(controls);
        auto before=previous;before.divider=counters[0];before.random_divider=counters[1];before.hold=counters[2];
        native->restore_modulation(before);controls.mode_enhancement=!controls.mode_enhancement;native->set_controls(controls);
        require(equal(native->modulation_state(),loaded),"Native Mod transition differs from the compiler seed/counters");
        record(mode,program,flag==64?"mod_key":"decay_key",loaded);++mode_compilations;
    }
    require(bool(csv),"Cannot write CSV");
    std::cout<<mode_compilations<<" Mod-key recompiles/native seeds exact; 24 Decay Opt keys avoid program reload\n";
    std::cout<<"4800 mode-key graph instruction rows invariant\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
