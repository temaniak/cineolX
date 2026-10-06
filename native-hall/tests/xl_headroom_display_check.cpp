// Offline private-ROM boundary oracle. Entry state, DIP samples and serial
// IRQ spans are local inputs; panel publication work is predicted natively.
#include "xl_reference.hpp"
#include "../desktop/headroom_display_clock_xl.hpp"
#include <new>
using namespace lexplug;using namespace lexplug::op;using xl_test::require;
using namespace cineol::xl;namespace hw=lexicon224x::cpu;
static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n){if(tracking)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(tracking && p)++releases;std::free(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
static HeadroomDisplayMemoryXL snapshot(const hw::Host& h) {
    HeadroomDisplayMemoryXL s;
    for(unsigned i=0;i<2;++i) {
        const unsigned a=0x3c63+i*7;auto& c=s.channels[i];
        c={h.memory[a],h.memory[a+1],h.memory[a+2],h.memory[a+3],h.memory[a+4]};
    }
    s.divider=h.memory[0x3c37];s.lamp=h.memory[0x3c71];return s;
}
static bool same(const HeadroomDisplayMemoryXL& a,const HeadroomDisplayMemoryXL& b) {
    if(a.divider!=b.divider || a.lamp!=b.lamp)return false;
    for(unsigned i=0;i<2;++i) {
        const auto& x=a.channels[i];const auto& y=b.channels[i];
        if(x.input!=y.input || x.cache!=y.cache || x.peak!=y.peak || x.hold!=y.hold || x.changed!=y.changed)return false;
    }
    return true;
}
struct Probe {
    hw::Host& h;HeadroomDisplayMemoryXL native{};HeadroomDisplayClockXL clock;
    bool active=false,in_irq=false,stopping=false,dip=false,publishing=false;
    unsigned calls=0,publications=0,dips=0,irq_count=0,errors=0;
    std::array<unsigned,4> panel_branches{};
    uint64_t begin=0,irq_states=0,irq_begin=0,publish_at=0;
    uint16_t return_pc=0,return_sp=0,irq_pc=0,irq_sp=0,publish_pc=0,publish_sp=0;
    PanelPublishMemoryXL panel{};
    unsigned word(unsigned a)const{return h.memory[a]|unsigned(h.memory[uint16_t(a+1)])<<8;}
    uint64_t local(uint64_t t)const{return t-begin-irq_states;}
    void compare(uint64_t t,HeadroomDisplayClockXL::Kind kind) {
        const auto event=clock.next();
        if(event.kind!=kind || event.work_state!=local(t)) {
            if(errors++<8)std::cerr<<"display boundary kind="<<unsigned(event.kind)<<'/'<<unsigned(kind)
                <<", work="<<event.work_state<<'/'<<local(t)<<'\n';
        }
    }
    void start() {
        for(unsigned pc:{0x0fc8u,0x052au,0x0ff3u,0x0ff5u,0x38u})h.pc_watches[pc]=true;
        h.pc_observer=[this](uint64_t t,hw::CpuSnapshot cpu) {
            h.pc_watches[cpu.pc]=true;
            if(!active) {
                if(cpu.pc!=0x0fc8 || stopping)return;
                active=true;begin=t;irq_states=0;native=snapshot(h);
                return_pc=uint16_t(word(cpu.sp));return_sp=uint16_t(cpu.sp+2);h.pc_watches[return_pc]=true;
                tracking=true;clock.reset(native,0);tracking=false;return;
            }
            if(cpu.pc==0x38) {
                require(!in_irq,"nested display IRQ");in_irq=true;irq_begin=t-11;
                irq_pc=uint16_t(word(cpu.sp));irq_sp=uint16_t(cpu.sp+2);h.pc_watches[irq_pc]=true;return;
            }
            if(in_irq) {
                if(cpu.pc!=irq_pc || cpu.sp!=irq_sp)return;
                irq_states+=t-irq_begin;in_irq=false;++irq_count;
            }
            if(publishing && cpu.pc==publish_pc && cpu.sp==publish_sp) {
                if(publish_at!=local(t) || panel.flags!=h.memory[0x3c14] || panel.receive_mode!=h.memory[0x3c15]) {
                    if(errors++<8)std::cerr<<"panel request work/state mismatch: "<<publish_at<<'/'<<local(t)<<'\n';
                }
                const unsigned work=unsigned(publish_at-clock.next().work_state);
                tracking=true;clock.complete_publish(work);tracking=false;
                publishing=false;
            }
            if(cpu.pc==0x052a) {
                compare(t,HeadroomDisplayClockXL::Kind::publish);
                panel={h.memory[0x3c14],h.memory[0x3c15]};
                const unsigned branch=panel.receive_mode==2?0:(panel.flags&1)?1:(panel.flags&128)?3:2;
                ++panel_branches[branch];
                tracking=true;const unsigned work=panel_publish_work(panel);tracking=false;
                publish_at=local(t)+work;
                publish_pc=uint16_t(word(cpu.sp));publish_sp=uint16_t(cpu.sp+2);h.pc_watches[publish_pc]=true;
                // Completion is delayed until the actual return, preserving
                // every publication boundary in the predicted work stream.
                publishing=true;++publications;return;
            }
            if(cpu.pc==0x0ff3){compare(t,HeadroomDisplayClockXL::Kind::dip);dip=true;return;}
            if(cpu.pc==0x0ff5 && dip) {
                dip=false;tracking=true;clock.complete_dip(cpu.a);tracking=false;++dips;return;
            }
            if(cpu.pc!=return_pc || cpu.sp!=return_sp)return;
            compare(t,HeadroomDisplayClockXL::Kind::finished);
            if(!same(native,snapshot(h))) {
                if(errors++<8)std::cerr<<"display state mismatch\n";
            }
            ++calls;active=false;
        };
    }
    void stop(){require(!active && !in_irq && !dip && !publishing,"unfinished display call");h.pc_observer={};h.pc_watches.fill(false);}
};
int main(int argc,char** argv) {
    require(argc==2 || argc==3,"usage: cineol_xl_headroom_display_check ROM_DIRECTORY [PROGRAM_INDEX]");
    const int selected=argc==3?std::atoi(argv[2]):-1;require(selected>=-1 && selected<22,"invalid display program");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);auto m=std::make_unique<Machine>(*engine);LarcOperator op(*m);
    require(!m->run_task([&]{return xl_test::boot(*m,op);}).failed,"display boot failed");
    unsigned errors=0,total=0,total_publications=0,total_dips=0;
    for(unsigned i=0;i<graphs.size();++i) {
        if(selected>=0 && selected!=int(i))continue;const auto info=graphs[i];
        require(!m->run_task([&]{return xl_test::select(*m,op,info.bank,info.program);}).failed,"display selection failed");
        Probe probe{engine->host()};probe.start();
        std::array<float,64> zero{};std::array<std::array<float,64>,4> out{};
        float* channels[]={out[0].data(),out[1].data(),out[2].data(),out[3].data()};
        for(unsigned frame=0;frame<96000;frame+=64) {
            const unsigned phase=frame%24000,bits=phase<4000?15:phase<8000?3:phase<12000?0:phase<16000?31:0;
            engine->host().set_level_detectors(0,bits);engine->host().set_level_detectors(1,phase<12000?bits:0);
            require(m->render(zero.data(),zero.data(),channels,64),"display render failed");
        }
        probe.stopping=true;
        for(unsigned frame=0;probe.active && frame<48000;frame+=64)
            require(m->render(zero.data(),zero.data(),channels,64),"display drain failed");
        probe.stop();require(probe.calls>10 && probe.publications && probe.dips,"insufficient display coverage");
        std::cout<<info.name<<": calls="<<probe.calls<<", publications="<<probe.publications<<", DIP="<<probe.dips
            <<", IRQ="<<probe.irq_count<<", panel branches="<<probe.panel_branches[0]<<'/'<<probe.panel_branches[1]
            <<'/'<<probe.panel_branches[2]<<'/'<<probe.panel_branches[3]<<", errors="<<probe.errors<<'\n'<<std::flush;
        errors+=probe.errors;total+=probe.calls;total_publications+=probe.publications;total_dips+=probe.dips;
    }
    require(total && !errors,"native headroom display differs");require(!allocations && !releases,"display allocates/releases");
    std::cout<<"Display state/work exact: calls="<<total<<", publications="<<total_publications<<", DIP="<<total_dips
        <<"; allocation/release=0; full serial scan remains separate\n";
}
