// Offline slow-entry oracle. Entry memory, physical headroom samples and
// nested feedback duration/IRQ spans are observed local inputs. No observed
// cadence or future sample is stored in the native control law.
#include "xl_reference.hpp"
#include "../desktop/slow_entry_clock_xl.hpp"
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

struct Probe {
    hw::Host& h;FastControlMemoryXL native{};SlowEntryClockXL clock;
    bool active=false,in_irq=false,compiling=false,stopping=false;
    unsigned calls=0,reads=0,compilers=0,irq_count=0,errors=0,state_errors=0,max_compilers=0,current_compilers=0;
    uint64_t begin=0,irq_states=0,irq_begin=0,compiler_at=0;
    uint16_t irq_pc=0,irq_sp=0,compiler_pc=0,compiler_sp=0;
    unsigned word(unsigned a)const{return h.memory[a]|unsigned(h.memory[uint16_t(a+1)])<<8;}
    uint64_t local(uint64_t t)const{return t-begin-irq_states;}
    void compare(uint64_t t,SlowEntryClockXL::Kind kind) {
        const auto event=clock.next();
        if(event.kind!=kind || event.work_state!=local(t)) {
            if(errors++<8)std::cerr<<"slow-entry boundary kind="<<unsigned(event.kind)<<'/'<<unsigned(kind)
                <<", work="<<event.work_state<<'/'<<local(t)<<'\n';
        }
    }
    void compare_state() {
        if(!same(native,snapshot(h))) {
            if(state_errors++<8) {
                const auto a=snapshot(h);std::cerr<<"slow-entry state held/average/flags/period: "
                    <<native.dynamics.held<<'/'<<unsigned(native.dynamics.average)<<'/'<<unsigned(native.dynamics.flags)<<'/'<<unsigned(native.dynamics.period)
                    <<" vs "<<a.dynamics.held<<'/'<<unsigned(a.dynamics.average)<<'/'<<unsigned(a.dynamics.flags)<<'/'<<unsigned(a.dynamics.period)<<'\n';
            }
        }
    }
    void start() {
        for(unsigned pc:{0x82cfu,0x8405u,0xafe4u,0x38u})h.pc_watches[pc]=true;
        h.pc_observer=[this](uint64_t t,hw::CpuSnapshot cpu) {
            h.pc_watches[cpu.pc]=true;
            if(!active) {
                if(cpu.pc!=0x82cf || stopping)return;
                active=true;begin=t;irq_states=0;current_compilers=0;native=snapshot(h);
                const unsigned flags=h.memory[0x3ccd];
                tracking=true;clock.reset(native,0,uint16_t(word(0x3e38)),h.memory[0x3e07],h.memory[0x3c5c],h.memory[0x3cd0],flags&1,flags&128);tracking=false;
                return;
            }
            if(cpu.pc==0x38) {
                require(!in_irq,"nested slow-entry IRQ");in_irq=true;irq_begin=t-11;
                irq_pc=uint16_t(word(cpu.sp));irq_sp=uint16_t(cpu.sp+2);h.pc_watches[irq_pc]=true;return;
            }
            if(in_irq) {
                if(cpu.pc!=irq_pc || cpu.sp!=irq_sp)return;
                irq_states+=t-irq_begin;in_irq=false;++irq_count;
            }
            if(compiling) {
                if(cpu.pc!=compiler_pc || cpu.sp!=compiler_sp)return;
                tracking=true;clock.complete_compiler(unsigned(local(t)-compiler_at),h.memory[0x3e14]);tracking=false;
                compare(t,SlowEntryClockXL::Kind::feedback_return);compare_state();
                tracking=true;clock.resume_after_compiler();tracking=false;compiling=false;
            }
            if(cpu.pc==0xafe4) {
                compare(t,SlowEntryClockXL::Kind::feedback);compare_state();compiling=true;compiler_at=local(t);
                compiler_pc=uint16_t(word(cpu.sp));compiler_sp=uint16_t(cpu.sp+2);h.pc_watches[compiler_pc]=true;
                ++compilers;++current_compilers;return;
            }
            if(cpu.pc!=0x8405)return;
            compare(t,SlowEntryClockXL::Kind::finished);compare_state();
            if(clock.display()!=h.memory[0x3e3d] || clock.monitor_peak()!=word(0x3e38))++state_errors;
            max_compilers=std::max(max_compilers,current_compilers);++calls;active=false;
        };
        h.port_trace=[this](uint64_t t,uint16_t pc,bool write,unsigned port,uint8_t value) {
            if(!active || write || (pc!=0x0fa2 && pc!=0x0fac))return;
            require(!in_irq && !compiling,"unexpected slow headroom read");
            compare(t,pc==0x0fa2?SlowEntryClockXL::Kind::left:SlowEntryClockXL::Kind::right);
            require(port==(pc==0x0fa2?8u:9u),"wrong slow headroom port");
            tracking=true;clock.complete_read(value);tracking=false;++reads;
        };
    }
    void stop(){require(!active && !in_irq && !compiling,"unfinished slow-entry call");h.pc_observer={};h.port_trace={};h.pc_watches.fill(false);}
};
static Task<void> mode(Machine& m,LarcOperator& op,unsigned bits) {
    for(unsigned which:{0u,2u}) {
        const unsigned mask=which==0?1:128;
        for(unsigned n=0;n<8;++n) {
            co_await op.setToggle(int(which),bool(bits&mask));co_await m.sleep(0.2);
            if(bool(m.peek(0x3ccd)&mask)==bool(bits&mask))break;
        }
        if(bool(m.peek(0x3ccd)&mask)!=bool(bits&mask))co_await fail("slow-entry mode did not settle");
    }
    co_await m.sleep(0.5);
}
int main(int argc,char** argv) {
    require(argc==2 || argc==3,"usage: cineol_xl_slow_entry_check ROM_DIRECTORY [PROGRAM_INDEX]");
    const int selected=argc==3?std::atoi(argv[2]):-1;require(selected>=-1 && selected<22,"invalid slow-entry program");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);auto m=std::make_unique<Machine>(*engine);LarcOperator op(*m);
    require(!m->run_task([&]{return xl_test::boot(*m,op);}).failed,"slow-entry boot failed");
    unsigned errors=0,total=0,total_compilers=0,total_reads=0;
    for(unsigned i=0;i<graphs.size();++i) {
        if(selected>=0 && selected!=int(i))continue;const auto info=graphs[i];
        require(!m->run_task([&]{return xl_test::select(*m,op,info.bank,info.program);}).failed,"slow-entry selection failed");
        for(unsigned variant=0;variant<4;++variant) {
            require(!m->run_task([&]{return mode(*m,op,(variant&1)|((variant&2)?128:0));}).failed,"slow-entry fixture failed");
            Probe probe{engine->host()};probe.start();
            std::array<float,64> zero{};std::array<std::array<float,64>,4> out{};
            float* channels[]={out[0].data(),out[1].data(),out[2].data(),out[3].data()};
            for(unsigned frame=0;frame<48000;frame+=64) {
                const unsigned phase=frame%24000,bits=phase<4000?15:phase<8000?3:phase<12000?0:phase<16000?31:0;
                engine->host().set_level_detectors(0,bits);engine->host().set_level_detectors(1,phase<12000?bits:0);
                require(m->render(zero.data(),zero.data(),channels,64),"slow-entry render failed");
            }
            probe.stopping=true;
            for(unsigned frame=0;probe.active && frame<48000;frame+=64)
                require(m->render(zero.data(),zero.data(),channels,64),"slow-entry drain failed");
            probe.stop();require(probe.calls>10 && probe.reads==2*probe.calls,"insufficient slow-entry coverage");
            std::cout<<info.name<<", variant="<<variant<<": calls="<<probe.calls<<", reads="<<probe.reads
                <<", compilers="<<probe.compilers<<", max compilers="<<probe.max_compilers<<", IRQ="<<probe.irq_count
                <<", work errors="<<probe.errors<<", state errors="<<probe.state_errors<<'\n'<<std::flush;
            errors+=probe.errors+probe.state_errors;total+=probe.calls;total_compilers+=probe.compilers;total_reads+=probe.reads;
        }
    }
    require(total && !errors,"native slow entry differs");require(!allocations && !releases,"slow entry allocates/releases");
    std::cout<<"Slow-entry state/read/work exact: calls="<<total<<", reads="<<total_reads<<", compilers="<<total_compilers
        <<"; allocation/release=0; nested compiler durations/IRQ remain local inputs\n";
}
