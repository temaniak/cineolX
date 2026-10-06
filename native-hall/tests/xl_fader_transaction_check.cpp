// Physical offline fader oracle. Entry calibration/context and IRQ spans are
// observed locally; pickup and completion work/state are native predictions.
#include "xl_reference.hpp"
#include "../desktop/bank.hpp"
#include "../desktop/fader_transaction_xl.hpp"
#include "../desktop/fader_calibration_xl.hpp"
#include <new>
using namespace lexplug;using namespace lexplug::op;
using namespace cineol::xl;using xl_test::require;
namespace hw=lexicon224x::cpu;
static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n){if(tracking)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(tracking && p)++releases;std::free(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
static unsigned word(const hw::Host& h,unsigned a){return h.memory[a]|unsigned(h.memory[uint16_t(a+1)])<<8;}
static DynamicsState snapshot(const hw::Host& h) {
    DynamicsState s;s.held=uint16_t(word(h,0x3e0f));s.average=h.memory[0x3c50];s.flags=h.memory[0x3c51];
    s.low=h.memory[0x3c52];s.mid=h.memory[0x3c53];s.trigger_peak=h.memory[0x3c54];s.stop_counter=h.memory[0x3c5f];
    s.stopped=h.memory[0x3e11];s.amount=h.memory[0x3e12];s.divider=h.memory[0x3e13];s.period=h.memory[0x3e14];
    s.peak_divider=h.memory[0x3c38];s.peak_input=h.memory[0x3c61];std::copy_n(h.memory.begin()+0x3e15,11,s.history.begin());return s;
}
static bool same(const DynamicsState& a,const DynamicsState& b) {
    return a.held==b.held && a.average==b.average && a.flags==b.flags && a.low==b.low && a.mid==b.mid &&
        a.trigger_peak==b.trigger_peak && a.stop_counter==b.stop_counter && a.stopped==b.stopped && a.amount==b.amount &&
        a.divider==b.divider && a.period==b.period && a.peak_divider==b.peak_divider && a.peak_input==b.peak_input && a.history==b.history;
}
static bool retained(const DynamicsState& a,const DynamicsState& b) {
    return a.held==b.held && a.average==b.average && a.low==b.low && a.mid==b.mid && a.trigger_peak==b.trigger_peak &&
        a.stop_counter==b.stop_counter && a.stopped==b.stopped && a.divider==b.divider && a.peak_divider==b.peak_divider && a.history==b.history;
}
struct Probe {
    hw::Host& h;
    struct Call {bool active=false;uint64_t begin=0,irq=0;uint16_t pc=0,sp=0;};
    Call pickup,finish,transaction,handler,calibration;bool in_irq=false;uint64_t irq_begin=0,irq_total=0;uint16_t irq_pc=0,irq_sp=0;
    FaderCalibrationMemoryXL calibrated{};FaderCalibrationWorkXL calibration_work{};
    uint16_t calibration_logical=0,calibration_pickup=0;
    unsigned calibrations=0,calibration_skipped=0,calibration_work_errors=0,calibration_state_errors=0;
    std::array<unsigned,14> calibration_types{};
    FaderPickupMemoryXL predicted{};FaderPickupWorkXL pickup_work{};uint16_t logical_address=0,pickup_address=0;
    DynamicsState completed{},before{};unsigned finish_work=0;
    std::array<uint8_t,3> headroom{};uint16_t peak=0;unsigned nested_slow=0,nested_fast=0;
    unsigned pickups=0,accepted=0,rejected=0,finishes=0,transactions=0,handlers=0,main_returns=0,compilers=0,irq_count=0,work_errors=0,state_errors=0,retention_errors=0,stopped_requests=0;
    std::array<unsigned,5> pickup_states{};
    std::array<unsigned,4> pickup_paths{};
    void begin(Call& c,uint64_t t,hw::CpuSnapshot cpu) {require(!c.active,"nested fader subworker");c={true,t,irq_total,uint16_t(word(h,cpu.sp)),uint16_t(cpu.sp+2)};h.pc_watches[c.pc]=true;}
    bool returned(const Call& c,hw::CpuSnapshot cpu)const{return c.active && cpu.pc==c.pc && cpu.sp==c.sp;}
    void work(const Call& c,uint64_t t,unsigned expected) {
        const auto actual=t-c.begin-(irq_total-c.irq);
        if(actual!=expected && work_errors++<8)std::cerr<<"fader work "<<expected<<'/'<<actual<<'\n';
    }
    void start() {
        for(unsigned pc:{0x85f2u,0x8614u,0x8627u,0x8649u,0x87a0u,0xa791u,0x82cfu,0x81b6u,0x38u})h.pc_watches[pc]=true;
        h.pc_observer=[this](uint64_t t,hw::CpuSnapshot cpu) {
            h.pc_watches[cpu.pc]=true;
            if(cpu.pc==0x38){
                // Only subtract interrupts inside the two locally timed
                // workers. Receive/parser control flow outside them is not
                // represented by this boundary oracle.
                if(!pickup.active && !finish.active && !calibration.active)return;
                require(!in_irq,"nested fader work IRQ");in_irq=true;irq_begin=t-11;
                irq_pc=uint16_t(word(h,cpu.sp));irq_sp=uint16_t(cpu.sp+2);h.pc_watches[irq_pc]=true;return;
            }
            if(in_irq){if(cpu.pc!=irq_pc || cpu.sp!=irq_sp)return;irq_total+=t-irq_begin;in_irq=false;++irq_count;}
            if(returned(calibration,cpu)) {
                const auto actual=t-calibration.begin-(irq_total-calibration.irq);
                if(actual!=calibration_work.work_states && calibration_work_errors++<8)
                    std::cerr<<"calibration work "<<calibration_work.work_states<<'/'<<actual<<'\n';
                if(h.memory[calibration_logical]!=calibrated.pickup.logical || h.memory[calibration_pickup]!=calibrated.pickup.pickup ||
                    h.memory[0x3e3d]!=calibrated.limit || uint8_t(cpu.bc)!=calibrated.physical || bool(cpu.flags&1)!=calibration_work.accepted)++calibration_state_errors;
                ++calibrations;calibration.active=false;
            }
            if(returned(pickup,cpu)) {
                work(pickup,t,pickup_work.work_states);
                if(h.memory[logical_address]!=predicted.logical || h.memory[pickup_address]!=predicted.pickup || bool(cpu.flags&1)!=pickup_work.accepted)++state_errors;
                ++pickups;accepted+=pickup_work.accepted;rejected+=!pickup_work.accepted;pickup.active=false;
            }
            if(returned(finish,cpu)){work(finish,t,finish_work);if(!same(completed,snapshot(h)))++state_errors;++finishes;finish.active=false;}
            if(returned(transaction,cpu)) {
                const std::array<uint8_t,3> got{h.memory[0x3c63],h.memory[0x3c6a],h.memory[0x3c62]};
                if(!retained(before,snapshot(h)) || headroom!=got || peak!=word(h,0x3e38) || nested_slow || nested_fast)++retention_errors;
                ++transactions;transaction.active=false;
            }
            if(returned(handler,cpu)){++handlers;main_returns+=cpu.pc==0x81a1;handler.active=false;}
            if(cpu.pc==0x85f2)begin(handler,t,cpu);
            if(handler.active && (cpu.pc==0x82cf || cpu.pc==0x81b6))++retention_errors;
            if(cpu.pc==0x8614){begin(transaction,t,cpu);before=snapshot(h);stopped_requests+=bool(before.flags&1);headroom={h.memory[0x3c63],h.memory[0x3c6a],h.memory[0x3c62]};peak=uint16_t(word(h,0x3e38));nested_slow=nested_fast=0;}
            if(transaction.active && cpu.pc==0x82cf)++nested_slow;
            if(transaction.active && cpu.pc==0x81b6)++nested_fast;
            if(transaction.active && cpu.pc==0xa791)++compilers;
            if(cpu.pc==0x8649) {
                const uint8_t type=h.memory[0x3c33],maximum=h.memory[uint16_t(word(h,0x3e03) + 0xc35e + cpu.hl)];
                if(ordinary_fader_calibration(type,uint8_t(cpu.bc>>8),maximum)) {
                    begin(calibration,t,cpu);calibration_logical=cpu.hl;calibration_pickup=cpu.de;
                    calibrated={{h.memory[cpu.hl],h.memory[cpu.de]},0,0};
                    tracking=true;calibration_work=calibrate_fader_work(calibrated,type,uint8_t(cpu.bc>>8),maximum,uint8_t(cpu.bc));tracking=false;
                    if(type<calibration_types.size())++calibration_types[type];
                } else ++calibration_skipped;
            }
            if(cpu.pc==0x87a0) {
                begin(pickup,t,cpu);logical_address=cpu.hl;pickup_address=cpu.de;predicted={h.memory[cpu.hl],h.memory[cpu.de]};
                if(predicted.pickup<pickup_states.size())++pickup_states[predicted.pickup];
                tracking=true;pickup_work=fader_pickup_work(predicted,cpu.a);tracking=false;
                if(pickup_work.work_states==64)++pickup_paths[0];
                else if(pickup_work.work_states==93)++pickup_paths[1];
                else if(pickup_work.work_states==99)++pickup_paths[2];
                else if(pickup_work.work_states==120)++pickup_paths[3];
                else require(false,"unexpected pickup work path");
            }
            if(cpu.pc==0x8627){
                require(transaction.active,"fader suffix without transaction");
                begin(finish,t,cpu);finish.pc=transaction.pc;finish.sp=transaction.sp;
                completed=snapshot(h);tracking=true;finish_work=finish_fader_transaction(completed);tracking=false;
            }
        };
    }
    void stop(){require(!pickup.active && !finish.active && !transaction.active && !handler.active && !calibration.active && !in_irq,"unfinished fader worker");h.pc_observer={};h.pc_watches.fill(false);}
};
static Task<void> active_setup(Machine& m,LarcOperator& op,const ProgramData& data) {
    for(unsigned attempt=0;attempt<8;++attempt){co_await op.setToggle(0,true);co_await m.sleep(.2);if(m.peek(0x3ccd)&1)break;}
    if(!(m.peek(0x3ccd)&1))co_await fail("fader Dynamic mode did not settle");
    const unsigned stop=data.dynamics.shared_stop?6:12;
    for(unsigned cell:{stop,stop+1,45u})for(unsigned p=0;p<data.page_count;++p)for(unsigned slot=0;slot<6;++slot)
        if(data.pages[p].cells[slot]==cell)co_await op.moveSlider(int(p+1),slot,cell==45?10:18);
    co_await op.gotoPage(1);co_await m.sleep(.2);
}
static Task<void> fixture(Machine& m,LarcOperator& op,const ProgramData& data) {
    co_await op.gotoPage(1);
    // Direct LARC fader messages preserve the firmware's soft pickup state.
    // No RAM seeds or operator takeOver helper are used for these sweeps.
    for(unsigned slot:{0u,1u}) {
        const unsigned cell=data.pages[0].cells[slot];
        if(cell<48){m.fader(slot,m.peek(uint16_t(0x3ca3+cell)));co_await m.sleep(.18);}
        for(unsigned raw:{2u,64u,254u,120u,2u,254u}){m.fader(slot,raw);co_await m.sleep(.18);}
    }
    for(unsigned cell:{data.dynamics.shared_stop?6u:12u,data.dynamics.shared_stop?7u:13u,45u}) {
        if(!data.control_active(cell))continue;
        for(unsigned p=0;p<data.page_count;++p)for(unsigned slot=0;slot<6;++slot)if(data.pages[p].cells[slot]==cell) {
            co_await op.gotoPage(int(p+1));for(unsigned raw:{2u,18u,120u,254u}){m.fader(slot,raw);co_await m.sleep(.18);}
        }
    }
    co_await op.gotoPage(1);co_await m.sleep(.5);
}
static Task<void> numeric_pages_fixture(Machine& m,LarcOperator& op,const ProgramData& data) {
    for(unsigned page=0;page<data.page_count;++page) {
        const auto& p=data.pages[page];if(p.type==5)continue;
        co_await op.gotoPage(int(page+1));
        for(unsigned slot=0;slot<6;++slot) {
            const unsigned cell=p.cells[slot];if(cell>=48 || !data.control_active(cell))continue;
            m.fader(slot,m.peek(uint16_t(0x3ca3+cell)));co_await m.sleep(.18);
            for(unsigned raw:{2u,8u,64u,254u}) {m.fader(slot,raw);co_await m.sleep(.18);}
        }
    }
    co_await op.gotoPage(1);co_await m.sleep(.5);
}
int main(int argc,char** argv) {
    bool active_tail=false,numeric_pages=false;
    while(argc>3) {
        const std::string option=argv[argc-1];
        if(option=="--active-tail")active_tail=true;
        else if(option=="--numeric-pages")numeric_pages=true;
        else break;
        --argc;
    }
    require(argc==3 || argc==4,"usage: cineol_xl_fader_transaction_check ROM_DIRECTORY PREPARED_BANK [PROGRAM_INDEX] [--active-tail] [--numeric-pages]");
    const int selected=argc==4?std::atoi(argv[3]):-1;require(selected>=-1 && selected<22,"invalid fader program");
    std::ifstream input(argv[2],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    auto bank=std::make_unique<Bank>();require(read_bank(bytes.data(),bytes.size(),*bank),"invalid fader bank");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);auto m=std::make_unique<Machine>(*engine);auto op=std::make_unique<LarcOperator>(*m);
    require(!m->run_task([&]{return xl_test::boot(*m,*op);}).failed,"fader boot failed");unsigned calls=0,errors=0;bool first=true;
    for(unsigned i=0;i<graphs.size();++i) {
        if(selected>=0 && selected!=int(i))continue;const auto info=graphs[i];
        if(numeric_pages && !first) {
            // Wide page sweeps can change the operator's selection context.
            // Boot each program independently instead of inheriting that state.
            op.reset();m.reset();engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);
            m=std::make_unique<Machine>(*engine);op=std::make_unique<LarcOperator>(*m);
            require(!m->run_task([&]{return xl_test::boot(*m,*op);}).failed,"isolated fader boot failed");
        }
        first=false;
        require(!m->run_task([&]{return xl_test::select(*m,*op,info.bank,info.program);}).failed,"fader selection failed");
        const auto& data=bank->programs[i];
        if(active_tail && data.dynamics.enabled) {
            require(!m->run_task([&]{return active_setup(*m,*op,data);}).failed,"fader active preparation failed");
            std::array<float,64> input{},zero{};std::array<std::array<float,64>,4> output{};
            float* channels[]={output[0].data(),output[1].data(),output[2].data(),output[3].data()};uint32_t random=17;
            for(unsigned frame=0;frame<12032;frame+=64){for(auto& x:input){random=random*1664525u+1013904223u;x=frame<4800?.12f*float(int32_t(random))/2147483648.f:0;}require(m->render(input.data(),zero.data(),channels,64),"fader active audio failed");}
        }
        Probe probe{engine->host()};probe.start();const auto result=m->run_task([&]{return numeric_pages?numeric_pages_fixture(*m,*op,data):fixture(*m,*op,data);});require(!result.failed,result.error.text);probe.stop();
        std::cout<<info.name<<": pickup="<<probe.pickups<<", accepted="<<probe.accepted<<", rejected="<<probe.rejected<<", states=";for(auto x:probe.pickup_states)std::cout<<x<<'/';
        std::cout<<", paths64/93/99/120=";for(auto x:probe.pickup_paths)std::cout<<x<<'/';
        std::cout<<", finish="<<probe.finishes<<", transactions="<<probe.transactions<<", handlers="<<probe.handlers<<", main returns="<<probe.main_returns<<", compilers="<<probe.compilers<<", stopped requests="<<probe.stopped_requests<<", active tail="<<(active_tail && data.dynamics.enabled)<<", IRQ="<<probe.irq_count<<", work errors="<<probe.work_errors<<", state errors="<<probe.state_errors<<", retention errors="<<probe.retention_errors<<'\n'<<std::flush;
        std::cout<<info.name<<": calibration="<<probe.calibrations<<", skipped="<<probe.calibration_skipped<<", calibration work errors="<<probe.calibration_work_errors<<", calibration state errors="<<probe.calibration_state_errors<<", page types=";for(auto count:probe.calibration_types)std::cout<<count<<'/';std::cout<<'\n'<<std::flush;
        require(!probe.calibration_work_errors && !probe.calibration_state_errors,"native ordinary fader calibration differs");
        require(probe.transactions && probe.transactions==probe.finishes,"insufficient fader transaction coverage");calls+=probe.transactions;errors+=probe.work_errors+probe.state_errors+probe.retention_errors;
        require(probe.handlers==probe.transactions && probe.main_returns==probe.handlers,"fader handler did not resume main reconciliation");
    }
    require(calls && !errors,"native fader boundaries or retained state differ");require(!allocations && !releases,"native fader law allocates/releases");
    std::cout<<"Physical fader pickup/finish and ordinary numeric calibration work/state exact; retained dynamics/headroom/monitor state; allocation/release=0; Size/variable-predelay/effect calibration and compiler wrapper remain separate\n";
}
