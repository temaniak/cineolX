// Local outer-pass oracle: nested Mod/active-auxiliary duration and IRQ spans
// are observed diagnostic inputs. Outer branches, idle auxiliary costs,
// monitor read/sample boundaries and retained peak are predicted natively.
#include "xl_reference.hpp"
#include "../desktop/scan_timing_xl.hpp"
#include "../desktop/monitor_pass_clock_xl.hpp"
#include <memory>
#include <new>
static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n){if(tracking)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(tracking && p)++releases;std::free(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
using namespace lexplug;using namespace lexplug::op;using xl_test::require;
using namespace cineol::xl;namespace hw=lexicon224x::cpu;

struct PassProbe {
    hw::Host& h;
    struct Call {unsigned at,work;uint16_t pc;};std::array<Call,3> calls{};
    struct Read {unsigned at,port;uint8_t value;uint64_t sample;};std::array<Read,2> reads{};
    unsigned call_count=0,read_count=0,descriptors=0,layout=0,aux_state=0,aux_descriptors=0;
    uint16_t peak=0,return_pc=0,return_sp=0,nested_pc=0,nested_sp=0,irq_return=0,irq_sp=0;
    uint64_t begin=0,irq_states=0,begin_irq_states=0,irq_begin=0;
    unsigned nested_at=0,passes=0,errors=0,clock_errors=0,irqs=0,mod_calls=0,aux_calls=0,active_aux=0;
    unsigned high_reads=0,low_reads=0,negative_high=0,negative_low=0,peak_updates=0;
    enum class Gap {none,slow,pass,fast};Gap gap=Gap::none;
    uint64_t gap_begin=0,gap_irq_states=0;unsigned pass_number=0,gaps=0,gap_errors=0;
    bool fast_active=false,slow_active=false;
    bool level_active=false;uint16_t level_return=0,level_sp=0,level_input=0;
    uint64_t level_begin=0,level_irq_states=0;unsigned levels=0,level_errors=0;
    bool active=false,nested=false,in_irq=false,stopping=false;
    explicit PassProbe(hw::Host& host):h(host){}
    unsigned word(unsigned a)const{return h.memory[a]|unsigned(h.memory[a+1])<<8;}
    unsigned local(uint64_t t)const{return unsigned(t-begin-(irq_states-begin_irq_states));}
    void returned(Gap kind,uint64_t t) {gap=kind;gap_begin=t;gap_irq_states=irq_states;}
    void check_gap(uint64_t t,unsigned expected) {
        const auto actual=t-gap_begin-(irq_states-gap_irq_states);++gaps;
        if(actual!=expected){if(gap_errors++<3)std::cerr<<"XL caller gap "<<expected<<" vs "<<actual<<'\n';}
    }
    void start() {
        for(unsigned pc:{0x8281u,0xad5cu,0xb315u,0x38u,0x82cfu,0x816cu,0x81b6u,0x8178u,0x0fb7u})h.pc_watches[pc]=true;
        h.pc_observer=[this](uint64_t t,hw::CpuSnapshot cpu) {
            h.pc_watches[cpu.pc]=true;
            if(cpu.pc==0x38) {
                require(!in_irq,"nested XL scan interrupt");in_irq=true;irq_begin=t-11;
                irq_return=uint16_t(word(cpu.sp));irq_sp=uint16_t(cpu.sp+2);h.pc_watches[irq_return]=true;return;
            }
            if(in_irq) {
                if(cpu.pc!=irq_return || cpu.sp!=irq_sp)return;
                in_irq=false;irq_states+=t-irq_begin;++irqs;
            }
            if(level_active && cpu.pc==level_return && cpu.sp==level_sp) {
                tracking=true;const auto predicted=level_work(level_input);tracking=false;
                const auto actual=t-level_begin-(irq_states-level_irq_states);
                if(predicted.level!=cpu.a || predicted.work_states!=actual) {
                    if(level_errors++<3)std::cerr<<"XL headroom encoding cost/value "<<predicted.work_states<<'/'<<unsigned(predicted.level)
                        <<" vs "<<actual<<'/'<<unsigned(cpu.a)<<'\n';
                }
                level_active=false;++levels;
            }
            if(cpu.pc==0x0fb7) {
                require(!level_active,"nested XL level encoding");level_active=true;
                level_input=cpu.hl;level_begin=t;level_irq_states=irq_states;
                level_return=uint16_t(word(cpu.sp));level_sp=uint16_t(cpu.sp+2);h.pc_watches[level_return]=true;
            }
            if(cpu.pc==0x82cf)slow_active=true;
            if(cpu.pc==0x816c && slow_active){slow_active=false;returned(Gap::slow,t);}
            if(cpu.pc==0x81b6) {
                if(gap==Gap::pass && (pass_number&1))check_gap(t,pass_to_fast_work);
                fast_active=true;
            }
            if(cpu.pc==0x8178 && fast_active){fast_active=false;returned(Gap::fast,t);}
            if(!active) {
                if(cpu.pc!=0x8281 || stopping)return;
                if(gap==Gap::slow)check_gap(t,slow_to_first_pass_work);
                else if(gap==Gap::pass && !(pass_number&1))check_gap(t,even_pass_to_next_pass_work);
                else if(gap==Gap::fast && pass_number>1)check_gap(t,fast_to_next_pass_work);
                pass_number=cpu.a;require(pass_number>=1 && pass_number<=9,"invalid XL scan pass ordinal");
                active=true;begin=t;begin_irq_states=irq_states;call_count=read_count=0;
                return_pc=uint16_t(word(cpu.sp));return_sp=uint16_t(cpu.sp+2);h.pc_watches[return_pc]=true;
                descriptors=h.memory[0x3cf6]&15;peak=uint16_t(word(0x3e38));return;
            }
            if(nested && cpu.pc==nested_pc && cpu.sp==nested_sp) {
                require(call_count>0,"XL missing nested call");calls[call_count-1].work=local(t)-nested_at;nested=false;
            }
            if(cpu.pc==0xad5c || cpu.pc==0xb315) {
                require(!nested && call_count<calls.size(),"XL scan nested call bound");
                nested=true;nested_at=local(t);calls[call_count++]={nested_at,0,cpu.pc};
                nested_pc=uint16_t(word(cpu.sp));nested_sp=uint16_t(cpu.sp+2);h.pc_watches[nested_pc]=true;
                if(cpu.pc==0xb315){layout=h.memory[0x3df9];aux_state=h.memory[0x3e07];aux_descriptors=h.memory[0x3d1a];}
                return;
            }
            if(cpu.pc!=return_pc || cpu.sp!=return_sp)return;
            if(nested || call_count<2 || read_count<1)std::cerr<<"XL pass end pc="<<std::hex<<cpu.pc<<std::dec<<", nested="<<nested
                <<", calls="<<call_count<<", reads="<<read_count<<", local="<<local(t)<<'\n';
            require(!nested && call_count>=2 && read_count>=1,"unfinished XL monitor pass");
            unsigned used_calls=0,used_reads=0;
            auto call=[&](unsigned at,unsigned pc) noexcept {
                if(used_calls>=call_count){++errors;return 0u;}
                const auto actual=calls[used_calls++];
                if(actual.at!=at || actual.pc!=pc)++errors;
                if(pc==0xad5c){++mod_calls;return actual.work;}
                ++aux_calls;
                const unsigned idle=auxiliary_idle_work(uint8_t(layout),uint8_t(aux_state),aux_descriptors);
                if(!idle){++active_aux;return actual.work;}
                if(idle!=actual.work)++errors;
                return idle;
            };
            tracking=true;
            const auto predicted=monitor_pass_work(descriptors,peak,
                [&](unsigned at)noexcept{return call(at,0xad5c);},
                [&](unsigned at)noexcept{return call(at,0xb315);},
                [&](unsigned port,unsigned at)noexcept {
                    if(used_reads>=read_count){++errors;return uint8_t(0);}
                    const auto actual=reads[used_reads++];
                    if(at!=actual.at || port!=actual.port)++errors;
                    return actual.value;
                });
            // Independently check the resumable engine interface against the
            // actual ordered reference events, not just the eager helper.
            MonitorPassClockXL clock;clock.reset(0,descriptors,peak);
            unsigned clock_calls=0,clock_reads=0;
            for(unsigned step=0;step<6 && clock.next().kind!=MonitorPassClockXL::Kind::finished;++step) {
                const auto event=clock.next();unsigned value=0;
                using Kind=MonitorPassClockXL::Kind;
                if(event.kind==Kind::monitor_high || event.kind==Kind::monitor_low) {
                    if(clock_reads>=read_count){++clock_errors;break;}
                    const auto actual=reads[clock_reads++];
                    if(actual.at!=event.work_state || actual.port!=(event.kind==Kind::monitor_high?7u:6u))++clock_errors;
                    value=actual.value;
                } else {
                    if(clock_calls>=call_count){++clock_errors;break;}
                    const auto actual=calls[clock_calls++];
                    const unsigned pc=event.kind==Kind::auxiliary?0xb315:0xad5c;
                    if(actual.at!=event.work_state || actual.pc!=pc)++clock_errors;
                    value=actual.work;
                    if(pc==0xb315) {
                        const unsigned idle=auxiliary_idle_work(uint8_t(layout),uint8_t(aux_state),aux_descriptors);
                        if(idle)value=idle;
                    }
                }
                clock.complete(value);
            }
            if(clock.next().kind!=MonitorPassClockXL::Kind::finished || clock.next().work_state!=local(t) || clock.peak()!=word(0x3e38) ||
                clock_calls!=call_count || clock_reads!=read_count)++clock_errors;
            tracking=false;
            if(predicted.work_states!=local(t) || predicted.peak!=word(0x3e38) || used_calls!=call_count || used_reads!=read_count ||
                (h.memory[0x3cf6]&15)!=descriptors) {
                if(errors++<4)std::cerr<<"XL outer pass cost/peak "<<predicted.work_states<<'/'<<predicted.peak<<" vs "<<local(t)<<'/'<<word(0x3e38)
                    <<", count="<<descriptors<<'\n';
            }
            peak_updates+=predicted.peak!=peak;active=false;++passes;returned(Gap::pass,t);
        };
        h.port_trace=[this](uint64_t t,uint16_t pc,bool write,unsigned port,uint8_t value) {
            if(!active || write || (pc!=0x82a0 && pc!=0x82ae))return;
            require(!in_irq && read_count<reads.size(),"XL monitor read bound");
            const uint64_t sample=hw::state_start(h.cycles)+hw::phi2_rise;
            if(sample!=(t+9)*hw::cpu_period+hw::phi2_rise)++errors;
            reads[read_count++]={local(t),port,value,sample};
            if(port==7){++high_reads;negative_high+=bool(value&128);}else{++low_reads;negative_low+=bool(value&128);}
        };
    }
    void stop() {
        require(!active && !nested && !in_irq && !level_active,"unfinished XL monitor observation");
        h.pc_observer={};h.port_trace={};h.pc_watches.fill(false);
    }
};
static Task<void> mode(Machine& m,LarcOperator& op,unsigned bits) {
    for(unsigned which=0;which<3;++which) {
        const bool enabled=bits&(which==0?4:which==1?1:2);
        co_await op.setToggle(int(which),enabled);co_await m.sleep(0.2);
    }
    const auto actual=co_await op.readToggles();
    if(actual.value[0]!=int(bool(bits&4)) || actual.value[1]!=int(bool(bits&1)) || actual.value[2]!=int(bool(bits&2)))
        co_await fail("XL scan pass physical mode did not settle");
    co_await m.sleep(0.5);
}
int main(int argc,char** argv) {
    require(argc==2 || argc==3,"usage: cineol_xl_scan_pass_check ROM_DIRECTORY [PROGRAM_INDEX]");
    const int selected=argc==3?std::atoi(argv[2]):-1;require(selected>=-1 && selected<22,"invalid XL scan index");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);
    auto machine=std::make_unique<Machine>(*engine);LarcOperator op(*machine);
    require(!machine->run_task([&]{return xl_test::boot(*machine,op);}).failed,"XL scan boot failed");
    unsigned total_passes=0,total_errors=0,total_reads=0,total_aux=0,total_irqs=0;
    for(unsigned index=0;index<22;++index) {
        if(selected>=0 && selected!=int(index))continue;
        const auto info=graphs[index];require(!machine->run_task([&]{return xl_test::select(*machine,op,info.bank,info.program);}).failed,"XL scan selection failed");
        const unsigned modes=index==0?8:((index>=14 && index<=16)?2:3);
        for(unsigned j=0;j<modes;++j) {
            const unsigned bits=modes==3?(j==2?7:j):j;
            require(!machine->run_task([&]{return mode(*machine,op,bits);}).failed,"XL scan mode failed");
            for(bool signal:{false,true}) {
                auto& host=engine->host();PassProbe probe{host};probe.start();uint32_t random=17;
                std::array<float,64> left{},right{};std::array<std::array<float,64>,4> output{};
                float* channels[]={output[0].data(),output[1].data(),output[2].data(),output[3].data()};
                for(unsigned frames=0;probe.passes<200 && frames<96000;frames+=64) {
                    for(unsigned i=0;i<64;++i)for(auto* input:{&left,&right}) {
                        random=random*1664525u+1013904223u;(*input)[i]=signal?.5f*float(int32_t(random))/2147483648.f:0;
                    }
                    require(machine->render(left.data(),right.data(),channels,64),"XL scan render failed");
                }
                probe.stopping=true;
                for(unsigned frames=0;(probe.active || probe.in_irq || probe.level_active) && frames<4800;frames+=64)require(machine->render(left.data(),right.data(),channels,64),"XL scan drain failed");
                probe.stop();require(probe.passes>=200,"insufficient XL scan pass entries");
                std::cout<<info.name<<", mode="<<bits<<", "<<(signal?"noise":"silence")<<": passes="<<probe.passes<<", errors="<<probe.errors
                    <<", stage errors="<<probe.clock_errors<<", caller gaps="<<probe.gaps<<", gap errors="<<probe.gap_errors
                    <<", level calls="<<probe.levels<<", level errors="<<probe.level_errors
                    <<", Mod="<<probe.mod_calls<<", auxiliary="<<probe.aux_calls<<", active-aux="<<probe.active_aux
                    <<", high/low reads="<<probe.high_reads<<'/'<<probe.low_reads<<", negative="<<probe.negative_high<<'/'<<probe.negative_low
                    <<", peak updates="<<probe.peak_updates<<", IRQ="<<probe.irqs<<std::endl;
                total_passes+=probe.passes;total_errors+=probe.errors+probe.clock_errors+probe.gap_errors+probe.level_errors;total_reads+=probe.high_reads+probe.low_reads;total_aux+=probe.active_aux;total_irqs+=probe.irqs;
            }
        }
    }
    std::cout<<"Native XL outer pass: "<<total_passes<<" passes, "<<total_reads<<" monitor reads, "<<total_aux<<" observed active auxiliary calls, "<<total_irqs
        <<" observed IRQ spans; nested Mod costs remain labelled local inputs\n";
    require(!allocations && !releases,"XL native scan pass touched heap");require(!total_errors,"XL native scan pass differs");
}
