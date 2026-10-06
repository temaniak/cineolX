// Local offline compiler oracle. Entry/cache/IRQ and graph phase are observed;
// arithmetic, write payloads, READY waits and final work are native predictions.
#include "xl_reference.hpp"
#include "../desktop/bank.hpp"
#include "../desktop/decay_compiler_xl.hpp"
#include "../desktop/coefficient_structure_xl.hpp"
#include <emulator/trace.hpp>
#include <new>
using namespace lexplug;using namespace lexplug::op;using xl_test::require;
using namespace cineol::xl;namespace hw=lexicon224x::cpu;
static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n){if(tracking)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(tracking && p)++releases;std::free(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
static unsigned word(const hw::Host& h,unsigned a){return h.memory[a]|unsigned(h.memory[uint16_t(a+1)])<<8;}
struct Context {
    DynamicsState state{};DecayCompilerMemoryXL compiler{};
    std::array<uint8_t,48> raw{};
};
static Context snapshot(const hw::Host& h) {
    Context c;std::copy_n(h.memory.begin()+0x3ca3,48,c.raw.begin());
    std::copy_n(h.memory.begin()+0x3c73,6,c.compiler.cached.begin());
    c.compiler.decay_time=uint16_t(word(h,0x3c5d));c.compiler.special=h.memory[0x3e07];
    c.compiler.feedback_index=h.memory[0x3e3e];c.compiler.extended_mask=(h.memory[0x3df9]&8)!=0;
    c.compiler.main_enabled=(h.memory[0x3df9]&128)==0;
    c.state.low=h.memory[0x3c52];c.state.mid=h.memory[0x3c53];
    c.state.amount=h.memory[0x3e12];c.state.period=h.memory[0x3e14];c.state.peak_input=h.memory[0x3c61];return c;
}
static bool same(const Context& a,const Context& b) {
    return a.state.low==b.state.low && a.state.mid==b.state.mid && a.state.amount==b.state.amount &&
        a.state.period==b.state.period && a.state.peak_input==b.state.peak_input && a.compiler.cached==b.compiler.cached &&
        a.compiler.decay_time==b.compiler.decay_time && a.compiler.special==b.compiler.special && a.compiler.feedback_index==b.compiler.feedback_index;
}
struct Probe final:hw::Trace {
    hw::Host& h;const ProgramData& data;Graph graph;
    bool active=false,stopping=false,in_irq=false,seeded=false,bus_pending=false;
    uint64_t last_activity=0,begin=0,irq_begin=0,irq_states=0;uint16_t return_pc=0,return_sp=0,irq_pc=0,irq_sp=0;
    struct Irq {unsigned at,duration;};std::array<Irq,64> irqs{};unsigned irq_count=0;
    struct Byte {unsigned at;uint64_t start,commit;ControlByteXL payload;};std::array<Byte,256> writes{};unsigned write_count=0;
    WcsTimingXL live_bus,bus;WcsTimingXL::Access access{};Context initial{};
    unsigned calls=0,prefix=0,errors=0,work_errors=0,state_errors=0,write_errors=0,bus_errors=0,total_writes=0,total_irq=0;
    Probe(hw::Host& host,const ProgramData& p,Graph g):h(host),data(p),graph(g){}
    unsigned local(uint64_t t)const{return unsigned(t-begin-irq_states);}
    void start() {
        const unsigned shadow=word(h,0x3e05);
        for(unsigned row=0;row<graph_info(graph).rows;++row) {
            require((h.memory[uint16_t(shadow+0x4003+(127-row)*4)]&3)==coefficient_lane_low2[unsigned(graph)][row],"native magnitude template differs");
            const unsigned actual=h.memory[uint16_t(shadow+0x4002+(127-row)*4)]&127;
            const unsigned expected=coefficient_sign_lane_low7[unsigned(graph)][row];
            if(actual!=expected)std::cerr<<"sign template row "<<row<<": native="<<expected<<", reference="<<actual<<'\n';
            require(actual==expected,"native sign template differs");
        }
        last_activity=h.cycles*hw::cpu_period;h.set_trace(this);
        for(unsigned pc:{0xa7a9u,0x38u})h.pc_watches[pc]=true;
        h.pc_observer=[this](uint64_t t,hw::CpuSnapshot cpu) {
            h.pc_watches[cpu.pc]=true;
            if(!active) {
                if(cpu.pc!=0xa7a9 || stopping)return;
                if(!seeded){++prefix;return;}
                active=true;begin=t;irq_states=0;irq_count=write_count=0;initial=snapshot(h);bus=live_bus;
                return_pc=uint16_t(word(h,cpu.sp));return_sp=uint16_t(cpu.sp+2);h.pc_watches[return_pc]=true;return;
            }
            if(cpu.pc==0x38) {
                require(!in_irq,"nested compiler IRQ");in_irq=true;irq_begin=t-11;
                irq_pc=uint16_t(word(h,cpu.sp));irq_sp=uint16_t(cpu.sp+2);h.pc_watches[irq_pc]=true;return;
            }
            if(in_irq) {
                if(cpu.pc!=irq_pc || cpu.sp!=irq_sp)return;
                require(irq_count<irqs.size(),"compiler IRQ capacity exceeded");
                irqs[irq_count++]={local(irq_begin),unsigned(t-irq_begin)};irq_states+=t-irq_begin;in_irq=false;
            }
            if(cpu.pc!=return_pc || cpu.sp!=return_sp)return;
            auto predicted=initial;unsigned used=0;
            auto resume=[&](unsigned at)noexcept {uint64_t wall=begin+at;for(unsigned i=0;i<irq_count;++i)if(irqs[i].at<=at)wall+=irqs[i].duration;return wall;};
            tracking=true;
            const unsigned work=decay_compile_work(data.dynamics,data.controls,predicted.raw,predicted.compiler,predicted.state,
                coefficient_lane_low2[unsigned(graph)],coefficient_sign_lane_low7[unsigned(graph)],
                [&](unsigned at,ControlByteXL value)noexcept {
                    const auto start=resume(at);const auto expected=bus.access(start+4,false);
                    if(used>=write_count){++write_errors;return unsigned(expected.finished_cpu_state-start);}
                    const auto& actual=writes[used++];
                    if(actual.at!=at || actual.start!=start || actual.commit!=expected.commit || value.row!=actual.payload.row ||
                        value.lane!=actual.payload.lane || value.value!=actual.payload.value) {
                        if(write_errors++<5)std::cerr<<"write "<<used<<": at "<<at<<'/'<<actual.at<<", row "<<value.row<<'/'<<actual.payload.row
                            <<", lane "<<value.lane<<'/'<<actual.payload.lane<<", value "<<unsigned(value.value)<<'/'<<unsigned(actual.payload.value)<<'\n';
                    }
                    return unsigned(expected.finished_cpu_state-start);
                });
            tracking=false;const auto actual=snapshot(h);
            if(work!=local(t)){if(work_errors++<5)std::cerr<<"compiler work "<<work<<" vs "<<local(t)<<", LF/MID="<<unsigned(initial.state.low)<<'/'<<unsigned(initial.state.mid)<<'\n';}
            if(!same(predicted,actual)) {
                if(state_errors++<5)std::cerr<<"compiler state: time "<<predicted.compiler.decay_time<<'/'<<actual.compiler.decay_time
                    <<", peak "<<unsigned(predicted.state.peak_input)<<'/'<<unsigned(actual.state.peak_input)
                    <<", amount "<<unsigned(predicted.state.amount)<<'/'<<unsigned(actual.state.amount)
                    <<", period "<<unsigned(predicted.state.period)<<'/'<<unsigned(actual.state.period)
                    <<", feedback "<<unsigned(predicted.compiler.feedback_index)<<'/'<<unsigned(actual.compiler.feedback_index)
                    <<", special "<<unsigned(predicted.compiler.special)<<'/'<<unsigned(actual.compiler.special)<<", cache ";
                if(state_errors<=5){for(unsigned j=0;j<6;++j)std::cerr<<unsigned(predicted.compiler.cached[j])<<'/'<<unsigned(actual.compiler.cached[j])<<' ';std::cerr<<'\n';}
            }
            if(used!=write_count)++write_errors;
            total_writes+=write_count;total_irq+=irq_count;++calls;active=false;
        };
        h.wcs_observer=[this](const hw::WcsWrite& w) {
            if(!active || in_irq)return;require(write_count<writes.size(),"compiler write capacity exceeded");
            writes[write_count++]={local(w.cpu_t1-4),w.cpu_t1-4,w.committed_at,{127u-unsigned(w.address-0x4000)/4,unsigned(w.address&3),w.value}};
        };
    }
    void stop(){require(!active && !in_irq && !bus_pending,"unfinished compiler observation");h.set_trace(nullptr);h.pc_observer={};h.wcs_observer={};h.pc_watches.fill(false);}
    void cpu_state(hw::Tick,uint64_t,uint64_t,bool)override{}
    void bus_command(hw::Tick,hw::BusCommand)override{}
    void row_phase(hw::Tick t,hw::RowPhase phase,uint64_t)override {
        if(seeded || phase!=hw::RowPhase::Fetch || h.dsp->pc!=0)return;
        const auto marker=t-hw::fetch_offset;if(marker<=last_activity+2*graph_info(graph).rows*hw::row)return;
        live_bus.reset_settled(graph,marker);seeded=true;
    }
    void fetch_displaced(hw::Tick,uint64_t)override{}
    void multiplicand_held(hw::Tick,hw::Hold)override{}
    void wcs_request(hw::Tick t,bool read,unsigned,unsigned)override {
        last_activity=t;if(!seeded)return;
        require(!bus_pending,"overlapping compiler WCS access");const auto tick=t-(read?hw::cpu_period+hw::phi2_rise:2*hw::cpu_period);
        require(tick%hw::cpu_period==0,"compiler access is not on CPU boundary");
        access=live_bus.access(tick/hw::cpu_period,read);bus_pending=true;
    }
    void wcs_grant(hw::Tick t,hw::Tick ack)override {
        last_activity=t;if(!seeded)return;require(bus_pending,"missing compiler grant");
        if(t!=access.grant || ack!=access.ack)++bus_errors;bus_pending=false;
    }
    void wcs_read_drive(hw::Tick,hw::Tick)override{}
    void wcs_commit(hw::Tick,unsigned,unsigned,uint8_t)override{}
    void aruck(hw::Tick,unsigned)override{}
    void history_sample(hw::Tick,unsigned)override{}
    void dport(hw::Tick,hw::DportEvent)override{}
};
static Task<void> fixture(Machine& m,LarcOperator& op,const ProgramData& p,bool unequal) {
    for(unsigned n=0;n<8;++n) {co_await op.setToggle(0,true);co_await m.sleep(.2);if(m.peek(0x3ccd)&1)break;}
    if(!(m.peek(0x3ccd)&1))co_await fail("compiler dynamics switch did not settle");
    for(unsigned page=0;page<p.page_count;++page)for(unsigned slot=0;slot<6;++slot) {
        const unsigned cell=p.pages[page].cells[slot],base=p.dynamics.shared_stop?6:12;
        if(cell==base)co_await op.moveSlider(int(page+1),slot,unequal?8:18);
        if(cell==base+1)co_await op.moveSlider(int(page+1),slot,unequal?56:18);
        if(cell==45)co_await op.moveSlider(int(page+1),slot,0);
    }
    co_await m.sleep(.5);
}
int main(int argc,char** argv) {
    const bool unequal=argc>3 && std::string(argv[argc-1])=="--unequal-stops";if(unequal)--argc;
    require(argc==3 || argc==4,"usage: cineol_xl_decay_compiler_check ROM_DIRECTORY PREPARED_BANK [PROGRAM_INDEX] [--unequal-stops]");
    const int selected=argc==4?std::atoi(argv[3]):-1;require(selected>=-1 && selected<22,"invalid compiler program");
    std::ifstream input(argv[2],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    auto bank=std::make_unique<Bank>();require(read_bank(bytes.data(),bytes.size(),*bank),"invalid compiler bank");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);auto m=std::make_unique<Machine>(*engine);LarcOperator op(*m);
    require(!m->run_task([&]{return xl_test::boot(*m,op);}).failed,"compiler boot failed");
    unsigned total_errors=0,total_calls=0;
    for(unsigned i=0;i<graphs.size();++i) {
        if(selected>=0 && selected!=int(i))continue;const auto info=graphs[i];const auto& p=bank->programs[i];if(!p.dynamics.enabled)continue;
        require(!m->run_task([&]{return xl_test::select(*m,op,info.bank,info.program);}).failed,"compiler selection failed");
        require(!m->run_task([&]{return fixture(*m,op,p,unequal);}).failed,"compiler fixture failed");
        Probe probe{engine->host(),p,Graph(i)};probe.start();
        std::array<float,64> zero{};std::array<std::array<float,64>,4> out{};float* channels[]={out[0].data(),out[1].data(),out[2].data(),out[3].data()};
        for(unsigned frame=0;frame<96000;frame+=64) {
            const unsigned phase=frame%24000,bits=phase<4000?15:phase<8000?3:phase<12000?0:phase<16000?31:0;
            engine->host().set_level_detectors(0,bits);engine->host().set_level_detectors(1,bits);
            require(m->render(zero.data(),zero.data(),channels,64),"compiler render failed");
        }
        probe.stopping=true;for(unsigned frame=0;probe.active && frame<48000;frame+=64)require(m->render(zero.data(),zero.data(),channels,64),"compiler drain failed");
        probe.stop();const unsigned errors=probe.work_errors+probe.state_errors+probe.write_errors+probe.bus_errors;
        std::cout<<info.name<<", unequal stops="<<unequal<<": calls="<<probe.calls<<", writes="<<probe.total_writes<<", IRQ="<<probe.total_irq<<", work errors="<<probe.work_errors
            <<", state errors="<<probe.state_errors<<", write errors="<<probe.write_errors<<", bus errors="<<probe.bus_errors<<", prefix="<<probe.prefix<<'\n'<<std::flush;
        total_calls+=probe.calls;total_errors+=errors;
    }
    require(total_calls && !total_errors,"native compiler differs from physical reference");require(!allocations && !releases,"native compiler allocated/released");
    std::cout<<"Native decay compiler work/writes exact; allocation/release=0; entry/IRQ/initial cache remain observed\n";
}
