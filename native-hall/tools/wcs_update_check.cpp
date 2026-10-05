// Offline cause isolation, with reference startup and CPU writes deliberately
// shared. This is not a normal-startup native-engine acceptance test.
#include "reference224.hpp"
#include <iostream>
#include <iomanip>

namespace {
using namespace lexicon224x;
using namespace lexicon224x::cpu;
using namespace native_hall;
using namespace sound_validation;
constexpr uint32_t payload_mask=0xfc800000u|0x3fffu;
using Wcs=std::array<uint32_t,100>;
Wcs snapshot(const Machine& m) {Wcs w{};std::copy_n(m.wcs,100,w.begin());return w;}
uint16_t execute_copy(Machine& m,const Machine& reference,uint32_t instruction) {
    // The immutable word fetched by the reference supplies topology. Input
    // holds and RESET are shared to isolate coefficient/address visibility.
    m.microinstruction=instruction;m.mi=decode(instruction,Model::Lexicon224);
    m.restart=reference.restart;m.fpc.input_sample=reference.fpc.input_sample;
    m.xreg_from_cpu=reference.xreg_from_cpu;
    const auto bus=source_value(m,m.mi);
    std::fill_n(m.operand_held,3,false);execute(m);return bus;
}
struct Difference {
    uint64_t samples=0,different=0;
    double square=0,reference_square=0;
    unsigned peak=0;
    void add(int16_t wanted,int16_t actual) {
        ++samples;different+=wanted!=actual;
        const int delta=int(actual)-int(wanted);peak=std::max(peak,unsigned(std::abs(delta)));
        square+=double(delta)*delta;reference_square+=double(wanted)*wanted;
    }
    double relative_rms() const {return reference_square?100*std::sqrt(square/reference_square):0;}
};
}

