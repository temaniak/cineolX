// Exercise queue/phase assertions in optimized offline checks too.
#undef NDEBUG
#include "../desktop/dac22448.hpp"
#include <analog/filters_224.hpp>
#include <isa-level-cpp/lexicon224x.hpp>
#include <emulator/timing.hpp>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <new>

static bool tracking=false;static unsigned allocations=0;
void* operator new(size_t n){if(tracking)++allocations;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,size_t) noexcept {std::free(p);}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
static void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}

static void check_capture_rows(const char* prefix) {
    constexpr unsigned networks[]={0,1,0,2,1,3};
    for(unsigned program=0;program<6;++program) {
        auto machine=std::make_unique<lexicon224x::Machine>();machine->model=lexicon224x::Model::Lexicon224;
        std::ifstream file(std::string(prefix)+"."+std::to_string(program)+".wcs",std::ios::binary);
        uint8_t image[512];require(bool(file.read(reinterpret_cast<char*>(image),512)),"missing independent WCS fixture");
        lexicon224x::load_wcs(*machine,image);unsigned found=0;
        for(unsigned row=0;row<300;++row) {
            lexicon224x::fetch(*machine);lexicon224x::converter_clock(*machine);
            for(unsigned c=0;c<4;++c)if(machine->dac_channels>>c&1) {
                if(row<100)continue; // FPC cold-start pass precedes the periodic schedule.
                unsigned expected=native_hall::Dac48::capture_rows[networks[program]][c];
                if(row<expected || (row-expected)%100!=0)
                    std::cerr<<"Program "<<program<<" channel "<<c<<" capture row "<<row<<" expected "<<expected<<" + 100*k\n";
                require(row>=expected && (row-expected)%100==0,"native DAC capture phase differs from FPC/WCS");found|=1u<<c;
            }
            lexicon224x::execute(*machine);
        }
        require(found==15,"missing DAC channel capture");
    }
    std::cout<<"Six WCS networks: DAC capture phases match independent FPC, including queued Chamber requests\n";
}

int main(int argc,char** argv) {
    require(argc<=2,"usage: dac_check [PROGRAMS.bank224 fixture prefix]");
    if(argc==2)check_capture_rows(argv[1]);
    require(native_hall::Dac48::row_ticks==lexicon224x::cpu::timing_224.row &&
            native_hall::Dac48::event_origin_ticks==lexicon224x::cpu::timing_224.first_marker+lexicon224x::cpu::timing_224.dac_observed,
            "DAC event origin differs from original-224 row/propagation timing");
    constexpr double pi=3.14159265358979323846;
    auto actual=std::make_unique<native_hall::Dac48>();
    for(unsigned network=0;network<4;++network) {
        double maximum=0;
        for(int frequency:{0,100,1000,4000,6000,8000,9000,9600,10000}) {
            auto modal=lexicon224x::analog::aout_224_modal();
            using Output=lexicon224x::analog::Output;
            Output reference[4]={Output(modal),Output(modal),Output(modal),Output(modal)};
            actual->prepare();actual->select_network(network);unsigned pass=0;uint32_t random=17;
            double error_power=0,power=0,peak_error=0;
            for(unsigned frame=0;frame<2*48000;++frame) {
                const uint64_t now=uint64_t(frame)*native_hall::Dac48::frame_ticks;
                // The network emits in the host frame containing the beginning
                // of its pass, before any later channel captures in that pass.
                if(uint64_t(pass)*native_hall::Dac48::pass_ticks<now+native_hall::Dac48::frame_ticks) {
                    std::array<float,4> values{};
                    for(unsigned c=0;c<4;++c) {
                        random=random*1664525u+1013904223u;
                        values[c]=frequency?float(.25*std::sin(2*pi*frequency*pass/20480.0+c*.7))
                            :(pass<10240?float(int32_t(random))/2147483648.f:0.f);
                        auto time=uint64_t(pass)*native_hall::Dac48::pass_ticks
                            +native_hall::Dac48::capture_rows[network][c]*native_hall::Dac48::row_ticks+native_hall::Dac48::event_origin_ticks;
                        reference[c].hold(time,values[c]);
                    }
                    actual->push(values.data());++pass;
                }
                auto output=actual->sample();
                for(unsigned c=0;c<4;++c) {
                    double expected=reference[c].sample(now),error=output[c]-expected;
                    peak_error=std::max(peak_error,std::abs(error));error_power+=error*error;power+=expected*expected;
                    require(std::isfinite(output[c]),"nonfinite DAC output");
                }
            }
            require(pass==40960,"native DAC clock drift");
            require(peak_error<3e-5,"event DAC differs from continuous-time reference");
            require(error_power<1e-8*power,"event DAC relative RMS error exceeds -80 dB");
            maximum=std::max(maximum,peak_error);
        }
        std::cout<<"Network "<<network<<": noise/silence + 8 tones, all channels, CT peak error="<<maximum<<'\n';
    }
    actual->prepare();uint32_t phase=0;std::array<float,4> input{.1f,-.1f,.05f,-.05f};
    tracking=true;
    // Bounded clocks, reset, and topology changes at every host/native phase.
    for(unsigned frame=0;frame<1000000;++frame) {
        if(frame%137==0)actual->select_network((frame/137)%4);
        if(frame%170003==0){actual->reset();phase=0;}
        if(phase<32){actual->push(input.data());phase+=75;}
        phase-=32;
        auto output=actual->sample();
        for(float v:output)require(std::isfinite(v) && std::abs(v)<4,"DAC switching/run-length failure");
    }
    tracking=false;require(allocations==0,"DAC processing/switching allocated");
    std::cout<<"1000000 frames: reset/all-phase switching bounded; allocations=0; storage="<<sizeof(*actual)<<" bytes\n";
}
