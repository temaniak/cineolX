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
    std::cout<<"Startup, sparse transfer peaks, held comparators, program continuity and 1,080,000 control passes: allocations=0\n";
}
