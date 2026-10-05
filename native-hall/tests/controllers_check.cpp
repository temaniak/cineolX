// Check observable scan/peak behavior, including optimized Release builds.
#undef NDEBUG
#include "../desktop/controllers224.hpp"
#include <isa-level-cpp/lexicon224x.hpp>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <vector>
static bool tracking=false;static unsigned allocations=0;
void* operator new(size_t n){if(tracking)++allocations;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,size_t) noexcept {std::free(p);}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
using namespace native_hall;
static void check_xreg(const char* prefix) {
    for(unsigned program=0;program<program_count;++program) {
        auto model=std::make_unique<lexicon224x::Machine>();model->model=lexicon224x::Model::Lexicon224;
        std::ifstream image(std::string(prefix)+"."+std::to_string(program)+".wcs",std::ios::binary);
        uint8_t bytes[512];assert(image.read(reinterpret_cast<char*>(bytes),512));
        lexicon224x::load_wcs(*model,bytes);unsigned count=0;
        for(unsigned row=0;row<100;++row) {
            lexicon224x::fetch(*model);lexicon224x::converter_clock(*model);
            if(model->mi.wr_xreg) {
                assert(row==0 || row==50);
                assert(model->mi.source==lexicon224x::FromADC);++count;
            }
            lexicon224x::execute(*model);
        }
        assert(count==2);
    }
    std::cout<<"Six independent WCS images: XREG holds input at rows 0/50\n";
}
static void check_clock_adapter(const ProgramBank& bank) {
    auto actual=std::make_unique<DesktopHall>();auto expected=std::make_unique<Hall>();
    const auto input=[](unsigned channel,uint64_t pass){return int16_t(uint16_t(pass*(channel?31:71)));};
    const auto mask=[](unsigned channel,uint64_t pass){return unsigned((pass/(channel?11:7))%32);};
    for(unsigned program=0;program<program_count;++program) {
        actual->prepare(bank,program);expected->prepare(bank,program);expected->restore_decay(actual->decay_state());
        ControlScan224 clock;clock.reset(program_networks[program]);
        std::array<uint64_t,2> last{};Controls controls;controls.predelay_ms=predelay_minima[program];
        actual->set_controls(controls);expected->set_controls(controls);
        tracking=true;
        for(uint64_t pass=0;pass<60000;++pass) {
            if(pass%2000==0) {
                controls.mode_enhancement=!controls.mode_enhancement;
                controls.decay_optimization=!controls.decay_optimization;
                actual->set_controls(controls);expected->set_controls(controls);
            }
            clock.run_until(pass*100,bank,program,*expected,controls,
                [&](uint64_t t){
                    if(t<3)return int16_t(0);
                    const unsigned channel=(t-3)%100>=50;
                    return input(channel,(t-3)/100);
                },[&](unsigned channel,uint64_t t){
                    // Independent held-register fixture: right captures at
                    // row 4; left at row 54 supplies the next pass's word.
                    const uint64_t phase=channel?6:56;
                    uint64_t capture=phase;
                    if(capture<last[channel])capture+=((last[channel]-capture+99)/100)*100;
                    unsigned held=0;
                    for(;capture<=t;capture+=100)held|=mask(channel,capture/100+(channel?0:1));
                    last[channel]=t;return held;
                },[](ControlScan224::Event,uint64_t,uint16_t){});
            int16_t out[4];actual->process(input(0,pass),input(1,pass),out,mask(0,pass)|(mask(1,pass)<<8));
            const auto a=actual->modulation_state(),b=expected->modulation_state();
            assert(a.descriptors==b.descriptors && a.coefficients==b.coefficients && a.offsets==b.offsets);
            assert(a.index==b.index && a.divider==b.divider && a.random_divider==b.random_divider && a.hold==b.hold);
            assert(!std::memcmp(&actual->decay_state(),&expected->decay_state(),sizeof(DecayState)));
        }
        tracking=false;
    }
    std::cout<<"360000 streamed passes: sparse byte reads, separate held detectors and mode changes match native event clock\n";
}
int main(int argc,char** argv) {
    if(argc==2)check_xreg(argv[1]);else assert(argc==1);
    auto bank=std::make_unique<ProgramBank>();
    for(unsigned p=0;p<program_count;++p) {
        auto& a=bank->programs[p];a.identity=program_identities[p];a.network=program_networks[p];
        a.modulation_rate_tenths.fill(10240);a.level_rate_tenths.fill(569);
        a.initial_decay.diffusion_period=15;a.initial_decay.diffusion_divider=11;
    }
    auto hall=std::make_unique<DesktopHall>();
    int16_t out[4];Controls controls;controls.mode_enhancement=controls.decay_optimization=false;
    for(unsigned program=0;program<program_count;++program) {
        hall->prepare(*bank,program);hall->set_controls(controls);
        assert(hall->decay_state().diffusion_period==0);
        assert(bank->programs[program].initial_decay.diffusion_period==15);
        // A short transfer peak between CPU reads is missed; headroom has
        // its own peak register and must remember the same short assertion.
        tracking=true;
        for(unsigned n=0;n<360;++n)hall->process(n==3?28672:0,n==3?28672:0,out);
        tracking=false;assert(hall->decay_state().held==31);
        hall->reset();hall->set_controls(controls);
        tracking=true;
        for(unsigned n=0;n<360;++n)hall->process(0,0,out,n==3?8:0);
        tracking=false;assert(hall->decay_state().held==239);
        hall->reset();hall->set_controls(controls);
        for(unsigned n=0;n<360;++n)hall->process(2048,2048,out);
        assert(hall->decay_state().held==191);
        auto state=hall->decay_state();state.stopped=1;state.amount=7;
        state.diffusion_period=15;state.diffusion_divider=4;state.history.fill(207);
        hall->restore_decay(state);
        auto modulation=hall->modulation_state();modulation.divider=11;
        modulation.random_divider=5;modulation.hold=7;modulation.index=177;
        hall->restore_modulation(modulation);
        tracking=true;hall->select_program((program+1)%program_count);tracking=false;
        assert(!std::memcmp(&state,&hall->decay_state(),sizeof state));
        const auto switched=hall->modulation_state();
        assert(switched.divider==11 && switched.random_divider==5 && switched.hold==7);
        assert(switched.index==bank->programs[(program+1)%program_count].modulation_index);
        hall->reset();assert(hall->decay_state().diffusion_period==0);
        tracking=true;
        for(unsigned n=0;n<180000;++n) {
            if(n%3000==0){controls.mode_enhancement=!controls.mode_enhancement;controls.decay_optimization=!controls.decay_optimization;hall->set_controls(controls);}
            hall->process(int16_t(n*71),int16_t(n*31),out,(n/1000)%32);
            assert(hall->decay_state().amount<=12);
        }
        tracking=false;
    }
    assert(allocations==0);
    // Old single-Hall profiles have a level clock, not a modulation-rate
    // table. Their desktop scan must be derived from that prepared clock.
    Profile profile;profile.level_rate_tenths.fill(100);hall->prepare(profile);
    controls.mode_enhancement=controls.decay_optimization=false;hall->set_controls(controls);
    for(unsigned n=0;n<1000;++n)hall->process(2048,2048,out);
    assert(hall->decay_state().held==31);
    for(unsigned n=0;n<1200;++n)hall->process(2048,2048,out);
    assert(hall->decay_state().held==191);
    if(argc==2) {
        std::ifstream file(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
        assert(read_bank(bytes.data(),bytes.size(),*bank));check_clock_adapter(*bank);
    }
    assert(allocations==0);
    std::cout<<"Startup, sparse transfer peaks, held comparators, program continuity and 1,080,000 control passes: allocations=0\n";
}
