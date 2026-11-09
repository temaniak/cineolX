// Offline regression for #13: old Plate automation must not read other lines.
#include "../desktop/engine22448.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
using namespace native_hall;
static void require(bool ok,const char* message) {
    if(!ok) {std::cerr<<message<<'\n';std::exit(1);}
}
int main(int argc,char** argv) {
    require(argc==2,"usage: native_224_predelay_check PROGRAMS.bank224");
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<ProgramBank>();
    require(read_bank(bytes.data(),bytes.size(),*bank),"invalid prepared bank");
    auto hall=std::make_unique<Hall>();
    for(unsigned program=0;program<program_count;++program) {
        hall->prepare(*bank,program);
        for(int offset=0;offset<=128;++offset) {
            Controls c;c.predelay_ms=predelay_minima[program]+offset;hall->set_controls(c);
            const unsigned expected=unsigned((program==1 || program==4)?std::min(offset,107):offset);
            const auto& data=bank->programs[program];
            for(unsigned i=0;i<data.predelay_count;++i)
                require(hall->offsets()[data.predelay_rows[i]]==data.predelay[expected][i],
                    "pre-delay address differs from the reachable ROM table entry");
        }
    }
    // Two independent histories: a legacy 0..128 ms automation ramp and
    // the same trajectory capped at the physical 107 ms limit. They must
    // agree on all four outputs through excitation, silence and retrigger.
    for(unsigned program:{1u,4u}) for(unsigned mode=0;mode<4;++mode) {
        auto legacy=std::make_unique<DesktopEngine48>(),capped=std::make_unique<DesktopEngine48>();
        legacy->prepare(*bank,program);capped->prepare(*bank,program);
        Parameters a;a.program=program;a.hall.mode_enhancement=mode&1;a.hall.decay_optimization=mode&2;
        Parameters b=a;uint32_t random=17;double signal=0,late=0;float peak=0;
        for(unsigned n=0;n<8*48000;++n) {
            if(n%128==0) {
                a.hall.predelay_ms=std::clamp((int(n)-48000)/1200,0,128);
                b.hall.predelay_ms=std::min(a.hall.predelay_ms,107);
                legacy->set_parameters(a);capped->set_parameters(b);
            }
            random=random*1664525u+1013904223u;
            const float input=(n<4*48000 || (n>=6*48000 && n<6*48000+4800))?
                .1f*float(int32_t(random))/2147483648.f:0;
            float l,r;std::array<float,4> actual{},expected{};
            legacy->process(input,input*.7f,l,r,true,&actual);
            capped->process(input,input*.7f,l,r,true,&expected);
            require(actual==expected,"legacy Plate automation differs from the capped trajectory");
            for(float value:actual) {
                require(std::isfinite(value),"nonfinite Plate output");peak=std::max(peak,std::abs(value));
                if(n>=3*48000 && n<4*48000)signal+=double(value)*value;
                if(n>=5*48000 && n<6*48000)late+=double(value)*value;
            }
        }
        require(peak<.3f && signal>1 && late<signal*.01,"silent or overexcited Plate / tail failed to decay");
        std::cout<<program_names[program]<<" mode "<<mode<<": 48 kHz / 128-frame updates, "
            "legacy ramp = 107 ms cap on four outputs, decay/retrigger finite, peak="<<peak<<'\n';
    }
}
