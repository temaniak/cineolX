// Local B000 oracle: profile/indices, quiet origin and IRQ spans are observed.
// Structural bits come from the native graph and are checked against ROM.
// Work, READY waits, byte payloads and commits are independently predicted.
#include "xl_reference.hpp"
#include "../desktop/bank.hpp"
#include "../desktop/feedback_timing_xl.hpp"
#include "../desktop/feedback_clock_xl.hpp"
#include "../desktop/coefficient_structure_xl.hpp"
#include <emulator/trace.hpp>
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
struct Probe final:hw::Trace {
    hw::Host& h;Graph graph;const DiffusionProfile& prepared;
    WcsTimingXL live_bus,entry_bus;WcsTimingXL::Access expected_access{};
    bool seeded=false,bus_pending=false;uint64_t last_activity=0;
    unsigned grants=0,bus_errors=0,prefix_requests=0,prefix_compilers=0;
    DiffusionProfile profile{};std::array<uint8_t,128> low2{};FeedbackBytesXL initial{};
    uint8_t normal_index=0,stop_index=0,reduction=0;
    struct Actual {unsigned at=0,work=0;ControlByteXL payload{};uint64_t wall_start=0,commit=0;FeedbackBytesXL before{},after{};};std::array<Actual,45> writes{};
    FeedbackBytesXL pending_bytes{};
    struct Irq {unsigned at,duration;};std::array<Irq,32> irqs{};unsigned irq_count=0,irq_at=0;
    unsigned count=0,pending_at=0,calls=0,write_total=0,cost_errors=0,write_errors=0,state_errors=0,stage_errors=0,interrupts=0;
    unsigned reused_targets=0,half_calls=0,separate_calls=0,structure_checks=0;
    uint64_t begin=0,irq_begin=0,irq_states=0;
    uint16_t return_pc=0,return_sp=0,irq_return=0,irq_sp=0;
    bool active=false,pending=false,in_irq=false,stopping=false;
    Probe(hw::Host& host,Graph g,const DiffusionProfile& p):h(host),graph(g),prepared(p){}
    unsigned word(unsigned a)const{return h.memory[a]|unsigned(h.memory[a+1])<<8;}
    unsigned local(uint64_t t)const{return unsigned(t-begin-irq_states);}
    FeedbackBytesXL bytes()const{return {h.memory[0x3e35],h.memory[0x3e36],h.memory[0x3e37]};}
    void snapshot() {
        profile={};profile.count=h.memory[0x3cf9]&15;profile.separate_stop=!(h.memory[0x3df9]&64);profile.half_scale=h.memory[0x3e0a]!=0;
        normal_index=h.memory[0x3e3e];stop_index=h.memory[0x3e3f];reduction=h.memory[0x3e12];initial=bytes();
        const unsigned descriptors=word(0x3cf7);
        require(profile.count==prepared.count && profile.separate_stop==prepared.separate_stop,"feedback profile layout differs from prepared bank");
        for(unsigned i=0;i<profile.count;++i) {
            const unsigned a=word(descriptors+3*i);
            require(a>=0x4003 && a<0x4200 && (a&3)==3,"unsupported feedback descriptor");
            auto& target=profile.targets[i];target.row=uint8_t(127-(a-0x4000)/4);target.scale_cap=h.memory[descriptors+3*i+2];
            require(target.row==prepared.targets[i].row && target.scale_cap==prepared.targets[i].scale_cap,"feedback target differs from bank");
        }
        const unsigned shadow=word(0x3e05);
        for(unsigned i=0;i<profile.count;++i)for(unsigned j=0;j<3;++j) {
            const unsigned row=profile.targets[i].row+j;++structure_checks;
            require((h.memory[uint16_t(shadow+0x4003+(127-row)*4)]&3)==low2[row],"native feedback lane structure differs");
        }
    }
    void start() {
        low2=coefficient_lane_low2[unsigned(graph)];const unsigned shadow=word(0x3e05);
        for(unsigned row=0;row<graph_info(graph).rows;++row){++structure_checks;require((h.memory[uint16_t(shadow+0x4003+(127-row)*4)]&3)==low2[row],"native graph coefficient structure differs");}
        last_activity=h.cycles*hw::cpu_period;h.set_trace(this);
        for(unsigned pc:{0xb000u,0xb4fcu,0xb4fdu,0x38u})h.pc_watches[pc]=true;
        h.pc_observer=[this](uint64_t t,hw::CpuSnapshot cpu) {
            h.pc_watches[cpu.pc]=true;
            if(active && cpu.pc==0x38) {
                require(!in_irq,"nested feedback IRQ");in_irq=true;irq_begin=t-11;irq_at=local(t-11);
                irq_return=uint16_t(word(cpu.sp));irq_sp=uint16_t(cpu.sp+2);h.pc_watches[irq_return]=true;return;
            }
            if(in_irq) {
                if(cpu.pc!=irq_return || cpu.sp!=irq_sp)return;
                require(irq_count<irqs.size(),"feedback IRQ bound exceeded");
                const unsigned duration=unsigned(t-irq_begin);irqs[irq_count++]={irq_at,duration};irq_states+=duration;++interrupts;in_irq=false;
            }
            if(pending){require(count>0,"missing feedback write");writes[count-1].work=local(t)-pending_at;writes[count-1].after=bytes();pending=false;}
            if(cpu.pc==0xb000) {
                if(stopping)return;
                if(!seeded){++prefix_compilers;return;}
                require(!active,"nested feedback compiler");active=true;begin=t;count=0;irq_states=0;irq_count=0;entry_bus=live_bus;
                return_pc=uint16_t(word(cpu.sp));return_sp=uint16_t(cpu.sp+2);h.pc_watches[return_pc]=true;snapshot();return;
            }
            if(!active)return;
            if(cpu.pc==0xb4fc){pending=true;pending_at=local(t);pending_bytes=bytes();return;}
            if(cpu.pc!=return_pc || cpu.sp!=return_sp)return;
            unsigned used=0;auto predicted_bytes=initial;auto stage_bus=entry_bus;
            auto resume=[&](unsigned at)noexcept {uint64_t wall=begin+at;for(unsigned i=0;i<irq_count;++i)if(irqs[i].at<=at)wall+=irqs[i].duration;return wall;};
            tracking=true;
            const auto plan=timed_feedback(profile,low2,normal_index,stop_index,reduction,predicted_bytes,entry_bus,resume,
                [&](unsigned at,uint64_t start,ControlByteXL payload,const WcsTimingXL::Access& access)noexcept {
                    if(used>=count){++write_errors;return;}
                    const auto& actual=writes[used++];
                    if(actual.at!=at || actual.wall_start!=start || actual.payload.row!=payload.row || actual.payload.lane!=payload.lane || actual.payload.value!=payload.value ||
                        actual.commit!=access.commit || actual.work!=access.finished_cpu_state-start) {
                        if(write_errors++<3)std::cerr<<"feedback write at="<<at<<" vs "<<actual.at<<", row="<<payload.row<<" vs "<<actual.payload.row
                            <<", byte="<<unsigned(payload.value)<<" vs "<<unsigned(actual.payload.value)<<'\n';
                    }
                });
            auto stage_bytes=initial;FeedbackClockXL clock;clock.reset(profile,low2,normal_index,stop_index,reduction,stage_bytes);
            unsigned stage_used=0;
            auto same=[](FeedbackBytesXL a,FeedbackBytesXL b)noexcept{return a.tag==b.tag && a.outer==b.outer && a.middle==b.middle;};
            for(unsigned step=0;step<92 && clock.next().kind!=FeedbackClockXL::Kind::finished;++step) {
                const auto event=clock.next();
                if(event.kind==FeedbackClockXL::Kind::write) {
                    if(stage_used>=count){++stage_errors;break;}
                    const auto& actual=writes[stage_used++];const auto start=resume(unsigned(event.work_state));const auto access=stage_bus.access(start+4,false);
                    if(event.work_state!=actual.at || start!=actual.wall_start || event.payload.row!=actual.payload.row || event.payload.lane!=actual.payload.lane ||
                        event.payload.value!=actual.payload.value || access.commit!=actual.commit || access.finished_cpu_state-start!=actual.work || !same(stage_bytes,actual.before))++stage_errors;
                    clock.complete_write(unsigned(access.finished_cpu_state-start));
                } else {
                    require(stage_used>0,"invalid feedback write-return order");const auto& actual=writes[stage_used-1];
                    if(event.work_state!=uint64_t(actual.at)+actual.work || !same(stage_bytes,actual.after))++stage_errors;
                    clock.resume_after_write();
                }
            }
            if(clock.next().kind!=FeedbackClockXL::Kind::finished || clock.next().work_state!=local(t) || resume(unsigned(clock.next().work_state))!=t ||
                stage_used!=count || !same(stage_bytes,bytes()))++stage_errors;
            tracking=false;
            if(plan.work_states!=local(t) || plan.finished_state!=t){if(cost_errors++<3)std::cerr<<"feedback cost "<<plan.work_states<<" vs "<<local(t)<<", targets="<<unsigned(profile.count)<<'\n';}
            const auto actual=bytes();if(actual.tag!=predicted_bytes.tag || actual.outer!=predicted_bytes.outer || actual.middle!=predicted_bytes.middle)++state_errors;
            write_errors+=used!=count;++calls;write_total+=count;half_calls+=profile.half_scale;separate_calls+=profile.separate_stop;
            for(unsigned i=1;i<profile.count;++i)if(profile.targets[i].scale_cap==profile.targets[i-1].scale_cap &&
                !(profile.separate_stop && !profile.half_scale && profile.count-i<3))++reused_targets;
            active=false;
        };
        h.wcs_observer=[this](const hw::WcsWrite& w) {
            if(!active)return;
            require(pending && count<writes.size() && w.writer_pc==0xb4fc,"unexpected feedback WCS write");
            writes[count++]={pending_at,0,{127u-unsigned(w.address-0x4000)/4,unsigned(w.address&3),w.value},w.cpu_t1-4,w.committed_at,pending_bytes,{}};
        };
    }
    void stop(){require(!active && !pending && !in_irq && !bus_pending,"unfinished feedback observation");h.set_trace(nullptr);h.pc_observer={};h.wcs_observer={};h.pc_watches.fill(false);}
    void cpu_state(hw::Tick,uint64_t,uint64_t,bool)override{}
    void bus_command(hw::Tick,hw::BusCommand)override{}
    void row_phase(hw::Tick t,hw::RowPhase phase,uint64_t)override {
        if(seeded || phase!=hw::RowPhase::Fetch || h.dsp->pc!=0)return;
        const auto marker=t-hw::fetch_offset;
        if(marker<=last_activity+2*graph_info(graph).rows*hw::row)return;
        live_bus.reset_settled(graph,marker);seeded=true;
    }
    void fetch_displaced(hw::Tick,uint64_t)override{}
    void multiplicand_held(hw::Tick,hw::Hold)override{}
    void wcs_request(hw::Tick t,bool read,unsigned,unsigned)override {
        last_activity=t;if(!seeded){++prefix_requests;return;}
        require(!bus_pending,"overlapping feedback T&C predictions");
        const auto tick=t-(read?hw::cpu_period+hw::phi2_rise:2*hw::cpu_period);require(tick%hw::cpu_period==0,"nonintegral feedback bus T1");
        tracking=true;expected_access=live_bus.access(tick/hw::cpu_period,read);tracking=false;bus_pending=true;
    }
    void wcs_grant(hw::Tick t,hw::Tick ack)override {
        last_activity=t;if(!seeded)return;require(bus_pending,"missing feedback grant prediction");
        if(t!=expected_access.grant || ack!=expected_access.ack)++bus_errors;bus_pending=false;++grants;
    }
    void wcs_read_drive(hw::Tick,hw::Tick)override{}
    void wcs_commit(hw::Tick,unsigned,unsigned,uint8_t)override{}
    void aruck(hw::Tick,unsigned)override{}
    void history_sample(hw::Tick,unsigned)override{}
    void dport(hw::Tick,hw::DportEvent)override{}
};
static Task<void> toggle(Machine& m,LarcOperator& op,unsigned which,bool enabled) {
    const unsigned mask=which==0?1:128;
    for(unsigned n=0;n<8;++n){co_await op.setToggle(int(which),enabled);co_await m.sleep(0.2);if(bool(m.peek(0x3ccd)&mask)==enabled)co_return;}
    co_await fail("feedback physical mode did not settle");
}
static Task<void> fixture(Machine& m,LarcOperator& op,const ProgramData& p,unsigned mode) {
    co_await toggle(m,op,0,bool(mode&1));co_await toggle(m,op,2,bool(mode&2));
    for(unsigned page=0;page<p.page_count;++page)for(unsigned slot=0;slot<6;++slot) {
        const unsigned cell=p.pages[page].cells[slot],stop=p.dynamics.shared_stop?6:12;
        if(cell==stop || cell==stop+1)co_await op.moveSlider(int(page+1),slot,18);
        if(cell==45)co_await op.moveSlider(int(page+1),slot,10);
    }
    co_await m.sleep(0.5);
}
static Task<void> move_mid(Machine& m,LarcOperator& op,unsigned page,unsigned slot,unsigned raw) {
    co_await op.moveSlider(int(page),slot,raw);co_await m.sleep(0.2);
}
static Task<void> finish_program(Machine& m,LarcOperator& op) {
    co_await toggle(m,op,0,false);co_await toggle(m,op,2,false);co_await m.sleep(1);
}
int main(int argc,char** argv) {
    require(argc==3 || argc==4,"usage: cineol_xl_feedback_timing_check ROM_DIRECTORY PREPARED_BANK [PROGRAM_INDEX]");
    const int selected=argc==4?std::atoi(argv[3]):-1;require(selected>=-1 && selected<22,"invalid feedback program");
    std::ifstream input(argv[2],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    auto bank=std::make_unique<Bank>();require(read_bank(bytes.data(),bytes.size(),*bank),"invalid feedback bank");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);auto m=std::make_unique<Machine>(*engine);LarcOperator op(*m);
    require(!m->run_task([&]{return xl_test::boot(*m,op);}).failed,"feedback boot failed");
    unsigned total_calls=0,total_writes=0,total_errors=0;
    for(unsigned i=0;i<22;++i) {
        if(selected>=0 && selected!=int(i))continue;const auto info=graphs[i];const auto& data=bank->programs[i];
        require(!m->run_task([&]{return xl_test::select(*m,op,info.bank,info.program);}).failed,"feedback selection failed");
        for(unsigned mode=0;mode<(data.dynamics.enabled?4u:1u);++mode) {
            require(!m->run_task([&]{return fixture(*m,op,data,mode);}).failed,"feedback fixture failed");
            Probe probe{engine->host(),Graph(i),data.controls.feedback};probe.start();
            std::array<float,64> zero{};std::array<std::array<float,64>,4> out{};float* channels[]={out[0].data(),out[1].data(),out[2].data(),out[3].data()};
            for(unsigned frame=0;!probe.seeded && frame<4800;frame+=64)require(m->render(zero.data(),zero.data(),channels,64),"feedback seed failed");
            require(probe.seeded,"feedback quiet origin unavailable");
            if(data.controls.feedback.count)for(unsigned page=0;page<data.page_count;++page)for(unsigned slot=0;slot<6;++slot)if(data.pages[page].cells[slot]==1) {
                for(unsigned raw:{64u,232u})require(!m->run_task([&]{return move_mid(*m,op,page+1,slot,raw);}).failed,"feedback MID edge failed");
            }
            for(unsigned frame=0;frame<96000;frame+=64) {
                const unsigned phase=frame%24000,bits=phase<4000?15:phase<8000?3:phase<12000?0:phase<16000?31:0;
                engine->host().set_level_detectors(0,bits);engine->host().set_level_detectors(1,bits);
                require(m->render(zero.data(),zero.data(),channels,64),"feedback render failed");
            }
            probe.stopping=true;for(unsigned frame=0;(probe.active || probe.bus_pending) && frame<4800;frame+=64)require(m->render(zero.data(),zero.data(),channels,64),"feedback drain failed");
            probe.stop();
            if(data.controls.feedback.count)require(probe.calls>0,"no physical feedback compiler calls");
            std::cout<<info.name<<", mode="<<mode<<": calls="<<probe.calls<<", writes="<<probe.write_total<<", work errors="<<probe.cost_errors<<", write errors="<<probe.write_errors
                <<", state errors="<<probe.state_errors<<", stage errors="<<probe.stage_errors<<", bus errors="<<probe.bus_errors<<", grants="<<probe.grants<<", IRQ="<<probe.interrupts
                <<", reused targets="<<probe.reused_targets<<", half calls="<<probe.half_calls<<", separate calls="<<probe.separate_calls
                <<", structure checks="<<probe.structure_checks
                <<", pre-seed requests="<<probe.prefix_requests<<", pre-seed compilers="<<probe.prefix_compilers<<std::endl;
            total_calls+=probe.calls;total_writes+=probe.write_total;total_errors+=probe.cost_errors+probe.write_errors+probe.state_errors+probe.stage_errors+probe.bus_errors;
        }
        require(!m->run_task([&]{return finish_program(*m,op);}).failed,"feedback program cleanup failed");
    }
    std::cout<<"Native XL feedback: "<<total_calls<<" calls, "<<total_writes<<" writes; predicted compiler work/READY/commits, observed profile/indices/structural bits/IRQ/origin; not free-running scan\n";
    require(!allocations && !releases,"native feedback work touched heap");require(!total_errors,"native feedback compiler differs");
}
