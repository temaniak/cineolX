// Local fast oracle. Entry context, comparator stimulus and IRQ are observed.
// Feedback work can be native; restore work remains a diagnostic input.
// Read/compile boundaries, intermediate transitions and final state are checked.
#include "xl_reference.hpp"
#include "../desktop/bank.hpp"
#include "../desktop/fast_control_timing_xl.hpp"
#include "../desktop/fast_control_clock_xl.hpp"
#include "../desktop/fast_feedback_timing_xl.hpp"
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
struct Probe final:hw::Trace {
    hw::Host& h;const ProgramData& data;
    Graph graph;bool predict_feedback=false,seeded=false,bus_pending=false;
    uint64_t last_activity=0;WcsTimingXL live_bus,native_bus;WcsTimingXL::Access expected_access{};
    unsigned bus_errors=0,grants=0,prefix_calls=0,prefix_requests=0,prediction_errors=0,predicted_feedback=0,structure_checks=0;
    DiffusionProfile feedback_profile{};std::array<uint8_t,128> low2{};FeedbackBytesXL feedback_bytes{};
    uint8_t normal_index=0,other_index=0;
    struct Irq {unsigned at,duration;};std::array<Irq,32> irq_spans{};unsigned irq_span_count=0,irq_work_at=0;
    std::array<unsigned,130> predicted_work{};
    struct Byte {unsigned at;uint64_t start,commit;ControlByteXL payload;};std::array<Byte,45*128> feedback_writes{};
    unsigned feedback_write_count=0,predicted_writes=0,write_prediction_errors=0;
    struct Call {unsigned at=0,work=0;uint16_t pc=0;FastControlMemoryXL before{},after{};};std::array<Call,130> compiler{};
    struct Read {unsigned at,port;uint8_t value;};std::array<Read,2> reads{};
    unsigned compiler_count=0,read_count=0,calls=0,errors=0,cost_errors=0,state_errors=0,boundary_errors=0,stage_errors=0;
    unsigned irq_count=0,feedback_count=0,restore_count=0,max_feedback=0,early=0,active_calls=0;
    unsigned pending_at=0,special=0,reset_period=0;
    uint16_t return_pc=0,return_sp=0,nested_pc=0,nested_sp=0,irq_return=0,irq_sp=0;
    uint64_t begin=0,irq_begin=0,irq_states=0;
    FastControlMemoryXL initial{};std::array<uint8_t,48> raw{};
    bool active=false,nested=false,in_irq=false,stopping=false;
    Probe(hw::Host& host,const ProgramData& p,Graph g,bool predict):h(host),data(p),graph(g),predict_feedback(predict){}
    unsigned word(unsigned a)const{return h.memory[a]|unsigned(h.memory[a+1])<<8;}
    unsigned local(uint64_t t)const{return unsigned(t-begin-irq_states);}
    void start() {
        if(predict_feedback) {
            low2=coefficient_lane_low2[unsigned(graph)];const unsigned shadow=word(0x3e05);
            for(unsigned row=0;row<graph_info(graph).rows;++row){++structure_checks;require((h.memory[uint16_t(shadow+0x4003+(127-row)*4)]&3)==low2[row],"native fast coefficient structure differs");}
            last_activity=h.cycles*hw::cpu_period;h.set_trace(this);
        }
        for(unsigned pc:{0x81b6u,0xafe4u,0xa7a9u,0x38u})h.pc_watches[pc]=true;
        h.pc_observer=[this](uint64_t t,hw::CpuSnapshot cpu) {
            h.pc_watches[cpu.pc]=true;
            if(!active) {
                if(cpu.pc!=0x81b6 || stopping)return;
                if(predict_feedback && !seeded){++prefix_calls;return;}
                active=true;begin=t;irq_states=0;compiler_count=read_count=0;initial=snapshot(h);
                irq_span_count=feedback_write_count=0;
                special=h.memory[0x3e07];reset_period=h.memory[0x3c5c];std::copy_n(h.memory.begin()+0x3ca3,48,raw.begin());
                if(predict_feedback) {
                    native_bus=live_bus;feedback_profile=data.controls.feedback;feedback_profile.half_scale=h.memory[0x3e0a]!=0;
                    normal_index=h.memory[0x3e3e];other_index=h.memory[0x3e3f];feedback_bytes={h.memory[0x3e35],h.memory[0x3e36],h.memory[0x3e37]};
                }
                return_pc=uint16_t(word(cpu.sp));return_sp=uint16_t(cpu.sp+2);h.pc_watches[return_pc]=true;return;
            }
            if(cpu.pc==0x38) {
                require(!in_irq,"nested fast-controller IRQ");in_irq=true;irq_begin=t-11;
                irq_work_at=local(t-11);
                irq_return=uint16_t(word(cpu.sp));irq_sp=uint16_t(cpu.sp+2);h.pc_watches[irq_return]=true;return;
            }
            if(in_irq) {
                if(cpu.pc!=irq_return || cpu.sp!=irq_sp)return;
                require(irq_span_count<irq_spans.size(),"fast local IRQ bound exceeded");
                irq_spans[irq_span_count++]={irq_work_at,unsigned(t-irq_begin)};
                irq_states+=t-irq_begin;in_irq=false;++irq_count;
            }
            if(nested) {
                if(cpu.pc!=nested_pc || cpu.sp!=nested_sp)return;
                auto& c=compiler[compiler_count-1];c.work=local(t)-c.at;c.after=snapshot(h);nested=false;
            }
            if(cpu.pc==0xafe4 || cpu.pc==0xa7a9) {
                require(compiler_count<compiler.size(),"fast compiler bound exceeded");
                compiler[compiler_count++]={local(t),0,cpu.pc,snapshot(h),{}};nested=true;
                nested_pc=uint16_t(word(cpu.sp));nested_sp=uint16_t(cpu.sp+2);h.pc_watches[nested_pc]=true;return;
            }
            if(cpu.pc!=return_pc || cpu.sp!=return_sp)return;
            require(!nested,"unfinished fast compiler");unsigned used_calls=0,used_reads=0,local_feedback=0;
            unsigned used_feedback_writes=0;
            auto resume=[&](unsigned at)noexcept {uint64_t wall=begin+at;for(unsigned i=0;i<irq_span_count;++i)if(irq_spans[i].at<=at)wall+=irq_spans[i].duration;return wall;};
            auto native=initial;
            auto compile=[&](unsigned at,DynamicsState& s,unsigned pc) noexcept {
                if(used_calls>=compiler_count){++boundary_errors;return 0u;}
                const unsigned call_index=used_calls++;const auto& actual=compiler[call_index];
                if(actual.at!=at || actual.pc!=pc)++boundary_errors;
                native.dynamics=s;
                if(!same(native,actual.before))++state_errors;
                unsigned result_work=actual.work;
                if(pc==0xafe4) {
                    ++feedback_count;++local_feedback;
                    if(predict_feedback) {
                        const auto predicted=feedback_from_fast(feedback_profile,low2,normal_index,other_index,s.amount,feedback_bytes,at,native_bus,resume,
                            [&](unsigned write_at,uint64_t start,ControlByteXL payload,const WcsTimingXL::Access& access)noexcept {
                                ++predicted_writes;
                                if(used_feedback_writes>=feedback_write_count){++write_prediction_errors;return;}
                                const auto& w=feedback_writes[used_feedback_writes++];
                                if(w.at!=write_at || w.start!=start || w.commit!=access.commit || w.payload.row!=payload.row || w.payload.lane!=payload.lane || w.payload.value!=payload.value)++write_prediction_errors;
                            });
                        result_work=predicted.work_states;++predicted_feedback;
                        if(result_work!=actual.work || predicted.finished_state!=resume(at+actual.work))++prediction_errors;
                    }
                }
                else {++restore_count;s.amount=1;s.peak_input=uint8_t(ControlProfile::decay_index(s.mid));}
                s.period=Dynamics::normal_period(data.dynamics,data.controls,raw,s.peak_input);
                s.feedback_mid=s.mid;s.feedback_amount=s.amount;
                native.dynamics=s;
                if(!same(native,actual.after))++state_errors;
                predicted_work[call_index]=result_work;
                return result_work; // Restore remains observed; feedback can be native.
            };
            tracking=true;
            const unsigned predicted=fast_control_work(native,uint8_t(special),uint8_t(reset_period),raw,
                [&](unsigned port,unsigned at)noexcept {
                    if(used_reads>=read_count){++boundary_errors;return uint8_t(0);}
                    const auto actual=reads[used_reads++];if(actual.at!=at || actual.port!=port)++boundary_errors;return actual.value;
                },[&](unsigned at,DynamicsState& s)noexcept{return compile(at,s,0xafe4);},
                [&](unsigned at,DynamicsState& s)noexcept{return compile(at,s,0xa7a9);});
            auto shadow=initial;FastControlClockXL clock;clock.reset(shadow,0,uint8_t(special),uint8_t(reset_period),raw);
            unsigned stage_calls=0,stage_reads=0;
            for(unsigned step=0;step<262 && clock.next().kind!=FastControlClockXL::Kind::finished;++step) {
                const auto event=clock.next();using Kind=FastControlClockXL::Kind;
                if(event.kind==Kind::left || event.kind==Kind::right) {
                    if(stage_reads>=read_count){++stage_errors;break;}
                    const auto actual=reads[stage_reads++];
                    if(actual.at!=event.work_state || actual.port!=(event.kind==Kind::left?8u:9u))++stage_errors;
                    clock.complete_read(actual.value);
                } else if(event.kind==Kind::feedback_return || event.kind==Kind::restore_return) {
                    if(!stage_calls){++stage_errors;break;}
                    const auto& actual=compiler[stage_calls-1];
                    if(event.work_state!=uint64_t(actual.at)+actual.work || !same(shadow,actual.after))++stage_errors;
                    clock.resume_after_compiler();
                } else {
                    if(stage_calls>=compiler_count){++stage_errors;break;}
                    const auto& actual=compiler[stage_calls++];const unsigned pc=event.kind==Kind::feedback?0xafe4:0xa7a9;
                    if(actual.at!=event.work_state || actual.pc!=pc || !same(shadow,actual.before))++stage_errors;
                    const uint8_t marker=event.kind==Kind::restore?uint8_t(ControlProfile::decay_index(shadow.dynamics.mid)):shadow.dynamics.peak_input;
                    const uint8_t period=Dynamics::normal_period(data.dynamics,data.controls,raw,marker);
                    clock.complete_compiler(predicted_work[stage_calls-1],period);
                }
            }
            if(clock.next().kind!=FastControlClockXL::Kind::finished || clock.next().work_state!=local(t) || !same(shadow,snapshot(h)) ||
                stage_calls!=compiler_count || stage_reads!=read_count)++stage_errors;
            tracking=false;
            if(predicted!=local(t)){if(cost_errors++<3)std::cerr<<"fast work "<<predicted<<" vs "<<local(t)<<", flags="<<unsigned(initial.dynamics.flags)
                <<", amount="<<unsigned(initial.dynamics.amount)<<'\n';}
            if(!same(native,snapshot(h)))++state_errors;
            if(used_calls!=compiler_count || used_reads!=read_count)++boundary_errors;
            if(predict_feedback && used_feedback_writes!=feedback_write_count)++write_prediction_errors;
            max_feedback=std::max(max_feedback,local_feedback);early+=!read_count;active_calls+=bool(read_count);++calls;active=false;
        };
        h.port_trace=[this](uint64_t t,uint16_t pc,bool write,unsigned port,uint8_t value) {
            if(!active || write || (pc!=0x81d4 && pc!=0x81d9))return;
            require(!nested && !in_irq && read_count<reads.size(),"unexpected fast detector read");
            if(hw::state_start(h.cycles)+hw::phi2_rise!=(t+9)*hw::cpu_period+hw::phi2_rise)++boundary_errors;
            reads[read_count++]={local(t),port,value};
        };
        if(predict_feedback)h.wcs_observer=[this](const hw::WcsWrite& w) {
            if(!active || !nested || compiler[compiler_count-1].pc!=0xafe4)return;
            require(feedback_write_count<feedback_writes.size(),"fast feedback write bound exceeded");
            feedback_writes[feedback_write_count++]={local(w.cpu_t1-4),w.cpu_t1-4,w.committed_at,{127u-unsigned(w.address-0x4000)/4,unsigned(w.address&3),w.value}};
        };
    }
    void stop(){require(!active && !nested && !in_irq && !bus_pending,"unfinished fast call");h.set_trace(nullptr);h.pc_observer={};h.port_trace={};h.wcs_observer={};h.pc_watches.fill(false);}
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
        last_activity=t;if(!seeded){++prefix_requests;return;}
        require(!bus_pending,"overlapping fast feedback grants");const auto tick=t-(read?hw::cpu_period+hw::phi2_rise:2*hw::cpu_period);
        require(tick%hw::cpu_period==0,"nonintegral fast feedback T1");tracking=true;expected_access=live_bus.access(tick/hw::cpu_period,read);tracking=false;bus_pending=true;
    }
    void wcs_grant(hw::Tick t,hw::Tick ack)override {
        last_activity=t;if(!seeded)return;require(bus_pending,"missing fast feedback grant");
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
    for(unsigned n=0;n<8;++n) {
        co_await op.setToggle(int(which),enabled);co_await m.sleep(0.2);
        if(bool(m.peek(0x3ccd)&mask)==enabled)co_return;
    }
    co_await fail("fast timing physical mode did not settle");
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
int main(int argc,char** argv) {
    const bool predict_feedback=argc>3 && std::string(argv[argc-1])=="--predicted-feedback";if(predict_feedback)--argc;
    require(argc==3 || argc==4,"usage: cineol_xl_fast_timing_check ROM_DIRECTORY PREPARED_BANK [PROGRAM_INDEX] [--predicted-feedback]");
    const int selected=argc==4?std::atoi(argv[3]):-1;require(selected>=-1 && selected<22,"invalid fast program");
    std::ifstream input(argv[2],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    auto bank=std::make_unique<Bank>();require(read_bank(bytes.data(),bytes.size(),*bank),"invalid fast timing bank");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);auto m=std::make_unique<Machine>(*engine);LarcOperator op(*m);
    require(!m->run_task([&]{return xl_test::boot(*m,op);}).failed,"fast timing boot failed");
    unsigned calls=0,errors=0,feedback=0,restore=0;
    for(unsigned i=0;i<22;++i) {
        if(selected>=0 && selected!=int(i))continue;const auto info=graphs[i];const auto& p=bank->programs[i];
        require(!m->run_task([&]{return xl_test::select(*m,op,info.bank,info.program);}).failed,"fast timing select failed");
        for(unsigned mode=0;mode<(p.dynamics.enabled?4u:1u);++mode) {
            require(!m->run_task([&]{return fixture(*m,op,p,mode);}).failed,"fast timing fixture failed");
            Probe probe{engine->host(),p,Graph(i),predict_feedback};probe.start();
            std::array<float,64> zero{};std::array<std::array<float,64>,4> out{};float* channels[]={out[0].data(),out[1].data(),out[2].data(),out[3].data()};
            for(unsigned frame=0;frame<96000;frame+=64) {
                const unsigned phase=frame%24000,bits=phase<4000?15:phase<8000?3:phase<12000?0:phase<16000?31:0;
                engine->host().set_level_detectors(0,bits);engine->host().set_level_detectors(1,bits);
                require(m->render(zero.data(),zero.data(),channels,64),"fast timing render failed");
            }
            probe.stopping=true;for(unsigned frame=0;(probe.active || probe.bus_pending) && frame<4800;frame+=64)require(m->render(zero.data(),zero.data(),channels,64),"fast timing drain failed");
            probe.stop();require(probe.calls>=100,"insufficient fast timing calls");
            std::cout<<info.name<<", mode="<<mode<<": calls="<<probe.calls<<", work errors="<<probe.cost_errors<<", state errors="<<probe.state_errors
                <<", boundary errors="<<probe.boundary_errors<<", stage errors="<<probe.stage_errors<<", early="<<probe.early<<", active="<<probe.active_calls
                <<", feedback="<<probe.feedback_count<<", max feedback/call="<<probe.max_feedback<<", restore="<<probe.restore_count<<", IRQ="<<probe.irq_count
                <<", predicted feedback="<<probe.predicted_feedback<<", prediction errors="<<probe.prediction_errors<<", predicted writes="<<probe.predicted_writes
                <<", write prediction errors="<<probe.write_prediction_errors<<", bus errors="<<probe.bus_errors<<", grants="<<probe.grants<<", pre-seed calls="<<probe.prefix_calls
                <<", structure checks="<<probe.structure_checks<<std::endl;
            calls+=probe.calls;errors+=probe.cost_errors+probe.state_errors+probe.boundary_errors+probe.stage_errors+probe.prediction_errors+probe.write_prediction_errors+probe.bus_errors;feedback+=probe.feedback_count;restore+=probe.restore_count;
        }
    }
    std::cout<<"Native XL fast work: "<<calls<<" calls, "<<feedback<<" feedback compilations, "<<restore<<" normal-decay restorations; "
        <<(predict_feedback?"predicted feedback work/writes; observed restore work, context, IRQ/origin":"observed compiler work/IRQ")<<" and comparator stimulation, not free-running scan\n";
    require(!allocations && !releases,"fast native work touched heap");require(!errors,"fast native work differs");
}
