// Offline post-ramp slow-stage oracle. Nested compiler/display/menu work and
// IRQ spans are observed local inputs. Dynamics counters, release arithmetic
// and the containing branch work are predicted independently.
#include "xl_reference.hpp"
#include "../desktop/slow_tail_clock_xl.hpp"
#include <new>
using namespace lexplug;using namespace lexplug::op;using xl_test::require;
using namespace cineol::xl;namespace hw=lexicon224x::cpu;
static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n){if(tracking)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(tracking && p)++releases;std::free(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
static FastControlMemoryXL snapshot(const hw::Host& h) {
    FastControlMemoryXL m;auto& s=m.dynamics;
    s.held=uint16_t(h.memory[0x3e0f]|unsigned(h.memory[0x3e10])<<8);
    s.average=h.memory[0x3c50];s.flags=h.memory[0x3c51];s.low=h.memory[0x3c52];s.mid=h.memory[0x3c53];
    s.trigger_peak=h.memory[0x3c54];s.stop_counter=h.memory[0x3c5f];s.stopped=h.memory[0x3e11];
    s.amount=h.memory[0x3e12];s.divider=h.memory[0x3e13];s.period=h.memory[0x3e14];
    s.peak_divider=h.memory[0x3c38];s.peak_input=h.memory[0x3c61];
    std::copy_n(h.memory.begin()+0x3e15,11,s.history.begin());
    m.headroom_left=h.memory[0x3c63];m.headroom_right=h.memory[0x3c6a];m.headroom_accum=h.memory[0x3c62];return m;
}
static bool same(const FastControlMemoryXL& a,const FastControlMemoryXL& b) {
    const auto& x=a.dynamics;const auto& y=b.dynamics;
    const unsigned actual[]={x.held,x.average,x.flags,x.low,x.mid,x.trigger_peak,x.stop_counter,x.stopped,x.amount,x.divider,x.period,x.peak_divider,x.peak_input,
        a.headroom_left,a.headroom_right,a.headroom_accum};
    const unsigned wanted[]={y.held,y.average,y.flags,y.low,y.mid,y.trigger_peak,y.stop_counter,y.stopped,y.amount,y.divider,y.period,y.peak_divider,y.peak_input,
        b.headroom_left,b.headroom_right,b.headroom_accum};
    for(unsigned i=0;i<std::size(actual);++i)if(actual[i]!=wanted[i])return false;
    return x.history==y.history;
}


static SlowTailMemoryXL tail_snapshot(const hw::Host& h) {
    return {h.memory[0x3cd6],h.memory[0x3c36],h.memory[0x3c35],{h.memory[0x3c39],h.memory[0x3c3a]}};
}
static bool same_tail(const SlowTailMemoryXL& a,const SlowTailMemoryXL& b) {
    return a.reconcile==b.reconcile && a.menu_flags==b.menu_flags && a.dip==b.dip && a.timers==b.timers;
}
struct Probe {
    hw::Host& h;FastControlMemoryXL native{};SlowTailMemoryXL tail{};SlowTailClockXL clock;
    bool active=false,in_irq=false,nested=false,stopping=false,dip=false;
    unsigned calls=0,reads=0,compilers=0,displays=0,configures=0,irq_count=0,errors=0,state_errors=0,max_compilers=0,current_compilers=0;
    uint64_t begin=0,irq_states=0,irq_begin=0,nested_at=0;
    uint16_t irq_pc=0,irq_sp=0,nested_pc=0,nested_sp=0;
    SlowTailClockXL::Kind pending=SlowTailClockXL::Kind::finished;
    unsigned word(unsigned a)const{return h.memory[a]|unsigned(h.memory[uint16_t(a+1)])<<8;}
    uint64_t local(uint64_t t)const{return t-begin-irq_states;}
    void compare(uint64_t t,SlowTailClockXL::Kind kind) {
        const auto event=clock.next();
        if(event.kind!=kind || event.work_state!=local(t)) {
            if(errors++<8)std::cerr<<"slow-tail boundary kind="<<unsigned(event.kind)<<'/'<<unsigned(kind)
                <<", work="<<event.work_state<<'/'<<local(t)<<'\n';
        }
    }
    void compare_state() {
        if(!same(native,snapshot(h)) || !same_tail(tail,tail_snapshot(h))) {
            if(state_errors++<8) {
                const auto a=snapshot(h);std::cerr<<"slow-tail state held/peak/divider/amount/period: "
                    <<native.dynamics.held<<'/'<<unsigned(native.dynamics.trigger_peak)<<'/'<<unsigned(native.dynamics.divider)
                    <<'/'<<unsigned(native.dynamics.amount)<<'/'<<unsigned(native.dynamics.period)
                    <<" vs "<<a.dynamics.held<<'/'<<unsigned(a.dynamics.trigger_peak)<<'/'<<unsigned(a.dynamics.divider)
                    <<'/'<<unsigned(a.dynamics.amount)<<'/'<<unsigned(a.dynamics.period)<<'\n';
            }
        }
    }
    void start() {
        for(unsigned pc:{0x84bbu,0x85afu,0xafe4u,0xa791u,0x0fc8u,0x8597u,0x859au,0x13cfu,0x38u})h.pc_watches[pc]=true;
        h.pc_observer=[this](uint64_t t,hw::CpuSnapshot cpu) {
            h.pc_watches[cpu.pc]=true;
            if(!active) {
                if(cpu.pc!=0x84bb || stopping)return;
                active=true;begin=t;irq_states=0;current_compilers=0;native=snapshot(h);tail=tail_snapshot(h);
                tracking=true;clock.reset(native,tail,0,uint16_t(word(0x3c5d)));tracking=false;return;
            }
            if(cpu.pc==0x38) {
                require(!in_irq,"nested slow-tail IRQ");in_irq=true;irq_begin=t-11;
                irq_pc=uint16_t(word(cpu.sp));irq_sp=uint16_t(cpu.sp+2);h.pc_watches[irq_pc]=true;return;
            }
            if(in_irq) {
                if(cpu.pc!=irq_pc || cpu.sp!=irq_sp)return;
                irq_states+=t-irq_begin;in_irq=false;++irq_count;
            }
            if(nested) {
                if(cpu.pc!=nested_pc || cpu.sp!=nested_sp)return;
                const unsigned work=unsigned(local(t)-nested_at);
                if(pending==SlowTailClockXL::Kind::parameters || pending==SlowTailClockXL::Kind::feedback) {
                    tracking=true;clock.complete_compiler(work,h.memory[0x3e14],uint16_t(word(0x3c5d)));tracking=false;
                    compare(t,SlowTailClockXL::Kind::compiler_return);compare_state();
                    tracking=true;clock.resume_after_compiler();tracking=false;
                } else if(pending==SlowTailClockXL::Kind::display) {
                    tracking=true;clock.complete_display(work);tracking=false;
                } else {
                    tracking=true;clock.complete_configure(work);tracking=false;
                }
                nested=false;
            }
            SlowTailClockXL::Kind kind=SlowTailClockXL::Kind::finished;
            if(cpu.pc==0xafe4)kind=SlowTailClockXL::Kind::feedback;
            else if(cpu.pc==0xa791)kind=SlowTailClockXL::Kind::parameters;
            else if(cpu.pc==0x0fc8)kind=SlowTailClockXL::Kind::display;
            else if(cpu.pc==0x13cf)kind=SlowTailClockXL::Kind::configure;
            if(kind!=SlowTailClockXL::Kind::finished) {
                compare(t,kind);compare_state();nested=true;nested_at=local(t);pending=kind;
                nested_pc=uint16_t(word(cpu.sp));nested_sp=uint16_t(cpu.sp+2);h.pc_watches[nested_pc]=true;
                if(kind==SlowTailClockXL::Kind::display)++displays;
                else if(kind==SlowTailClockXL::Kind::configure)++configures;
                else {++compilers;++current_compilers;}
                return;
            }
            if(cpu.pc==0x8597){compare(t,SlowTailClockXL::Kind::dip);dip=true;return;}
            if(cpu.pc==0x859a && dip) {
                dip=false;tracking=true;clock.complete_dip(cpu.a);tracking=false;++reads;return;
            }
            if(cpu.pc!=0x85af)return;
            compare(t,SlowTailClockXL::Kind::finished);compare_state();
            max_compilers=std::max(max_compilers,current_compilers);++calls;active=false;
        };
    }
    void stop(){require(!active && !in_irq && !nested && !dip,"unfinished slow-tail call");h.pc_observer={};h.pc_watches.fill(false);}
};
static Task<void> mode(Machine& m,LarcOperator& op,unsigned bits) {
    for(unsigned which:{0u,2u}) {
        const unsigned mask=which==0?1:128;
        for(unsigned n=0;n<8;++n) {
            co_await op.setToggle(int(which),bool(bits&mask));co_await m.sleep(0.2);
            if(bool(m.peek(0x3ccd)&mask)==bool(bits&mask))break;
        }
        if(bool(m.peek(0x3ccd)&mask)!=bool(bits&mask))co_await fail("slow-tail mode did not settle");
    }
    co_await m.sleep(0.5);
}
int main(int argc,char** argv) {
    require(argc==2 || argc==3,"usage: cineol_xl_slow_entry_check ROM_DIRECTORY [PROGRAM_INDEX]");
    const int selected=argc==3?std::atoi(argv[2]):-1;require(selected>=-1 && selected<22,"invalid slow-tail program");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);auto m=std::make_unique<Machine>(*engine);LarcOperator op(*m);
    require(!m->run_task([&]{return xl_test::boot(*m,op);}).failed,"slow-tail boot failed");
    unsigned errors=0,total=0,total_compilers=0,total_reads=0;
    for(unsigned i=0;i<graphs.size();++i) {
        if(selected>=0 && selected!=int(i))continue;const auto info=graphs[i];
        require(!m->run_task([&]{return xl_test::select(*m,op,info.bank,info.program);}).failed,"slow-tail selection failed");
        for(unsigned variant=0;variant<4;++variant) {
            require(!m->run_task([&]{return mode(*m,op,(variant&1)|((variant&2)?128:0));}).failed,"slow-tail fixture failed");
            Probe probe{engine->host()};probe.start();
            std::array<float,64> zero{};std::array<std::array<float,64>,4> out{};
            float* channels[]={out[0].data(),out[1].data(),out[2].data(),out[3].data()};
            for(unsigned frame=0;frame<48000;frame+=64) {
                const unsigned phase=frame%24000,bits=phase<4000?15:phase<8000?3:phase<12000?0:phase<16000?31:0;
                engine->host().set_level_detectors(0,bits);engine->host().set_level_detectors(1,phase<12000?bits:0);
                require(m->render(zero.data(),zero.data(),channels,64),"slow-tail render failed");
            }
            probe.stopping=true;
            for(unsigned frame=0;probe.active && frame<48000;frame+=64)
                require(m->render(zero.data(),zero.data(),channels,64),"slow-tail drain failed");
            probe.stop();require(probe.calls>10 && probe.displays==probe.calls,"insufficient slow-tail coverage");
            std::cout<<info.name<<", variant="<<variant<<": calls="<<probe.calls<<", reads="<<probe.reads
                <<", displays="<<probe.displays<<", configures="<<probe.configures<<", compilers="<<probe.compilers<<", max compilers="<<probe.max_compilers<<", IRQ="<<probe.irq_count
                <<", work errors="<<probe.errors<<", state errors="<<probe.state_errors<<'\n'<<std::flush;
            errors+=probe.errors+probe.state_errors;total+=probe.calls;total_compilers+=probe.compilers;total_reads+=probe.reads;
        }
    }
    require(total && !errors,"native slow tail differs");require(!allocations && !releases,"slow tail allocates/releases");
    std::cout<<"Slow-tail state/read/work exact: calls="<<total<<", reads="<<total_reads<<", compilers="<<total_compilers
        <<"; allocation/release=0; nested compiler/display/menu durations and IRQ remain local inputs\n";
}
