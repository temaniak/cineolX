// Independent X/XL AIN component and integrated Native48 input-policy checks.
#undef NDEBUG
#ifndef CINEOL_XL_INPUT_HEADER
#define CINEOL_XL_INPUT_HEADER "../desktop/input_xl48.hpp"
#endif
#include CINEOL_XL_INPUT_HEADER
#include "../desktop/adc_xl48.hpp"
#include <analog/filters.hpp>
#include <emulator/timing.hpp>
#include <cstdlib>
#include <iostream>
#include <new>
#include <set>
#include <vector>

static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n){if(tracking)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {if(tracking && p)++releases;std::free(p);}
void operator delete(void* p,size_t) noexcept {::operator delete(p);}
void operator delete[](void* p) noexcept {::operator delete(p);}
static void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
using A=cineol::xl::EventAdc48;
static int16_t reference_adc(double value) {
    const auto converted=lexicon224x::analog::convert(5.0*value);
    int code=int(converted.code);if(code&2048)code-=4096;
    return int16_t(code*int(1u<<(4-converted.iga)));
}
template<cineol::xl::Graph graph> static void check_policy() {
    constexpr unsigned rows=cineol::xl::graph_info(graph).rows;
    const auto timing=lexicon224x::cpu::timing_224x;
    const uint64_t period=rows*timing.row,origin=timing.first_marker+timing.converter_offset;
    cineol::xl::Input48<graph> actual;
    for(int frequency:{-3,-2,-1,0,100,1000,4000,8000,12000,16000,20000}) {
        actual.prepare();auto modal=lexicon224x::analog::ain_modal();
        lexicon224x::analog::Input reference[2]={{modal,A::frame_ticks},{modal,A::frame_ticks}};
        unsigned pass=0,clock_errors=0,word_errors=0,detector_errors=0,bare_errors=0;uint32_t random=17;
        double peak_error=0,power=0,error_power=0;
        std::vector<std::array<float,2>> history(48000);
        for(unsigned frame=0;frame<history.size();++frame) {
            auto& raw=history[frame];
            for(unsigned c=0;c<2;++c) {
                random=random*1664525u+1013904223u;
                if(frequency>0)raw[c]=float(.1*std::sin(6.283185307179586*frequency*frame/48000.0+c*.9));
                else if(frequency==0)raw[c]=frame<24000?float(int32_t(random))/2147483648.f:0;
                else if(frequency==-1)raw[c]=(frame==c+1 || frame==c+33)?(c?-.75f:1.f):0;
                else if(frequency==-2)raw[c]=frame<24000?float((int(frame/137)%13-6)*.112):0;
                else raw[c]=frame==0?(c?-.75f:1.f):0;
            }
        }
        for(unsigned frame=0;frame<history.size();++frame) {
            const auto& raw=history[frame];for(unsigned c=0;c<2;++c)reference[c].push(raw[c]);
            const bool due=uint64_t(pass)*period+39*timing.row+origin<uint64_t(frame+1)*A::frame_ticks;
            unsigned emitted=0;tracking=true;
            actual.process(raw.data(),[&](const cineol::xl::InputFrame& values) {
                ++emitted;
                // The older policy can emit in a different host frame. Compare
                // its same indexed pass to the independently derived hold time
                // anyway, retaining a voltage/word failure instead of stopping
                // at a schedule/API mismatch. Reference allocation is offline.
                tracking=false;double wanted[2];unsigned detectors=0;
                for(unsigned c=0;c<2;++c) {
                    const int hold=c?3:53-int(rows);
                    const int64_t time=int64_t(pass)*int64_t(period)+int64_t(hold)*int64_t(timing.row)+int64_t(origin)-int64_t(reference[c].latency());
                    wanted[c]=reference[c].sample(time<0?0:uint64_t(time));
                    detectors|=lexicon224x::analog::level_detectors(5.0*wanted[c]);
                }
                tracking=true;
                for(unsigned c=0;c<2;++c) {
                    const double error=values.circuit[c]-wanted[c];peak_error=std::max(peak_error,std::abs(error));
                    power+=wanted[c]*wanted[c];error_power+=error*error;
                    require(std::isfinite(values.circuit[c]),"nonfinite XL input policy");
                    if(values.clean[c]!=reference_adc(wanted[c]))++word_errors;
                    const int load=c?39:89-int(rows);
                    const int64_t load_time=int64_t(pass)*int64_t(period)+int64_t(load)*int64_t(timing.row)+int64_t(origin);
                    const float pins=load_time<0?0:history[unsigned(uint64_t(load_time)/A::frame_ticks)][c];
                    const int code=int(std::lround(std::clamp(pins,-1.f,1.f)*2047.f));
                    if(values.raw[c]!=pins || values.bare[c]!=int16_t(code*16))++bare_errors;
                }
                if(values.detectors!=detectors)++detector_errors;
                ++pass;
            });
            tracking=false;if(emitted!=unsigned(due))++clock_errors;
        }
        std::cout<<cineol::xl::graph_info(graph).name<<" policy frequency="<<frequency<<" passes="<<pass
            <<" peak="<<peak_error<<" relative="<<std::sqrt(error_power/power)<<" words="<<word_errors
            <<" detectors="<<detector_errors<<" bare="<<bare_errors<<" clocks="<<clock_errors<<'\n'<<std::flush;
        require(peak_error<1e-10 && error_power<1e-16*power && word_errors==0 && detector_errors==0 && bare_errors==0,
            "XL input policy voltage/words/comparators/bypass differ from independent model");
        require(clock_errors==0,"XL input policy deadline differs from independent load schedule");
    }
}
int main(int argc,char** argv) {
    require(argc<=3,"usage: xl_adc_check [GRAPH_ROWS=0] [--policy]");
    require(A::row_ticks==lexicon224x::cpu::timing_224x.row &&
        A::event_origin_ticks==lexicon224x::cpu::timing_224x.first_marker+lexicon224x::cpu::timing_224x.converter_offset,
        "XL ADC clocks differ from independent model");
    uint32_t random=17;
    for(unsigned n=0;n<250000;++n) {
        random=random*1664525u+1013904223u;const double value=2.0*double(int32_t(random))/2147483648.0;
        require(A::adc(value)==reference_adc(value),"XL ADC range/code differs");
        require(A::detectors(value)==lexicon224x::analog::level_detectors(5.0*value),"XL level comparators differ");
    }
    for(unsigned range=0;range<4;++range)for(int code=-2048;code<2048;++code) {
        const double tie=(code+.5)/2047.0/double(1u<<range);
        for(double value:{tie,std::nextafter(tie,-INFINITY),std::nextafter(tie,INFINITY)})
            require(A::adc(value)==reference_adc(value),"XL ADC tie/adjacent rounding differs");
    }
    std::set<unsigned> row_counts;
    for(const auto& graph:cineol::xl::graphs)row_counts.insert(graph.rows);
    const unsigned selected=argc>=2?unsigned(std::stoul(argv[1])):0;
    require(!selected || row_counts.contains(selected),"unsupported XL graph rows");
    const bool policy=argc==3;require(!policy || std::string(argv[2])=="--policy","unknown XL ADC option");
    if(policy) {
        cineol::xl::each_graph([&]<cineol::xl::Graph graph>(){if(!selected || cineol::xl::graph_info(graph).rows==selected)check_policy<graph>();});
        require(allocations==0 && releases==0,"XL input policy processing touched heap");
        std::cout<<"XL input policy CT/words/detectors/deadlines pass; processing heap new/delete=0/0\n";return 0;
    }
    uint64_t total=0;
    A actual;
    for(unsigned rows:row_counts)if(!selected || rows==selected) {
        for(int frequency:{-3,-2,-1,0,100,1000,4000,8000,12000,16000,20000}) {
            actual.prepare(rows);auto modal=lexicon224x::analog::ain_modal();
            lexicon224x::analog::Input reference[2]={{modal,A::frame_ticks},{modal,A::frame_ticks}};
            unsigned pass=0;random=17;double peak_error=0,power=0,error_power=0;
            const uint64_t period=rows*lexicon224x::cpu::timing_224x.row;
            const auto origin=lexicon224x::cpu::timing_224x.first_marker+lexicon224x::cpu::timing_224x.converter_offset;
            for(unsigned frame=0;frame<48000;++frame) {
                float raw[2];
                for(unsigned c=0;c<2;++c) {
                    random=random*1664525u+1013904223u;
                    if(frequency>0)raw[c]=float(.1*std::sin(6.283185307179586*frequency*frame/48000.0+c*.9));
                    else if(frequency==0)raw[c]=frame<24000?float(int32_t(random))/2147483648.f:0;
                    else if(frequency==-1)raw[c]=(frame==c+1 || frame==c+33)?(c?-.75f:1.f):0;
                    else if(frequency==-2)raw[c]=frame<24000?float((int(frame/137)%13-6)*.112):0;
                    else raw[c]=frame==0?(c?-.75f:1.f):0;
                    reference[c].push(raw[c]);
                }
                // Derive the deadline from the physical right load (ROM 39),
                // independent of the prototype's process clock/constants.
                const bool due=uint64_t(pass)*period+39*lexicon224x::cpu::timing_224x.row+origin<uint64_t(frame+1)*A::frame_ticks;
                double wanted[2]{};
                if(due)for(unsigned c=0;c<2;++c) {
                    const int hold=c?3:53-int(rows);
                    const int64_t time=int64_t(pass)*int64_t(period)+int64_t(hold)*int64_t(lexicon224x::cpu::timing_224x.row)
                        +int64_t(origin)-int64_t(reference[c].latency());
                    wanted[c]=reference[c].sample(time<0?0:uint64_t(time));
                }
                unsigned emitted=0;tracking=true;
                actual.process(raw,[&](const double* values) {
                    ++emitted;
                    for(unsigned c=0;c<2;++c) {
                        const double error=values[c]-wanted[c];peak_error=std::max(peak_error,std::abs(error));
                        power+=wanted[c]*wanted[c];error_power+=error*error;
                        require(std::isfinite(values[c]),"nonfinite XL input circuit");
                        if(A::adc(values[c])!=reference_adc(wanted[c])) {
                            std::cerr<<"rows="<<rows<<" frequency="<<frequency<<" frame="<<frame<<" channel="<<c
                                <<" actual="<<values[c]<<" reference="<<wanted[c]<<" error="<<error<<'\n';
                            require(false,"XL reconstructed ADC word differs from CT reference");
                        }
                        require(A::detectors(values[c])==lexicon224x::analog::level_detectors(5.0*wanted[c]),"XL reconstructed comparator differs");
                        ++total;
                    }
                    ++pass;
                });
                tracking=false;require(emitted==unsigned(due),"XL input deadline differs from FPC load schedule");
            }
            const auto expected=(uint64_t(48000)*A::frame_ticks-origin-39*lexicon224x::cpu::timing_224x.row-1)/period+1;
            require(pass==expected,"XL ADC pass count drifts");
            std::cout<<"rows="<<rows<<" frequency="<<frequency<<" passes="<<pass<<" peak="<<peak_error
                <<" relative="<<std::sqrt(error_power/power)<<'\n'<<std::flush;
            require(peak_error<1e-10 && error_power<1e-16*power,"XL input circuit differs from continuous-time model");
        }
    }
    require(allocations==0 && releases==0,"XL ADC prototype processing touched heap storage");
    std::cout<<"XL ADC prototype: "<<total<<" words/comparators exact; processing heap new/delete=0/0; storage="<<sizeof(A)<<'\n';
}
