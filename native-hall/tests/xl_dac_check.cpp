// Independent FPC clocks + continuous-time X/XL output circuit.
// Synthetic WR_DA words isolate the converter from ROM/controller timing.
#undef NDEBUG
#include "../desktop/dac_xl48.hpp"
#ifndef CINEOL_XL_OUTPUT_HEADER
#define CINEOL_XL_OUTPUT_HEADER "../desktop/output_xl48.hpp"
#define CINEOL_XL_ENGINE_HEADER "../core/engine48.hpp"
#endif
#include CINEOL_XL_OUTPUT_HEADER
#include CINEOL_XL_ENGINE_HEADER
#include <analog/filters.hpp>
#include <isa-level-cpp/lexicon224x.hpp>
#include <emulator/timing.hpp>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>
#include <new>

static bool tracking=false;
static uint64_t allocations=0,releases=0;
void* operator new(size_t n) {if(tracking)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {if(tracking && p)++releases;std::free(p);}
void operator delete(void* p,size_t) noexcept {::operator delete(p);}
void operator delete[](void* p) noexcept {::operator delete(p);}
static void require(bool ok,const char* message) {if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
using namespace cineol::xl;

template<Graph graph> struct Pattern {
    std::array<unsigned,graph_info(graph).rows> masks{};
    Pattern() {
        const int16_t left=0,right=0;
        #include "../desktop/native_graph_dispatch.inc"
    }
    template<unsigned Row,unsigned Op,unsigned RA,unsigned WA,bool Transfer,bool Zero,unsigned Outputs,bool Shift,bool WriteX=false>
    void node(int16_t=0) {masks[Row]=Outputs;}
};
static float fpc_value(const lexicon224x::Converter& c) {
    int code=int(c.dac_code()^0x800);if(code&0x800)code-=4096;
    return float(code*int(1u<<(4-(c.output_gain&3))))/32768.f;
}
static int16_t input_word(unsigned pass,unsigned row,unsigned rows,int frequency) {
    if(frequency) return int16_t(std::lround(12000*std::sin(6.283185307179586*frequency*pass*rows*168750/576000000000.0+row*.017)));
    if(pass*rows>576000000000ULL/168750/2) return 0;
    uint32_t random=(pass*131+row+17)*747796405u+2891336453u;
    random=((random>>((random>>28)+4))^random)*277803737u;
    return int16_t((random>>22)^random);
}
template<Graph graph> struct PolicyEmitter {
    Output48<graph>& output;unsigned pass;int frequency;std::array<float,4>& last;
    void run() {
        const int16_t left=0,right=0;
        #include "../desktop/native_graph_dispatch.inc"
    }
    template<unsigned Row,unsigned Op,unsigned RA,unsigned WA,bool Transfer,bool Zero,unsigned Outputs,bool Shift,bool WriteX=false>
    void node(int16_t=0) {
        if constexpr(Outputs) {
            const auto value=native_hall::Engine48::dac(input_word(pass,Row,graph_info(graph).rows,frequency))/32768.f;
            output.template request<Row,Outputs>(value);
            for(unsigned c=0;c<4;++c)if((Outputs>>c)&1)last[c]=value;
        }
    }
};
template<Graph graph> static void check_graph(bool use_policy,bool selected) {
    constexpr unsigned rows=graph_info(graph).rows;
    const Pattern<graph> pattern;
    double maximum=0;
    for(int frequency:{0,100,1000,8000,15000,20000}) {
        EventDac48 actual;actual.prepare();
        Output48<graph> policy;policy.prepare();std::array<float,4> last{};
        auto modal=lexicon224x::analog::aout_modal();
        using Output=lexicon224x::analog::Output;
        Output reference[4]={Output(modal),Output(modal),Output(modal),Output(modal)};
        lexicon224x::Converter converter;
        struct Capture {uint64_t time;unsigned mask;float value;};
        std::deque<Capture> captures;std::array<float,4> held{};
        uint64_t ref_row=0,pass=0,capture_count=0,raw_errors=0;
        double peak_error=0,error_power=0,power=0;
        for(unsigned frame=0;frame<48000;++frame) {
            const uint64_t beginning=uint64_t(frame)*EventDac48::frame_ticks,end=beginning+EventDac48::frame_ticks;
            const unsigned mask=selected?((1u<<((frame/137)%4))|(1u<<((frame/997)%4))):15;
            const unsigned transport=use_policy?unsigned(16.0*9*rows/640+0.5):0;
            const uint64_t expected_time=frame<transport?0:end-uint64_t(transport)*EventDac48::frame_ticks;
            if(use_policy && pass*rows*EventDac48::row_ticks<=beginning) {
                tracking=true;PolicyEmitter<graph>{policy,unsigned(pass),frequency,last}.run();policy.push(last.data());tracking=false;++pass;
            } else if(!use_policy && pass*rows*EventDac48::row_ticks<end) {
                // Native graph emits a complete pass in its first host frame.
                for(unsigned r=0;r<rows;++r) if(pattern.masks[r]) {
                    const int16_t word=input_word(unsigned(pass),r,rows,frequency);
                    const int32_t clock=int32_t(pass*rows*EventDac48::row_ticks+uint64_t(r)*EventDac48::row_ticks+EventDac48::converter_origin-beginning);
                    tracking=true;actual.request(clock,pattern.masks[r],native_hall::Engine48::dac(word)/32768.f);tracking=false;
                }
                ++pass;
            }
            // Independent FPC executes only physical clocks reached by this
            // frame; conversion normalization and waiting overwrite are its own.
            while(lexicon224x::cpu::timing_224x.marker(ref_row)+lexicon224x::cpu::timing_224x.converter_offset<=end) {
                const unsigned r=unsigned(ref_row%rows),p=unsigned(ref_row/rows);
                const auto previous=converter.output_selects();
                lexicon224x::clock_converter(converter,0,0,0,0,false,pattern.masks[r]!=0,pattern.masks[r],uint16_t(input_word(p,r,rows,frequency)));
                const unsigned selected=converter.output_selects();
                if(selected && selected!=previous) {
                    const uint64_t time=lexicon224x::cpu::timing_224x.marker(ref_row)+lexicon224x::cpu::timing_224x.dac_observed;
                    const float value=fpc_value(converter);
                    for(unsigned c=0;c<4;++c)if((selected>>c)&1)reference[c].hold(time,value);
                    captures.push_back({time,selected,value});++capture_count;
                }
                ++ref_row;
            }
            while(!captures.empty() && captures.front().time<=expected_time) {
                const auto event=captures.front();captures.pop_front();
                for(unsigned c=0;c<4;++c)if((event.mask>>c)&1)held[c]=event.value;
            }
            tracking=true;EventDac48::Frame output;
            if(use_policy){
                if constexpr(requires {policy.sample(mask);})output.analog=policy.sample(mask);
                else {output.analog=policy.sample();for(unsigned c=0;c<4;++c)if(!((mask>>c)&1))output.analog[c]=0;}
                output.raw=policy.raw_sample();
            } else output=actual.sample(mask);tracking=false;
            for(unsigned c=0;c<4;++c) {
                if(output.raw[c]!=held[c])++raw_errors;
                const double physical=reference[c].sample(expected_time);
                const double expected=((mask>>c)&1)?physical:0,error=output.analog[c]-expected;
                require(std::isfinite(output.analog[c]),"nonfinite XL DAC output");
                peak_error=std::max(peak_error,std::abs(error));error_power+=error*error;power+=expected*expected;
            }
        }
        std::cout<<graph_info(graph).name<<" policy="<<use_policy<<" selected="<<selected<<" frequency="<<frequency<<" captures="<<capture_count<<" peak="<<peak_error<<" relative="<<std::sqrt(error_power/power)<<" raw_errors="<<raw_errors<<'\n'<<std::flush;
        require(peak_error<3e-5,"XL event DAC CT peak error exceeds 3e-5");
        require(error_power<1e-8*power,"XL event DAC CT RMS error exceeds -80 dB");
        require(raw_errors==0,"XL DAC capture timing/value mismatch");
        maximum=std::max(maximum,peak_error);
    }
    std::cout<<graph_info(graph).name<<": cold/steady captures + noise/silence + five tones pass, max="<<maximum<<'\n';
}
static void check_dormant_channels() {
    EventDac48 actual;actual.prepare();
    auto modal=lexicon224x::analog::aout_modal();using Output=lexicon224x::analog::Output;
    Output reference[4]={Output(modal),Output(modal),Output(modal),Output(modal)};
    lexicon224x::Converter converter;
    struct Capture {uint64_t time;unsigned mask;float value;};
    std::deque<Capture> captures;std::array<float,4> held{};
    uint64_t row=0,requests=0,observations=0;double peak_error=0,power=0,error_power=0;
    for(unsigned frame=0;frame<96000;++frame) {
        const uint64_t start=uint64_t(frame)*EventDac48::frame_ticks,end=start+EventDac48::frame_ticks;
        while(lexicon224x::cpu::timing_224x.marker(row)+lexicon224x::cpu::timing_224x.converter_offset<=end) {
            const unsigned position=unsigned(row%8000),block=unsigned(row/8000);
            const bool write=position==1 || position==2 || position==40;
            constexpr unsigned masks[]={15,9,6,8};
            const unsigned mask=position==2?0:masks[block%4];
            const auto word=int16_t((block*747796405u+position*2891336453u)>>16);
            if(write) {
                const int32_t clock=int32_t(lexicon224x::cpu::timing_224x.marker(row)+lexicon224x::cpu::timing_224x.converter_offset-start);
                tracking=true;actual.request(clock,mask,native_hall::Engine48::dac(word)/32768.f);tracking=false;++requests;
            }
            const auto previous=converter.output_selects();
            lexicon224x::clock_converter(converter,0,0,0,0,false,write,mask,uint16_t(word));
            const auto selected=converter.output_selects();
            if(selected && selected!=previous) {
                const uint64_t time=lexicon224x::cpu::timing_224x.marker(row)+lexicon224x::cpu::timing_224x.dac_observed;
                const float value=fpc_value(converter);
                for(unsigned c=0;c<4;++c)if((selected>>c)&1)reference[c].hold(time,value);
                captures.push_back({time,selected,value});++observations;
            }
            ++row;
        }
        while(!captures.empty() && captures.front().time<=end) {
            const auto capture=captures.front();captures.pop_front();
            for(unsigned c=0;c<4;++c)if((capture.mask>>c)&1)held[c]=capture.value;
        }
        const unsigned mask=(1u<<((frame/137)%4))|(1u<<((frame/991)%4));
        tracking=true;const auto output=actual.sample(mask);tracking=false;
        for(unsigned c=0;c<4;++c) {
            require(output.raw[c]==held[c],"dormant/zero-mask FPC capture differs");
            const double value=reference[c].sample(end),expected=((mask>>c)&1)?value:0;
            const double error=output.analog[c]-expected;peak_error=std::max(peak_error,std::abs(error));
            power+=expected*expected;error_power+=error*error;
            require(std::isfinite(output.analog[c]),"nonfinite dormant-channel output");
        }
    }
    require(requests>1000 && observations>1000,"dormant fixture did not exercise converter events");
    require(peak_error<3e-5 && error_power<1e-8*power,"dormant/zero-mask/channel-recall circuit differs from CT reference");
    std::cout<<"96000 dormant/zero-mask/recall frames: "<<requests<<" requests, "<<observations
        <<" captures, peak="<<peak_error<<", relative="<<std::sqrt(error_power/power)<<'\n';
}
int main(int argc,char** argv) {
    require(argc<=4,"usage: cineol_xl_dac_check [XL_INDEX] [--policy] [--selected]");
    require(EventDac48::row_ticks==lexicon224x::cpu::timing_224x.row &&
        EventDac48::converter_origin==lexicon224x::cpu::timing_224x.first_marker+lexicon224x::cpu::timing_224x.converter_offset &&
        EventDac48::capture_origin==lexicon224x::cpu::timing_224x.first_marker+lexicon224x::cpu::timing_224x.dac_observed,"XL converter timing constants differ from independent model");
    for(unsigned word=0;word<65536;++word) {
        lexicon224x::Converter c;
        for(unsigned row=0;row<34;++row)lexicon224x::clock_converter(c,0,0,0,0,false,row==0,1,uint16_t(word));
        require(native_hall::Engine48::dac(int16_t(word))/32768.f==fpc_value(c),"XL DAC normalization differs from independent FPC");
    }
    const unsigned index=argc>=2?unsigned(std::stoul(argv[1])):22;
    bool use_policy=false,selected=false;
    for(unsigned i=2;i<unsigned(argc);++i) {
        const std::string option=argv[i];
        if(option=="--policy") {require(!use_policy,"duplicate --policy");use_policy=true;}
        else if(option=="--selected") {require(!selected,"duplicate --selected");selected=true;}
        else require(false,"unknown XL DAC option");
    }
    require(index<=22,"invalid XL graph index");
    each_graph([&]<Graph graph>(){if(index==22 || unsigned(graph)==index)check_graph<graph>(use_policy,selected);});
    check_dormant_channels();
    require(allocations==0 && releases==0,"XL DAC request/sample allocated or released heap storage");
    std::cout<<"65536 normalization words pass; processing heap new/delete=0/0; per-instance storage="<<sizeof(EventDac48)<<'\n';
}
