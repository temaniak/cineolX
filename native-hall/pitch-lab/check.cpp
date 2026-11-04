#include "core.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <iterator>
#include <stdexcept>
#include <cstdlib>
#include <new>
#include <limits>

using namespace cineol::pitch_lab;
static thread_local bool counting=false;
static size_t allocations=0,releases=0;
void* operator new(size_t n){if(counting)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(counting&&p)++releases;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,size_t)noexcept{::operator delete(p);}
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv)try {
    std::unique_ptr<native_hall::ProgramBank> bank;
    if(argc>1) {
        std::ifstream file(argv[1],std::ios::binary);std::vector<char> data{std::istreambuf_iterator<char>(file),{}};
        bank=std::make_unique<native_hall::ProgramBank>();require(native_hall::read_bank(data.data(),data.size(),*bank),"invalid bank");
    }
    auto core=std::make_unique<Core>();core->prepare(bank.get());
    std::cout<<"core latency="<<core->latency()<<" frames, selected pitch latency="<<core->pitch_latency()<<" frames\n";
    // Every preallocated reference configuration must process its first signal
    // without allocating. Include formants, transients, silence and odd timing.
    for(unsigned w=0;w<3;++w)for(unsigned o=0;o<3;++o) {
        auto v=preset(0);v[window]=float(w);v[overlap]=float(o);v[output_db]=0;
        v[formant_preserve]=(w==2);v[formant_shift]=(o==2)?-3.f:0;
        counting=true;core->set_values(v);core->clear();double power=0;float peak=0;
        for(unsigned n=0;n<72000;++n) {
            const float x=n<24000?.08f*std::sin(n*.057595865f):0;
            const auto y=core->process(x,n%3000==0?.03f:x*.7f);
            for(float a:y){require(std::isfinite(a),"nonfinite pitch output");power+=double(a)*a;peak=std::max(peak,std::abs(a));}
        }
        counting=false;
        require(power>1e-6 && peak<4,"silent or excessive pitch output");
        std::cout<<"reference window="<<(2048u<<w)<<" overlap="<<(4u<<o)<<" energy="<<power<<" peak="<<peak<<'\n';
    }
    // Monitor selection cannot modify the loop or Hall histories.
    auto other=std::make_unique<Core>();other->prepare(bank.get());
    auto v=preset(bank?4:5);v[feedback]=.25f;v[semitones]=7;v[monitor]=1;core->set_values(v);core->clear();
    v[monitor]=6;other->set_values(v);other->clear();
    counting=true;
    for(unsigned n=0;n<144000;++n) {
        const float x=n<24000?.06f*std::sin(n*.09f):0;
        core->process(x,x*.4f);other->process(x,x*.4f);
        require(core->meters()[hall_wet_node].power==other->meters()[hall_wet_node].power,"monitor changed Hall history");
        require(core->meters()[loop_node].power==other->meters()[loop_node].power,"monitor changed feedback history");
    }
    counting=false;
    // All six routing recipes, input-silent intervals and nonfinite guards.
    for(unsigned recipe=0;recipe<6;++recipe) {
        v=preset(recipe);v[feedback]=.3f;
        counting=true;core->set_values(v);core->clear();
        for(unsigned n=0;n<144000;++n) {
            const float x=n<12000?.04f*std::sin(n*.13f):0;
            const auto out=core->process(x,x*.3f);
            for(float a:out)require(std::isfinite(a)&&std::abs(a)<4,"routing output invalid");
        }
        core->process(std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity());
        counting=false;
    }
    require(allocations==0&&releases==0,"core processing/setters/reset allocate or release heap memory");
    std::cout<<"nine reference configurations, six routes, monitor invariance, reset, tails and finite guards passed; allocations="<<allocations<<" releases="<<releases<<'\n';
}catch(const std::exception& e){counting=false;std::cerr<<e.what()<<'\n';return 1;}
