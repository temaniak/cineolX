#include "../desktop/engine22448.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#elif defined(_MSC_VER) && defined(_M_ARM64)
#include <float.h>
#endif
int main(int argc,char** argv) try {
    using namespace native_hall;
    if(argc!=3)throw std::runtime_error("usage: native_224_cpu_benchmark BANK OUTPUT_CSV");
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
    _mm_setcsr(_mm_getcsr()|0x8040); // Match the desktop ScopedNoDenormals policy.
#elif defined(__aarch64__)
    uint64_t status=0;
    asm volatile("mrs %0, fpcr" : "=r"(status));
    status|=uint64_t(1)<<24; // ARM FZ, as in JUCE ScopedNoDenormals.
    asm volatile("msr fpcr, %0" : : "r"(status));
#elif defined(_MSC_VER) && defined(_M_ARM64)
    _control87(_DN_FLUSH,_MCW_DN);
#endif
    std::ifstream file(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<ProgramBank>();if(!read_bank(bytes.data(),bytes.size(),*bank))throw std::runtime_error("Invalid bank");
    auto engine=std::make_unique<DesktopEngine48>();
    std::vector<std::array<float,2>> input(96000);uint32_t random=17;
    for(unsigned n=0;n<48000;++n) {
        random=random*1664525u+1013904223u;float noise=float(int32_t(random))/2147483648.f;
        input[n]={.05f*noise+.03f*std::sin(n*.07f),.03f*noise+.02f*std::sin(n*.13f)};
    }
    std::ofstream csv(argv[2]);if(!csv)throw std::runtime_error("Cannot write CSV");
    csv<<std::setprecision(10)<<"program,mode,repeat,seconds,checksum,storage_bytes\n";
    for(unsigned program=0;program<6;++program)for(unsigned mode=0;mode<3;++mode) {
        for(unsigned repeat=0;repeat<3;++repeat) {
            engine->prepare(*bank,program);Parameters p;p.program=program;p.hall.predelay_ms=predelay_minima[program];
            p.hall.mode_enhancement=p.hall.decay_optimization=mode!=0;p.analog=mode!=2;engine->set_parameters(p);
            for(unsigned n=0;n<2000;++n){float l,r;engine->process(0,0,l,r);}
            double checksum=0;const auto begin=std::chrono::steady_clock::now();
            for(const auto& x:input){float l,r;engine->process(x[0],x[1],l,r);checksum+=l+r;}
            const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
            if(!std::isfinite(checksum))throw std::runtime_error("Nonfinite output");
            csv<<program<<','<<mode<<','<<repeat<<','<<seconds<<','<<checksum<<','<<sizeof(*engine)<<'\n';
        }
    }
    std::cout<<"CPU measurements saved; preparation excluded; desktop denormal policy enabled on x86/ARM64\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
