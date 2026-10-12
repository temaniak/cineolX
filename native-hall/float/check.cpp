#include "hall.hpp"
#include "../desktop/engine22448.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <cstdlib>
#include <new>

static bool count_allocations=false;
static size_t allocations=0;
void* operator new(size_t n) {if(count_allocations)++allocations;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,size_t) noexcept {std::free(p);}
void operator delete[](void* p,size_t) noexcept {std::free(p);}
static void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}

// Independent ideal ARU oracle: original shared 16K RAM and all three
// multiply edges, but real-valued arithmetic (no quantization/saturation).
// Production uses separate fractional delay lines and collapsed sums.
struct IdealRows {
    std::array<float,16384> memory{};
    std::array<float,4> registers{},output{};
    std::array<int8_t,100> c{};
    std::array<uint16_t,100> offsets{};
    unsigned position=1;
    float operand=0,partial=0,acc=0,rr=0;
    unsigned previous=0;bool negative=false;
    void edge(unsigned pair,bool neg,bool load,float value,bool zero) {
        float sum=acc+partial;
        float product=((pair&2)?operand:0)+((pair&1)?operand*.5f:0);
        partial=neg?-product:product;operand=load?value:operand*.25f;acc=zero?0:sum;
    }
    template<unsigned Row,unsigned Op,unsigned RA,unsigned WA,bool Transfer,bool Zero,unsigned Outputs=0>
    void node(float input=0) {
        edge((previous>>2)&3,negative,false,0,false);
        float bus=0;
        if constexpr(Op==1)bus=memory[(position-offsets[Row])&0x3fff];
        if constexpr(Op==2 || Op==4)bus=rr;
        if constexpr(Op==3)bus=input;
        if constexpr(Op==2)memory[(position-offsets[Row])&0x3fff]=bus;
        for(unsigned i=0;i<4;++i)if(Outputs&(1u<<i))output[i]=bus;
        registers[WA]=bus;edge(previous&3,negative,true,registers[RA]*8,false);
        if constexpr(Transfer)rr=(acc+partial)*.125f;
        const int coef=c[Row];const unsigned magnitude=unsigned(coef<0?-coef:coef);
        edge((magnitude>>4)&3,coef<0,false,0,Zero);previous=magnitude;negative=coef<0;
    }
    std::array<float,4> process(float left,float right) {
        const unsigned network=0;
        #include "../core/program_networks.inc"
        ++position;return output;
    }
};

