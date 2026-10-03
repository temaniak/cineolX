#include "../core/engine48.hpp"
#include <isa-level-cpp/lexicon224x.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <new>
#include <cstdlib>
#include <chrono>
using namespace native_hall;
static bool tracking=false;static unsigned allocations=0;
void* operator new(size_t n){if(tracking)++allocations;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,size_t) noexcept {std::free(p);}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
static void require(bool ok,const char* text){if(!ok){std::cerr<<text<<'\n';std::exit(1);}}
int main(int argc,char** argv) {
    require(argc==2,"usage: bank_check PROGRAMS.bank224");
    std::ifstream file(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<ProgramBank>();require(read_bank(bytes.data(),bytes.size(),*bank),"invalid bank");
    auto broken=bytes;broken.back()^=1;auto rejected=std::make_unique<ProgramBank>();
    require(!read_bank(broken.data(),broken.size(),*rejected),"corrupt bank accepted");
    // Valid checksums must not bypass structural/range validation.
    rejected->programs[0]=bank->programs[0];rejected->programs[0].tail.rows[0]=100;
    require(!rejected->programs[0].valid(0),"bad coefficient row accepted");
    auto hall=std::make_unique<Hall>();auto engine=std::make_unique<Engine48>();
    for(unsigned program=0;program<program_count;++program) {
        // Freeze the control clock only for the independent arithmetic test.
        auto quiet=std::make_unique<ProgramBank>(*bank);quiet->programs[program].level_rate_tenths.fill(0);
        quiet->programs[program].initial_decay.amount=0;
        hall->prepare(*quiet,program);Controls c;c.mode_enhancement=c.decay_optimization=false;
        c.predelay_ms=predelay_minima[program];hall->set_controls(c);
        hall->select_program(program);
        require(hall->offsets()==quiet->programs[program].offsets,"selection changed minimum pre-delay");
        auto model=std::make_unique<lexicon224x::Machine>();model->model=lexicon224x::Model::Lexicon224;
        std::ifstream image(std::string(argv[1])+"."+std::to_string(program)+".wcs",std::ios::binary);
        uint8_t data[512];require(bool(image.read(reinterpret_cast<char*>(data),512)),"missing independent WCS fixture");
        lexicon224x::load_wcs(*model,data);
        uint32_t random=17;uint64_t wanted_saturations=0;
        for(unsigned frame=0;frame<12000;++frame) {
            if(frame%257==0) {
                unsigned n=frame/257;c.bass=1+(n*7)%31;c.mid=(n*11)%32;c.crossover=(n*13)%32;
                c.treble=(n*17)%32;c.depth=(n*19)%72;c.diffusion=1+(n*23)%63;
                c.predelay_ms=predelay_minima[program]+int((n*29)%129);hall->set_controls(c);
                for(unsigned r=0;r<100;++r) {
                    int coeff=hall->coefficients()[r];
                    model->wcs[r]=(model->wcs[r]&~(0xfc000000u|0x800000u|0x3fffu))
                        |uint32_t(std::abs(coeff))<<26|(coeff<0?0x800000u:0)|uint32_t(~hall->offsets()[r]&0x3fff);
                }
            }
            random=random*1664525u+1013904223u;int16_t left=int16_t(random>>16);
            random=random*1664525u+1013904223u;int16_t right=int16_t(random>>16),wanted[4]{},actual[4];
            for(unsigned r=0;r<100;++r) {
                lexicon224x::fetch(*model);lexicon224x::converter_clock(*model);
                if(r==0) model->fpc.input_sample=uint16_t(left);
                if(r==50) model->fpc.input_sample=uint16_t(right);
                if(model->mi.wr_da) for(unsigned channel=0;channel<4;++channel)
                    if(model->mi.channels&(1u<<channel)) wanted[channel]=int16_t(lexicon224x::source_value(*model,model->mi));
                lexicon224x::execute(*model);
                unsigned sat=model->saturated;
                wanted_saturations+=(sat&1)+((sat>>1)&1)+((sat>>2)&1);
                // XFER samples the same adder again just before its third edge.
                if(model->mi.xfer && (sat&4)) ++wanted_saturations;
            }
            hall->process(left,right,actual);
            require(std::equal(actual,actual+4,wanted),"native output differs from independent row machine");
            require(hall->accumulator()==model->ACC && hall->result()==model->RR,"native arithmetic state differs");
            require(hall->saturation_count()==wanted_saturations,"saturation diagnostic differs from independent ARU");
        }
        require(std::equal(hall->memory().begin(),hall->memory().end(),model->memory,
            [](int16_t a,uint16_t b){return uint16_t(a)==b;}),"delay memory differs");
        std::cout<<program_names[program]<<": 12000 full-scale stereo frames + 47 parameter sets bit exact; SAT="<<wanted_saturations<<" exact\n";
        engine->prepare(*bank,program);Parameters p;p.program=program;p.hall.predelay_ms=predelay_minima[program];
        engine->set_parameters(p);float peak=0;double energy=0,tail=0;
        auto start=std::chrono::steady_clock::now();tracking=true;
        for(unsigned n=0;n<3*48000;++n) {
            if(n%960==0) {p.hall.mode_enhancement=(n/960)%2;p.hall.decay_optimization=(n/1920)%2;engine->set_parameters(p);}
            float l,r;engine->process(n<12000?0.08f*std::sin(n*0.21f):0,n==0?0.2f:0,l,r);
            require(std::isfinite(l) && std::isfinite(r),"nonfinite 48k audio");
            peak=std::max({peak,std::abs(l),std::abs(r)});energy+=double(l)*l+double(r)*r;
            if(n>48000) tail+=double(l)*l+double(r)*r;
        }
        const auto retained_memory=engine->hall().memory();engine->set_parameters(p);
        require(engine->hall().memory()==retained_memory,"same-program control update cleared the tail");
        tracking=false;double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        require(peak<4 && energy>1e-6 && tail>1e-8,"silent or runaway program");
        std::cout<<"  48k signal/tail, mode toggles: peak="<<peak<<" tail energy="<<tail<<" host elapsed="<<elapsed<<"s/3s audio\n";
    }
    engine->prepare(*bank,0);Parameters p;p.program=0;p.hall.predelay_ms=24;p.mix=0;engine->set_parameters(p);
    for(unsigned i=0;i<10000;++i){float l,r;engine->process(0,0,l,r);}
    tracking=true;
    // All directed hard switches must take effect at the control update.
    for(unsigned from=0;from<program_count;++from) for(unsigned to=0;to<program_count;++to) {
        p.program=from;engine->set_parameters(p);
        require(engine->active_program()==from,"source selection was deferred");
        for(unsigned n=0;n<128;++n){float l,r;engine->process(0,0,l,r);}
        p.program=to;engine->set_parameters(p);
        require(engine->active_program()==to,"target selection was deferred");
        if(from!=to) require(std::all_of(engine->hall().memory().begin(),engine->hall().memory().end(),
            [](int16_t word){return word==0;}),"old delay tail survived hard switch");
        for(unsigned n=0;n<128;++n) {
            float l,r;engine->process(n==0?0.3f:0,n==0?-0.2f:0,l,r);
            require(std::abs(l-(n==70?0.3f:0))<1e-6f && std::abs(r-(n==70?-0.2f:0))<1e-6f,"dry path changed during switch");
        }
    }
    // Rapid switching and simultaneous parameter changes have no pending stage.
    p.mix=1;
    for(unsigned n=0;n<240000;++n) {
        if(n%96==0) {
            p.program=(n/96)%6;p.hall.depth=(n/96)%72;p.hall.diffusion=1+(n/96)%63;
            p.hall.predelay_ms=predelay_minima[p.program]+int((n/96)%129);
            p.analog=(n/1920)%2;engine->set_parameters(p);
            require(engine->active_program()==p.program,"rapid selection was deferred");
        }
        float l,r;engine->process(0.08f*std::sin(n*0.1f),0.04f*std::sin(n*0.11f),l,r);
        require(std::isfinite(l) && std::isfinite(r) && std::abs(l)<4 && std::abs(r)<4,"switching stress failed");
    }
    p.program=5;engine->set_parameters(p);
    tracking=false;require(engine->active_program()==5,"last selection was deferred");
    require(allocations==0,"heap allocation in processing/control/switching");
    std::cout<<"36 directed hard switches: immediate, delay cleared, dry exact; 2500 rapid switches: immediate, bounded; heap allocations=0\n";
    std::cout<<"Storage: bank="<<sizeof(ProgramBank)<<" program="<<sizeof(ProgramProfile)<<" engine="<<sizeof(Engine48)<<" bytes\n";
}
