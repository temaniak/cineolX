// Offline text-publication boundary oracle. Text length/header and IRQ
// spans are observed context; work, UART output/deadline and state are native.
#include "xl_reference.hpp"
#include "../desktop/panel_text_work_xl.hpp"
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
 hw::Host& h;PanelPublishMemoryXL panel{};SerialTxMemoryXL serial{};PanelTextWorkXL predicted{};
 bool active=false,in_irq=false,stopping=false;unsigned calls=0,errors=0,state_errors=0,deadline_errors=0,irq_count=0;
 uint64_t begin=0,irq_begin=0,irq_states=0;uint16_t return_pc=0,return_sp=0,irq_pc=0,irq_sp=0;uint8_t header=0;
 unsigned word(unsigned a)const{return h.memory[a]|unsigned(h.memory[uint16_t(a+1)])<<8;}
 void start() {
  for(unsigned pc:{0x0581u,0x0592u,0x057cu,0x38u})h.pc_watches[pc]=true;
  h.pc_observer=[this](uint64_t t,hw::CpuSnapshot cpu){
   h.pc_watches[cpu.pc]=true;
   if(!active) {
    if(cpu.pc!=0x0581 || stopping)return;
    active=true;begin=t;irq_states=0;return_pc=uint16_t(word(cpu.sp));return_sp=uint16_t(cpu.sp+2);h.pc_watches[return_pc]=true;
    panel={h.memory[0x3c14],h.memory[0x3c15]};serial={h.memory[0x3c10],0};header=uint8_t(cpu.a|128);
    unsigned count=0;while(h.memory[0x3f4f+count]){require(count<48,"unbounded panel text");++count;}
    tracking=true;predicted=panel_text_publish_work(panel,serial,uint8_t(count));tracking=false;return;
   }
   if(cpu.pc==0x38) {
    require(!in_irq,"nested text-publication IRQ");in_irq=true;irq_begin=t-11;irq_pc=uint16_t(word(cpu.sp));irq_sp=uint16_t(cpu.sp+2);h.pc_watches[irq_pc]=true;return;
   }
   if(in_irq) {
    if(cpu.pc!=irq_pc || cpu.sp!=irq_sp)return;
    irq_states+=t-irq_begin;in_irq=false;++irq_count;
    // RX work before DI can alter receive context; it is not text-work law.
    panel.receive_mode=h.memory[0x3c15];
   }
   if(cpu.pc==0x0592 || cpu.pc==0x057c) {
    const unsigned expected=cpu.pc==0x0592?predicted.command_start:predicted.write_start;
    if(t-begin-irq_states!=expected) {
     if(errors++<8)std::cerr<<"text command/output boundary "<<expected<<'/'<<t-begin-irq_states<<'\n';
    }
    return;
   }
   if(cpu.pc!=return_pc || cpu.sp!=return_sp)return;
   if(t-begin-irq_states!=predicted.work_states) {
    if(errors++<8)std::cerr<<"text publication work "<<predicted.work_states<<'/'<<t-begin-irq_states<<'\n';
   }
   if(panel.flags!=h.memory[0x3c14] || serial.remaining!=h.memory[0x3c10] || panel.receive_mode!=h.memory[0x3c15] || word(0x3c0c)!=0x3f4f || h.remote.transmitting!=header || h.remote.command!=0x27)++state_errors;
   const uint64_t deadline=begin+irq_states+predicted.write_start+10+serial_character_states_xl;
   if(!h.remote.transmit_time || *h.remote.transmit_time!=deadline) {
    if(deadline_errors++<8)std::cerr<<"text deadline "<<deadline<<'/'<<h.remote.transmit_time.value_or(0)<<'\n';
   }
   unsigned remaining=0;while(h.memory[0x3f4f+remaining]){require(remaining<48,"unbounded final text");++remaining;}
   if(serial.text_remaining!=remaining)++state_errors;
   ++calls;active=false;
  };
 }
 void stop(){require(!active && !in_irq,"unfinished text publication");h.pc_observer={};h.pc_watches.fill(false);}
};
static Task<void> fixture(Machine& m,LarcOperator& op,unsigned index,PagesReading& pages) {
 const auto info=graphs[index];co_await xl_test::select(m,op,info.bank,info.program);
 co_await op.readPages(pages);co_await op.gotoPage(1);co_await m.sleep(4);
}
int main(int argc,char** argv) {
 require(argc==2 || argc==3,"usage: cineol_xl_panel_text_check ROM_DIRECTORY [PROGRAM_INDEX]");
 const int selected=argc==3?std::atoi(argv[2]):-1;require(selected>=-1 && selected<22,"invalid text program");
 auto e=std::make_unique<Engine>(0);xl_test::load(*e,argv[1]);auto m=std::make_unique<Machine>(*e);LarcOperator op(*m);
 require(!m->run_task([&]{return xl_test::boot(*m,op);}).failed,"text-publication boot failed");
 auto pages=std::make_unique<PagesReading>();unsigned total=0,errors=0;
 for(unsigned i=0;i<graphs.size();++i) {
  if(selected>=0 && selected!=int(i))continue;Probe probe{e->host()};probe.start();
  const auto result=m->run_task([&]{return fixture(*m,op,i,*pages);});require(!result.failed,result.error.text);
  probe.stopping=true;std::array<float,64> zero{};std::array<std::array<float,64>,4> out{};float* channels[]={out[0].data(),out[1].data(),out[2].data(),out[3].data()};
  for(unsigned n=0;probe.active && n<48000;n+=64)require(m->render(zero.data(),zero.data(),channels,64),"text-publication drain failed");
  probe.stop();require(probe.calls>=5,"insufficient text publication calls");
  std::cout<<graphs[i].name<<": calls="<<probe.calls<<", IRQ="<<probe.irq_count<<", work errors="<<probe.errors<<", state errors="<<probe.state_errors<<", deadline errors="<<probe.deadline_errors<<'\n'<<std::flush;
  total+=probe.calls;errors+=probe.errors+probe.state_errors+probe.deadline_errors;
 }
 require(total && !errors,"native text publication differs");require(!allocations && !releases,"text publication allocates/releases");
 std::cout<<"Text-publication work/state/deadline exact: calls="<<total<<"; allocation/release=0; entry text/header and IRQ context remain local inputs\n";
}
