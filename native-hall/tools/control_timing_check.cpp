#include "reference224.hpp"
#include "../desktop/control_timing224.hpp"
#include <iostream>
#include <limits>

// Independent offline timing oracle. The native model sees the state at each
// actual routine entry, not CPU opcodes or the trace's instruction durations.
int main(int argc,char** argv) try {
    using namespace native_hall;using namespace sound_validation;
    require(argc==4,"usage: native_224_control_timing_check ROM_DIRECTORY BANK OUTPUT_CSV");
    auto bank=read_bank(argv[2]);auto reference=std::make_unique<lexplug::Engine>(1);
    auto& h=reference->host();h.audio_observer={};load_roms(*reference,argv[1]);
    wait(h,9000);button(h,1,4);
    std::ofstream csv(argv[3]);require(bool(csv),"Cannot write timing CSV");
    csv<<"program,depth,mode,amplitude,checks,instruction_mismatches,bus_mismatches,write_mismatches,min_cycles,max_cycles,wcs_writes,min_write_cycles,max_write_cycles,payload_mismatches\n";
    unsigned total=0,total_bad=0,total_bus_bad=0,total_write_bad=0,total_writes=0,total_payload_bad=0;
    for(unsigned program=0;program<program_count;++program) {
        button(h,0,program_identities[program]);wait(h,400);
        // Verify the native slot description against the independent graph.
        for(unsigned row=0;row<100;++row) {
            const auto mi=lexicon224x::decode(h.dsp->wcs[row],lexicon224x::Model::Lexicon224);
            const unsigned network=program_networks[program];
            const bool protect=network==1?(row!=46 && row!=69 && row!=93):
                network==2?row!=94:(row!=46 && row!=95);
            require(mi.protect==protect && mi.reset==(row==99),"Unsupported native WCS grant graph");
        }
        for(unsigned depth:{0u,7u,21u,35u,54u,71u}) {
            Controls c;c.depth=int(depth);c.predelay_ms=predelay_minima[program];
            c.bass=1+(depth*7)%31;c.mid=(depth*11)%32;
            c.mode_enhancement=c.decay_optimization=false;
            h.memory[0x3f65]=h.memory[0x3f55]=program_identities[program];
            compile_controls(h,program,c);
            for(unsigned mode:{0u,1u,2u,3u})for(unsigned amplitude:{0u,128u,1024u,2047u}) {
                h.memory[0x3f65]=h.memory[0x3f55]=uint8_t(program_identities[program]|((mode&1)?64:0)|((mode&2)?128:0));
                h.set_audio(amplitude,0,amplitude,0);
                const unsigned detector=amplitude==2047?31:amplitude>=1024?7:amplitude?1:0;
                h.set_level_detectors(0,detector);h.set_level_detectors(1,detector);
                wait(h,100); // Finish control/mode changes before observing a call.
                unsigned checks=0,bad=0,bus_bad=0,write_bad=0,writes=0,expected=0,bus_expected=0,waits=0;
                std::array<std::pair<uint64_t,unsigned>,6> predicted_writes{};
                std::array<ControlWrite224,6> payloads{};unsigned payload_bad=0;
                unsigned predicted_count=0,observed_count=0;
                unsigned previous=0,min_cycles=std::numeric_limits<unsigned>::max(),max_cycles=0;
                unsigned min_write=std::numeric_limits<unsigned>::max(),max_write=0;
                uint64_t start=0,last=0;bool active=false,initialized=false;
                WcsTiming224 bus;
                for(unsigned pc=0xc7c;pc<=0xde5;++pc)h.pc_watches[pc]=true;
                h.pc_watches[0xbb]=h.pc_watches[0x1b5]=true;
                h.pc_observer=[&](uint64_t t,lexicon224x::cpu::CpuSnapshot cpu) {
                    h.pc_watches[cpu.pc]=true;
                    if(active && (previous==0xdd8 || previous==0xdd9 || previous==0xda5 || previous==0xdb7)) {
                        const unsigned duration=unsigned(t-last);
                        require(duration>=7,"Invalid WCS instruction duration");
                        waits+=duration-7;++writes;
                        min_write=std::min(min_write,duration);max_write=std::max(max_write,duration);
                        if(checks>=2 && (observed_count>=predicted_count ||
                            predicted_writes[observed_count].first!=last || predicted_writes[observed_count].second!=duration))++write_bad;
                        ++observed_count;
                    }
                    if(cpu.pc==0xc7c) {
                        const auto state=modulation(h);
                        if(!initialized){bus.reset(program_networks[program],t,h.dsp->pc);initialized=true;}
                        expected=modulation_cycles224(*bank,program,state,bool(mode&1),[](unsigned){return 7u;});
                        predicted_count=observed_count=0;
                        bool bounded=true;
                        bus_expected=modulation_cycles224(*bank,program,state,bool(mode&1),[&](unsigned cycles,ControlWrite224 payload){
                            const unsigned duration=bus.write(t+cycles);
                            bounded=bounded && duration>=7 && duration<=109 && predicted_count<predicted_writes.size();
                            if(predicted_count<predicted_writes.size()) {
                                payloads[predicted_count]=payload;predicted_writes[predicted_count++]={t+cycles,duration};
                            }
                            return duration;
                        });
                        require(bounded,"Unbounded native WCS timing");
                        active=true;start=t;waits=0;
                    } else if(active && (cpu.pc==0xbb || cpu.pc==0x1b5)) {
                        active=false;++checks;const unsigned duration=unsigned(t-start);
                        min_cycles=std::min(min_cycles,duration);max_cycles=std::max(max_cycles,duration);
                        if(duration-waits!=expected)++bad;
                        // Initial pair state may retain an earlier control
                        // write. Two calls cross an unaffected frame reset.
                        if(checks>2 && duration!=bus_expected)++bus_bad;
                        if(checks>2 && observed_count!=predicted_count)++write_bad;
                    }
                    previous=cpu.pc;last=t;
                };
                h.wcs_observer=[&](const lexicon224x::cpu::WcsWrite& w) {
                    if(active && (observed_count>=predicted_count || !matches(h,w,payloads[observed_count]))) {
                        if(payload_bad++<3)std::cerr<<"payload p="<<program<<" depth="<<depth<<" mode="<<mode
                            <<" row="<<(127-(w.address-0x4000)/4)<<" lane="<<(w.address&3)<<" index="<<observed_count<<'\n';
                    }
                };
                wait(h,300);h.pc_observer={};h.wcs_observer={};h.pc_watches.fill(false);
                require(checks>100,"Insufficient complete routine timings");
                csv<<program<<','<<depth<<','<<mode<<','<<amplitude<<','<<checks<<','<<bad<<','<<bus_bad<<','<<write_bad<<','<<min_cycles<<','<<max_cycles<<','<<writes<<','
                   <<(writes?min_write:0)<<','<<max_write<<','<<payload_bad<<'\n';csv.flush();
                if(bad || bus_bad || write_bad)std::cerr<<program_names[program]<<" depth="<<depth<<" mode="<<mode<<" amplitude="<<amplitude<<": instructions="<<bad<<" bus="<<bus_bad<<" writes="<<write_bad<<'\n';
                total+=checks;total_bad+=bad;total_bus_bad+=bus_bad;total_write_bad+=write_bad;total_writes+=writes;total_payload_bad+=payload_bad;
            }
        }
        std::cout<<program_names[program]<<": timing matrix recorded\n";
    }
    std::cout<<total<<" complete calls, "<<total_writes<<" writes, instruction mismatches="<<total_bad<<", bus mismatches="<<total_bus_bad<<", write mismatches="<<total_write_bad<<", payload mismatches="<<total_payload_bad<<'\n';
    require(!total_bad && !total_bus_bad && !total_write_bad && !total_payload_bad,"Native control timing/payload differs from ROM");
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
