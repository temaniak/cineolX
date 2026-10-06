// Native Mod branch/T&C coupling at actual firmware entries. Profile/state,
// row-zero fixture origin and IRQ spans are observed local-oracle inputs.
// READY waits and every commit are predicted; the full scan remains separate.
#include "xl_reference.hpp"
#include "xl_modulation_cycles.hpp"
#include <cstring>
#include <memory>
#include <emulator/trace.hpp>
#include <new>
static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n){if(tracking)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(tracking && p)++releases;std::free(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
using namespace lexplug;using namespace lexplug::op;using xl_test::require;
using namespace cineol::xl;

static Task<void> toggle(Machine& m,LarcOperator& op,bool enabled) {
    for(unsigned n=0;n<8;++n) {
        co_await op.setToggle(1,enabled);co_await m.sleep(0.2);
        if(bool(m.peek(0x3ccd)&64)==enabled) co_return;
    }
    co_await fail("XL timing Mod key did not settle");
}
namespace hw=lexicon224x::cpu;
struct TimingProbe final:hw::Trace {
    lexicon224x::cpu::Host& h;
    Graph graph;WcsTimingXL live_bus,entry_bus;
    WcsTimingXL::Access expected_access{};
    bool seeded=false,bus_pending=false;uint64_t last_activity=0,seed_marker=0;
    unsigned requests=0,grants=0,bus_errors=0,prefix_requests=0;
    struct Irq {unsigned work_at=0,duration=0;};std::array<Irq,32> irq_spans{};
    unsigned irq_count=0,irq_work_at=0;
    explicit TimingProbe(hw::Host& host,Graph g):h(host),graph(g){}
    ModulationProfile profile{};ModulationState initial{};
    struct Actual {unsigned at=0,duration=0;xl_test::InterpolationWrite payload{};uint64_t wall_start=0,commit=0;};
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
        last_activity=h.cycles*hw::cpu_period;h.set_trace(this);
        std::copy_n(h.memory.begin()+0x8000,4096,profile.sequence.begin());profile.startup_byte=h.memory[59];
        for(unsigned pc:{0x38u,0xad5cu,0xae59u,0xae5au,0xae6bu,0xae6cu,0xae8du,0xae8eu,0xae8fu}) h.pc_watches[pc]=true;
        h.pc_observer=[this](uint64_t t,lexicon224x::cpu::CpuSnapshot cpu) {
            h.pc_watches[cpu.pc]=true;
            if(active && cpu.pc==0x38) {
                require(!in_interrupt,"nested XL interrupt");in_interrupt=true;interrupt_begin=t;
                irq_work_at=unsigned(t-11-begin-interrupt_states);
                interrupt_return=word(cpu.sp);return_was_watched=h.pc_watches[interrupt_return];
                h.pc_watches[interrupt_return]=true;return;
            }
            if(in_interrupt) {
                if(cpu.pc!=interrupt_return) return;
                // The injected RST 7 acknowledges the interrupt in eleven
                // states before vector 0038's first instruction is observed.
                require(irq_count<irq_spans.size(),"native Mod local IRQ bound exceeded");
                const unsigned duration=unsigned(t-interrupt_begin+11);
                irq_spans[irq_count++]={irq_work_at,duration};
                interrupt_states+=duration;++interrupts;in_interrupt=false;
                h.pc_watches[interrupt_return]=return_was_watched;
            }
            const uint64_t relative=t-begin-interrupt_states;
            if(pending) {require(count==pending_index+1,"timing write not observed");writes[pending_index].duration=unsigned(relative)-pending_at;pending=false;}
            if(cpu.pc==0xad5c) {
                if(stopping || !seeded) return;
                require(!active,"nested XL interpolation call");active=true;begin=t;count=0;interrupt_states=0;irq_count=0;entry_bus=live_bus;
                return_pc=word(cpu.sp);h.pc_watches[return_pc]=true;
                enabled=h.memory[0x3ccd]&64;snapshot();return;
            }
            if(!active) return;
            if(cpu.pc==0xae59 || cpu.pc==0xae6b || cpu.pc==0xae8d || cpu.pc==0xae8e) {
                pending=true;pending_at=unsigned(relative);pending_index=count;
            } else if(cpu.pc==return_pc) {
                active=false;h.pc_watches[return_pc]=false;unsigned used=0;
                auto resume=[&](unsigned at) noexcept {
                    uint64_t wall=begin+at;
                    for(unsigned i=0;i<irq_count;++i)if(irq_spans[i].work_at<=at)wall+=irq_spans[i].duration;
                    return wall;
                };
                tracking=true;
                const auto plan=timed_modulation(profile,initial,enabled,entry_bus,resume,
                    [&](unsigned at,uint64_t start,ControlByteXL payload,const WcsTimingXL::Access& access) noexcept {
                        if(used>=count){++write_errors;return;}
                        const auto& actual=writes[used++];
                        if(at!=actual.at || start!=actual.wall_start || payload.row!=actual.payload.row || payload.lane!=actual.payload.lane || payload.value!=actual.payload.value ||
                            access.commit!=actual.commit || access.finished_cpu_state-start!=actual.duration)++write_errors;
                    });
                tracking=false;
                const unsigned predicted=plan.work_states;
                if(plan.finished_state!=t)++cost_errors;
                if(predicted!=relative && cost_errors++<3) std::cerr<<"XL Mod cost "<<predicted<<" vs "<<relative
                    <<", enabled="<<enabled<<", taps="<<unsigned(profile.flags&15)<<", divider="<<unsigned(initial.divider)<<'\n';
                write_errors+=used!=count;++calls;write_total+=count;
            }
        };
        h.wcs_observer=[this](const lexicon224x::cpu::WcsWrite& w) {
            if(!active) return;
            require(pending && count< writes.size(),"unexpected interpolation WCS write");
            writes[count++]={pending_at,0,{127u-unsigned(w.address-0x4000)/4,unsigned(w.address&3),w.value},w.cpu_t1-4,w.committed_at};
        };
    }
    void stop() {
        require(!active && !pending && !in_interrupt,"unfinished local timing observation");
        require(!bus_pending,"unfinished native T&C prediction");h.set_trace(nullptr);h.pc_observer={};h.wcs_observer={};h.pc_watches.fill(false);
    }
    void cpu_state(hw::Tick,uint64_t,uint64_t,bool)override{}
    void bus_command(hw::Tick,hw::BusCommand)override{}
    void row_phase(hw::Tick time,hw::RowPhase phase,uint64_t)override {
        if(seeded || phase!=hw::RowPhase::Fetch || h.dsp->pc!=0)return;
        const auto marker=time-hw::fetch_offset;
        const auto quiet=2*graph_info(graph).rows*hw::row;
        if(marker<=last_activity+quiet)return;
        // A quiet completed pass fixes pair/RESETD state without importing
        // reference flip-flops. Only this local component fixture's origin
        // is observed; it is not used in ordinary native/reference renders.
        live_bus.reset_settled(graph,marker);seeded=true;seed_marker=marker;
    }
    void fetch_displaced(hw::Tick,uint64_t)override{}
    void multiplicand_held(hw::Tick,hw::Hold)override{}
    void wcs_request(hw::Tick time,bool reading,unsigned,unsigned)override {
        last_activity=time;++requests;if(!seeded){++prefix_requests;return;}
        require(!bus_pending,"overlapping native T&C predictions");
        const auto t1=time-(reading?hw::cpu_period+hw::phi2_rise:2*hw::cpu_period);
        require(t1%hw::cpu_period==0,"nonintegral reference data-bus T1");
        tracking=true;expected_access=live_bus.access(t1/hw::cpu_period,reading);tracking=false;bus_pending=true;
    }
    void wcs_grant(hw::Tick marker,hw::Tick ack)override {
        last_activity=marker;if(!seeded)return;
        require(bus_pending,"unexpected reference T&C grant");
        if(expected_access.grant!=marker || expected_access.ack!=ack) {
            if(bus_errors++<3)std::cerr<<"native T&C grant/XACK "<<expected_access.grant<<"/"<<expected_access.ack<<" vs "<<marker<<"/"<<ack<<'\n';
        }
        bus_pending=false;++grants;
    }
    void wcs_read_drive(hw::Tick,hw::Tick)override{}
    void wcs_commit(hw::Tick,unsigned,unsigned,uint8_t)override{}
    void aruck(hw::Tick,unsigned)override{}
    void history_sample(hw::Tick,unsigned)override{}
    void dport(hw::Tick,hw::DportEvent)override{}

};
int main(int argc,char** argv) {
    require(argc==2 || argc==3,"usage: cineol_xl_timed_modulation_check ROM_DIRECTORY [PROGRAM_INDEX]");
    const int selected=argc==3?std::atoi(argv[2]):-1;require(selected>=-1 && selected<22,"invalid XL timing index");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);
    auto machine=std::make_unique<Machine>(*engine);LarcOperator op(*machine);
    require(!machine->run_task([&]{return xl_test::boot(*machine,op);}).failed,"XL timing boot failed");
    unsigned calls=0,writes=0,errors=0;
    for(unsigned index=0;index<graphs.size();++index) {
        if(selected>=0 && selected!=int(index)) continue;
        const auto info=graphs[index];require(!machine->run_task([&]{return xl_test::select(*machine,op,info.bank,info.program);}).failed,"XL timing selection failed");
        auto window=[&](const char* fixture) {
            auto& host=engine->host();TimingProbe probe{host,Graph(index)};
            std::array<float,64> zero{};std::array<std::array<float,64>,4> outputs{};
            float* channels[]={outputs[0].data(),outputs[1].data(),outputs[2].data(),outputs[3].data()};
            probe.start();
            // Keep the operator's frame clock and serial transport running
            // with the host. Direct Host stepping would leave them behind.
            for(unsigned frames=0;probe.calls<200 && frames<96000;frames+=64)
                require(machine->render(zero.data(),zero.data(),channels,64),"XL timing render failed");
            probe.stopping=true;
            for(unsigned frames=0;(probe.active || probe.bus_pending) && frames<4800;frames+=64)
                require(machine->render(zero.data(),zero.data(),channels,64),"XL timing drain failed");
            probe.stop();
            require(probe.calls>=200,"insufficient XL interpolation timing entries");
            std::cout<<info.name<<", "<<fixture<<": "<<probe.calls<<" local calls, "<<probe.write_total
                <<" WCS writes, cost="<<probe.cost_errors<<", write-boundary/payload="<<probe.write_errors
                <<", interrupts="<<probe.interrupts<<", predicted grants="<<probe.grants<<", bus errors="<<probe.bus_errors<<", pre-seed requests="<<probe.prefix_requests<<std::endl;
            calls+=probe.calls;writes+=probe.write_total;errors+=probe.cost_errors+probe.write_errors+probe.bus_errors;
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
    std::cout<<"Native timed XL Mod: "<<calls<<" calls, "<<writes<<" writes; predicted READY waits/commits, observed local IRQ/origin, not free-running scheduling\n";
    require(allocations==0 && releases==0,"native Mod work/grant computation touched heap");
    require(!errors,"XL local interpolation timing differs from firmware");
}
