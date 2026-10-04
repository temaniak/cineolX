#include "../desktop/concert48.hpp"
#include <analog/filters.hpp>
#include "../core/rate48.hpp"
#include "xl_reference.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <new>

static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n) {if(tracking) ++allocations;if(void* p=std::malloc(n?n:1)) return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {if(tracking && p) ++releases;std::free(p);}
void operator delete(void* p,size_t) noexcept {::operator delete(p);}
void* operator new[](size_t n) {return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
using namespace lexplug;using namespace lexplug::op;
using xl_test::require;using xl_test::load;using xl_test::boot;using xl_test::ShapeCheck;
static Task<void> change(Machine& machine,LarcOperator& op,unsigned set) {
    const unsigned raw=18+set*36;
    co_await op.moveSlider(1,0,raw);co_await op.moveSlider(1,1,254-raw);
    co_await op.moveSlider(1,4,raw);co_await op.moveSlider(2,3,raw);
    co_await op.moveSlider(2,4,raw);co_await op.moveSlider(2,5,raw);
    co_await op.moveSlider(3,0,raw);co_await op.moveSlider(4,0,raw);
    co_await op.moveSlider(5,0,raw);
    co_await machine.sleep(1);
}
static Task<void> enable_modulation(Machine& machine,LarcOperator& op,unsigned bank,unsigned program) {
    require(co_await op.selectProgram(int(bank),int(program)),"XL program reload failed");
    co_await op.setToggle(0,false);co_await op.setToggle(2,false);
    co_await op.setToggle(1,true);co_await machine.sleep(1);
}
static Task<void> check_chorus(Machine& machine,LarcOperator& op) {
    cineol::xl::ModulationProfile compiled;
    for(unsigned raw=2;raw<255;raw+=8) {
        co_await op.moveSlider(2,2,raw);compiled.set_chorus(uint8_t(raw));
        require(compiled.period==machine.peek(0x3cd2) && compiled.step==machine.peek(0x3cd4),
            "native Chorus compiler differs from firmware");
    }
    co_await op.moveSlider(2,2,128);
}
static Task<void> wait_modulation(Machine& machine) {co_await machine.sleep(5);}
template<cineol::xl::Graph graph> static void check_modulation(Engine& engine,Machine& machine,LarcOperator& op) {
    using Core=cineol::xl::Network<graph>;
    constexpr auto info=cineol::xl::graph_info(graph);
    auto result=machine.run_task([&]{return enable_modulation(machine,op,info.bank,info.program);});
    require(!result.failed,"modulation preparation failed");
    result=machine.run_task([&]{return check_chorus(machine,op);});require(!result.failed,"Chorus fixture preparation failed");
    auto& host=engine.host();ShapeCheck shape{*host.dsp,graph};shape.run();
    require(shape.valid,"Mode Enhancement changes the supported graph");
    auto word=[&](unsigned a){return unsigned(host.memory[a])|unsigned(host.memory[a+1])<<8;};
    cineol::xl::ModulationProfile profile;
    std::copy_n(host.memory.begin()+0x8000,4096,profile.sequence.begin());
    profile.flags=host.memory[0x3cf6];profile.period=host.memory[0x3cd2];
    profile.hold=host.memory[0x3cd3];profile.step=host.memory[0x3cd4];profile.mask=host.memory[0x3cd5];
    const unsigned descriptors=word(0x3cf4);
    const unsigned taps=profile.flags&15;
    require(taps<=profile.max_taps,"unsupported XL modulation tap count");
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
                require(predicted.coefficients[r]==actual.coefficients[r] && predicted.offsets[r]==actual.offsets[r],
                    "native XL modulation tap differs from firmware");
        } else {started=true;first=cycles;native.reset(wanted);predicted=actual;}
        native.step(profile,predicted);last=cycles;++calls;host.pc_watches[0xad5c]=true;
    };
    result=machine.run_task([&]{return wait_modulation(machine);});require(!result.failed,"modulation oracle failed");
    host.pc_watches[0xad5c]=false;host.pc_observer={};
    require(calls>1000,"modulation fixture did not exercise enough transitions");
    profile.rate_tenths=uint32_t(std::lround(double(calls-1)*20480000.0/double(last-first)));
    require(profile.valid(Core::rows),"invalid prepared modulation profile");
    std::cout<<info.name<<" Chorus: 32 compiler positions exact; modulation: "<<calls-1<<" firmware transitions, "<<taps<<" taps/state exact; nominal calls="<<profile.rate_tenths/10.0<<" Hz\n";
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
    require(argc==2 || (argc==3 && std::string(argv[2])=="--rich-only"),"usage: cineol_xl_concert_check ROM_DIRECTORY [--rich-only]");
    auto engine=std::make_unique<Engine>(0);load(*engine,argv[1]);
    auto machine=std::make_unique<Machine>(*engine);LarcOperator op(*machine);
    auto result=machine->run_task([&]{return boot(*machine,op);});require(!result.failed,"firmware boot failed");
    if(argc==3) {
        check_modulation<cineol::xl::Graph::rich_chamber>(*engine,*machine,op);
        require(allocations==0 && releases==0,"Rich Chamber modulation allocated");return 0;
    }
    auto native=std::make_unique<cineol::xl::Concert>();
    auto reference=std::make_unique<lexicon224x::Machine>();
    uint32_t random=17;uint64_t wanted_saturations=0;
    for(unsigned fixture=0;fixture<6;++fixture) {
        if(fixture) {
            result=machine->run_task([&]{return change(*machine,op,fixture-1);});
            if(result.failed) std::cerr<<result.error.text<<'\n';
            require(!result.failed,"firmware control preparation failed");
        }
        const auto& original=*engine->host().dsp;
        ShapeCheck shape{original};shape.run();require(shape.valid,"unsupported Concert Hall graph variation");
        cineol::xl::ConcertSettings settings;
        for(unsigned row=0;row<settings.rows;++row) {
            auto mi=lexicon224x::decode(original.wcs[row]);
            settings.coefficients[row]=int8_t(mi.negative?-int(mi.coefficient):int(mi.coefficient));
            settings.offsets[row]=uint16_t(~mi.low);
        }
        if(fixture==0) require(native->prepare(settings),"native prepare failed");
        else require(native->set_settings(settings),"native control update failed");
        std::copy(std::begin(original.wcs),std::end(original.wcs),std::begin(reference->wcs));
        tracking=true;
        for(unsigned frame=0;frame<12000;++frame) {
            random=random*1664525u+1013904223u;int16_t left=int16_t(random>>16);
            random=random*1664525u+1013904223u;int16_t right=int16_t(random>>16),actual[4],wanted[4]{};
            for(unsigned row=0;row<105;++row) {
                lexicon224x::fetch(*reference);lexicon224x::converter_clock(*reference);
                if(row==0) reference->fpc.input_sample=uint16_t(left);
                if(row==52) reference->fpc.input_sample=uint16_t(right);
                if(reference->mi.wr_da) for(unsigned c=0;c<4;++c)
                    if(reference->mi.channels&(1u<<c)) wanted[c]=int16_t(lexicon224x::source_value(*reference,reference->mi));
                lexicon224x::execute(*reference);
                const unsigned sat=reference->saturated;
                wanted_saturations+=(sat&1)+((sat>>1)&1)+((sat>>2)&1);
                if(reference->mi.xfer && (sat&4)) ++wanted_saturations;
            }
            native->process(left,right,actual);
            require(std::equal(actual,actual+4,wanted),"native/reference output differs");
            require(native->accumulator()==reference->ACC && native->result()==reference->RR,"native arithmetic state differs");
            require(native->saturation_count()==wanted_saturations,"native saturation count differs");
        }
        tracking=false;
        require(std::equal(native->memory().begin(),native->memory().end(),reference->memory,
            [](int16_t a,uint16_t b){return uint16_t(a)==b;}),"native delay memory differs");
        auto audio=std::make_unique<cineol::xl::Concert48>();require(audio->prepare(settings),"XL audio preparation failed");
        double energy=0,tail=0;float peak=0;tracking=true;
        for(unsigned n=0;n<144000;++n) {
            float l,r;audio->process(n==0?0.5f:0,n<12000?0.05f*std::sin(n*0.17f):0,l,r);
            require(std::isfinite(l) && std::isfinite(r),"nonfinite XL audio");
            peak=std::max({peak,std::abs(l),std::abs(r)});energy+=double(l)*l+double(r)*r;
            if(n>48000) tail+=double(l)*l+double(r)*r;
        }
        tracking=false;require(peak<4 && energy>1e-6 && tail>1e-10,"silent/runaway XL audio or missing tail");
        audio->set_global(0,0,true,0,2);
        for(unsigned n=0;n<10000;++n) {float l,r;audio->process(0,0,l,r);}
        tracking=true;
        for(unsigned n=0;n<128;++n) {
            float l,r;audio->process(n==0?0.3f:0,n==0?-0.2f:0,l,r);
            require(std::abs(l-(n==56?0.3f:0))<1e-6f && std::abs(r-(n==56?-0.2f:0))<1e-6f,"XL dry alignment failed");
        }
        tracking=false;
        std::cout<<"Concert Hall fixture "<<fixture<<": 12000 stereo frames, A-D/state/memory/saturation exact\n";
    }
    // Compare both sampled circuit chains to the independent double-precision
    // modal model on the same 48 kHz grid. This does not assert event-timed
    // equivalence of the complete original converter/analog hardware.
    for(bool input:{true,false}) {
        cineol::xl::Analog48 filter;filter.prepare(input);
        const auto modal=input?lexicon224x::analog::ain_modal():lexicon224x::analog::aout_modal();
        lexicon224x::analog::Chain reference_filter(modal);float previous=0;double error=0;
        for(unsigned n=0;n<48000;++n) {
            const float sample=0.2f*std::sin(n*0.37f)+0.1f*std::sin(n*1.9f);
            reference_filter.advance(12000000,previous,double(sample-previous)*48000);
            const double expected=reference_filter.output(sample);
            const float actual=filter.process(sample);error=std::max(error,std::abs(actual-expected));previous=sample;
        }
        require(error<0.0001,"XL filter differs from independent circuit model");
        std::cout<<(input?"AIN":"AOUT")<<" sampled circuit max error="<<error<<'\n';
    }
    check_modulation<cineol::xl::Graph::concert>(*engine,*machine,op);
    check_modulation<cineol::xl::Graph::bright_hall>(*engine,*machine,op);
    check_modulation<cineol::xl::Graph::dark_hall>(*engine,*machine,op);
    check_modulation<cineol::xl::Graph::plate>(*engine,*machine,op);
    check_modulation<cineol::xl::Graph::room>(*engine,*machine,op);
    require(allocations==0 && releases==0,"allocation/release in native audio");
    native_hall::RationalFilter<128,189,64,2> down;native_hall::RationalFilter<189,128,32,4> up;
    down.prepare();up.prepare();unsigned count=0;float input[2]{};
    tracking=true;
    for(unsigned n=0;n<189000;++n) down.process(input,[&](const float*){++count;});
    tracking=false;require(count==128000 && allocations==0 && releases==0,"rational XL clock drift/allocation");
    count=0;float four[4]{};tracking=true;
    for(unsigned n=0;n<128000;++n) up.process(four,[&](const float*){++count;});
    tracking=false;require(count==189000 && allocations==0 && releases==0,"rational XL output clock drift/allocation");
    native->reset();require(std::all_of(native->memory().begin(),native->memory().end(),[](int16_t v){return v==0;}),"tail reset failed");
    const auto start=std::chrono::steady_clock::now();int16_t output[4];
    for(unsigned n=0;n<325079;++n) native->process(int16_t(n),int16_t(-int(n)),output);
    std::cout<<"Native core: "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()
        <<"s / ~10s audio; storage="<<sizeof(*native)<<" bytes; SAT="<<native->saturation_count()<<"; allocation/release=0; exact rational clocks; tail reset pass\n";
}
