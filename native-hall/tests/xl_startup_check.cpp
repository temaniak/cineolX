// Physical v8.21 program/key/fader oracle. ROMs and prepared banks stay private.
// This does not inject RAM flags, a modulation clock, or WCS into native audio.
#include "xl_reference.hpp"
#include "../desktop/bank.hpp"
#include "../desktop/concert48.hpp"
#include <memory>
#include <new>
using namespace lexplug;using namespace lexplug::op;
using namespace cineol::xl;using xl_test::require;
static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n) {if(tracking) ++allocations;if(void* p=std::malloc(n?n:1)) return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {if(tracking && p) ++releases;std::free(p);}
void operator delete(void* p,size_t) noexcept {::operator delete(p);}
void* operator new[](size_t n) {return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}

static Task<void> pause(Machine& m,double seconds) {co_await m.sleep(seconds);}
static Task<void> pages_read(LarcOperator& op,PagesReading& pages) {co_await op.readPages(pages);}
static Task<void> move(Machine& m,LarcOperator& op,unsigned page,unsigned slot,uint8_t raw) {
    co_await op.moveSlider(page,slot,raw);co_await m.sleep(2);
    if(!co_await op.recordSettled()) co_await fail("XL fader record did not settle.");
}
static Task<void> toggle(Machine& m,LarcOperator& op,unsigned which,bool on) {
    const unsigned mask=which==0?1:which==1?64:128;
    for(unsigned attempt=0;attempt<8;++attempt) {
        co_await op.setToggle(which,on);co_await m.sleep(0.2);
        if(bool(m.peek(0x3ccd)&mask)==on) co_return;
    }
    co_await fail("XL physical toggle did not settle.");
}
template<class State> static void set_startup_lookup(State& state,bool startup) {
    // Keep this independently compiled oracle usable with frozen old headers.
    if constexpr(requires(State& s) {s.startup_lookup;}) state.startup_lookup=startup;
}
template<class State> static bool startup_lookup(const State& state) {
    if constexpr(requires(const State& s) {s.startup_lookup;}) return state.startup_lookup;
    else return false;
}

struct Probe {
    lexicon224x::cpu::Host& host;
    unsigned compiles=0,returns=0;
    bool pending=false;
    std::array<uint8_t,3> before{};
    ModulationState state{};
    ModulationProfile profile{};
    NetworkSettings<128> settings{};
    const ModulationProfile* step_profile=nullptr;
    Modulator step_native;
    NetworkSettings<128> step_settings{};
    unsigned step_calls=0,step_mismatches=0;
    bool step_started=false;
    unsigned word(unsigned a) const {return unsigned(host.memory[a])|unsigned(host.memory[a+1])<<8;}
    std::array<uint8_t,3> counters() const {return {host.memory[0x3e44],host.memory[0x3e45],host.memory[0x3e46]};}
    void start() {
        host.pc_watches[0xab4f]=host.pc_watches[0xad5c]=true;
        host.pc_observer=[this](uint64_t,lexicon224x::cpu::CpuSnapshot cpu) {
            host.pc_watches[cpu.pc]=true;
            if(cpu.pc==0xab4f) {require(!pending,"nested XL program compile");before=counters();pending=true;++compiles;return;}
            if(!pending) {
                if(!step_profile) return;
                const auto compiled_state=state;const auto compiled_settings=settings;
                observe_state();
                if(step_started) {
                    const auto& got=step_native.state();
                    step_mismatches+=got.index!=state.index || startup_lookup(got)!=startup_lookup(state) || got.divider!=state.divider ||
                        got.random_divider!=state.random_divider || got.random_hold!=state.random_hold ||
                        got.phase!=state.phase || got.address_low!=state.address_low;
                    for(unsigned i=0;i<(profile.flags&15);++i) for(unsigned pair=0;pair<2;++pair) {
                        const unsigned row=profile.rows[i]+pair;
                        step_mismatches+=step_settings.coefficients[row]!=settings.coefficients[row] ||
                            step_settings.offsets[row]!=settings.offsets[row];
                    }
                } else {step_native.reset(state);step_settings=settings;step_started=true;}
                step();state=compiled_state;settings=compiled_settings;return;
            }
            pending=false;++returns;
            // AB52 resets the sequence pointer. AD5C is observed before the
            // first enabled/disabled work-loop call after the compiler.
            require(word(0x3e47)==59,"XL program compiler did not reset sequence index to 59");
            require(counters()==before,"XL program compiler changed interpolation counters");
            observe_state();
            if(step_profile) {step_native.reset(state);step_settings=settings;step_started=true;step();}
        };
    }
    void observe_state() {
            state={};state.index=uint16_t(word(0x3e47)&4095);
            set_startup_lookup(state,(word(0x3e47)&0x8000)==0);
            const auto current=counters();state.divider=current[0];state.random_divider=current[1];state.random_hold=current[2];
            profile.flags=host.memory[0x3cf6];
            const unsigned descriptors=word(0x3cf4),count=profile.flags&15;
            for(unsigned i=0;i<count;++i) {
                unsigned a=word(descriptors+i*5);
                require(a>=0x4003 && a<0x4200 && (a&3)==3,"bad compiled interpolation descriptor");
                profile.rows[i]=uint8_t(127-(a-0x4000)/4);profile.caps[i]=host.memory[descriptors+i*5+2];
                state.address_low[i]=host.memory[descriptors+i*5+3];state.phase[i]=host.memory[descriptors+i*5+4];
            }
            for(unsigned row=0;row<128;++row) {
                const auto mi=lexicon224x::decode(host.dsp->wcs[row]);
                settings.coefficients[row]=int8_t(mi.negative?-int(mi.coefficient):int(mi.coefficient));
                settings.offsets[row]=uint16_t(~mi.low);
            }
    }
    void step() {
        if(host.memory[0x3ccd]&64) {step_native.step(*step_profile,step_settings);++step_calls;}
    }
    void stop() {require(!pending && compiles==returns,"incomplete XL compiler observation");
        host.pc_watches[0xab4f]=host.pc_watches[0xad5c]=false;host.pc_observer={};}
};

template<Graph graph> static unsigned check(Engine& engine,Machine& m,LarcOperator& op,const Bank& bank) {
    constexpr auto info=graph_info(graph);const auto& data=bank.programs[unsigned(graph)];
    auto run=[&](auto&& make){require(!m.run_task(make).failed,"XL startup operator failed");};
    Probe probe{engine.host()};probe.start();
    run([&]{return xl_test::select(m,op,info.bank,info.program);});
    // Start from physically confirmed Mod-off before testing edges. Selecting
    // a program restores its factory flags; each toggle is checked in RAM.
    run([&]{return toggle(m,op,0,false);});run([&]{return toggle(m,op,1,false);});
    run([&]{return toggle(m,op,2,false);});
    const auto compiled=probe.state;const auto compiled_settings=probe.settings;
    const auto compiled_profile=probe.profile;
    require((compiled_profile.flags&15)==(data.modulation.flags&15),"prepared interpolation count differs from ROM");
    auto core=std::make_unique<Network<graph>>();
    require(core->prepare(data.template settings<graph>()) &&
        core->set_modulation(data.modulation,data.initial_modulation,false),"native startup preparation failed");
    unsigned mismatches=0;
    if(data.modulation.flags&15) {
        const auto& native=core->modulation_state();
        mismatches+=native.index!=compiled.index;
        mismatches+=startup_lookup(native)!=startup_lookup(compiled);
        for(unsigned i=0;i<(compiled_profile.flags&15);++i) {
            require(data.modulation.rows[i]==compiled_profile.rows[i] && data.modulation.caps[i]==compiled_profile.caps[i],
                "prepared interpolation profile differs from ROM");
            mismatches+=native.phase[i]!=compiled.phase[i];mismatches+=native.address_low[i]!=compiled.address_low[i];
            for(unsigned r:{unsigned(compiled_profile.rows[i]),unsigned(compiled_profile.rows[i])+1}) {
                mismatches+=core->settings().coefficients[r]!=compiled_settings.coefficients[r];
                mismatches+=(core->settings().offsets[r]&255)!=(compiled_settings.offsets[r]&255);
            }
        }
    }
    const unsigned startup_mismatches=mismatches;
    const unsigned selected=probe.compiles;
    // Diagnostic law oracle at actual firmware entries, beginning before the
    // physical Mod-on key. It covers the SBC lookup and transition to NVS;
    // it does not claim a free-running native clock or inject native audio.
    // The bank deliberately normalizes inactive interpolation profiles.
    // Their private firmware loop counters are not an active native law;
    // startup/key/Size observations still run for these graphs.
    probe.step_profile=(data.modulation.flags&15)?&data.modulation:nullptr;
    run([&]{return toggle(m,op,1,true);});
    require(probe.compiles>selected,"physical Mod-on did not recompile XL program");
    require(probe.state.phase==compiled.phase && probe.state.address_low==compiled.address_low,
        "physical Mod-on did not restore interpolation descriptors");
    run([&]{return pause(m,1.37);});
    if(data.modulation.flags&15) require(probe.step_calls>100,"startup modulation transition window too short");
    const unsigned step_mismatches=probe.step_mismatches;probe.step_profile=nullptr;
    mismatches+=step_mismatches;
    const unsigned no_compile=probe.compiles;
    for(unsigned which:{2u,0u}) {run([&]{return toggle(m,op,which,true);});run([&]{return toggle(m,op,which,false);});}
    require(probe.compiles==no_compile,"Dynamic/Opt key unexpectedly recompiles XL program");
    auto audio=std::make_unique<Native48<graph>>();
    require(audio->prepare(data.template settings<graph>()) &&
        audio->set_modulation(data.modulation,data.initial_modulation,true),"native Size preparation failed");
    audio->set_dynamics(data.dynamics,data.initial_dynamics);
    auto raw=data.controls.factory;raw[42]&=uint8_t(~129u);
    audio->set_native_controls(data.template settings<graph>(),data.controls,raw,false);
    PagesReading pages;run([&]{return pages_read(op,pages);});unsigned chorus_moves=0,size_moves=0;
    for(unsigned p=0;p<unsigned(pages.count);++p) for(unsigned slot=0;slot<6;++slot) {
        const auto value=pages.pages[p].sliders[slot];
        const bool chorus=std::strncmp(value.shown.name,"CHORUS",6)==0;
        const bool size=std::strncmp(value.shown.name,"SIZE",4)==0;
        if(!chorus && !size) continue;
        const unsigned before=probe.compiles;
        // Use both ends: a nearby raw value can stay in the same Size bin.
        for(unsigned endpoint:{2u,254u}) {
            tracking=true;
            for(unsigned n=0;n<10000;++n) {float l,r;audio->process(n<1000?0.08f:0,0,l,r,true);}
            tracking=false;
            const auto retained=audio->concert().modulation_state();
            const auto tail=std::make_unique<std::array<int16_t,Network<graph>::delay_words>>(audio->concert().memory());
            const unsigned previous=probe.compiles;
            run([&]{return move(m,op,p+1,slot,uint8_t(endpoint));});
            for(unsigned cell=0;cell<raw.size();++cell) raw[cell]=m.peek(uint16_t(op.recordBase()+cell));
            tracking=true;audio->set_native_controls(data.template settings<graph>(),data.controls,raw,false);tracking=false;
            if(size && probe.compiles>previous) {
                const auto& got=audio->concert().modulation_state();
                if((data.modulation.flags&15) && (got.index!=probe.state.index || got.phase!=probe.state.phase))
                    std::cerr<<info.name<<" Size page="<<p+1<<" slot="<<slot<<" endpoint="<<endpoint
                        <<" raw-size="<<unsigned(raw[43])<<'/'<<unsigned(raw[44])<<" native-index="<<got.index
                        <<" ROM-index="<<probe.state.index<<" record-base="<<std::hex<<op.recordBase()<<std::dec
                        <<" mirrored-size="<<unsigned(engine.host().memory[0x3c9e])<<'/'<<unsigned(engine.host().memory[0x3cce])<<'\n';
                require(got.divider==retained.divider && got.random_divider==retained.random_divider &&
                    got.random_hold==retained.random_hold,"native Size edge reset counters");
                require(audio->concert().memory()==*tail,"native Size edge cleared the tail");
                if(data.modulation.flags&15) {
                    mismatches+=got.index!=probe.state.index;
                    mismatches+=startup_lookup(got)!=startup_lookup(probe.state);
                    for(unsigned i=0;i<(compiled_profile.flags&15);++i) {
                        mismatches+=got.phase[i]!=probe.state.phase[i];mismatches+=got.address_low[i]!=probe.state.address_low[i];
                        for(unsigned r:{unsigned(compiled_profile.rows[i]),unsigned(compiled_profile.rows[i])+1}) {
                            mismatches+=audio->concert().settings().coefficients[r]!=probe.settings.coefficients[r];
                            mismatches+=audio->concert().settings().offsets[r]!=probe.settings.offsets[r];
                        }
                    }
                }
            }
        }
        if(chorus) {require(probe.compiles==before,"Chorus unexpectedly recompiles XL program");++chorus_moves;}
        if(size) {require(probe.compiles>before,"Size endpoints did not recompile XL program");++size_moves;}
        run([&]{return move(m,op,p+1,slot,value.raw);});
    }
    const unsigned size_mismatches=mismatches-startup_mismatches-step_mismatches;
    const unsigned before_off=probe.compiles;run([&]{return toggle(m,op,1,false);});
    require(probe.compiles>before_off,"physical Mod-off did not recompile XL program");
    require(probe.state.phase==compiled.phase && probe.state.address_low==compiled.address_low,
        "physical Mod-off did not restore interpolation descriptors");
    // A native key edge restores the independently observed compiler state,
    // retaining its own free-running counters, graph registers and full tail.
    // The reference counter values are deliberately not injected here.
    core->enable_modulation(true);
    tracking=true;
    for(unsigned n=0;n<72000;++n) {int16_t out[4];core->process(n<2000?1000:0,n<2000?-500:0,out);}
    tracking=false;
    const auto counters=core->modulation_state();
    const auto memory=std::make_unique<std::array<int16_t,Network<graph>::delay_words>>(core->memory());
    const auto accumulator=core->accumulator();const auto result=core->result();
    const auto saturations=core->saturation_count();
    tracking=true;core->enable_modulation(false);tracking=false;
    const auto reset=core->modulation_state();
    require(reset.divider==counters.divider && reset.random_divider==counters.random_divider &&
        reset.random_hold==counters.random_hold,"native Mod edge reset retained counters");
    require(core->memory()==*memory && core->accumulator()==accumulator && core->result()==result &&
        core->saturation_count()==saturations,"native Mod edge cleared or altered the tail/graph state");
    if(data.modulation.flags&15) {
        mismatches+=reset.index!=compiled.index;
        mismatches+=startup_lookup(reset)!=startup_lookup(compiled);
        for(unsigned i=0;i<(compiled_profile.flags&15);++i) {
            mismatches+=reset.phase[i]!=compiled.phase[i];mismatches+=reset.address_low[i]!=compiled.address_low[i];
            for(unsigned r:{unsigned(compiled_profile.rows[i]),unsigned(compiled_profile.rows[i])+1}) {
                mismatches+=core->settings().coefficients[r]!=compiled_settings.coefficients[r];
                mismatches+=(core->settings().offsets[r]&255)!=(compiled_settings.offsets[r]&255);
            }
        }
    }
    probe.stop();
    std::cout<<info.name<<": "<<probe.compiles<<" compiles; seed=59, counters retained; Chorus="<<chorus_moves
        <<", Size="<<size_moves<<"; startup="<<startup_mismatches<<", Size="<<size_mismatches
        <<", Mod="<<mismatches-startup_mismatches-size_mismatches-step_mismatches
        <<", initial steps="<<probe.step_calls<<'/'<<step_mismatches
        <<((data.modulation.flags&15)?" errors":" (inactive interpolation; not compared)")
        <<"; native startup/Mod/Size mismatches="<<mismatches<<std::endl;
    return mismatches;
}

int main(int argc,char** argv) {
    require(argc==3 || argc==4,"usage: cineol_xl_startup_check ROM_DIRECTORY BANK [PROGRAM_INDEX]");
    std::ifstream input(argv[2],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    auto bank=std::make_unique<Bank>();require(read_bank(bytes.data(),bytes.size(),*bank),"invalid XL startup bank");
    const int selected=argc==4?std::atoi(argv[3]):-1;require(selected<22 && selected>=-1,"invalid program index");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);
    auto machine=std::make_unique<Machine>(*engine);LarcOperator op(*machine);
    require(!machine->run_task([&]{return xl_test::boot(*machine,op);}).failed,"XL startup boot failed");
    unsigned mismatches=0;
    each_graph([&]<Graph graph>() {if(selected<0 || selected==int(graph)) mismatches+=check<graph>(*engine,*machine,op,*bank);});
    require(allocations==0 && releases==0,"native XL startup/Mod edge allocated or released memory");
    require(mismatches==0,"native XL startup differs from physical compiler state");
    std::cout<<"XL physical startup/key oracle passed\n";
}
