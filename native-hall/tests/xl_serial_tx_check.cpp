// Offline settled TX ISR oracle. Current panel/UART context is observed;
// branch work, output time, character deadline and state are native predictions.
#include "xl_reference.hpp"
#include "../desktop/serial_tx_work_xl.hpp"
#include <new>
using namespace lexplug;using namespace lexplug::op;using xl_test::require;
using namespace cineol::xl;namespace hw=lexicon224x::cpu;
static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n){if(tracking)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(tracking && p)++releases;std::free(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
struct Probe {
    hw::Host& h;PanelPublishMemoryXL panel{};SerialTxMemoryXL serial{};SerialTxWorkXL predicted{};
    bool active=false,stopping=false;
    unsigned calls=0,skipped=0,errors=0,state_errors=0,deadline_errors=0;
    std::array<unsigned,5> branches{};
    uint64_t begin=0;uint16_t return_pc=0,return_sp=0;
    unsigned word(unsigned a)const{return h.memory[a]|unsigned(h.memory[uint16_t(a+1)])<<8;}
    void start() {
        h.pc_watches[0x38]=true;
        h.pc_observer=[this](uint64_t t,hw::CpuSnapshot cpu) {
            h.pc_watches[cpu.pc]=true;
            if(!active) {
                if(cpu.pc!=0x38 || stopping)return;
                const unsigned status=h.remote.status();
                if((status&0x3b)!=1){++skipped;return;}
                active=true;begin=t-11;
                return_pc=uint16_t(word(cpu.sp));return_sp=uint16_t(cpu.sp+2);h.pc_watches[return_pc]=true;
                panel={h.memory[0x3c14],h.memory[0x3c15]};serial.remaining=h.memory[0x3c10];serial.text_remaining=0;
                if(panel.flags&4) {
                    unsigned address=word(0x3c0c);
                    while(h.memory[uint16_t(address++)]) {
                        require(serial.text_remaining<100,"unbounded serial text");++serial.text_remaining;
                    }
                }
                tracking=true;predicted=serial_tx_work(panel,serial);tracking=false;
                ++branches[unsigned(predicted.action)];return;
            }
            if(cpu.pc!=return_pc || cpu.sp!=return_sp)return;
            if(t-begin!=predicted.work_states) {
                if(errors++<8)std::cerr<<"serial work "<<predicted.work_states<<'/'<<t-begin<<", branch="<<unsigned(predicted.action)<<'\n';
            }
            if(panel.flags!=h.memory[0x3c14] || serial.remaining!=h.memory[0x3c10] || panel.receive_mode!=h.memory[0x3c15])++state_errors;
            if(predicted.action==SerialTxWorkXL::Action::disable) {
                if(h.remote.command&1 || h.remote.transmit_time)++deadline_errors;
            } else {
                const uint64_t deadline=begin+predicted.write_start+10+serial_character_states_xl;
                if(!h.remote.transmit_time || *h.remote.transmit_time!=deadline) {
                    if(deadline_errors++<8)std::cerr<<"serial deadline "<<deadline<<'/'<<h.remote.transmit_time.value_or(0)<<'\n';
                }
            }
            ++calls;active=false;
        };
    }
    void stop(){require(!active,"unfinished serial ISR");h.pc_observer={};h.pc_watches.fill(false);}
};
int main(int argc,char** argv) {
    require(argc==2 || argc==3,"usage: cineol_xl_serial_tx_check ROM_DIRECTORY [PROGRAM_INDEX]");
    const int selected=argc==3?std::atoi(argv[2]):-1;require(selected>=-1 && selected<22,"invalid serial program");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);auto m=std::make_unique<Machine>(*engine);LarcOperator op(*m);
    require(!m->run_task([&]{return xl_test::boot(*m,op);}).failed,"serial boot failed");
    unsigned total=0,total_errors=0;
    for(unsigned i=0;i<graphs.size();++i) {
        if(selected>=0 && selected!=int(i))continue;const auto info=graphs[i];
        require(!m->run_task([&]{return xl_test::select(*m,op,info.bank,info.program);}).failed,"serial selection failed");
        Probe probe{engine->host()};probe.start();
        std::array<float,64> zero{};std::array<std::array<float,64>,4> out{};
        float* channels[]={out[0].data(),out[1].data(),out[2].data(),out[3].data()};
        for(unsigned frame=0;frame<96000;frame+=64) {
            const unsigned phase=frame%24000,bits=phase<4000?15:phase<8000?3:phase<12000?0:phase<16000?31:0;
            engine->host().set_level_detectors(0,bits);engine->host().set_level_detectors(1,phase<12000?bits:0);
            require(m->render(zero.data(),zero.data(),channels,64),"serial render failed");
        }
        probe.stopping=true;for(unsigned frame=0;probe.active && frame<48000;frame+=64)
            require(m->render(zero.data(),zero.data(),channels,64),"serial drain failed");
        probe.stop();require(probe.calls>10,"insufficient serial calls");
        std::cout<<info.name<<": calls="<<probe.calls<<", skipped RX/error="<<probe.skipped<<", branches=";
        for(unsigned n:probe.branches)std::cout<<n<<'/';
        std::cout<<", work errors="<<probe.errors<<", state errors="<<probe.state_errors<<", deadline errors="<<probe.deadline_errors<<'\n'<<std::flush;
        total+=probe.calls;total_errors+=probe.errors+probe.state_errors+probe.deadline_errors;
    }
    require(total && !total_errors,"native serial TX law differs");require(!allocations && !releases,"serial TX allocates/releases");
    std::cout<<"Settled serial TX work/state/deadline exact; allocation/release=0; entry context remains observed\n";
}
