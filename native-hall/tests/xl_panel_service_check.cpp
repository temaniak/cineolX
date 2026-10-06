// Offline slow-return dispatch oracle. Title work, inner title timer clears
// and IRQ spans are observed context. Dispatch and both status workers predict
// their work and outer timer/flag state independently.
#include "xl_reference.hpp"
#include "../desktop/panel_service_clock_xl.hpp"
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
static PanelServiceMemoryXL snapshot(const hw::Host& h) {
 return {h.memory[0x3c36],{h.memory[0x3c39],h.memory[0x3c3a],h.memory[0x3c3b]}};
}
static bool same(const PanelServiceMemoryXL& a,const PanelServiceMemoryXL& b) {
 return a.menu_flags==b.menu_flags && a.timers==b.timers;
}
struct Probe {
 hw::Host& h;PanelServiceMemoryXL native{};PanelServiceClockXL clock;
 bool active=false,nested=false,in_irq=false,stopping=false,worker_cleared=false;
 uint64_t begin=0,irq_states=0,irq_begin=0,nested_at=0;
 uint16_t return_pc=0,return_sp=0,irq_pc=0,irq_sp=0,nested_pc=0,nested_sp=0;
 bool synthetic=false;unsigned seeded=0;
 PanelPublishMemoryXL worker_panel{};SerialTxMemoryXL worker_serial{};PanelTextWorkXL worker_work{};bool predicted_worker=false;unsigned native_status_workers=0;
 unsigned calls=0,errors=0,state_errors=0,irq_count=0;std::array<unsigned,4> branches{};
 unsigned word(unsigned a)const{return h.memory[a]|unsigned(h.memory[uint16_t(a+1)])<<8;}
 uint64_t local(uint64_t t)const{return t-begin-irq_states;}
 void compare(uint64_t t,PanelServiceClockXL::Kind kind) {
  const auto e=clock.next();
  if(e.kind!=kind || e.work_state!=local(t)) {
   if(errors++<8)std::cerr<<"panel dispatch boundary "<<unsigned(e.kind)<<'/'<<unsigned(kind)<<", work="<<e.work_state<<'/'<<local(t)<<'\n';
  }
  if(!same(native,snapshot(h))) {
   if(state_errors++<8) {const auto actual=snapshot(h);std::cerr<<"panel dispatch state kind="<<unsigned(kind)<<", native flags="<<unsigned(native.menu_flags)<<", actual flags="<<unsigned(actual.menu_flags)<<", timers=";for(auto x:native.timers)std::cerr<<unsigned(x)<<'/';std::cerr<<" vs ";for(auto x:actual.timers)std::cerr<<unsigned(x)<<'/';std::cerr<<'\n';}
  }
 }
 void start() {
  for(unsigned pc:{0x85afu,0x8848u,0x1629u,0x1618u,0x12d4u,0x38u})h.pc_watches[pc]=true;
  h.pc_observer=[this](uint64_t t,hw::CpuSnapshot cpu){
   h.pc_watches[cpu.pc]=true;
   if(!active) {
    if(cpu.pc!=0x85af || stopping)return;
    // Explicit diagnostic boundary coverage. These two reference-only RAM
    // seeds do not enter physical sound fixtures or native plugin processing.
    if(synthetic && seeded<2 && h.memory[0x3c14]==128) {
     h.memory[0x3c36]=uint8_t(seeded?48:32);++seeded;
    }
    active=true;begin=t;irq_states=0;native=snapshot(h);return_pc=uint16_t(word(cpu.sp));return_sp=uint16_t(cpu.sp+2);h.pc_watches[return_pc]=true;
    tracking=true;clock.reset(native,h.memory[0x3c14],0);tracking=false;++branches[unsigned(clock.next().kind)];return;
   }
   if(cpu.pc==0x38) {
    require(!in_irq,"nested panel IRQ");in_irq=true;irq_begin=t-11;irq_pc=uint16_t(word(cpu.sp));irq_sp=uint16_t(cpu.sp+2);h.pc_watches[irq_pc]=true;return;
   }
   if(in_irq) {
    if(cpu.pc!=irq_pc || cpu.sp!=irq_sp)return;
    irq_states+=t-irq_begin;in_irq=false;++irq_count;
   }
   if(nested) {
    if(cpu.pc==0x12d4)worker_cleared=true;
    if(cpu.pc!=nested_pc || cpu.sp!=nested_sp)return;
    unsigned worker_states=unsigned(local(t)-nested_at);
    if(predicted_worker) {
     if(worker_states!=worker_work.work_states || worker_panel.flags!=h.memory[0x3c14]) {
      if(errors++<8)std::cerr<<"native status worker work/state "<<worker_work.work_states<<'/'<<worker_states<<'\n';
     }
     worker_states=worker_work.work_states;++native_status_workers;
    }
    tracking=true;clock.complete_worker(worker_states,worker_cleared);tracking=false;nested=false;
   }
   if(cpu.pc==return_pc && cpu.sp==return_sp) {
    compare(t,PanelServiceClockXL::Kind::finished);++calls;active=false;return;
   }
   auto kind=PanelServiceClockXL::Kind::finished;
   if(cpu.pc==0x8848)kind=PanelServiceClockXL::Kind::title;
   else if(cpu.pc==0x1629)kind=PanelServiceClockXL::Kind::status;
   else if(cpu.pc==0x1618)kind=PanelServiceClockXL::Kind::alternate_status;
   if(kind==PanelServiceClockXL::Kind::finished)return;
   compare(t,kind);nested=true;worker_cleared=false;nested_at=local(t);
   predicted_worker=kind!=PanelServiceClockXL::Kind::title;
   if(predicted_worker) {
    worker_panel={h.memory[0x3c14],h.memory[0x3c15]};worker_serial={h.memory[0x3c10],0};
    tracking=true;worker_work=panel_status_text_work(worker_panel,worker_serial,kind==PanelServiceClockXL::Kind::alternate_status);tracking=false;
   }
   if(kind==PanelServiceClockXL::Kind::title){nested_pc=return_pc;nested_sp=return_sp;}
   else {nested_pc=uint16_t(word(cpu.sp));nested_sp=uint16_t(cpu.sp+2);h.pc_watches[nested_pc]=true;}
  };
 }
 void stop(){require(!active && !nested && !in_irq,"unfinished panel dispatch");h.pc_observer={};h.pc_watches.fill(false);}
};
int main(int argc,char** argv) {
 require(argc>=2 && argc<=4,"usage: cineol_xl_panel_service_check ROM_DIRECTORY [PROGRAM_INDEX] [--synthetic-dispatch]");
 const bool synthetic=argc==4;require(!synthetic || std::string(argv[3])=="--synthetic-dispatch","invalid panel dispatch option");
 const int selected=argc>=3?std::atoi(argv[2]):-1;require(selected>=-1 && selected<22,"invalid panel-service program");
 auto e=std::make_unique<Engine>(0);xl_test::load(*e,argv[1]);auto m=std::make_unique<Machine>(*e);LarcOperator op(*m);
 require(!m->run_task([&]{return xl_test::boot(*m,op);}).failed,"panel-service boot failed");
 unsigned total=0,errors=0,titles=0;
 for(unsigned i=0;i<graphs.size();++i) {
  if(selected>=0 && selected!=int(i))continue;const auto info=graphs[i];
  require(!m->run_task([&]{return xl_test::select(*m,op,info.bank,info.program);}).failed,"panel-service selection failed");
  Probe probe{e->host()};probe.synthetic=synthetic;probe.start();std::array<float,64> zero{};std::array<std::array<float,64>,4> out{};float* channels[]={out[0].data(),out[1].data(),out[2].data(),out[3].data()};
  for(unsigned n=0;n<288000;n+=64)require(m->render(zero.data(),zero.data(),channels,64),"panel-service render failed");
  probe.stopping=true;for(unsigned n=0;probe.active && n<48000;n+=64)require(m->render(zero.data(),zero.data(),channels,64),"panel-service drain failed");
  probe.stop();require(!synthetic || (probe.seeded==2 && probe.branches[1] && probe.branches[2]),"insufficient synthetic status branches");require(probe.calls>10,"insufficient panel dispatch calls");
  std::cout<<info.name<<": calls="<<probe.calls<<", branches=";for(auto n:probe.branches)std::cout<<n<<'/';std::cout<<", native status workers="<<probe.native_status_workers<<", synthetic status seeds="<<probe.seeded<<", IRQ="<<probe.irq_count<<", work errors="<<probe.errors<<", state errors="<<probe.state_errors<<'\n'<<std::flush;
  total+=probe.calls;errors+=probe.errors+probe.state_errors;titles+=probe.branches[0];
 }
 require(total && titles && !errors,"native panel-service dispatch differs");require(!allocations && !releases,"panel-service dispatch allocates/releases");
 std::cout<<"Panel-service dispatch work/state exact: calls="<<total<<", titles="<<titles<<"; allocation/release=0; nested title work, inner title timer clears and IRQ spans remain local inputs; both status worker costs predicted natively\n";
}
