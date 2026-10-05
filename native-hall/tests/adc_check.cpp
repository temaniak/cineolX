// Keep clock/history assertions active in optimized offline validation.
#undef NDEBUG
#include "../desktop/adc22448.hpp"
#include <analog/filters_224.hpp>
#include <isa-level-cpp/lexicon224x.hpp>
#include <emulator/timing.hpp>
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
static int16_t reference_adc(double value) {
    const auto converted=lexicon224x::analog::convert(5.0*value);
    int code=int(converted.code);if(code&2048)code-=4096;
    return int16_t(code*int(1u<<(4-converted.iga)));
}
static void check_holds(const char* prefix) {
    for(unsigned program=0;program<6;++program) {
        auto machine=std::make_unique<lexicon224x::Machine>();machine->model=lexicon224x::Model::Lexicon224;
        std::ifstream file(std::string(prefix)+"."+std::to_string(program)+".wcs",std::ios::binary);
        uint8_t image[512];require(bool(file.read(reinterpret_cast<char*>(image),512)),"missing independent WCS fixture");
        lexicon224x::load_wcs(*machine,image);
        machine->adc_left=100;machine->adc_right=200;machine->gain_left=machine->gain_right=0;
        unsigned last_load[2]{},found=0;
        for(unsigned row=0;row<400;++row) {
            lexicon224x::fetch(*machine);
            if(machine->fpc.count==39)last_load[1]=row;
            if(machine->fpc.count==89)last_load[0]=row;
            lexicon224x::converter_clock(*machine);
            // The first completed left word still comes from the cold scan.
            if(row>=200 && machine->mi.op==lexicon224x::OPER && machine->mi.source==lexicon224x::FromADC) {
                const unsigned c=(row%100==50)?1:0;
                require(row%100==50 || row%100==0,"unexpected ADC read row");
                require(machine->fpc.input_sample==(c?3200:1600),"ADC channel or gain expansion differs from FPC");
                const int hold=int(last_load[c])-36-int(row/100)*100;
                if(hold!=native_hall::Adc48::hold_rows[c])
                    std::cerr<<"Program "<<program<<" ADC channel "<<c<<" read row "<<row<<" last load "<<last_load[c]<<" hold phase "<<hold<<'\n';
                require(hold==native_hall::Adc48::hold_rows[c],"native ADC hold phase differs from FPC/WCS");found|=1u<<c;
            }
            lexicon224x::execute(*machine);
        }
        require(found==3,"missing independent ADC channel read");
    }
    std::cout<<"Six WCS networks: ADC channel order, completed words and separate hold phases match independent FPC\n";
}
int main(int argc,char** argv) {
    require(argc<=2,"usage: adc_check [PROGRAMS.bank224 fixture prefix]");
    if(argc==2)check_holds(argv[1]);
    using A=native_hall::Adc48;
    require(A::row_ticks==lexicon224x::cpu::timing_224.row &&
            A::event_origin_ticks==lexicon224x::cpu::timing_224.first_marker+lexicon224x::cpu::timing_224.converter_offset,
            "ADC origin differs from original-224 clock timing");
    uint32_t random=17;
    for(unsigned n=0;n<250000;++n) {
        random=random*1664525u+1013904223u;
        double value=2.0*double(int32_t(random))/2147483648.0;
        require(A::adc(value)==reference_adc(value),"ADC range/code differs from independent converter");
        require(A::detectors(value)==lexicon224x::analog::level_detectors(5.0*value),"input detector differs from reference");
    }
    for(unsigned range=0;range<4;++range)for(int code=-2048;code<2048;++code) {
        const double tie=(code+0.5)/2047.0/double(1u<<range);
        for(double value:{tie,std::nextafter(tie,-INFINITY),std::nextafter(tie,INFINITY)})
            require(A::adc(value)==reference_adc(value),"ADC half-code rounding differs from independent converter");
    }
    std::cout<<"ADC: 250000 seeded values + half-code/adjacent values in four ranges exactly match reference\n";
    auto actual=std::make_unique<A>();constexpr double pi=3.14159265358979323846;
    for(int frequency:{-2,-1,0,100,1000,4000,6000,8000,9000,9600,10000,12000,16000,20000}) {
        actual->prepare();auto modal=lexicon224x::analog::ain_224_modal();
        lexicon224x::analog::Input reference[2]={{modal,A::frame_ticks},{modal,A::frame_ticks}};
        unsigned pass=0,conversions=0;random=17;double power=0,error_power=0,peak_error=0;
        for(unsigned frame=0;frame<96000;++frame) {
            float raw[2];
            for(unsigned c=0;c<2;++c) {
                random=random*1664525u+1013904223u;
                if(frequency>0)raw[c]=float(.1*std::sin(2*pi*frequency*frame/48000.0+c*.9));
                else if(frequency==0)raw[c]=frame<48000?float(int32_t(random))/2147483648.f:0;
                else if(frequency==-1)raw[c]=(frame==c+1 || frame==c+33)?(c?-.75f:1.f):0;
                else raw[c]=frame<48000?float((int(frame/137)%13-6)*.112):0;
                reference[c].push(raw[c]);
            }
            double wanted[2]{};
            const bool due=uint64_t(pass)*A::pass_ticks<uint64_t(frame+1)*A::frame_ticks;
            if(due)for(unsigned c=0;c<2;++c) {
                const int64_t time=int64_t(pass)*A::pass_ticks+int64_t(A::hold_rows[c])*A::row_ticks
                    +A::event_origin_ticks-int64_t(A::latency_frames)*A::frame_ticks;
                wanted[c]=reference[c].sample(time<0?0:uint64_t(time));
            }
            unsigned emitted=0;tracking=true;
            actual->process(raw,[&](const double* values) {
                ++emitted;
                for(unsigned c=0;c<2;++c) {
                    const double error=values[c]-wanted[c];peak_error=std::max(peak_error,std::abs(error));
                    error_power+=error*error;power+=wanted[c]*wanted[c];
                    require(std::isfinite(values[c]),"nonfinite input circuit output");
                    require(A::adc(values[c])==reference_adc(wanted[c]),"reconstructed ADC word differs from CT reference");
                    require(A::detectors(values[c])==lexicon224x::analog::level_detectors(5.0*wanted[c]),"reconstructed level differs from CT reference");
                    ++conversions;
                }
                ++pass;
            });
            tracking=false;require(emitted==unsigned(due),"native ADC clock differs from independent schedule");
        }
        require(pass==40960 && conversions==81920,"ADC count drift");
        require(peak_error<1e-10 && error_power<1e-16*power,"input boundary differs from continuous-time reference");
        std::cout<<frequency<<" Hz/test: CT peak error="<<peak_error<<", relative RMS="<<10*std::log10(error_power/power)
            <<" dB; 81920 ADC words exact\n";
    }
    actual->prepare();unsigned emitted=0;tracking=true;
    for(unsigned frame=0;frame<1000000;++frame) {
        if(frame%170003==0)actual->reset();
        float raw[]={float(frame%17)*.03f,-float(frame%31)*.02f};
        actual->process(raw,[&](const double* values){++emitted;for(unsigned c=0;c<2;++c)require(std::isfinite(values[c]),"ADC reset/run-length failure");});
    }
    tracking=false;require(allocations==0,"ADC processing/reset allocated");
    std::cout<<"1000000 frames: clock/history/reset bounds pass; allocations=0; storage="<<sizeof(*actual)<<" bytes\n";
}