int main(int argc,char** argv) try {
    require(argc==3,"usage: native_224_wcs_update_check ROM_DIRECTORY OUTPUT_CSV");
    std::ofstream csv(argv[2]);require(bool(csv),"Cannot write isolation CSV");
    csv<<"program,mode,passes,calls,writes,samples,call_different,call_peak,call_relative_rms_percent,pass_different,pass_peak,pass_relative_rms_percent,exact_mismatches\n";
    auto engine=std::make_unique<lexplug::Engine>(1);auto& h=engine->host();
    h.audio_observer={};load_roms(*engine,argv[1]);wait(h,9000);button(h,1,4);
    for(unsigned program=0;program<program_count;++program)for(unsigned mode:{0u,3u}) {
        h.row_observer={};h.pc_observer={};h.wcs_observer={};h.pc_watches.fill(false);
        h.set_audio(0,0,0,0);h.set_level_detectors(0,0);h.set_level_detectors(1,0);
        button(h,0,program_identities[program]);wait(h,400);
        Controls c;c.bass=3;c.mid=15;c.depth=35;c.predelay_ms=predelay_minima[program];
        c.mode_enhancement=c.decay_optimization=false;
        h.memory[0x3f65]=h.memory[0x3f55]=program_identities[program];compile_controls(h,program,c);
        h.memory[0x3f65]=h.memory[0x3f55]=uint8_t(program_identities[program]|(mode?192:0));wait(h,300);
        auto exact=std::make_unique<Machine>(),at_call=std::make_unique<Machine>(),at_pass=std::make_unique<Machine>();
        Wcs current{},previous{},pass{};Tick changed=0;bool initialized=false,ready=false;
        uint64_t passes=0,calls=0,writes=0,exact_bad=0;uint32_t random=224;
        Difference call_error,pass_error;
        for(unsigned pc:{0xbbu,0x1b5u,0x228u})h.pc_watches[pc]=true;
        h.pc_observer=[&](uint64_t,CpuSnapshot cpu) {
            h.pc_watches[cpu.pc]=true;
            if(cpu.pc==0xbb || cpu.pc==0x1b5 || cpu.pc==0x228) {
                ready=true;if(!initialized)return;
                ++calls;previous=current;current=snapshot(*h.dsp);changed=h.now();
            }
        };
        h.wcs_observer=[&](const WcsWrite& w) {
            if(!initialized)return;
            require(w.writer_pc==0xdd8 || w.writer_pc==0xdd9 || w.writer_pc==0xda5 ||
                w.writer_pc==0xdb7 || w.writer_pc==0xe3a,"Unexpected compiler write in stable-control fixture");
            ++writes;
        };
        h.row_observer=[&](uint64_t row_number,const Machine& reference) {
            const unsigned row=(reference.pc+99)%100;
            if(!initialized) {
                if(!ready || row!=99)return;
                *exact=*at_call=*at_pass=reference;current=previous=pass=snapshot(reference);
                initialized=true;
            } else {
                // If a routine finished between this row's fetch and execute,
                // use the preceding group. Never sample a future CPU update.
                const auto fetch_time=timing_224.marker(row_number)+timing_224.fetch_offset;
                const auto& group=changed>fetch_time?previous:current;
                if(row==0)pass=group;
                const uint32_t topology=reference.microinstruction&~payload_mask;
                const auto wanted=execute_copy(*exact,reference,reference.microinstruction);
                const auto call_bus=execute_copy(*at_call,reference,topology|(group[row]&payload_mask));
                const auto pass_bus=execute_copy(*at_pass,reference,topology|(pass[row]&payload_mask));
                if(exact->ACC!=reference.ACC || exact->RR!=reference.RR || exact->cpc!=reference.cpc ||
                    !std::equal(exact->R,exact->R+4,reference.R))++exact_bad;
                if(reference.mi.wr_da && reference.mi.source==FromRR) {
                    // WR_DA consumes RR before this row executes. Exclude
                    // CPU-fed gain/control words from the audio metric.
                    for(unsigned channel=0;channel<4;++channel)if(reference.mi.channels&(1u<<channel)) {
                        call_error.add(int16_t(wanted),int16_t(call_bus));pass_error.add(int16_t(wanted),int16_t(pass_bus));
                    }
                }
                if(row==99 && !std::equal(exact->memory,exact->memory+16384,reference.memory))++exact_bad;
            }
            if(row==99) {
                ++passes;
                unsigned left=0,right=0;
                if(passes<=10240) {
                    random=random*1664525u+1013904223u;left=(random>>16)&4095;
                    random=random*1664525u+1013904223u;right=(random>>16)&4095;
                }
                h.set_audio(left,0,right,0);
                const auto mask=[](unsigned code){unsigned a=code>=2048?4096-code:code;return a>=1536?31u:a>=768?7u:a?1u:0u;};
                h.set_level_detectors(0,mask(left));h.set_level_detectors(1,mask(right));
            }
        };
        wait(h,3000);h.row_observer={};h.pc_observer={};h.wcs_observer={};h.pc_watches.fill(false);
        std::cout<<"Coverage p="<<program<<" mode="<<mode<<": passes="<<passes<<", calls="<<calls
            <<", writes="<<writes<<", samples="<<call_error.samples<<", exact_bad="<<exact_bad<<'\n'<<std::flush;
        require(initialized && passes>61000 && calls>1000 && call_error.samples>100000,"Insufficient isolation coverage");
        require(!exact_bad,"Immutable fetched-word reconstruction differs from reference DSP");
        if(!mode)require(!call_error.different && !pass_error.different,"Both-off negative control differs");
        if(mode)require(writes>1000,"No modulation/diffusion writes observed");
        csv<<program<<','<<mode<<','<<passes<<','<<calls<<','<<writes<<','<<call_error.samples<<','
            <<call_error.different<<','<<call_error.peak<<','<<std::setprecision(10)<<call_error.relative_rms()<<','
            <<pass_error.different<<','<<pass_error.peak<<','<<pass_error.relative_rms()<<','<<exact_bad<<'\n';csv.flush();
        std::cout<<program_names[program]<<" mode="<<mode<<": calls="<<calls<<", writes="<<writes
            <<", call RMS="<<call_error.relative_rms()<<"%, pass RMS="<<pass_error.relative_rms()
            <<"%, exact mismatches="<<exact_bad<<'\n'<<std::flush;
    }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
