#include "../desktop/bank.hpp"
#include "../desktop/concert48.hpp"
#include "xl_reference.hpp"
#include <cstring>
#include <new>
using namespace lexplug;using namespace lexplug::op;using xl_test::require;
using namespace cineol::xl;
static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n) {if(tracking) ++allocations;if(void* p=std::malloc(n?n:1)) return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {if(tracking && p) ++releases;std::free(p);}
void operator delete(void* p,size_t) noexcept {::operator delete(p);}
void* operator new[](size_t n) {return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
static DynamicsState state(const lexicon224x::cpu::Host& h) {
    DynamicsState s;s.held=uint16_t(h.memory[0x3e0f]|unsigned(h.memory[0x3e10])<<8);
    s.average=h.memory[0x3c50];s.flags=h.memory[0x3c51];s.low=h.memory[0x3c52];s.mid=h.memory[0x3c53];
    s.trigger_peak=h.memory[0x3c54];s.stop_counter=h.memory[0x3c5f];s.stopped=h.memory[0x3e11];
    s.amount=h.memory[0x3e12];s.divider=h.memory[0x3e13];s.period=h.memory[0x3e14];
    s.peak_divider=h.memory[0x3c38];s.peak_input=h.memory[0x3c61];
    std::copy_n(h.memory.begin()+0x3e15,11,s.history.begin());return s;
}
static void compare(const DynamicsState& a,const DynamicsState& b,const char* name,unsigned tick) {
    const unsigned actual[]={a.held,a.average,a.flags,a.low,a.mid,a.trigger_peak,a.stop_counter,a.stopped,
        a.amount,a.divider,a.period,a.peak_divider,a.peak_input};
    const unsigned wanted[]={b.held,b.average,b.flags,b.low,b.mid,b.trigger_peak,b.stop_counter,b.stopped,
        b.amount,b.divider,b.period,b.peak_divider,b.peak_input};
    for(unsigned i=0;i<std::size(actual);++i) if(actual[i]!=wanted[i]) {
        std::cerr<<name<<" tick="<<tick<<" state-field="<<i<<" native="<<actual[i]<<" firmware="<<wanted[i]<<'\n';
        require(false,"XL dynamics state differs from firmware");
    }
    require(a.history==b.history,"XL optimization history differs from firmware");
}
static Task<void> set_toggle(Machine& m,LarcOperator& op,unsigned index,bool enabled) {
    const unsigned mask=index==0?1:128;
    // Dynamic compilation can drop a LARC key. Retry the real operator action
    // and verify the stored switch before observing controller transitions.
    for(unsigned attempt=0;attempt<8;++attempt) {
        co_await op.setToggle(int(index),enabled);co_await m.sleep(0.2);
        if(bool(m.peek(0x3ccd)&mask)==enabled) co_return;
    }
    co_await fail("dynamics fixture switch did not settle");
}
static Task<void> fixture(Machine& m,LarcOperator& op,const ProgramData& data,bool dynamic,bool opt) {
    co_await set_toggle(m,op,0,dynamic);co_await set_toggle(m,op,2,opt);
    // Use visibly distinct normal/stop times and exercise the actual fader
    // calibration rather than injecting logical control bytes into firmware.
    for(unsigned p=0;p<data.page_count;++p) for(unsigned slot=0;slot<6;++slot) {
        const unsigned cell=data.pages[p].cells[slot];
        const unsigned stop=data.dynamics.shared_stop?6:12;
        if(cell==stop || cell==stop+1) co_await op.moveSlider(p+1,slot,18);
        if(cell==45) co_await op.moveSlider(p+1,slot,10);
    }
    co_await m.sleep(0.5);
}
static Task<void> finish_fixture(Machine& m,LarcOperator& op) {
    co_await set_toggle(m,op,0,false);co_await set_toggle(m,op,2,false);co_await m.sleep(1);
}
static void check(Engine& engine,Machine& m,LarcOperator& op,const ProgramData& data,unsigned program) {
    const auto info=graphs[program];auto& host=engine.host();
    auto result=m.run_task([&]{return xl_test::select(m,op,info.bank,info.program);});if(result.failed) std::cerr<<result.error.text<<"\n";require(!result.failed,"dynamics selection failed");
    if(!data.dynamics.enabled) {std::cout<<info.name<<": no reverb dynamics in firmware\n"<<std::flush;return;}
    unsigned total=0;
    for(unsigned combination=0;combination<4;++combination) {
        const bool dynamic=combination&1,opt=combination&2;
        result=m.run_task([&]{return fixture(m,op,data,dynamic,opt);});require(!result.failed,"dynamics fixture failed");
        Dynamics native,fast;bool pending=false,fast_pending=false;unsigned ticks=0,fast_ticks=0;
        std::array<uint8_t,48> initial_raw;std::copy_n(host.memory.begin()+0x3ca3,48,initial_raw.begin());
        uint8_t feedback_mid=initial_raw[1],feedback_amount=0;bool fitted=!data.controls.feedback.count;
        for(unsigned index=1;index<32 && !fitted;++index) for(unsigned amount=0;amount<=20 && !fitted;++amount) {
            auto candidate=initial_raw;candidate[1]=uint8_t(index*8);NetworkSettings<128> settings;
            data.controls.apply_feedback(settings,candidate,amount);bool match=true;
            for(unsigned t=0;t<data.controls.feedback.count;++t) for(unsigned pair=0;pair<3;++pair) {
                const unsigned row=data.controls.feedback.targets[t].row+pair;const auto mi=lexicon224x::decode(host.dsp->wcs[row]);
                match&=settings.coefficients[row]==(mi.negative?-int(mi.coefficient):int(mi.coefficient));
            }
            if(match) {feedback_mid=candidate[1];feedback_amount=uint8_t(amount);fitted=true;}
        }
        require(fitted,"initial feedback state cannot be represented natively");
        host.pc_watches[0xb000]=host.pc_watches[0x8317]=host.pc_watches[0x85af]=host.pc_watches[0x81ee]=host.pc_watches[0x823d]=true;
        auto observed=[&] {auto s=state(host);s.feedback_mid=feedback_mid;s.feedback_amount=feedback_amount;return s;};
        host.pc_observer=[&](uint64_t,lexicon224x::cpu::CpuSnapshot cpu) {
            host.pc_watches[cpu.pc]=true;
            if(cpu.pc==0xb000) {feedback_mid=uint8_t(host.memory[0x3e3e]<<3);feedback_amount=host.memory[0x3e12];}
            else if(cpu.pc==0x81ee) {
                fast.reset(observed());std::array<uint8_t,48> raw;std::copy_n(host.memory.begin()+0x3ca3,48,raw.begin());
                tracking=true;fast.fast_step(uint8_t(cpu.bc>>8),data.dynamics,data.controls,raw);tracking=false;fast_pending=true;
            } else if(cpu.pc==0x823d && fast_pending) {
                compare(fast.state(),state(host),(std::string(info.name)+" fast").c_str(),fast_ticks++);fast_pending=false;
            } else if(cpu.pc==0x8317) {
                native.reset(observed());std::array<uint8_t,48> raw;std::copy_n(host.memory.begin()+0x3ca3,48,raw.begin());
                if(bool(raw[42]&1)!=dynamic || bool(raw[42]&128)!=opt) {
                    std::cerr<<info.name<<" combination="<<combination<<" raw toggles="<<unsigned(raw[42])<<'\n';
                    require(false,"firmware dynamics switches differ from fixture");
                }
                tracking=true;native.slow_step(uint8_t(cpu.bc>>8),host.memory[0x3e3d],data.dynamics,data.controls,raw,dynamic,opt);tracking=false;pending=true;
            } else if(cpu.pc==0x85af && pending) {
                compare(native.state(),state(host),(std::string(info.name)+" slow combination="+std::to_string(combination)).c_str(),ticks++);pending=false;
                std::array<uint8_t,48> raw;std::copy_n(host.memory.begin()+0x3ca3,48,raw.begin());
                raw[0]=native.state().low;raw[1]=native.state().mid;
                NetworkSettings<128> predicted;predicted.coefficients=data.coefficients;predicted.offsets=data.offsets;
                tracking=true;data.controls.apply_size(predicted,raw);data.controls.apply_static(predicted,raw,native.state().amount);
                auto feedback_raw=raw;feedback_raw[1]=native.state().feedback_mid;
                data.controls.apply_feedback(predicted,feedback_raw,native.state().feedback_amount);tracking=false;
                auto coefficient=[&](unsigned row) {
                    const auto mi=lexicon224x::decode(host.dsp->wcs[row]);const int wanted=mi.negative?-int(mi.coefficient):int(mi.coefficient);
                    if(predicted.coefficients[row]!=wanted) {
                        std::cerr<<info.name<<" dynamic row="<<row<<" native="<<int(predicted.coefficients[row])<<" firmware="<<wanted<<'\n';
                        require(false,"dynamic coefficient compiler differs from firmware");
                    }
                };
                for(unsigned t=0;t<data.controls.feedback.count;++t) for(unsigned pair=0;pair<3;++pair)
                    coefficient(data.controls.feedback.targets[t].row+pair);
                for(unsigned slot=0;slot<2;++slot) for(unsigned t=0;t<data.controls.slots[slot].group[0].count;++t) {
                    const auto& target=data.controls.slots[slot].group[0].targets[t];coefficient(target.row);
                    if(slot==1 && (target.meta&64)) coefficient(target.second_row);
                }
            }
        };
        // 48 kHz block input is set explicitly on the hardware comparators.
        // Pulse, drop, retrigger and silence exercise both trigger directions.
        constexpr unsigned frames=48000*2;
        std::array<float,64> zero{},output[4];float* channels[]={output[0].data(),output[1].data(),output[2].data(),output[3].data()};
        for(unsigned frame=0;frame<frames;frame+=64) {
            const unsigned phase=frame%24000;
            const unsigned bits=phase<4000?15:phase<8000?3:phase<12000?0:phase<16000?31:0;
            host.set_level_detectors(0,bits);host.set_level_detectors(1,bits);
            require(m.render(zero.data(),zero.data(),channels,64),"reference rendering failed");
        }
        host.pc_watches[0xb000]=host.pc_watches[0x8317]=host.pc_watches[0x85af]=host.pc_watches[0x81ee]=host.pc_watches[0x823d]=false;host.pc_observer={};
        require(ticks>20,"insufficient dynamics transitions");total+=ticks+fast_ticks;
    }
    std::cout<<info.name<<": "<<total<<" slow/fast firmware transitions exact, all four switch combinations\n"<<std::flush;
    result=m.run_task([&]{return finish_fixture(m,op);});require(!result.failed,"dynamics fixture cleanup failed");
}
template<Graph graph> static void audio_check(const ProgramData& data) {
    if(!data.dynamics.enabled) return;
    auto plain=std::make_unique<Native48<graph>>(),gated=std::make_unique<Native48<graph>>(),optimized=std::make_unique<Native48<graph>>();
    auto raw=data.controls.factory;raw[42]&=uint8_t(~1u);
    // Inverse Room's factory output taps are muted. Open the level controls
    // so the audio check observes dynamics rather than a silent preset.
    for(const auto& control:data.controls.slots) if(control.kind==ControlKind::level) raw[control.cell]=128;
    const unsigned stop=data.dynamics.shared_stop?6:12;raw[stop]=8;raw[stop+1]=32;raw[45]=0;
    auto settings=data.template settings<graph>();
    for(auto* path:{plain.get(),gated.get(),optimized.get()}) {
        require(path->prepare(settings),"dynamics audio prepare failed");path->set_dynamics(data.dynamics,data.initial_dynamics);
        path->set_global(0,1,1,0,2);
    }
    plain->set_native_controls(settings,data.controls,raw,false);optimized->set_native_controls(settings,data.controls,raw,true);
    raw[42]|=1;gated->set_native_controls(settings,data.controls,raw,false);
    double gate_difference=0,opt_difference=0;
    tracking=true;
    for(unsigned frame=0;frame<48000*2;++frame) {
        const float input=frame<24000?0.4f*std::sin(frame*0.117f):0;
        float l[3],r[3];plain->process(input,input,l[0],r[0]);gated->process(input,input,l[1],r[1]);optimized->process(input,input,l[2],r[2]);
        for(unsigned i=0;i<3;++i) require(std::isfinite(l[i]) && std::isfinite(r[i]) && std::abs(l[i])<4 && std::abs(r[i])<4,"dynamic audio nonfinite/runaway");
        if(frame>=24000) {
            gate_difference+=std::abs(l[0]-l[1])+std::abs(r[0]-r[1]);
            opt_difference+=std::abs(l[0]-l[2])+std::abs(r[0]-r[2]);
        }
    }
    tracking=false;
    if(gate_difference<=1e-7) std::cerr<<graph_info(graph).name<<" native gate difference="<<gate_difference
        <<" mid="<<unsigned(plain->dynamics_state().mid)<<'/'<<unsigned(gated->dynamics_state().mid)
        <<" flags="<<unsigned(gated->dynamics_state().flags)<<" feedback targets="<<unsigned(data.controls.feedback.count)<<'\n';
    require(gate_difference>1e-7,"dynamic switch did not change native audio");
    if(data.controls.feedback.count) require(opt_difference>1e-7,"optimization switch did not change native audio");
    std::cout<<graph_info(graph).name<<": 48k native audio switches change tails, finite, allocation-free\n"<<std::flush;
}
int main(int argc,char** argv) {
    require(argc==3 || argc==4,"usage: cineol_xl_dynamics_check ROM_DIRECTORY PREPARED_BANK [PROGRAM_INDEX_OR_RANGE]");
    std::ifstream input(argv[2],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    auto bank=std::make_unique<Bank>();require(read_bank(bytes.data(),bytes.size(),*bank),"invalid dynamics bank");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);auto machine=std::make_unique<Machine>(*engine);LarcOperator op(*machine);
    auto result=machine->run_task([&]{return xl_test::boot(*machine,op);});require(!result.failed,"boot failed");
    const unsigned first=argc==4?unsigned(std::atoi(argv[3])):0;
    const char* separator=argc==4?std::strchr(argv[3],':'):nullptr;
    const unsigned last=argc<4?unsigned(graphs.size()-1):separator?unsigned(std::atoi(separator+1)):first;
    require(first<=last && last<graphs.size(),"invalid program range");
    for(unsigned i=first;i<=last;++i) check(*engine,*machine,op,bank->programs[i],i);
    // The optional range limits the slower firmware oracle; native audio is
    // always exercised for every graph.
    each_graph([&]<Graph graph>() {audio_check<graph>(bank->programs[unsigned(graph)]);});
    require(!allocations && !releases,"dynamics allocated or released memory");
    std::cout<<"Native dynamics allocation/release=0\n";
}
