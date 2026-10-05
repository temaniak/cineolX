// Offline full-scan oracle. Clock, controller state and stable panel context
// are captured once per case. Every later event is predicted independently.
#include "reference224.hpp"
#include "../desktop/control_scan224.hpp"
#include <iostream>
#include <cstring>
using namespace native_hall;using namespace sound_validation;
struct Signal {
    bool steady=false;unsigned amplitude=0;
    uint64_t first=0;static constexpr uint64_t period=20000;
    static constexpr std::array<unsigned,8> codes{{0,128,1024,2047,0,3968,2048,4095}};
    unsigned code(unsigned channel,uint64_t t) const {if(steady)return amplitude;if(t<first)return 0;return codes[((t-first)/period+channel*3)%codes.size()];}
    unsigned detector(unsigned channel,uint64_t t) const {auto c=code(channel,t);unsigned a=c>=2048?4096-c:c;return a>=2047?31:a>=1024?7:a>=128?1:0;}
    uint16_t word(uint64_t t,uint64_t origin) const {
        // The input read ends after the DSP word becomes visible: a
        // separate execute/capture follows the fetch at rows 0/50.
        const auto row=t-origin-3;const unsigned channel=row%100>=50;
        const auto capture=origin+row/100*100+channel*50;
        const auto c=code(channel,capture-10);return uint16_t(c*16);
    }
    unsigned held(unsigned channel,uint64_t begin,uint64_t end,uint64_t origin) const {
        const uint64_t phase=channel?4:54;
        uint64_t capture=origin+phase+2;
        if(capture<begin)capture+=((begin-capture+99)/100)*100;
        unsigned mask=0;
        for(;capture<=end;capture+=100)mask|=detector(channel,capture-1);
        return mask;
    }
};
int main(int argc,char** argv) try {
    require(argc==4 || argc==5,"usage: native_224_scan_timing_check ROM_DIRECTORY BANK OUTPUT_CSV [steady|dynamic]");
    const bool steady=argc==5 && std::string(argv[4])=="steady";
    require(argc==4 || steady || std::string(argv[4])=="dynamic","Invalid scan fixture");
    std::ofstream csv(argv[3]);require(bool(csv),"Cannot write scan CSV");
    csv<<"program,mode,amplitude,events,clock_mismatches,state_mismatches,byte_mismatches,writes,payload_mismatches,visibility_mismatches\n";
    auto bank=read_bank(argv[2]);auto engine=std::make_unique<lexplug::Engine>(1);auto& h=engine->host();
    h.audio_observer={};load_roms(*engine,argv[1]);wait(h,9000);button(h,1,4);
    struct Event {uint64_t cycle;unsigned pc;ModulationState mod;DecayState decay;uint16_t word;};
    unsigned total=0,clocks=0,states=0,words=0,write_total=0,payload_total=0,visibility_total=0;
    const std::vector<unsigned> amplitudes=steady?std::vector<unsigned>{0,128,2047}:std::vector<unsigned>{0};
    for(unsigned p=0;p<6;++p)for(unsigned mode:{0u,1u,2u,3u})for(unsigned amplitude:amplitudes) {
        h.row_observer={};h.pc_observer={};h.wcs_observer={};h.pc_watches.fill(false);h.set_audio(0,0,0,0);h.set_level_detectors(0,0);h.set_level_detectors(1,0);
        button(h,0,program_identities[p]);wait(h,400);Controls c;c.depth=35;c.bass=3;c.mid=15;c.predelay_ms=predelay_minima[p];c.mode_enhancement=c.decay_optimization=false;
        h.memory[0x3f65]=h.memory[0x3f55]=program_identities[p];compile_controls(h,p,c);
        c.mode_enhancement=mode&1;c.decay_optimization=mode&2;h.memory[0x3f65]=h.memory[0x3f55]=uint8_t(program_identities[p]|((mode&1)?64:0)|((mode&2)?128:0));wait(h,200);
        Signal signal;signal.steady=steady;signal.amplitude=amplitude;
        if(steady){h.set_audio(amplitude,0,amplitude,0);h.set_level_detectors(0,signal.detector(0,0));h.set_level_detectors(1,signal.detector(1,0));wait(h,100);}
        std::vector<Event> events;unsigned index=0,bad=0,state_bad=0,word_bad=0;bool initialized=false;uint64_t origin=0,end=0;
        struct ScheduledWrite {uint64_t visible;ControlWrite224 payload;};
        std::vector<ScheduledWrite> scheduled;unsigned write_index=0,payload_bad=0,visibility_bad=0;
        uint64_t row_clock_offset=0;
        auto native=std::make_unique<Hall>();native->prepare(*bank,p);native->set_controls(c);
        for(unsigned pc:{0xc7cu,0x191u,0x19fu,0x755u,0x779u})h.pc_watches[pc]=true;
        h.pc_observer=[&](uint64_t actual,lexicon224x::cpu::CpuSnapshot cpu) {
            h.pc_watches[cpu.pc]=true;
            if(!initialized) {
                if(cpu.pc!=0xc7c || word(h,cpu.sp)!=0xbb || (cpu.bc>>8)!=0)return;
                initialized=true;origin=actual-h.dsp->pc;signal.first=origin+100020;end=actual+uint64_t(steady?800:2000)*2048;
                // The reference physical row index and CPU-state count have
                // different epochs. Align their next fetch once, alongside
                // the existing initial PC/scan alignment, never per write.
                const auto fetch_base=lexicon224x::cpu::timing_224.first_marker+lexicon224x::cpu::timing_224.fetch_offset;
                const auto next_physical_fetch=(h.now()-fetch_base)/lexicon224x::cpu::timing_224.row+1;
                require(actual>=next_physical_fetch && actual-next_physical_fetch<=2,"Unsupported CPU/fetch epoch");
                row_clock_offset=actual-next_physical_fetch;
                native->restore_modulation(modulation(h));DecayState decay;std::memcpy(&decay,&h.memory[0x3e32],17);native->restore_decay(decay);
                ControlScan224 scan;scan.reset(program_networks[p],actual,h.dsp->pc);std::array<unsigned,9> panels{};
                require(word(h,0x3e5b)==0 && h.memory[0x3e2f]==0,"Unsupported scan entry context");
                require(h.memory[0x3e61]==(c.bass+c.mid)/2 && h.memory[0x3f71]==bank->programs[p].decay_amount,"Native level context differs");
                for(unsigned channel=0;channel<9;++channel) {auto raw=channel<6?h.panel.pots[channel]:h.panel.switches[channel-6];panels[channel]=panel_cycles224(channel,raw,h.memory[0x3f20+channel]);require(panels[channel]!=0,"Panel unstable");}
                scan.set_panel_costs(panels);scan.set_display_check(h.memory[0x3f4c]==32);std::array<uint64_t,2> last{{actual,actual}};
                scan.run_until(end,*bank,p,*native,c,[&](uint64_t t){return signal.word(t,origin);},[&](unsigned ch,uint64_t t){auto mask=signal.held(ch,last[ch],t,origin);last[ch]=t;return mask;},
                    [&](ControlScan224::Event event,uint64_t t,uint16_t value){
                        unsigned pc=event==ControlScan224::Event::Modulation?0xc7c:event==ControlScan224::Event::TransferHigh?0x191:event==ControlScan224::Event::TransferLow?0x19f:event==ControlScan224::Event::Level?0x755:0x779;
                        events.push_back({t,pc,native->modulation_state(),native->decay_state(),value});
                    },[&](uint64_t visible,ControlWrite224 payload){scheduled.push_back({visible,payload});});
            }
            if(index==events.size())return;const auto& e=events[index++];
            if(actual!=e.cycle || cpu.pc!=e.pc){if(bad++<5)std::cerr<<"clock p="<<p<<" mode="<<mode<<" pc="<<std::hex<<cpu.pc<<'/'<<e.pc<<std::dec<<" delta="<<int64_t(actual)-int64_t(e.cycle)<<'\n';}
            if(cpu.pc==0xc7c && (!equal(modulation(h),e.mod) || std::memcmp(&e.decay,&h.memory[0x3e32],17)))++state_bad;
            unsigned actual_word=cpu.pc==0x779?cpu.hl:cpu.a;
            const uint16_t inverted=uint16_t(~e.word);
            unsigned predicted_word=cpu.pc==0x779?e.word:cpu.pc==0x191?uint8_t(inverted>>8):uint8_t(inverted);
            if(cpu.pc!=0xc7c && cpu.pc!=0x755 && actual_word!=predicted_word){if(word_bad++<8)std::cerr<<"word p="<<p<<" mode="<<mode<<" pc="<<std::hex<<cpu.pc<<std::dec<<" got="<<actual_word<<" predicted="<<predicted_word<<" phase="<<(actual-origin)%100<<'\n';}
        };
        h.wcs_observer=[&](const lexicon224x::cpu::WcsWrite& w) {
            if(!initialized)return;
            require(write_index<scheduled.size(),"Unexpected free-running WCS write");
            const auto& predicted=scheduled[write_index++];
            if(!matches(h,w,predicted.payload))++payload_bad;
            const auto base=lexicon224x::cpu::timing_224.first_marker+lexicon224x::cpu::timing_224.fetch_offset;
            const auto period=lexicon224x::cpu::timing_224.row;
            const uint64_t first_fetch=(w.committed_at-base+period-1)/period+row_clock_offset;
            if(first_fetch!=predicted.visible) {
                if(visibility_bad++<3)std::cerr<<"visibility p="<<p<<" mode="<<mode<<" got="<<first_fetch
                    <<" predicted="<<predicted.visible<<'\n';
            }
        };
        for(unsigned attempts=0;attempts<100 && !initialized;++attempts)wait(h,1);
        require(initialized,"No complete scan entry observed within 100 ms");
        h.row_observer=[&](uint64_t,const lexicon224x::Machine&) {
            const uint64_t t=h.cycles;
            h.set_audio(signal.code(0,t),0,signal.code(1,t),0);
            h.set_level_detectors(0,signal.detector(0,t));h.set_level_detectors(1,signal.detector(1,t));
        };
        h.run_until(end);h.row_observer={};h.pc_observer={};h.wcs_observer={};h.pc_watches.fill(false);
        require(index>100,"Insufficient scan predictions");
        csv<<p<<','<<mode<<','<<amplitude<<','<<index<<','<<bad<<','<<state_bad<<','<<word_bad<<','<<write_index<<','<<payload_bad<<','<<visibility_bad<<'\n';csv.flush();total+=index;clocks+=bad;states+=state_bad;words+=word_bad;write_total+=write_index;payload_total+=payload_bad;visibility_total+=visibility_bad;
    }
    std::cout<<"TOTAL "<<total<<" clock "<<clocks<<" state "<<states<<" word "<<words<<" writes "<<write_total
        <<" payload "<<payload_total<<" visibility "<<visibility_total<<'\n';require(!clocks && !states && !words && !payload_total && !visibility_total,"Free-running native scan differs from ROM");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
