#include "../desktop/concert.hpp"
#include "xl_reference.hpp"
#include <cmath>
#include <memory>
#include <new>
static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n) {if(tracking) ++allocations;if(void* p=std::malloc(n?n:1)) return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {if(tracking && p) ++releases;std::free(p);}
void operator delete(void* p,size_t) noexcept {::operator delete(p);}
void* operator new[](size_t n) {return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
using namespace lexplug;using namespace lexplug::op;using xl_test::require;
static Task<void> enable_modulation(Machine& machine,LarcOperator& op,unsigned bank,unsigned program) {
    require(co_await op.selectProgram(int(bank),int(program)),"XL program reload failed");
    co_await op.setToggle(0,false);co_await op.setToggle(2,false);
    co_await op.setToggle(1,true);co_await machine.sleep(1);
}
static Task<void> check_chorus(Machine& machine,LarcOperator& op,unsigned page,unsigned slider,unsigned column) {
    cineol::xl::ModulationProfile compiled;
    for(unsigned raw=2;raw<255;raw+=8) {
        co_await op.moveSlider(page,slider,raw);compiled.set_chorus(op.stored(column,slider));
        require(compiled.period==machine.peek(0x3cd2) && compiled.step==machine.peek(0x3cd4),
            "native Chorus compiler differs from firmware");
    }
    co_await op.moveSlider(page,slider,128);
}
static Task<void> wait_modulation(Machine& machine) {co_await machine.sleep(5);}
template<cineol::xl::Graph graph> static void check_modulation(Engine& engine,Machine& machine,LarcOperator& op) {
    using Core=cineol::xl::Network<graph>;
    constexpr auto info=cineol::xl::graph_info(graph);
    auto result=machine.run_task([&]{return enable_modulation(machine,op,info.bank,info.program);});
    require(!result.failed,"modulation preparation failed");
    PagesReading pages;result=machine.run_task([&]()->Task<void>{co_await op.readPages(pages);});
    require(!result.failed,"modulation page metadata failed");
    unsigned chorus_page=0,chorus_slot=0,chorus_column=0;
    for(unsigned p=0;p<unsigned(pages.count);++p) for(unsigned slot=0;slot<6;++slot)
        if(std::strncmp(pages.pages[p].sliders[slot].shown.name,"CHORUS",6)==0) {
            chorus_page=p+1;chorus_slot=slot;chorus_column=pages.pages[p].column;
        }
    if(chorus_page) {
        result=machine.run_task([&]{return check_chorus(machine,op,chorus_page,chorus_slot,chorus_column);});
        require(!result.failed,"Chorus fixture preparation failed");
    }
    auto& host=engine.host();xl_test::ShapeCheck shape{*host.dsp,graph};shape.run();
    require(shape.valid,"Mode Enhancement changes the supported graph");
    auto word=[&](unsigned a){return unsigned(host.memory[a])|unsigned(host.memory[a+1])<<8;};
    cineol::xl::ModulationProfile profile;
    std::copy_n(host.memory.begin()+0x8000,4096,profile.sequence.begin());
    profile.flags=host.memory[0x3cf6];profile.period=host.memory[0x3cd2];
    profile.hold=host.memory[0x3cd3];profile.step=host.memory[0x3cd4];profile.mask=host.memory[0x3cd5];
    const unsigned descriptors=word(0x3cf4);
    const unsigned taps=profile.flags&15;
    require(taps<=profile.max_taps,"unsupported XL modulation tap count");
    if(!taps) {std::cout<<info.name<<": no interpolation targets, no active modulation required\n"<<std::flush;return;}
    for(unsigned i=0;i<taps;++i) {
        const unsigned address=word(descriptors+i*5);
        require(address>=0x4003 && address<0x4200 && (address&3)==3,"bad modulation descriptor address");
        profile.rows[i]=uint8_t(127-(address-0x4000)/4);profile.caps[i]=host.memory[descriptors+i*5+2];
        for(unsigned pair=0;pair<2;++pair) profile.negative[i][pair]=
            lexicon224x::decode(host.dsp->wcs[profile.rows[i]+pair]).negative;
    }
    auto state=[&] {
        cineol::xl::ModulationState s;
        s.divider=host.memory[0x3e44];s.random_divider=host.memory[0x3e45];s.random_hold=host.memory[0x3e46];
        s.index=uint16_t(word(0x3e47)&4095);
        for(unsigned i=0;i<taps;++i) {s.address_low[i]=host.memory[descriptors+i*5+3];s.phase[i]=host.memory[descriptors+i*5+4];}
        return s;
    };
    auto settings=[&] {
        typename Core::Settings s;
        for(unsigned row=0;row<s.rows;++row) {auto mi=lexicon224x::decode(host.dsp->wcs[row]);
            s.coefficients[row]=int8_t(mi.negative?-int(mi.coefficient):int(mi.coefficient));s.offsets[row]=uint16_t(~mi.low);}
        return s;
    };
    cineol::xl::Modulator native;typename Core::Settings predicted;
    bool started=false;unsigned calls=0;uint64_t first=0,last=0;
    // Each call boundary checks the previous native transition against the
    // 8080's RAM and every actual WCS tap pair, including boundary reversals.
    host.pc_watches[0xad5c]=true;
    host.pc_observer=[&](uint64_t cycles,lexicon224x::cpu::CpuSnapshot) {
        const auto wanted=state();const auto actual=settings();
        if(started) {
            const auto& got=native.state();
            require(got.index==wanted.index && got.divider==wanted.divider && got.random_divider==wanted.random_divider &&
                got.random_hold==wanted.random_hold && got.address_low==wanted.address_low && got.phase==wanted.phase,
                "native XL modulation state differs from firmware");
            for(unsigned i=0;i<taps;++i) for(unsigned r:{unsigned(profile.rows[i]),unsigned(profile.rows[i])+1})
                if(predicted.coefficients[r]!=actual.coefficients[r] || predicted.offsets[r]!=actual.offsets[r]) {
                    std::cerr<<info.name<<" tap="<<i<<" row="<<r<<" native="<<int(predicted.coefficients[r])
                        <<" ROM="<<int(actual.coefficients[r])<<" offset="<<predicted.offsets[r]
                        <<" ROM-offset="<<actual.offsets[r]<<" cap="<<unsigned(profile.caps[i])<<" flags="<<unsigned(profile.flags)<<'\n';
                    require(false,"native XL modulation tap differs from firmware");
                }
        } else {started=true;first=cycles;native.reset(wanted);predicted=actual;}
        native.step(profile,predicted);last=cycles;++calls;host.pc_watches[0xad5c]=true;
    };
    result=machine.run_task([&]{return wait_modulation(machine);});require(!result.failed,"modulation oracle failed");
    host.pc_watches[0xad5c]=false;host.pc_observer={};
    require(calls>1000,"modulation fixture did not exercise enough transitions");
    profile.rate_tenths=uint32_t(std::lround(double(calls-1)*20480000.0/double(last-first)));
    require(profile.valid(Core::rows),"invalid prepared modulation profile");
    std::cout<<info.name<<" Chorus: 32 compiler positions exact; modulation: "<<calls-1<<" firmware transitions, "<<taps<<" taps/state exact; nominal calls="<<profile.rate_tenths/10.0<<" Hz\n"<<std::flush;
    // Validate the internal rational control clock independently of its
    // measured nominal rate and guard reset/reprepare behavior.
    auto audio=std::make_unique<Core>();const auto initial=state();const auto initial_settings=settings();
    require(audio->prepare(initial_settings) && audio->set_modulation(profile,initial,true),"native modulation setup failed");
    auto invalid_profile=profile;invalid_profile.period=0;
    auto invalid_state=initial;invalid_state.divider=0;
    require(!audio->set_modulation(invalid_profile,initial,true) && !audio->set_modulation(profile,invalid_state,true),
        "invalid native modulation configuration accepted");
    native.reset(initial);predicted=initial_settings;uint64_t clock=0;
    tracking=true;
    for(unsigned n=0;n<72000;++n) {
        int16_t output[4];audio->process(0,0,output);
        clock+=uint64_t(profile.rate_tenths)*audio->rate_denominator;
        if(clock>=uint64_t(audio->rate_numerator)*10) {clock-=uint64_t(audio->rate_numerator)*10;native.step(profile,predicted);}
        require(audio->settings().coefficients==predicted.coefficients && audio->settings().offsets==predicted.offsets,
            "rational modulation clock drift");
    }
    tracking=false;
    audio->reset();require(audio->settings().coefficients==initial_settings.coefficients &&
        audio->settings().offsets==initial_settings.offsets && audio->modulation_state().phase==initial.phase,"modulation reset mismatch");
    require(audio->prepare(initial_settings),"static reprepare failed");
    tracking=true;
    for(unsigned n=0;n<1000;++n) {int16_t out[4];audio->process(0,0,out);}
    tracking=false;
    require(audio->settings().coefficients==initial_settings.coefficients && audio->settings().offsets==initial_settings.offsets,
        "modulation survived static reprepare");
}
int main(int argc,char** argv) {
    require(argc==2 || argc==3,"usage: cineol_xl_modulation_check ROM_DIRECTORY [PROGRAM_INDEX]");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);
    auto machine=std::make_unique<Machine>(*engine);LarcOperator op(*machine);
    auto result=machine->run_task([&]{return xl_test::boot(*machine,op);});require(!result.failed,"XL boot failed");
    const int selected=argc==3?std::atoi(argv[2]):-1;
    cineol::xl::each_graph([&]<cineol::xl::Graph graph>() {
        if(selected<0 || selected==int(graph)) check_modulation<graph>(*engine,*machine,op);
    });
    require(allocations==0 && releases==0,"native modulation allocated/released memory");
    std::cout<<"All requested native modulation checks passed; allocation/release=0\n";
}
