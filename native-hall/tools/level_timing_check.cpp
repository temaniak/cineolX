// Offline local level-controller oracle; ROM clocks/states are supplied at
// each entry. The separate scan oracle tests a free-running clock.
#include "reference224.hpp"
#include "../desktop/level_timing224.hpp"
#include <iostream>
#include <cstring>
int main(int argc,char** argv) try {
    using namespace native_hall;using namespace sound_validation;
    require(argc==4,"usage: native_224_level_timing_check ROM_DIRECTORY BANK OUTPUT_CSV");
    std::ofstream csv(argv[3]);require(bool(csv),"Cannot write level CSV");
    csv<<"program,mean,mode,calls,instruction_mismatches,bus_mismatches,write_mismatches,state_mismatches,payload_mismatches\n";
    auto bank=read_bank(argv[2]);
    auto engine=std::make_unique<lexplug::Engine>(1);auto& h=engine->host();
    h.audio_observer={};load_roms(*engine,argv[1]);wait(h,9000);button(h,1,4);
    unsigned total=0,bad_total=0,bus_total=0,event_total=0,state_total=0,payload_total=0;
    for(unsigned p=0;p<6;++p) {
        button(h,0,program_identities[p]);wait(h,400);
        for(unsigned mean:{0u,1u,7u,16u,24u,25u,31u})for(unsigned mode:{0u,1u,2u,3u}) {
            Controls c;c.predelay_ms=predelay_minima[p];c.bass=c.mid=mean;
            c.mode_enhancement=c.decay_optimization=false;
            h.memory[0x3f65]=h.memory[0x3f55]=program_identities[p];compile_controls(h,p,c);
            h.memory[0x3f65]=h.memory[0x3f55]=uint8_t(program_identities[p]|((mode&1)?64:0)|((mode&2)?128:0));
            // Start loud, then let the held/history/amount branches evolve.
            h.set_audio(2047,0,2047,0);h.set_level_detectors(0,31);h.set_level_detectors(1,31);wait(h,100);
            unsigned checks=0,bad=0,bus_bad=0,event_bad=0,state_bad=0;
            uint64_t start=0,last=0;unsigned previous=0,waits=0,expected=0,bus_expected=0,observed=0;
            bool active=false,initialized=false;WcsTiming224 bus;DecayState initial{};DecayController controller;
            std::array<std::pair<uint64_t,unsigned>,12> predicted{};unsigned predicted_count=0;
            std::array<ControlWrite224,12> payloads{};unsigned payload_bad=0;
            uint16_t level_word=0;uint8_t actual_mean=0,period=0;bool display=false,half=false;
            for(unsigned pc=0x755;pc<=0x8c8;++pc)h.pc_watches[pc]=true;
            for(unsigned pc=0xe15;pc<=0xf88;++pc)h.pc_watches[pc]=true;
            for(unsigned pc=0x4e2;pc<=0x4ed;++pc)h.pc_watches[pc]=true;
            h.pc_watches[0x228]=true;
            h.pc_observer=[&](uint64_t t,lexicon224x::cpu::CpuSnapshot cpu) {
                h.pc_watches[cpu.pc]=true;
                if(active && previous==0xe3a) {
                    const unsigned d=unsigned(t-last);require(d>=7 && d<=109,"WCS instruction bound changed");waits+=d-7;
                    if(checks>=2 && (observed>=predicted_count || predicted[observed]!=std::pair<uint64_t,unsigned>{last,d})) {
                        if(event_bad++<3)std::cerr<<"event p="<<p<<" mean="<<mean<<" mode="<<mode<<" pc="<<std::hex<<previous<<std::dec<<" got="<<last<<","<<d<<" write_index="<<observed<<" expected_count="<<predicted_count<<'\n';
                    }
                    ++observed;
                }
                if(cpu.pc==0x755) {
                    start=t;active=true;waits=0;
                    std::memcpy(&initial,&h.memory[0x3e32],17);
                    require(h.memory[0x3e23]==4,"Unsupported loop gain count");
                    actual_mean=h.memory[0x3e61];period=h.memory[0x3f71];display=h.memory[0x3f4c]==32;half=h.memory[0x3e2f]&1;require(!half,"Unsupported half-gain context");
                    if(!initialized){bus.reset(program_networks[p],t,h.dsp->pc);initialized=true;}
                } else if(active && cpu.pc==0x779) {
                    require(t-start==373,"Prefix changed");level_word=cpu.hl;
                    expected=level_cycles224(*bank,p,initial,level_word,actual_mean,bool(mode&2),period,display,half,[](unsigned){return 7u;});
                    predicted_count=observed=0;
                    bus_expected=level_cycles224(*bank,p,initial,level_word,actual_mean,bool(mode&2),period,display,half,[&](unsigned at,ControlWrite224 payload){
                        require(predicted_count<12,"More than twelve loop coefficient writes");auto d=bus.write(start+at);require(d>=7 && d<=109,"Unbounded native WCS wait");payloads[predicted_count]=payload;predicted[predicted_count++]={start+at,d};return d;
                    });
                    controller.reset(initial);controller.step(DecayController::level_from_word(level_word),actual_mean,bool(mode&2),period);
                } else if(active && cpu.pc==0x228) {
                    active=false;unsigned duration=unsigned(t-start);
                    if(duration-waits!=expected){if(bad++<4)std::cerr<<"cost p="<<p<<" mean="<<mean<<" mode="<<mode<<" level="<<unsigned(DecayController::level_from_word(level_word))<<" held="<<unsigned(initial.held)<<" amount="<<unsigned(initial.amount)<<" stop="<<unsigned(initial.stopped)<<" peakdiv="<<unsigned(initial.peak_divider)<<" diffdiv="<<unsigned(initial.diffusion_divider)<<" display="<<display<<" got="<<duration-waits<<" expected="<<expected<<" diff="<<int(duration-waits)-int(expected)<<'\n';}
                    if(checks>=2 && duration!=bus_expected)++bus_bad;
                    if(checks>=2 && observed!=predicted_count)++event_bad;
                    if(std::memcmp(&controller.state(),&h.memory[0x3e32],17))++state_bad;
                    ++checks;
                }
                if(active && cpu.pc==0xe1a) require(cpu.hl<0x4000 || cpu.hl>=0x4200,"Expected ordinary ROM coefficient template");
                previous=cpu.pc;last=t;
            };
            h.wcs_observer=[&](const lexicon224x::cpu::WcsWrite& w) {
                if(active && (observed>=predicted_count || !matches(h,w,payloads[observed]))) {
                    if(payload_bad++<3)std::cerr<<"level payload p="<<p<<" mean="<<mean<<" mode="<<mode
                        <<" row="<<(127-(w.address-0x4000)/4)<<" lane="<<(w.address&3)<<" index="<<observed<<'\n';
                }
            };
            wait(h,80);h.set_audio(0,0,0,0);h.set_level_detectors(0,0);h.set_level_detectors(1,0);wait(h,800);
            h.pc_observer={};h.wcs_observer={};h.pc_watches.fill(false);
            total+=checks;bad_total+=bad;bus_total+=bus_bad;event_total+=event_bad;state_total+=state_bad;payload_total+=payload_bad;
            csv<<p<<','<<mean<<','<<mode<<','<<checks<<','<<bad<<','<<bus_bad<<','<<event_bad<<','<<state_bad<<','<<payload_bad<<'\n';csv.flush();
            require(checks>20,"Insufficient level timings");
        }
    }
    std::cout<<"TOTAL "<<total<<" cost "<<bad_total<<" bus "<<bus_total<<" event "<<event_total<<" state "<<state_total<<" payload "<<payload_total<<'\n';
    require(!bad_total && !bus_total && !event_total && !state_total && !payload_total,"Native level timing/payload differs from ROM");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
