// Offline local oracle for gradual LF/MID steps and compiler boundaries.
// Compiler durations/effects and IRQ spans are observed inputs here; this
// check does not validate a free-running native scheduler or compiler law.
#include "xl_reference.hpp"
#include "../desktop/bank.hpp"
#include "../desktop/decay_transition_xl.hpp"
#include <new>
using namespace lexplug;using namespace lexplug::op;using xl_test::require;
using namespace cineol::xl;namespace hw=lexicon224x::cpu;
static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n){if(tracking)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(tracking && p)++releases;std::free(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
static DynamicsState snapshot(const hw::Host& h) {
    DynamicsState s;s.flags=h.memory[0x3c51];s.low=h.memory[0x3c52];s.mid=h.memory[0x3c53];
    s.stop_counter=h.memory[0x3c5f];return s;
}
static bool same(const DynamicsState& a,const DynamicsState& b) {
    return a.flags==b.flags && a.low==b.low && a.mid==b.mid && a.stop_counter==b.stop_counter;
}
struct Probe {
    hw::Host& h;const ProgramData& data;
    DynamicsState native{};DecayTransitionXL clock;
    bool active=false,compiling=false,in_irq=false,stopping=false;
    unsigned calls=0,compiles=0,returns=0,delayed=0,irq_count=0,errors=0,max_compiles=0,current_compiles=0;
    uint64_t begin=0,irq_states=0,irq_begin=0,compiler_at=0;
    uint16_t irq_pc=0,irq_sp=0;
    std::array<uint8_t,48> raw{};
    Probe(hw::Host& host,const ProgramData& p):h(host),data(p){}
    unsigned word(unsigned a)const{return h.memory[a]|unsigned(h.memory[uint16_t(a+1)])<<8;}
    uint64_t local(uint64_t t)const{return t-begin-irq_states;}
    void compare(uint64_t t,DecayTransitionXL::Kind kind) {
        const auto event=clock.next();const auto actual=snapshot(h);
        if(event.kind!=kind || event.work_state!=local(t) || !same(native,actual)) {
            if(errors++<8)std::cerr<<"decay boundary pc time="<<local(t)<<", native time="<<event.work_state
                <<", kind="<<unsigned(event.kind)<<'/'<<unsigned(kind)<<", LF/MID="<<unsigned(native.low)<<'/'<<unsigned(native.mid)
                <<" vs "<<unsigned(actual.low)<<'/'<<unsigned(actual.mid)<<", flags="<<unsigned(native.flags)<<'/'<<unsigned(actual.flags)
                <<", delay="<<unsigned(native.stop_counter)<<'/'<<unsigned(actual.stop_counter)<<'\n';
        }
    }
    void start() {
        for(unsigned pc:{0x8405u,0x84bbu,0xa7a9u,0x8491u,0x38u})h.pc_watches[pc]=true;
        h.pc_observer=[this](uint64_t t,hw::CpuSnapshot cpu) {
            h.pc_watches[cpu.pc]=true;
            if(!active) {
                if(cpu.pc!=0x8405 || stopping)return;
                active=true;begin=t;irq_states=0;current_compiles=0;native=snapshot(h);
                std::copy_n(h.memory.begin()+0x3ca3,48,raw.begin());
                tracking=true;clock.reset(native,0,data.dynamics.shared_stop,raw);tracking=false;return;
            }
            if(cpu.pc==0x38) {
                require(!in_irq,"nested decay IRQ");in_irq=true;irq_begin=t-11;
                irq_pc=uint16_t(word(cpu.sp));irq_sp=uint16_t(cpu.sp+2);h.pc_watches[irq_pc]=true;return;
            }
            if(in_irq) {
                if(cpu.pc!=irq_pc || cpu.sp!=irq_sp)return;
                irq_states+=t-irq_begin;in_irq=false;++irq_count;
            }
            if(cpu.pc==0xa7a9 && !compiling) {
                compare(t,DecayTransitionXL::Kind::compile);compiler_at=local(t);compiling=true;
                ++compiles;++current_compiles;return;
            }
            if(compiling) {
                if(cpu.pc!=0x8491)return;
                tracking=true;clock.complete_compiler(unsigned(local(t)-compiler_at));tracking=false;
                compare(t,DecayTransitionXL::Kind::compiler_return);++returns;compiling=false;
                tracking=true;clock.resume_after_compiler();tracking=false;return;
            }
            if(cpu.pc!=0x84bb)return;
            compare(t,DecayTransitionXL::Kind::finished);max_compiles=std::max(max_compiles,current_compiles);
            delayed+=!current_compiles && bool(raw[45]);++calls;active=false;
        };
    }
    void stop(){require(!active && !compiling && !in_irq,"unfinished decay transition");h.pc_observer={};h.pc_watches.fill(false);}
};
static Task<void> fixture(Machine& m,LarcOperator& op,const ProgramData& p,unsigned stop_low,unsigned stop_mid,unsigned delay) {
    for(unsigned n=0;n<8;++n) {
        co_await op.setToggle(0,true);co_await m.sleep(0.2);
        if(m.peek(0x3ccd)&1)break;
    }
    if(!(m.peek(0x3ccd)&1))co_await fail("decay switch did not settle");
    for(unsigned page=0;page<p.page_count;++page)for(unsigned slot=0;slot<6;++slot) {
        const unsigned cell=p.pages[page].cells[slot],base=p.dynamics.shared_stop?6:12;
        if(cell==base)co_await op.moveSlider(int(page+1),slot,stop_low);
        if(cell==base+1)co_await op.moveSlider(int(page+1),slot,stop_mid);
        if(cell==45)co_await op.moveSlider(int(page+1),slot,delay);
    }
    co_await m.sleep(0.5);
}
int main(int argc,char** argv) {
    require(argc==3 || argc==4,"usage: cineol_xl_decay_transition_check ROM_DIRECTORY PREPARED_BANK [PROGRAM_INDEX]");
    const int selected=argc==4?std::atoi(argv[3]):-1;require(selected>=-1 && selected<22,"invalid decay program");
    std::ifstream input(argv[2],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    auto bank=std::make_unique<Bank>();require(read_bank(bytes.data(),bytes.size(),*bank),"invalid decay bank");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);auto m=std::make_unique<Machine>(*engine);LarcOperator op(*m);
    require(!m->run_task([&]{return xl_test::boot(*m,op);}).failed,"decay boot failed");
    unsigned errors=0,total=0,total_compiles=0;
    for(unsigned i=0;i<graphs.size();++i) {
        if(selected>=0 && selected!=int(i))continue;const auto info=graphs[i];const auto& p=bank->programs[i];
        if(!p.dynamics.enabled)continue;
        for(unsigned variant=0;variant<3;++variant) {
            require(!m->run_task([&]{return xl_test::select(*m,op,info.bank,info.program);}).failed,"decay selection failed");
            require(!m->run_task([&]{return fixture(*m,op,p,variant?8:18,variant?56:18,variant==2?80:0);}).failed,"decay fixture failed");
            Probe probe{engine->host(),p};probe.start();
            std::array<float,64> zero{};std::array<std::array<float,64>,4> out{};
            float* channels[]={out[0].data(),out[1].data(),out[2].data(),out[3].data()};
            for(unsigned frame=0;frame<96000;frame+=64) {
                const unsigned phase=frame%24000,bits=phase<4000?15:phase<8000?3:phase<12000?0:phase<16000?31:0;
                engine->host().set_level_detectors(0,bits);engine->host().set_level_detectors(1,bits);
                require(m->render(zero.data(),zero.data(),channels,64),"decay render failed");
            }
            probe.stopping=true;
            for(unsigned frame=0;probe.active && frame<48000;frame+=64)
                require(m->render(zero.data(),zero.data(),channels,64),"decay drain failed");
            probe.stop();require(probe.calls>10,"insufficient decay transitions");
            std::cout<<info.name<<", variant="<<variant<<": transitions="<<probe.calls<<", compiles="<<probe.compiles
                <<", max compiles="<<probe.max_compiles<<", IRQ="<<probe.irq_count<<", errors="<<probe.errors<<'\n'<<std::flush;
            errors+=probe.errors;total+=probe.calls;total_compiles+=probe.compiles;
        }
    }
    require(total && total_compiles,"no observed gradual compilation");require(!errors,"native decay boundaries differ");
    require(!allocations && !releases,"native decay transitions allocate/release");
    std::cout<<"Decay transition state/work boundaries exact; allocation/release=0; compiler work remains observed\n";
}