int main(int argc,char** argv) try {
    require(argc==2,"usage: cineol_float_hall_check BANK224");
    std::ifstream file(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<native_hall::ProgramBank>();
    require(native_hall::read_bank(bytes.data(),bytes.size(),*bank),"invalid bank");
    native_hall::Controls controls;controls.mode_enhancement=controls.decay_optimization=false;
    // Native-rate equivalence checks the graph and shared-memory dependency
    // reconstruction independently of sample-rate conversion and listening.
    double maximum_error=0;
    for(unsigned setting=0;setting<3;++setting) {
        controls.predelay_ms=24+int(setting)*64;controls.diffusion=1+int(setting)*31;
        native_hall::Hall compiler;compiler.prepare(*bank,2);compiler.set_controls(controls);
        auto oracle=std::make_unique<IdealRows>();oracle->c=compiler.coefficients();oracle->offsets=compiler.offsets();
        auto core=std::make_unique<cineol::floating::Hall<20480>>();require(core->prepare(*bank),"core prepare");core->set_controls(controls);
        uint32_t random=17;
        for(unsigned n=0;n<80000;++n) {
            random=random*1664525u+1013904223u;
            const float l=n<10000?float(int32_t(random))/2147483648.f*.1f:0;
            const float r=n<10000?.07f*std::sin(n*.17f):0;
            const auto expected=oracle->process(l,r),actual=core->process(l,r);
            for(unsigned c=0;c<4;++c)maximum_error=std::max(maximum_error,double(std::abs(expected[c]-actual[c])));
        }
    }
    std::cout<<"native-rate ideal graph maximum error="<<maximum_error<<'\n';
    require(maximum_error<2e-6,"collapsed graph differs from independent real-valued ARU");

    auto core=std::make_unique<cineol::floating::Hall48>();require(core->prepare(*bank),"48 kHz prepare");
    controls=native_hall::Controls{};controls.mode_enhancement=controls.decay_optimization=false;core->set_controls(controls);
    for(unsigned i=0;i<1000;++i)for(float y:core->process(0,0))require(y==0,"float silence must be exact");
    core->reset();double early=0,late=0;float peak=0;
    count_allocations=true;
    for(unsigned n=0;n<480000;++n) {
        const auto y=core->process(n==0?.5f:0,0);
        for(float v:y) {
            require(std::isfinite(v),"nonfinite impulse response");peak=std::max(peak,std::abs(v));
            if(n<96000)early+=v*v;if(n>384000)late+=v*v;
        }
    }
    count_allocations=false;
    require(peak>1e-4f && peak<2,"missing or runaway impulse response");
    require(late<early*.1,"tail does not decay");require(allocations==0,"processing allocated memory");
    std::cout<<"48 kHz impulse peak="<<peak<<" late/early energy="<<late/early<<" storage="<<core->storage_bytes()<<" allocations="<<allocations<<'\n';
    core->reset();require(core->process(0,0)==std::array<float,4>{},"reset retains tail");
    for(float v:core->process(NAN,INFINITY))require(std::isfinite(v),"nonfinite input guard");
    // Long tail plus all exposed control extremes, live predelay and both modes.
    uint32_t random=71;
    count_allocations=true;
    for(unsigned n=0;n<480000;++n) {
        if(n%24000==0) {
            const unsigned step=n/24000;
            controls.bass=1+int(step*7%31);controls.mid=int(step*11%32);
            controls.crossover=int(step*13%32);controls.treble=int(step*17%32);
            controls.depth=int(step*23%72);controls.diffusion=1+int(step*29%63);
            controls.predelay_ms=24+int(step*31%129);
            controls.mode_enhancement=step&1;controls.decay_optimization=step&2;core->set_controls(controls);
        }
        random=random*1664525u+1013904223u;
        const float x=n<192000?.05f*float(int32_t(random))/2147483648.f:0;
        for(float v:core->process(x,-x*.7f))require(std::isfinite(v) && std::abs(v)<4,"live controls unstable");
    }
    count_allocations=false;require(allocations==0,"live controls allocated");
    std::cout<<"live controls, modulation, optimization, silence, reset and finite guards passed\n";
    // Quiet tails without integer rounding can expose marginal feedback.
    // Hold maximum decay/brightness for 90 s at both diffusion endpoints.
    for(int diffusion:{1,63}) {
        controls=native_hall::Controls{};
        controls.bass=controls.mid=controls.crossover=controls.treble=31;
        controls.depth=71;controls.diffusion=diffusion;
        controls.mode_enhancement=true;controls.decay_optimization=false;
        core->set_controls(controls);core->reset();early=late=0;peak=0;
        count_allocations=true;
        for(unsigned n=0;n<90*48000;++n) {
            const auto y=core->process(n<48000?.05f*std::sin(n*.05f):0,n<48000?.02f*std::sin(n*.17f):0);
            for(float v:y) {
                require(std::isfinite(v),"long-tail nonfinite");peak=std::max(peak,std::abs(v));
                if(n>48000 && n<3*48000)early+=v*v;if(n>87*48000)late+=v*v;
            }
        }
        count_allocations=false;
        // The original A/C maximum-decay fixture itself retains about 14%
        // energy here. Require bounded decay, not an artificially short tail.
        require(peak<2 && early>0 && late<early,"maximum-decay feedback fails long-tail check");
        require(allocations==0,"long-tail processing allocated");
        std::cout<<"90 s maximum-decay diffusion="<<diffusion<<" peak="<<peak<<" late/early="<<late/early<<'\n';
    }
    // Same-host assembled CPU check, single instances; not a hardware margin test.
    auto original=std::make_unique<native_hall::DesktopEngine48>();original->prepare(*bank,2);
    native_hall::Parameters params;params.hall.mode_enhancement=params.hall.decay_optimization=false;original->set_parameters(params);
    controls=params.hall;core->reset();core->set_controls(controls);
    double checksum=0;
    const auto time=[&](auto render) {
        const auto begin=std::chrono::steady_clock::now();
        for(unsigned n=0;n<96000;++n)checksum+=render(n);
        return std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    };
    const double a=time([&](unsigned n){float l,r;original->process(.05f*std::sin(n*.1f),0,l,r,true);return double(l+r);});
    const double b=time([&](unsigned n){auto y=core->process(.05f*std::sin(n*.1f),0);return double(y[0]+y[2]);});
    std::cout<<"2 seconds audio: original="<<a<<" s float="<<b<<" s checksum="<<checksum<<'\n';
} catch(const std::exception& error) {count_allocations=false;std::cerr<<error.what()<<'\n';return 1;}
