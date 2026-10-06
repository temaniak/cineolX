// Local cycle/payload oracle at actual v8.21 entries. It deliberately uses
// observed WCS instruction waits; it does not validate a free-running scan.
#include "xl_reference.hpp"
#include "xl_modulation_cycles.hpp"
#include <cstring>
#include <memory>
using namespace lexplug;using namespace lexplug::op;using xl_test::require;
using namespace cineol::xl;

static Task<void> toggle(Machine& m,LarcOperator& op,bool enabled) {
    for(unsigned n=0;n<8;++n) {
        co_await op.setToggle(1,enabled);co_await m.sleep(0.2);
        if(bool(m.peek(0x3ccd)&64)==enabled) co_return;
    }
    co_await fail("XL timing Mod key did not settle");
}
struct TimingProbe {
    lexicon224x::cpu::Host& h;
    ModulationProfile profile{};ModulationState initial{};
    struct Actual {unsigned at=0,duration=0;xl_test::InterpolationWrite payload{};};
    std::array<Actual,64> writes{};
    unsigned count=0,pending_at=0,pending_index=0;
    unsigned calls=0,write_total=0,cost_errors=0,write_errors=0;
    uint64_t begin=0,interrupt_begin=0,interrupt_states=0;
    unsigned return_pc=0,interrupt_return=0,interrupts=0;
    bool active=false,pending=false,enabled=false,in_interrupt=false,return_was_watched=false,stopping=false;
    unsigned word(unsigned a) const {return h.memory[a]|unsigned(h.memory[a+1])<<8;}
    void snapshot() {
        profile.flags=h.memory[0x3cf6];profile.period=h.memory[0x3cd2];profile.hold=h.memory[0x3cd3];
        profile.step=h.memory[0x3cd4];profile.mask=h.memory[0x3cd5];
        initial={};initial.index=uint16_t(word(0x3e47)&4095);initial.startup_lookup=!(word(0x3e47)&0x8000);
        initial.divider=h.memory[0x3e44];initial.random_divider=h.memory[0x3e45];initial.random_hold=h.memory[0x3e46];
        const unsigned descriptors=word(0x3cf4);
        for(unsigned i=0;i<(profile.flags&15);++i) {
            const unsigned a=word(descriptors+5*i);
            require(a>=0x4003 && a<0x4200 && (a&3)==3,"unsupported interpolation timing descriptor");
            profile.rows[i]=uint8_t(127-(a-0x4000)/4);profile.caps[i]=h.memory[descriptors+5*i+2];
            initial.address_low[i]=h.memory[descriptors+5*i+3];initial.phase[i]=h.memory[descriptors+5*i+4];
        }
    }
    void start() {
        std::copy_n(h.memory.begin()+0x8000,4096,profile.sequence.begin());profile.startup_byte=h.memory[59];
        for(unsigned pc:{0x38u,0xad5cu,0xae59u,0xae5au,0xae6bu,0xae6cu,0xae8du,0xae8eu,0xae8fu}) h.pc_watches[pc]=true;
        h.pc_observer=[this](uint64_t t,lexicon224x::cpu::CpuSnapshot cpu) {
            h.pc_watches[cpu.pc]=true;
            if(active && cpu.pc==0x38) {
                require(!in_interrupt,"nested XL interrupt");in_interrupt=true;interrupt_begin=t;
                interrupt_return=word(cpu.sp);return_was_watched=h.pc_watches[interrupt_return];
                h.pc_watches[interrupt_return]=true;return;
            }
            if(in_interrupt) {
                if(cpu.pc!=interrupt_return) return;
                // The injected RST 7 acknowledges the interrupt in eleven
                // states before vector 0038's first instruction is observed.
                interrupt_states+=t-interrupt_begin+11;++interrupts;in_interrupt=false;
                h.pc_watches[interrupt_return]=return_was_watched;
            }
            const uint64_t relative=t-begin-interrupt_states;
            if(pending) {require(count==pending_index+1,"timing write not observed");writes[pending_index].duration=unsigned(relative)-pending_at;pending=false;}
            if(cpu.pc==0xad5c) {
                if(stopping) return;
                require(!active,"nested XL interpolation call");active=true;begin=t;count=0;interrupt_states=0;
                return_pc=word(cpu.sp);h.pc_watches[return_pc]=true;
                enabled=h.memory[0x3ccd]&64;snapshot();return;
            }
            if(!active) return;
            if(cpu.pc==0xae59 || cpu.pc==0xae6b || cpu.pc==0xae8d || cpu.pc==0xae8e) {
                pending=true;pending_at=unsigned(relative);pending_index=count;
            } else if(cpu.pc==return_pc) {
                active=false;h.pc_watches[return_pc]=false;unsigned used=0;
                const unsigned predicted=xl_test::modulation_cycles(profile,initial,enabled,[&](unsigned at,xl_test::InterpolationWrite payload) {
                    if(used>=count) {++write_errors;return 7u;}
                    const auto& actual=writes[used++];
                    if(at!=actual.at || payload.row!=actual.payload.row || payload.lane!=actual.payload.lane || payload.value!=actual.payload.value) ++write_errors;
                    return actual.duration;
                });
                if(predicted!=relative && cost_errors++<3) std::cerr<<"XL Mod cost "<<predicted<<" vs "<<relative
                    <<", enabled="<<enabled<<", taps="<<unsigned(profile.flags&15)<<", divider="<<unsigned(initial.divider)<<'\n';
                write_errors+=used!=count;++calls;write_total+=count;
            }
        };
        h.wcs_observer=[this](const lexicon224x::cpu::WcsWrite& w) {
            if(!active) return;
            require(pending && count< writes.size(),"unexpected interpolation WCS write");
            writes[count++]={pending_at,0,{127u-unsigned(w.address-0x4000)/4,unsigned(w.address&3),w.value}};
        };
    }
    void stop() {
        require(!active && !pending && !in_interrupt,"unfinished local timing observation");
        h.pc_observer={};h.wcs_observer={};h.pc_watches.fill(false);
    }
};
int main(int argc,char** argv) {
    require(argc==2 || argc==3,"usage: cineol_xl_modulation_timing_check ROM_DIRECTORY [PROGRAM_INDEX]");
    const int selected=argc==3?std::atoi(argv[2]):-1;require(selected>=-1 && selected<22,"invalid XL timing index");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);
    auto machine=std::make_unique<Machine>(*engine);LarcOperator op(*machine);
    require(!machine->run_task([&]{return xl_test::boot(*machine,op);}).failed,"XL timing boot failed");
    unsigned calls=0,writes=0,errors=0;
    for(unsigned index=0;index<graphs.size();++index) {
        if(selected>=0 && selected!=int(index)) continue;
        const auto info=graphs[index];require(!machine->run_task([&]{return xl_test::select(*machine,op,info.bank,info.program);}).failed,"XL timing selection failed");
        auto window=[&](const char* fixture) {
            auto& host=engine->host();TimingProbe probe{host};
            std::array<float,64> zero{};std::array<std::array<float,64>,4> outputs{};
            float* channels[]={outputs[0].data(),outputs[1].data(),outputs[2].data(),outputs[3].data()};
            probe.start();
            // Keep the operator's frame clock and serial transport running
            // with the host. Direct Host stepping would leave them behind.
            for(unsigned frames=0;probe.calls<200 && frames<96000;frames+=64)
                require(machine->render(zero.data(),zero.data(),channels,64),"XL timing render failed");
            probe.stopping=true;
            for(unsigned frames=0;probe.active && frames<4800;frames+=64)
                require(machine->render(zero.data(),zero.data(),channels,64),"XL timing drain failed");
            probe.stop();
            require(probe.calls>=200,"insufficient XL interpolation timing entries");
            std::cout<<info.name<<", "<<fixture<<": "<<probe.calls<<" local calls, "<<probe.write_total
                <<" WCS writes, cost="<<probe.cost_errors<<", write-boundary/payload="<<probe.write_errors
                <<", interrupts="<<probe.interrupts<<std::endl;
            calls+=probe.calls;writes+=probe.write_total;errors+=probe.cost_errors+probe.write_errors;
        };
        for(bool enabled:{false,true}) {
            require(!machine->run_task([&]{return toggle(*machine,op,enabled);}).failed,"XL timing key failed");
            window(enabled?"Mod on, factory Chorus":"Mod off");
        }
        PagesReading pages;require(!machine->run_task([&]{return xl_test::read_pages(op,pages);}).failed,"XL timing pages failed");
        unsigned chorus_page=0,chorus_slot=0;
        for(unsigned page=0;page<unsigned(pages.count);++page) for(unsigned slot=0;slot<6;++slot)
            if(std::strncmp(pages.pages[page].sliders[slot].shown.name,"CHORUS",6)==0) {chorus_page=page+1;chorus_slot=slot;}
        if(chorus_page) for(unsigned raw:{2u,130u,250u}) {
            require(!machine->run_task([&]{return xl_test::move_slider(*machine,op,chorus_page,chorus_slot,raw,0.5);}).failed,"XL timing Chorus fader failed");
            const auto label=std::string("Mod on, Chorus ")+std::to_string(raw);window(label.c_str());
        }
        require(!machine->run_task([&]{return toggle(*machine,op,false);}).failed,"XL timing cleanup key failed");
    }
    std::cout<<"Local XL Mod timing: "<<calls<<" calls, "<<writes<<" writes; observed waits, not free-running scheduling\n";
    require(!errors,"XL local interpolation timing differs from firmware");
}
