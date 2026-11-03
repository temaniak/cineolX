#include "dual_hall.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <limits>

static bool counting=false;
static size_t allocations=0,releases=0;
void* operator new(size_t n){if(counting)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(counting && p)++releases;std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,size_t)noexcept{::operator delete(p);}
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static native_hall::Parameters parameters(const native_hall::Controls& c){native_hall::Parameters p;p.hall=c;return p;}
struct Window {
    double sum=0,squares=0;uint64_t count=0;
    void add(float v){sum+=v;squares+=double(v)*v;++count;}
    double power()const {const double mean=sum/count;return std::max(0.,squares/count-mean*mean);}
};
static double ac_db(const std::array<Window,2>& w){return 10*std::log10(std::max(1e-30,(w[0].power()+w[1].power())*.5));}
int main(int argc,char** argv)try {
    require(argc>=2,"usage: cineol_dual_hall_check BANK [tail-metrics.csv]");
    std::ifstream file(argv[1],std::ios::binary);
    const std::vector<char> bytes{std::istreambuf_iterator<char>(file),{}};
    auto bank=std::make_unique<native_hall::ProgramBank>();
    require(native_hall::read_bank(bytes.data(),bytes.size(),*bank),"invalid original bank");
    auto tanks=std::make_unique<cineol::dual_hall::Tanks>();
    auto a=std::make_unique<native_hall::DesktopEngine48>(),b=std::make_unique<native_hall::DesktopEngine48>();
    // Independent engine oracle: feeds, selected A/C outputs, simultaneous
    // reciprocal return and shared live controls; includes silent-tail frames.
    for(float feedback:{0.f,.25f,.5f})for(bool modes:{false,true}) {
        native_hall::Controls c;c.mode_enhancement=c.decay_optimization=modes;
        tanks->prepare(*bank,c,feedback);a->prepare(*bank,2);b->prepare(*bank,2);
        a->set_parameters(parameters(c));b->set_parameters(parameters(c));
        std::array<float,2> wet{};double error=0,right_energy=0;
        counting=true;
        for(unsigned n=0;n<144000;++n) {
            if(n==24000) {
                c.diffusion=45;c.treble=19;c.depth=33;c.modulation_period=3;c.modulation_step=12;c.optimization_response=8;
                tanks->set_controls(c);a->set_parameters(parameters(c));b->set_parameters(parameters(c));
            }
            const float l=n<24000?.06f*std::sin(n*.17f):0;
            const float il=l+feedback*wet[1],ir=feedback*wet[0];float discard;
            a->process(il,il,wet[0],discard,true);b->process(ir,ir,discard,wet[1],true);
            const auto actual=tanks->process(l,0);
            for(unsigned ch=0;ch<2;++ch)error=std::max(error,double(std::abs(actual[ch]-wet[ch])));
            if(n>4800 && n<48000)right_energy+=double(actual[1])*actual[1];
        }
        counting=false;
        require(error==0,"wrapper differs from independent complete Hall B engines");
        if(feedback>0)require(right_energy>1e-4,"left excitation did not reach right tank");
        std::cout<<"reference feedback="<<feedback<<" modes="<<modes<<" max_error="<<error<<" right_energy="<<right_energy<<'\n';
    }
    // Coupling automation never resets histories and has a bounded 20 ms slew.
    native_hall::Controls c;tanks->prepare(*bank,c,0);
    double tail_energy=0;float previous=0;
    counting=true;
    for(unsigned n=0;n<144000;++n) {
        if(n%12000==0)tanks->set_crossfeedback((n/12000)&1?.5f:0.f);
        const auto wet=tanks->process(n<12000?.05f*std::sin(n*.11f):0,0);
        const float feedback=tanks->crossfeedback();
        require(std::abs(feedback-previous)<=cineol::dual_hall::Tanks::feedback_step+1e-7f,"feedback slew exceeded");previous=feedback;
        for(float v:wet){require(std::isfinite(v) && std::abs(v)<4,"nonfinite or excessive output");if(n>48000)tail_energy+=double(v)*v;}
    }
    tanks->set_crossfeedback(std::numeric_limits<float>::infinity());
    for(unsigned n=0;n<2000;++n)tanks->process(std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity());
    counting=false;
    require(tail_energy>1e-5,"automation cleared the ringing tail");
    require(tanks->crossfeedback()==0,"invalid feedback was not sanitized");
    require(allocations==0 && releases==0,"core processing or setters allocate/release heap memory");
    // Re-prepare clears both external return and engine histories.
    tanks->prepare(*bank,c,.25f);a->prepare(*bank,2);b->prepare(*bank,2);
    a->set_parameters(parameters(c));b->set_parameters(parameters(c));std::array<float,2> wet{};
    for(unsigned n=0;n<4800;++n) {
        const float il=.25f*wet[1],ir=.25f*wet[0];float discard;
        a->process(il,il,wet[0],discard,true);b->process(ir,ir,discard,wet[1],true);
        require(tanks->process(0,0)==wet,"prepare retained old external feedback");
    }
    if(argc>=3) {
        std::ofstream csv(argv[2]);require(bool(csv),"cannot write metrics");
        csv<<"recipe,modes,feedback,seconds,peak,early_ac_db,late_ac_db,saturations\n";
        for(unsigned recipe=0;recipe<3;++recipe)for(bool modes:{false,true})for(float feedback:{0.f,.25f,.5f}) {
            native_hall::Controls ctrl;ctrl.mode_enhancement=ctrl.decay_optimization=modes;
            if(recipe==1){ctrl.bass=5;ctrl.mid=5;ctrl.diffusion=63;}
            if(recipe==2){ctrl.bass=31;ctrl.mid=31;ctrl.treble=31;ctrl.diffusion=63;ctrl.predelay_ms=152;}
            const unsigned seconds=recipe==2?90:30,total=48000*seconds;
            tanks->prepare(*bank,ctrl,feedback);std::array<Window,2> early,late;double peak=0;uint32_t rng=17;
            counting=true;
            for(unsigned n=0;n<total;++n) {
                rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;
                const float input=n<24000?(int32_t(rng)/2147483648.f)*.08f:0;
                const auto out=tanks->process(input,0);
                for(unsigned ch=0;ch<2;++ch) {
                    const float v=out[ch];
                    require(std::isfinite(v) && std::abs(v)<4,"tail output nonfinite or excessive");peak=std::max(peak,double(std::abs(v)));
                    if(n>=24000 && n<72000)early[ch].add(v);
                    if(n>=total-2*48000)late[ch].add(v);
                }
            }
            counting=false;
            csv<<recipe<<','<<modes<<','<<feedback<<','<<seconds<<','<<peak<<','<<ac_db(early)<<','<<ac_db(late)<<','<<tanks->saturations()<<'\n';csv.flush();
            // Original integer recurrence can settle at a quantized residual
            // floor. A quiet floor is not a growing reverberation tail.
            if(recipe<2)require(ac_db(late)<-80 || ac_db(late)<ac_db(early)-20,"ordinary stopped-input tail did not decay");
        }
    }
    require(allocations==0 && releases==0,"tail processing allocates/releases");
    std::cout<<"shared controls, causal reciprocal routing, coupling slew, preserved tails, reset and finite guards pass; allocations="<<allocations<<" releases="<<releases<<'\n';
}catch(const std::exception& e){counting=false;std::cerr<<e.what()<<'\n';return 1;}
