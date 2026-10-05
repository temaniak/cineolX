#include "../desktop/engine22448.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
using namespace native_hall;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void settle(DesktopEngine48& engine,unsigned frames=40000) {
    for(unsigned n=0;n<frames;++n){float l,r;engine.process(0,0,l,r);}
}
int main(int argc,char** argv) try {
    check(argc==2,"usage: native_224_gain_timing_check BANK");
    std::ifstream file(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<ProgramBank>();check(read_bank(bytes.data(),bytes.size(),*bank),"Invalid bank");
    auto dry=std::make_unique<DesktopEngine48>(),wet=std::make_unique<DesktopEngine48>(),mix=std::make_unique<DesktopEngine48>(),scaled=std::make_unique<DesktopEngine48>();
    double maximum_gain_error=0;float maximum_mix_error=0;
    for(unsigned program=0;program<6;++program) {
        Parameters p;p.program=program;p.hall.predelay_ms=predelay_minima[program];
        p.hall.mode_enhancement=p.hall.decay_optimization=false;
        for(auto* engine:{dry.get(),wet.get(),mix.get()}){engine->prepare(*bank,program);engine->set_parameters(p);}
        p.mix=0;p.input_db=12;dry->set_parameters(p);p.mix=.5f;p.input_db=0;mix->set_parameters(p);
        for(auto* engine:{dry.get(),wet.get(),mix.get()})settle(*engine);
        std::array<std::array<float,2>,70> history{};uint32_t random=17;
        for(unsigned n=0;n<48000;++n) {
            random=random*1664525u+1013904223u;float left=.04f*float(int32_t(random))/2147483648.f,right=.025f*std::sin(n*.13f);
            float dl,dr,wl,wr,ml,mr;dry->process(left,right,dl,dr);wet->process(left,right,wl,wr);mix->process(left,right,ml,mr);
            check(dl==history[n%70][0] && dr==history[n%70][1],"Dry delay/gain changed");history[n%70]={left,right};
            maximum_mix_error=std::max({maximum_mix_error,std::abs(ml-(dl+wl)*.5f),std::abs(mr-(dr+wr)*.5f)});
        }
        for(float db:{-36.f,-12.f,0.f,12.f}) {
            wet->prepare(*bank,program);scaled->prepare(*bank,program);p.mix=1;p.input_db=db;wet->set_parameters(p);p.input_db=0;scaled->set_parameters(p);
            settle(*wet);settle(*scaled);const float gain=std::pow(10.f,db/20.f);double actual_energy=0,expected_energy=0;
            for(unsigned n=0;n<96000;++n) {
                const float input=n<48000?.03f*(std::sin(n*.07f)+.5f*std::sin(n*.13f)):0;
                float al,ar,el,er;wet->process(input,-input,al,ar);scaled->process(input*gain,-input*gain,el,er);
                check(std::isfinite(al) && std::isfinite(ar),"Nonfinite gain render");
                actual_energy+=double(al)*al+double(ar)*ar;expected_energy+=double(el)*el+double(er)*er;
            }
            check(expected_energy>0,"Silent gain fixture");
            maximum_gain_error=std::max(maximum_gain_error,std::abs(10*std::log10(actual_energy/expected_energy)));
        }
    }
    check(maximum_mix_error<2e-6f,"Mix differs from linear dry/wet interpolation");
    check(maximum_gain_error<.01,"Input Gain differs materially from scaling before AIN");
    std::cout<<"Six programs: dry exact at 70 frames with +12dB Input Gain; settled half mix peak error="<<maximum_mix_error
             <<"; 24 pre-AIN gain cases max energy error="<<maximum_gain_error<<" dB\n";
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
