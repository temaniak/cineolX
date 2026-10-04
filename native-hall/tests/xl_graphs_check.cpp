#include "../desktop/concert48.hpp"
#include "../desktop/diffusion.hpp"
#include "xl_reference.hpp"
#include <chrono>
#include <cstdlib>
#include <memory>
#include <new>

static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n) {if(tracking) ++allocations;if(void* p=std::malloc(n?n:1)) return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {if(tracking && p) ++releases;std::free(p);}
void operator delete(void* p,size_t) noexcept {::operator delete(p);}
void* operator new[](size_t n) {return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
using namespace lexplug;using namespace lexplug::op;using xl_test::require;

static Task<void> change(Machine& m,LarcOperator& op,unsigned fixture,const PagesReading& pages) {
    const unsigned raw=fixture==1?34:202;
    for(int page=0;page<pages.count;++page) for(unsigned slot=0;slot<6;++slot)
        if(std::strcmp(pages.pages[page].sliders[slot].shown.name,"INACTIVE")!=0)
            co_await op.moveSlider(page+1,slot,slot==1?255-raw:raw);
    co_await m.sleep(1);
}

template<cineol::xl::Graph graph>
static Task<void> check_diffusion(Machine& machine,LarcOperator& op,Engine& engine) {
    using Core=cineol::xl::Network<graph>;
    constexpr auto info=cineol::xl::graph_info(graph);
    co_await xl_test::select(machine,op,info.bank,info.program);
    auto& host=engine.host();
    auto word=[&](unsigned a){return unsigned(host.memory[a])|unsigned(host.memory[a+1])<<8;};
    cineol::xl::DiffusionProfile profile;
    profile.count=host.memory[0x3cf9]&15;
    profile.separate_stop=(host.memory[0x3df9]&64)==0;
    const unsigned descriptors=word(0x3cf7);
    for(unsigned i=0;i<profile.count;++i) {
        const unsigned address=word(descriptors+i*3);
        xl_test::require(address>=0x4003 && address<0x4200 && (address&3)==3,"bad XL diffusion descriptor");
        auto& target=profile.targets[i];
        target.row=uint8_t(127-(address-0x4000)/4);target.scale_cap=host.memory[descriptors+i*3+2];
        for(unsigned j=0;j<3;++j) target.negative[j]=lexicon224x::decode(host.dsp->wcs[target.row+j]).negative;
    }
    xl_test::require(profile.valid(Core::rows),"invalid XL diffusion targets");
    unsigned diffusion_record=0;
    host.pc_watches[0xb2a3]=true;
    host.pc_observer=[&](uint64_t,lexicon224x::cpu::CpuSnapshot cpu) {diffusion_record=cpu.bc;};
    co_await op.moveSlider(2,4,2);
    host.pc_watches[0xb2a3]=false;host.pc_observer={};
    xl_test::require(diffusion_record!=0,"Diffusion compiler record not found");
    cineol::xl::DiffusionProfile direct;
    direct.count=host.memory[diffusion_record+2]&15;direct.half_scale=true;
    const unsigned diffusion_targets=word(diffusion_record);
    for(unsigned i=0;i<direct.count;++i) {
        const unsigned address=word(diffusion_targets+i*3);
        xl_test::require(address>=0x4003 && address<0x4200 && (address&3)==3,"bad direct diffusion descriptor");
        auto& target=direct.targets[i];
        target.row=uint8_t(127-(address-0x4000)/4);target.scale_cap=host.memory[diffusion_targets+i*3+2];
        for(unsigned j=0;j<3;++j) target.negative[j]=lexicon224x::decode(host.dsp->wcs[target.row+j]).negative;
    }
    xl_test::require(direct.valid(Core::rows),"invalid direct diffusion targets");
    unsigned cases=0;
    for(unsigned definition:{2u,66u,130u,250u}) {
        co_await op.moveSlider(2,5,definition);
        for(unsigned raw=2;raw<255;raw+=4) {
            co_await op.moveSlider(2,4,raw);
            typename Core::Settings settings;
            for(unsigned row=0;row<Core::rows;++row) {
                const auto mi=lexicon224x::decode(host.dsp->wcs[row]);
                settings.coefficients[row]=int8_t(mi.negative?-int(mi.coefficient):int(mi.coefficient));
            }
            const auto definition_record=op.stored(1,5);
            const auto normal=cineol::xl::DiffusionProfile::feedback_index(host.memory[0x3c74],definition_record);
            const auto stop=cineol::xl::DiffusionProfile::feedback_index(host.memory[0x3c7a],definition_record);
            if(normal!=host.memory[0x3e3e] || stop!=host.memory[0x3e3f]) {
                std::cerr<<info.name<<" diffusion="<<raw<<" definition="<<definition<<" native="<<int(normal)<<'/'<<int(stop)
                    <<" firmware="<<int(host.memory[0x3e3e])<<'/'<<int(host.memory[0x3e3f])
                    <<" RAM raw="<<int(host.memory[0x3c74])<<'/'<<int(host.memory[0x3c7a])<<" definition record="<<int(definition_record)
                    <<" limit="<<int(host.memory[0x3e0b])<<'\n';
                xl_test::require(false,"native diffusion quantization differs");
            }
            profile.apply(settings,normal,stop,host.memory[0x3e12]);
            direct.apply(settings,cineol::xl::DiffusionProfile::diffusion_index(uint8_t(raw)),0,0);
            for(const auto* targets:{&profile,&direct}) for(unsigned i=0;i<targets->count;++i) for(unsigned j=0;j<3;++j) {
                const unsigned row=targets->targets[i].row+j;
                const auto mi=lexicon224x::decode(host.dsp->wcs[row]);
                if(settings.coefficients[row]!=int8_t(mi.negative?-int(mi.coefficient):int(mi.coefficient))) {
                    std::cerr<<info.name<<" raw="<<raw<<" definition="<<definition<<" target="<<i<<" row="<<row
                        <<" native="<<int(settings.coefficients[row])<<" firmware="<<(mi.negative?-int(mi.coefficient):int(mi.coefficient))<<'\n';
                    xl_test::require(false,"native diffusion coefficient differs");
                }
            }
            ++cases;
        }
    }
    std::cout<<info.name<<" Diffusion/Definition: "<<cases<<" composed controls, "<<unsigned(profile.count)<<'+'<<unsigned(direct.count)<<" allpass targets exact\n";
}

template<cineol::xl::Graph graph>
static void check_graph(Engine& engine,Machine& machine,LarcOperator& op) {
    using Core=cineol::xl::Network<graph>;using Audio=cineol::xl::Native48<graph>;
    constexpr auto info=cineol::xl::graph_info(graph);
    auto result=machine.run_task([&]{return xl_test::select(machine,op,info.bank,info.program);});
    require(!result.failed,"XL graph selection failed");
    PagesReading pages;result=machine.run_task([&]()->Task<void>{co_await op.readPages(pages);});
    require(!result.failed,"XL graph pages failed");
    std::cout<<"Checking "<<info.name<<" across "<<pages.count<<" pages\n"<<std::flush;
    auto native=std::make_unique<Core>();auto reference=std::make_unique<lexicon224x::Machine>();
    uint32_t random=23;uint64_t wanted_saturations=0;
    for(unsigned fixture=0;fixture<3;++fixture) {
        if(fixture) {
            result=machine.run_task([&]{return change(machine,op,fixture,pages);});
            require(!result.failed,"XL graph controls failed");
        }
        const auto& original=*engine.host().dsp;
        xl_test::ShapeCheck shape{original,graph};shape.run();require(shape.valid,"unsupported XL graph variation");
        typename Core::Settings settings;
        std::array<unsigned,2> adc_rows{};unsigned inputs=0;
        for(unsigned row=0;row<Core::rows;++row) {
            auto mi=lexicon224x::decode(original.wcs[row]);
            settings.coefficients[row]=int8_t(mi.negative?-int(mi.coefficient):int(mi.coefficient));
            settings.offsets[row]=uint16_t(~mi.low);
            if(mi.op==lexicon224x::OPER && mi.source==lexicon224x::FromADC) {
                require(inputs<2,"unsupported XL converter layout");adc_rows[inputs++]=row;
            }
        }
        require(inputs==2,"missing XL inputs");
        if(!fixture) require(native->prepare(settings),"XL native prepare failed");
        else require(native->set_settings(settings),"XL native control update failed");
        std::copy(std::begin(original.wcs),std::end(original.wcs),std::begin(reference->wcs));
        tracking=true;
        for(unsigned frame=0;frame<24000;++frame) {
            random=random*1664525u+1013904223u;const int16_t left=int16_t(random>>16);
            random=random*1664525u+1013904223u;const int16_t right=int16_t(random>>16);
            int16_t actual[4],wanted[4]{};
            for(unsigned row=0;row<Core::rows;++row) {
                lexicon224x::fetch(*reference);lexicon224x::converter_clock(*reference);
                if(row==adc_rows[0]) reference->fpc.input_sample=uint16_t(left);
                if(row==adc_rows[1]) reference->fpc.input_sample=uint16_t(right);
                if(reference->mi.wr_da) for(unsigned c=0;c<4;++c)
                    if(reference->mi.channels&(1u<<c)) wanted[c]=int16_t(lexicon224x::source_value(*reference,reference->mi));
                lexicon224x::execute(*reference);
                const unsigned sat=reference->saturated;
                wanted_saturations+=(sat&1)+((sat>>1)&1)+((sat>>2)&1);
                if(reference->mi.xfer && (sat&4)) ++wanted_saturations;
            }
            native->process(left,right,actual);
            require(std::equal(actual,actual+4,wanted),"XL native A-D outputs differ");
            require(native->accumulator()==reference->ACC && native->result()==reference->RR,"XL native arithmetic differs");
            require(uint16_t(native->control_output())==reference->xreg_to_cpu,"XL native envelope/peak output differs");
            require(native->saturation_count()==wanted_saturations,"XL native saturation count differs");
        }
        tracking=false;
        require(std::equal(native->memory().begin(),native->memory().end(),reference->memory,
            [](int16_t a,uint16_t b){return uint16_t(a)==b;}),"XL native delay memory differs");
        auto audio=std::make_unique<Audio>();require(audio->prepare(settings),"XL audio prepare failed");
        double energy=0,tail=0;float peak=0;tracking=true;
        for(unsigned n=0;n<96000;++n) {
            float l,r;audio->process(n==0?0.5f:0,n<4800?0.05f*std::sin(n*0.17f):0,l,r);
            require(std::isfinite(l) && std::isfinite(r),"nonfinite XL graph audio");
            peak=std::max({peak,std::abs(l),std::abs(r)});energy+=double(l)*l+double(r)*r;
            if(n>48000) tail+=double(l)*l+double(r)*r;
        }
        tracking=false;
        require(peak<4 && energy>1e-6,"silent/runaway XL graph");
        if constexpr(info.bank!=4 && graph!=cineol::xl::Graph::inverse_room)
            require(tail>1e-14,"missing reverb tail");
        audio->set_global(0,0,true,0,2);
        for(unsigned n=0;n<10000;++n) {float l,r;audio->process(0,0,l,r);}
        tracking=true;
        for(unsigned n=0;n<128;++n) {
            float l,r;audio->process(n==0?0.3f:0,n==0?-0.2f:0,l,r);
            require(std::abs(l-(n==Audio::latency_samples?0.3f:0))<1e-6f &&
                std::abs(r-(n==Audio::latency_samples?-0.2f:0))<1e-6f,"XL graph dry alignment differs");
        }
        tracking=false;
    }
    native_hall::RationalFilter<Audio::down_num,Audio::down_den,64,2> down;
    native_hall::RationalFilter<Audio::down_den,Audio::down_num,32,4> up;
    down.prepare();up.prepare();unsigned count=0;float two[2]{},four[4]{};tracking=true;
    for(unsigned n=0;n<Audio::down_den*100;++n) down.process(two,[&](const float*){++count;});
    require(count==Audio::down_num*100,"XL graph input clock drift");count=0;
    for(unsigned n=0;n<Audio::down_num*100;++n) up.process(four,[&](const float*){++count;});
    require(count==Audio::down_den*100,"XL graph output clock drift");tracking=false;
    const auto start=std::chrono::steady_clock::now();int16_t out[4];
    for(unsigned n=0;n<Core::rate_numerator*10/Core::rate_denominator;++n) native->process(int16_t(n),int16_t(-int(n)),out);
    std::cout<<info.name<<": 72000 stereo frames across 3 controls, A-D/state/memory/saturation exact; "
        <<Core::rows<<" rows, "<<double(Core::rate_numerator)/Core::rate_denominator<<" Hz, ratios "
        <<Audio::down_num<<'/'<<Audio::down_den<<", delay="<<Audio::latency_samples<<"; signal/tail/dry/clocks pass; core "
        <<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"s/~10s audio\n";
}

int main(int argc,char** argv) {
    require(argc==2 || argc==3,"usage: cineol_xl_graphs_check ROM_DIRECTORY [--rich-only|--all|PROGRAM_INDEX]");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);
    auto machine=std::make_unique<Machine>(*engine);LarcOperator op(*machine);
    auto result=machine->run_task([&]{return xl_test::boot(*machine,op);});require(!result.failed,"XL boot failed");
    if(argc==3) {
        const std::string selection=argv[2];
        if(selection=="--rich-only") {
            check_graph<cineol::xl::Graph::rich_chamber>(*engine,*machine,op);
            result=machine->run_task([&]{return check_diffusion<cineol::xl::Graph::rich_chamber>(*machine,op,*engine);});
        } else {
            const int selected=selection=="--all"?-1:std::atoi(argv[2]);
            cineol::xl::each_graph([&]<cineol::xl::Graph graph>() {
                if(selected<0 || selected==int(graph)) check_graph<graph>(*engine,*machine,op);
            });
        }
        require(!result.failed && allocations==0 && releases==0,"XL graph validation failed");return 0;
    }
    check_graph<cineol::xl::Graph::bright_hall>(*engine,*machine,op);
    check_graph<cineol::xl::Graph::dark_hall>(*engine,*machine,op);
    check_graph<cineol::xl::Graph::plate>(*engine,*machine,op);
    check_graph<cineol::xl::Graph::room>(*engine,*machine,op);
    check_graph<cineol::xl::Graph::rich_chamber>(*engine,*machine,op);
    result=machine->run_task([&]{return check_diffusion<cineol::xl::Graph::concert>(*machine,op,*engine);});
    require(!result.failed,"Concert diffusion sweep failed");
    result=machine->run_task([&]{return check_diffusion<cineol::xl::Graph::bright_hall>(*machine,op,*engine);});
    require(!result.failed,"Bright diffusion sweep failed");
    result=machine->run_task([&]{return check_diffusion<cineol::xl::Graph::dark_hall>(*machine,op,*engine);});
    require(!result.failed,"Dark diffusion sweep failed");
    result=machine->run_task([&]{return check_diffusion<cineol::xl::Graph::plate>(*machine,op,*engine);});
    require(!result.failed,"Plate diffusion sweep failed");
    result=machine->run_task([&]{return check_diffusion<cineol::xl::Graph::room>(*machine,op,*engine);});
    require(!result.failed,"Room diffusion sweep failed");
    result=machine->run_task([&]{return check_diffusion<cineol::xl::Graph::rich_chamber>(*machine,op,*engine);});
    require(!result.failed,"Rich Chamber diffusion sweep failed");
    require(allocations==0 && releases==0,"allocation/release in XL native audio");
    std::cout<<"Native XL audio: allocation/release=0; private fixtures only\n";
}
