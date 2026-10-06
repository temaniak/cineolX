// Offline physical-key v8.21 controller/bus chronology. No reference state is
// injected into a native engine. Private traces must stay under ignored build/.
#include "../tests/xl_reference.hpp"
#include "../desktop/bank.hpp"
#include <emulator/trace.hpp>
#include <emulator/cpu_disassembler.hpp>
#include <iomanip>
#include <cstring>
#include <optional>

using namespace lexplug;
using namespace lexplug::op;
using namespace cineol::xl;
using xl_test::require;
namespace hw=lexicon224x::cpu;

static Task<void> toggle(Machine& m,LarcOperator& op,unsigned which,bool enabled) {
    const unsigned mask=which==0?1:which==1?64:128;
    for(unsigned attempt=0;attempt<8;++attempt) {
        co_await op.setToggle(int(which),enabled);
        if(bool(m.peek(uint16_t(op.recordBase()+42))&mask)==enabled)co_return;
        co_await m.sleep(0.2);
    }
    co_await fail("XL trace physical toggle did not settle");
}
static Task<void> setup(Machine& m,LarcOperator& op,const ProgramData& data,unsigned index,unsigned mode) {
    co_await m.sleep(16);
    const auto info=graphs[index];co_await xl_test::select(m,op,info.bank,info.program);
    for(unsigned page=0;page<data.page_count;++page)for(unsigned slot=0;slot<6;++slot) {
        const unsigned cell=data.pages[page].cells[slot];
        if(cell<48 && m.peek(uint16_t(op.recordBase()+cell))!=data.controls.factory[cell])
            co_await op.moveSlider(int(page+1),slot,data.controls.factory[cell]);
    }
    if(!co_await op.recordSettled())co_await fail("XL trace factory controls did not settle");
    co_await toggle(m,op,0,bool(mode&4));co_await toggle(m,op,1,bool(mode&1));co_await toggle(m,op,2,bool(mode&2));
    const auto actual=co_await op.readToggles();
    if(actual.value[0]!=int(bool(mode&4)) || actual.value[1]!=int(bool(mode&1)) || actual.value[2]!=int(bool(mode&2)))
        co_await fail("XL trace physical toggles differ");
    co_await m.sleep(0.5);
}
struct Probe final:hw::Trace {
    hw::Host& h;std::ofstream& events;std::ofstream& instructions;std::ofstream& states;
    struct Frame {unsigned id,pc,return_pc,sp;uint64_t entry;};
    std::vector<Frame> stack;std::optional<Frame> irq;
    std::array<unsigned,65536> watches{};
    std::array<unsigned,3> entries{},returns{};
    uint64_t global_row=0;unsigned grants=0,commits=0,displaced=0,interrupts=0;
    bool all_instructions=false;
    explicit Probe(hw::Host& host,std::ofstream& output,std::ofstream& listing,std::ofstream& snapshots,bool all):
        h(host),events(output),instructions(listing),states(snapshots),all_instructions(all){}
    unsigned word(unsigned address)const{return h.memory[address]|unsigned(h.memory[address+1])<<8;}
    static bool fixed(unsigned pc){return pc==0x38 || pc==0xad5c || pc==0x82cf || pc==0x81b6;}
    void watch(unsigned pc,int delta){watches[pc]=unsigned(int(watches[pc])+delta);h.pc_watches[pc]=all_instructions || fixed(pc) || watches[pc];}
    void record(const char* kind,uint64_t time,unsigned pc,unsigned row=0,unsigned lane=0,unsigned value=0,uint64_t aux=0) {
        const auto cpu=h.snapshot();
        events<<kind<<','<<time<<','<<time/hw::cpu_period<<','<<pc<<','<<cpu.sp<<','<<cpu.bc<<','<<cpu.de<<','<<cpu.hl
            <<','<<unsigned(cpu.a)<<','<<unsigned(cpu.flags)<<','<<unsigned(cpu.interrupt_enabled)<<','<<row<<','<<lane<<','<<value<<','<<aux
            <<','<<word(0x3e47)<<','<<unsigned(h.memory[0x3e44])<<','<<unsigned(h.memory[0x3e45])<<','<<unsigned(h.memory[0x3e46])
            <<','<<unsigned(h.memory[0x3ccd])<<','<<unsigned(h.memory[0x3cf6])<<'\n';
        if(std::strcmp(kind,"controller_entry")==0 || std::strcmp(kind,"controller_return")==0) {
            states<<kind<<','<<time/hw::cpu_period<<','<<pc<<','<<row<<','<<word(0x3e0f);
            for(unsigned address:{0x3c50u,0x3c51u,0x3c52u,0x3c53u,0x3c54u,0x3c5fu,0x3e11u,0x3e12u,0x3e13u,0x3e14u,0x3c38u,0x3c61u,
                0x3c62u,0x3c63u,0x3c6au,0x3c5bu,0x3df9u,0x3e07u})states<<','<<unsigned(h.memory[address]);
            states<<','<<word(0x3e38);
            for(unsigned i=0;i<11;++i)states<<','<<unsigned(h.memory[0x3e15+i]);
            for(unsigned i=0;i<48;++i)states<<','<<unsigned(h.memory[0x3ca3+i]);
            states<<'\n';
        }
    }
    void start() {
        events<<"kind,tick,cpu_state,pc,sp,bc,de,hl,a,flags,irq_enabled,row,lane,value,aux,mod_index,divider,random_divider,random_hold,control_flags,descriptor_flags\n";
        states<<"kind,cpu_state,pc,controller,held,average,dyn_flags,low,mid,trigger_peak,stop_counter,stopped,amount,divider,period,peak_divider,peak_input,headroom_accum,hr_left_memory,hr_right_memory,base_period,layout_flags,mid_special,monitor_peak";
        for(unsigned i=0;i<11;++i)states<<",history"<<i;
        for(unsigned i=0;i<48;++i)states<<",raw"<<i;
        states<<'\n';
        if(all_instructions)instructions<<"cpu_state,pc,sp,bc,de,hl,a,flags,irq_enabled\n";
        h.pc_watches.fill(all_instructions);
        for(unsigned pc:{0x38u,0xad5cu,0x82cfu,0x81b6u})h.pc_watches[pc]=true;
        h.pc_observer=[this](uint64_t time,hw::CpuSnapshot cpu){
            if(all_instructions)instructions<<time<<','<<cpu.pc<<','<<cpu.sp<<','<<cpu.bc<<','<<cpu.de<<','<<cpu.hl
                <<','<<unsigned(cpu.a)<<','<<unsigned(cpu.flags)<<','<<unsigned(cpu.interrupt_enabled)<<'\n';
            if(irq && cpu.pc==irq->return_pc && cpu.sp==uint16_t(irq->sp+2)) {
                record("irq_return",time*hw::cpu_period,cpu.pc,0,0,0,time-irq->entry+11);
                watch(irq->return_pc,-1);irq.reset();
            }
            if(cpu.pc==0x38) {
                require(!irq,"nested XL serial IRQ");irq=Frame{3,cpu.pc,word(cpu.sp),cpu.sp,time};
                watch(irq->return_pc,1);++interrupts;
                record("irq_entry",(time-11)*hw::cpu_period,cpu.pc,0,0,irq->return_pc,11);
            }
            if(!irq) {
                if(!stack.empty() && cpu.pc==stack.back().return_pc && cpu.sp==uint16_t(stack.back().sp+2)) {
                    const auto frame=stack.back();stack.pop_back();++returns[frame.id];watch(frame.return_pc,-1);
                    record("controller_return",time*hw::cpu_period,cpu.pc,frame.id,0,frame.pc,time-frame.entry);
                }
                if(cpu.pc==0xad5c || cpu.pc==0x82cf || cpu.pc==0x81b6) {
                    const unsigned id=cpu.pc==0xad5c?0:cpu.pc==0x82cf?1:2;
                    require(stack.size()<8,"nested XL controller depth exceeds trace bound");
                    const unsigned ret=word(cpu.sp);stack.push_back({id,cpu.pc,ret,cpu.sp,time});watch(ret,1);++entries[id];
                    record("controller_entry",time*hw::cpu_period,cpu.pc,id,0,ret,stack.size());
                }
            }
            h.pc_watches[cpu.pc]=all_instructions || fixed(cpu.pc) || watches[cpu.pc];
        };
        h.wcs_observer=[this](const hw::WcsWrite& w){
            record("write_bus_t1",w.cpu_t1*hw::cpu_period,w.writer_pc,127-(w.address-0x4000)/4,w.address&3,w.value,w.committed_at);
        };
        h.port_trace=[this](uint64_t time,uint16_t pc,bool write,unsigned port,uint8_t value){
            // The existing API reports the IN/OUT instruction start. For
            // reads it runs while Host::cpu_state samples DBIN at phi2 rise,
            // before cycles increments: retain that actual sample separately.
            record(write?"port_write":"port_read",time*hw::cpu_period,pc,0,port,value);
            if(!write)record("port_read_sample",hw::state_start(h.cycles)+hw::phi2_rise,pc,0,port,value,time);
        };
        h.set_trace(this);
    }
    void stop(){h.set_trace(nullptr);h.pc_observer={};h.wcs_observer={};h.port_trace={};h.pc_watches.fill(false);}
    void cpu_state(hw::Tick,uint64_t,uint64_t,bool)override{}
    void bus_command(hw::Tick,hw::BusCommand)override{}
    void row_phase(hw::Tick,hw::RowPhase,uint64_t row)override{global_row=row;}
    void fetch_displaced(hw::Tick time,uint64_t row)override{++displaced;record("fetch_displaced",time,0,unsigned(row),0,h.dsp->pc);}
    void multiplicand_held(hw::Tick time,hw::Hold clock)override{record("operand_hold",time,0,unsigned(global_row),unsigned(clock));}
    void wcs_request(hw::Tick time,bool read,unsigned row,unsigned lane)override{record(read?"wcs_read_request":"wcs_write_request",time,0,row,lane);}
    void wcs_grant(hw::Tick marker,hw::Tick ack)override{++grants;record("wcs_grant",marker,0,unsigned(global_row),0,0,ack);}
    void wcs_read_drive(hw::Tick drive,hw::Tick release)override{record("wcs_read_drive",drive,0,0,0,0,release);}
    void wcs_commit(hw::Tick time,unsigned row,unsigned lane,uint8_t value)override{++commits;record("wcs_commit",time,0,row,lane,value);}
    void aruck(hw::Tick,unsigned)override{}
    void history_sample(hw::Tick,unsigned)override{}
    void dport(hw::Tick,hw::DportEvent)override{}
};
static void hash_bytes(uint64_t& hash,const void* data,size_t bytes) {
    const auto* p=static_cast<const unsigned char*>(data);
    for(size_t i=0;i<bytes;++i){hash^=p[i];hash*=1099511628211ull;}
}
int main(int argc,char** argv) {
    const char* usage="usage: cineol_xl_controller_trace ROM_DIRECTORY BANK OUTPUT INDEX MODE [SECONDS=2] [WARMUP_MS=1000] [BLOCK=64] [silence|noise|steps] [--instructions|--no-trace]\n";
    bool all=false,traced=true;
    if(argc>1 && std::string(argv[argc-1])=="--instructions"){all=true;--argc;}
    else if(argc>1 && std::string(argv[argc-1])=="--no-trace"){traced=false;--argc;}
    require(argc>=6 && argc<=10,usage);
    const unsigned index=unsigned(std::stoul(argv[4])),mode=unsigned(std::stoul(argv[5]));
    const unsigned seconds=argc>6?unsigned(std::stoul(argv[6])):2,warmup=argc>7?unsigned(std::stoul(argv[7])):1000,block=argc>8?unsigned(std::stoul(argv[8])):64;
    const std::string fixture=argc>9?argv[9]:"silence";
    require(index<22 && mode<8 && seconds>=1 && seconds<=10 && warmup<=10000 && block>0 && block<=4096,"invalid XL trace selection/window");
    require(fixture=="silence" || fixture=="noise" || fixture=="steps","invalid XL trace input");
    const auto output=std::filesystem::weakly_canonical(std::filesystem::path(argv[3]));
    const auto private_relative=output.lexically_relative(std::filesystem::weakly_canonical(std::filesystem::current_path()/"build"));
    require(!private_relative.empty() && !private_relative.is_absolute() && *private_relative.begin()!="..",
        "private XL traces must stay under repository build/; run from repository root");
    require(!std::filesystem::exists(output/"metadata.csv"),"trace output already contains a fixture");std::filesystem::create_directories(output);
    std::ifstream file(argv[2],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<Bank>();require(read_bank(bytes.data(),bytes.size(),*bank),"invalid XL trace bank");
    const auto& data=bank->programs[index];require(data.dynamics.enabled || !(mode&6),"reverb modes requested for effect graph");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);engine->set_analog(true);
    auto machine=std::make_unique<Machine>(*engine);LarcOperator op(*machine);
    const auto done=machine->run_task([&]{return setup(*machine,op,data,index,mode);});require(!done.failed,"XL trace physical setup failed");
    xl_test::ShapeCheck shape{*engine->host().dsp,Graph(index)};shape.run();require(shape.valid,"XL trace unsupported graph shape");
    std::array<float,4096> left{},right{};std::array<std::array<float,4096>,4> audio{};
    float* channels[]={audio[0].data(),audio[1].data(),audio[2].data(),audio[3].data()};
    for(unsigned n=0;n<warmup*48;n+=block)require(machine->render(left.data(),right.data(),channels,int(std::min(block,warmup*48-n))),"XL trace warmup failed");
    std::ofstream events(output/"events.csv"),states(output/"controller-states.csv"),instructions;
    if(all)instructions.open(output/"instructions.csv");
    require(bool(events) && bool(states) && (!all || bool(instructions)),"cannot open XL trace outputs");
    auto& host=engine->host();Probe probe{host,events,instructions,states,all};const auto start=host.cycles;
    if(all) {
        // A bounded private listing helps derive native branch/stage costs.
        // It is firmware-derived evidence, never a public source artifact.
        std::ofstream listing(output/"controller-listing.txt");
        listing<<lexicon224x::lens::cpu_listing(host.memory.data(),0x8000,1000);
        listing<<lexicon224x::lens::cpu_listing(host.memory.data(),0x0fa2,100);
        listing<<lexicon224x::lens::cpu_listing(host.memory.data(),0x1130,12);
        listing<<lexicon224x::lens::cpu_listing(host.memory.data(),0xb315,300);
        listing<<lexicon224x::lens::cpu_listing(host.memory.data(),0x0038,100);
        listing<<lexicon224x::lens::cpu_listing(host.memory.data(),0x03ee,200);
        listing<<lexicon224x::lens::cpu_listing(host.memory.data(),0xa791,150);
        listing<<lexicon224x::lens::cpu_listing(host.memory.data(),0xafd0,200);
        listing<<lexicon224x::lens::cpu_listing(host.memory.data(),0x1100,150);
        listing<<lexicon224x::lens::cpu_listing(host.memory.data(),0xb4f0,50);
        listing<<lexicon224x::lens::cpu_listing(host.memory.data(),0x11ca,80);
        require(bool(listing),"cannot finish private XL controller listing");
    }
    if(traced)probe.start();uint32_t random=17;uint64_t input_hash=14695981039346656037ull,audio_hash=input_hash;
    for(unsigned n=0;n<seconds*48000;n+=block) {
        const unsigned count=std::min(block,seconds*48000-n);
        for(unsigned i=0;i<count;++i)for(unsigned c=0;c<2;++c) {
            random=random*1664525u+1013904223u;float value=0;
            if(fixture=="noise" && n+i<9600)value=.08f*float(int32_t(random))/2147483648.f;
            if(fixture=="steps")value=((n+i)/6000)%4<2?((c?-.1f:.1f)*float(1+((n+i)/6000)%2)):0;
            (c?right:left)[i]=value;hash_bytes(input_hash,&value,sizeof(value));
        }
        require(machine->render(left.data(),right.data(),channels,int(count)),"XL trace render failed");
        for(unsigned i=0;i<count;++i)for(unsigned c=0;c<4;++c){require(std::isfinite(audio[c][i]),"nonfinite XL trace audio");hash_bytes(audio_hash,&audio[c][i],sizeof(float));}
    }
    const auto end=host.cycles;if(traced)probe.stop();
    uint64_t memory_hash=14695981039346656037ull,wcs_hash=memory_hash,delay_hash=memory_hash,cpu_hash=memory_hash,pipeline_hash=memory_hash;
    hash_bytes(memory_hash,host.memory.data(),host.memory.size());hash_bytes(wcs_hash,host.dsp->wcs,sizeof(host.dsp->wcs));hash_bytes(delay_hash,host.dsp->memory,sizeof(host.dsp->memory));
    const auto cpu=host.snapshot();const auto& dsp=*host.dsp;
    const std::array<uint64_t,8> cpu_fields={cpu.pc,cpu.sp,cpu.bc,cpu.de,cpu.hl,cpu.a,cpu.flags,cpu.interrupt_enabled};
    const std::array<uint64_t,13> pipeline_fields={dsp.pc,dsp.cpc,dsp.microinstruction,uint64_t(dsp.operand),uint64_t(dsp.partial),dsp.partial_negative,
        uint64_t(dsp.ACC),uint64_t(dsp.RR),dsp.xreg_to_cpu,dsp.xreg_from_cpu,dsp.counter_clear,dsp.restart,dsp.reset_pulse};
    hash_bytes(cpu_hash,cpu_fields.data(),sizeof(cpu_fields));hash_bytes(pipeline_hash,pipeline_fields.data(),sizeof(pipeline_fields));hash_bytes(pipeline_hash,dsp.R,sizeof(dsp.R));
    std::ofstream meta(output/"metadata.csv");
    meta<<"program,mode,fixture,seed,amplitude,warmup_ms,seconds,block,setup_protocol,traced,instructions,start_cycles,end_cycles,input_fnv64,audio_fnv64,memory_fnv64,wcs_fnv64,delay_fnv64,cpu_final_fnv64,dsp_pipeline_fnv64,mod_calls,slow_calls,fast_calls,grants,commits,displaced,irqs,open_controllers,open_irq,nominal_mod_tenths,nominal_slow_tenths,nominal_fast_tenths\n";
    meta<<index<<','<<mode<<','<<fixture<<",17,"<<(fixture=="steps"?.2f:fixture=="noise"?.08f:0.f)<<','<<warmup<<','<<seconds<<','<<block<<",physical_factory_keys_v1,"<<traced<<','<<all<<','<<start<<','<<end
        <<','<<input_hash<<','<<audio_hash<<','<<memory_hash<<','<<wcs_hash<<','<<delay_hash<<','<<cpu_hash<<','<<pipeline_hash;
    for(auto count:probe.entries)meta<<','<<count;
    meta<<','<<probe.grants<<','<<probe.commits<<','<<probe.displaced<<','<<probe.interrupts<<','<<probe.stack.size()<<','<<bool(probe.irq)
        <<','<<data.modulation.rate_tenths<<','<<data.dynamics.slow_rate_tenths<<','<<data.dynamics.fast_rate_tenths<<'\n';
    require(bool(meta) && bool(events) && bool(states) && (!all || bool(instructions)),"cannot finish XL trace outputs");
    std::cout<<graphs[index].name<<", mode="<<mode<<", "<<fixture<<": Mod/slow/fast="<<probe.entries[0]<<'/'<<probe.entries[1]<<'/'<<probe.entries[2]
        <<", grants="<<probe.grants<<", commits="<<probe.commits<<", displaced="<<probe.displaced<<", IRQ="<<probe.interrupts<<'\n';
}
