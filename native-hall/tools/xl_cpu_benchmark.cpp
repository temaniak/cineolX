// Offline prepared-bank benchmark. No ROM execution or plugin installation.
#include "../desktop/runtime.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <new>
#include <cstdlib>
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#elif defined(_MSC_VER) && defined(_M_ARM64)
#include <float.h>
#endif

static bool preparation=false,processing=false;
static size_t prepared_bytes=0;static unsigned allocations=0,releases=0;
void* operator new(size_t n) {
    if(preparation) prepared_bytes+=n;if(processing) ++allocations;
    if(void* p=std::malloc(n?n:1)) return p;throw std::bad_alloc();
}
void operator delete(void* p) noexcept {if(processing && p) ++releases;std::free(p);}
void operator delete(void* p,size_t) noexcept {::operator delete(p);}
void* operator new[](size_t n) {return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
using namespace cineol::xl;
static void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
static std::array<uint8_t,48> controls(const ProgramData& data,bool modes) {
    auto raw=data.controls.factory;raw[42]&=uint8_t(~0xc1u);
    if(modes) raw[42]|=uint8_t(data.dynamics.enabled?0xc1:0x40);
    for(const auto& control:data.controls.slots) if(control.kind==ControlKind::level) raw[control.cell]=128;
    data.resolve_controls(raw);return raw;
}
static void apply(Runtime& engine,const ProgramData& data,const std::array<uint8_t,48>& raw,bool modes) {
    engine.controls(raw,modes,0,1,1,0,2,modes && data.dynamics.enabled);
}
int main(int argc,char** argv) try {
    check(argc==3 || argc==4,"usage: cineol_xl_cpu_benchmark XL_BANK OUTPUT_CSV [REPEATS=5]");
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
    _mm_setcsr(_mm_getcsr()|0x8040);
#elif defined(__aarch64__)
    uint64_t status=0;asm volatile("mrs %0, fpcr" : "=r"(status));status|=uint64_t(1)<<24;
    asm volatile("msr fpcr, %0" : : "r"(status));
#elif defined(_MSC_VER) && defined(_M_ARM64)
    _control87(_DN_FLUSH,_MCW_DN);
#endif
    std::ifstream file(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<Bank>();check(read_bank(bytes.data(),bytes.size(),*bank),"Invalid XL bank");
    preparation=true;auto engine=std::make_unique<Runtime>(),tail=std::make_unique<Runtime>();preparation=false;
    const size_t runtime_bytes=prepared_bytes;
    constexpr unsigned frames=96000,block=128,workloads=6;
    const unsigned repeats=argc==4?unsigned(std::atoi(argv[3])):5;
    check(repeats>=1 && repeats<=20,"Invalid benchmark repeat count");
    std::vector<std::array<float,2>> input(frames);uint32_t random=17;
    for(unsigned n=0;n<48000;++n) {
        random=random*1664525u+1013904223u;const float noise=float(int32_t(random))/2147483648.f;
        input[n]={.05f*noise+.03f*std::sin(n*.07f),.03f*noise+.02f*std::sin(n*.13f)};
    }
    std::ofstream csv(argv[2]);check(bool(csv),"Cannot write CPU CSV");
    csv<<std::setprecision(12)<<"program,workload,repeat,seconds,audio_seconds,checksum,peak,rate,block,two_runtime_storage_bytes,bank_bytes,allocations,releases\n";
    // Alternate workload order between repeats. Select/warmup and CSV writes
    // are untimed. Overlap omits the plugin's retirement fades/host converters.
    for(unsigned program=0;program<graphs.size();++program) for(unsigned repeat=0;repeat<repeats;++repeat)
        for(unsigned ordinal=0;ordinal<workloads;++ordinal) {
            const unsigned workload=repeat&1?workloads-1-ordinal:ordinal;
            const bool modes=workload!=0,overlap=workload==2 || workload==3;
            const unsigned previous=workload==3?unsigned(Graph::resonant_chords):program;
            const auto& data=bank->programs[program];auto raw=controls(data,modes);
            engine->select(*bank,program);apply(*engine,data,raw,modes);
            const auto& old=bank->programs[previous];const auto old_raw=controls(old,true);
            if(overlap) {tail->select(*bank,previous);apply(*tail,old,old_raw,true);}
            processing=true;
            for(unsigned n=0;n<48000;++n) {
                float l,r;engine->process(0,0,l,r,true);
                if(overlap) tail->process(input[n][0],input[n][1],l,r,true);
            }
            processing=false;
            double checksum=0;float peak=0;
            const auto begin=std::chrono::steady_clock::now();processing=true;
            for(unsigned n=0;n<frames;++n) {
                if(workload==5 && n%block==0) {
                    const bool enhanced=((n/block)&1)!=0;
                    auto changed=raw;changed[42]=uint8_t((changed[42]&~64u)|(enhanced?64:0));
                    if(data.controls.size.enabled) {
                        changed[43]=enhanced?254:2;changed[44]=enhanced?2:254;
                    }
                    data.resolve_controls(changed);
                    engine->controls(changed,enhanced,0,1,1,0,2,data.dynamics.enabled);
                }
                if(workload==4 && n%block==0) {
                    auto changed=raw;changed[0]=uint8_t(std::min(255u,unsigned(raw[0])+((n/block)&1)*8));
                    data.resolve_controls(changed);apply(*engine,data,changed,modes);
                }
                float l,r;engine->process(input[n][0],input[n][1],l,r,true);
                if(overlap) {float tl,tr;tail->process(0,0,tl,tr,true);l+=tl;r+=tr;}
                checksum+=l+r;peak=std::max({peak,std::abs(l),std::abs(r)});
            }
            processing=false;const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
            check(std::isfinite(checksum) && std::isfinite(peak) && peak<8,"Nonfinite/runaway benchmark output");
            check(allocations==0 && releases==0,"Allocation/release during warmup/process/control edge");
            csv<<program<<','<<workload<<','<<repeat<<','<<seconds<<",2,"<<checksum<<','<<peak<<",48000,128,"<<runtime_bytes<<','
               <<sizeof(Bank)<<','<<allocations<<','<<releases<<'\n';
        }
    check(bool(csv),"Cannot finish CPU CSV");
    std::cout<<"22 XL programs, "<<workloads<<" workloads, "<<repeats
        <<" repeats; preparation untimed; desktop denormal policy; allocation/release=0\n";
} catch(const std::exception& error) {preparation=processing=false;std::cerr<<error.what()<<'\n';return 1;}
