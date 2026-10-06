// Offline normal program-title boundary oracle. Entry panel context and IRQ
// spans are observed; title work, timer clears and text length are native.
#include "xl_reference.hpp"
#include "../desktop/panel_title_clock_xl.hpp"
#include <new>
using namespace lexplug;using namespace lexplug::op;using xl_test::require;
using namespace cineol::xl;namespace hw=lexicon224x::cpu;
static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n){if(tracking)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(tracking && p)++releases;std::free(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}


static PanelTitleMemoryXL title_snapshot(const hw::Host& h) {
 PanelTitleMemoryXL m{h.memory[0x3c3c],h.memory[0x3c3d],h.memory[0x3c3e],h.memory[0x3e3d]};
 std::copy_n(h.memory.begin()+0x3c40,7,m.selection.begin());return m;
}
static PanelServiceMemoryXL service_snapshot(const hw::Host& h) {
 return {h.memory[0x3c36],{h.memory[0x3c39],h.memory[0x3c3a],h.memory[0x3c3b]}};
}
static PanelTitleContextXL context(const hw::Host& h) {
 return {h.memory[0x3c08],{h.memory[0x2031],255,h.memory[0x3c2c],h.memory[0x3c2d],h.memory[0x2032],h.memory[0x2033],h.memory[0x2034]}};
}
struct Probe {
 hw::Host& h;PanelTitleMemoryXL memory{};PanelServiceMemoryXL service{};
 PanelPublishMemoryXL panel{};SerialTxMemoryXL serial{};PanelTitleClockXL clock;
 bool active=false,in_irq=false,publishing=false,stopping=false;
 uint64_t begin=0,irq_begin=0,irq_states=0;uint16_t return_pc=0,return_sp=0,irq_pc=0,irq_sp=0,publish_pc=0,publish_sp=0;
 uint64_t header_deadline=0;
 unsigned calls=0,skipped=0,errors=0,state_errors=0,publications=0,formats=0,irqs=0;
 std::array<unsigned,3> modes{};
 unsigned word(unsigned a)const{return h.memory[a]|unsigned(h.memory[uint16_t(a+1)])<<8;}
 uint64_t local(uint64_t t)const{return t-begin-irq_states;}
 void compare(uint64_t t,PanelTitleClockXL::Kind kind) {
  const auto e=clock.next();if(e.kind!=kind || e.work_state!=local(t)) {
   if(errors++<8)std::cerr<<"title boundary "<<unsigned(e.kind)<<'/'<<unsigned(kind)<<", work="<<e.work_state<<'/'<<local(t)<<'\n';
  }
  const auto got=title_snapshot(h);const auto tail=service_snapshot(h);
  if(memory.view!=got.view || memory.last_slider!=got.last_slider || memory.last_column!=got.last_column || memory.compiler_request!=got.compiler_request || memory.selection!=got.selection || service.menu_flags!=tail.menu_flags || service.timers!=tail.timers) {
   if(state_errors++<8)std::cerr<<"title state differs at kind="<<unsigned(kind)<<", view="<<unsigned(memory.view)<<'/'<<unsigned(got.view)<<'\n';
  }
 }
 void start() {
  for(unsigned pc:{0x1506u,0x0581u,0x1545u,0x38u})h.pc_watches[pc]=true;
  h.pc_observer=[this](uint64_t t,hw::CpuSnapshot cpu){
   h.pc_watches[cpu.pc]=true;
   if(!active) {
    if(cpu.pc!=0x1506 || stopping)return;
    memory=title_snapshot(h);service=service_snapshot(h);panel={h.memory[0x3c14],h.memory[0x3c15]};serial={h.memory[0x3c10],0};
    tracking=true;clock.reset(memory,service,panel,serial,context(h),0);tracking=false;
    if(clock.next().kind==PanelTitleClockXL::Kind::unsupported){++skipped;return;}
    active=true;begin=t;irq_states=0;return_pc=uint16_t(word(cpu.sp));return_sp=uint16_t(cpu.sp+2);h.pc_watches[return_pc]=true;
    ++modes[memory.selection[0]==1?0:memory.selection[0]==2?1:2];return;
   }
   if(cpu.pc==0x38){require(!in_irq,"nested title IRQ");in_irq=true;irq_begin=t-11;irq_pc=uint16_t(word(cpu.sp));irq_sp=uint16_t(cpu.sp+2);h.pc_watches[irq_pc]=true;return;}
   if(in_irq){if(cpu.pc!=irq_pc || cpu.sp!=irq_sp)return;irq_states+=t-irq_begin;in_irq=false;++irqs;}
   if(publishing && cpu.pc==publish_pc && cpu.sp==publish_sp) {
    tracking=true;clock.complete_publish();tracking=false;publishing=false;
    if(panel.flags!=h.memory[0x3c14])++state_errors;
    const auto event=clock.next();
    header_deadline=begin+irq_states+event.work_state-24-279+205+10+serial_character_states_xl;
   }
   if(cpu.pc==0x0581) {
    compare(t,PanelTitleClockXL::Kind::publish);publishing=true;publish_pc=uint16_t(word(cpu.sp));publish_sp=uint16_t(cpu.sp+2);h.pc_watches[publish_pc]=true;++publications;return;
   }
   if(cpu.pc==0x1545){compare(t,PanelTitleClockXL::Kind::format);tracking=true;clock.complete_format();tracking=false;++formats;return;}
   if(cpu.pc!=return_pc || cpu.sp!=return_sp)return;
   compare(t,PanelTitleClockXL::Kind::finished);
   unsigned characters=0;while(h.memory[0x3f4f+characters]){require(characters<48,"unbounded title text");++characters;}
   if(serial.text_remaining!=characters || panel.flags!=h.memory[0x3c14] || !h.remote.transmit_time || *h.remote.transmit_time!=header_deadline)++state_errors;
   ++calls;active=false;
  };
 }
 void stop(){require(!active && !publishing && !in_irq,"unfinished title worker");h.pc_observer={};h.pc_watches.fill(false);}
};
static Task<void> fixture(Machine& m,LarcOperator& op,unsigned index,PagesReading& pages) {
 const auto info=graphs[index];co_await xl_test::select(m,op,info.bank,info.program);co_await op.readPages(pages);co_await op.gotoPage(1);co_await m.sleep(4);
}
int main(int argc,char** argv) {
 require(argc==2 || argc==3,"usage: cineol_xl_panel_title_check ROM_DIRECTORY [PROGRAM_INDEX]");
 const int selected=argc==3?std::atoi(argv[2]):-1;require(selected>=-1 && selected<22,"invalid title program");
 auto e=std::make_unique<Engine>(0);xl_test::load(*e,argv[1]);auto m=std::make_unique<Machine>(*e);LarcOperator op(*m);
 require(!m->run_task([&]{return xl_test::boot(*m,op);}).failed,"title boot failed");auto pages=std::make_unique<PagesReading>();unsigned total=0,errors=0;
 for(unsigned i=0;i<graphs.size();++i) {
  if(selected>=0 && selected!=int(i))continue;Probe probe{e->host()};probe.start();const auto result=m->run_task([&]{return fixture(*m,op,i,*pages);});require(!result.failed,result.error.text);
  probe.stopping=true;std::array<float,64> zero{};std::array<std::array<float,64>,4> out{};float* channels[]={out[0].data(),out[1].data(),out[2].data(),out[3].data()};
  for(unsigned n=0;probe.active && n<48000;n+=64)require(m->render(zero.data(),zero.data(),channels,64),"title drain failed");probe.stop();require(probe.calls,"no normal title coverage");
  std::cout<<graphs[i].name<<": calls="<<probe.calls<<", publications="<<probe.publications<<", formats="<<probe.formats<<", skipped menus="<<probe.skipped<<", modes=";for(auto x:probe.modes)std::cout<<x<<'/';std::cout<<", IRQ="<<probe.irqs<<", work errors="<<probe.errors<<", state errors="<<probe.state_errors<<'\n'<<std::flush;
  total+=probe.calls;errors+=probe.errors+probe.state_errors;
 }
 require(total && !errors,"native normal title worker differs");require(!allocations && !releases,"title worker allocates/releases");std::cout<<"Normal title work/state exact: calls="<<total<<"; allocation/release=0; entry panel context and IRQ spans observed; special menu/control-field workers excluded\n";
}
