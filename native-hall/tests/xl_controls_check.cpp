#include "../desktop/bank.hpp"
#include "xl_reference.hpp"
#include <memory>
#include <new>
static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n) {if(tracking) ++allocations;if(void* p=std::malloc(n?n:1)) return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {if(tracking && p) ++releases;std::free(p);}
void operator delete(void* p,size_t) noexcept {::operator delete(p);}
void* operator new[](size_t n) {return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
using namespace lexplug;using namespace lexplug::op;using xl_test::require;
using namespace cineol::xl;
static bool static_kind(ControlKind kind) {
    return kind==ControlKind::low_decay || kind==ControlKind::mid_decay || kind==ControlKind::filter ||
        kind==ControlKind::depth || kind==ControlKind::level || kind==ControlKind::pan ||
        kind==ControlKind::predelay || kind==ControlKind::delay ||
        kind==ControlKind::diffusion || kind==ControlKind::definition;
}
template<Graph graph>
static void check_static(Engine& engine,Machine& machine,LarcOperator& op,const Bank& bank) {
    constexpr auto info=graph_info(graph);const auto& data=bank.programs[unsigned(graph)];
    auto result=machine.run_task([&]{return xl_test::select(machine,op,info.bank,info.program);});
    require(!result.failed,"control program selection failed");
    auto& host=engine.host();unsigned cases=0;
    for(unsigned page=0;page<data.page_count;++page) for(unsigned slider=0;slider<6;++slider) {
        const unsigned cell=data.pages[page].column*6+slider;
        bool active=false;
        for(const auto& slot:data.controls.slots) if(slot.cell==cell && static_kind(slot.kind) && slot.groups && (slot.group[0].count || (slot.kind==ControlKind::definition && data.controls.feedback.count))) active=true;
        if(!active) continue;
        for(unsigned raw:{2u,66u,130u,250u}) {
            result=machine.run_task([&]()->Task<void>{co_await op.moveSlider(int(page+1),slider,raw);co_await machine.sleep(0.1);});
            require(!result.failed,"control fixture failed");
            NetworkSettings<128> predicted;
            for(unsigned row=0;row<info.rows;++row) {
                predicted.coefficients[row]=data.coefficients[row];
                predicted.offsets[row]=data.offsets[row];
            }
            std::array<uint8_t,48> values;std::copy_n(host.memory.begin()+0x3ca3,values.size(),values.begin());
            tracking=true;data.controls.apply_size(predicted,values);data.controls.apply_static(predicted,values);tracking=false;
            for(unsigned row=0;row<info.rows;++row) {
                bool interpolation=false;
                for(unsigned tap=0;tap<(data.modulation.flags&15);++tap)
                    interpolation|=row==data.modulation.rows[tap] || row==unsigned(data.modulation.rows[tap])+1;
                if(interpolation) continue;
                const auto mi=lexicon224x::decode(host.dsp->wcs[row]);
                const int wanted=mi.negative?-int(mi.coefficient):int(mi.coefficient);
                if(predicted.coefficients[row]!=wanted || ((mi.op==lexicon224x::MEMR || mi.op==lexicon224x::MEMW) && predicted.offsets[row]!=uint16_t(~mi.low))) {
                    std::cerr<<info.name<<" complete compiler page="<<page+1<<" slider="<<slider<<" row="<<row
                        <<" native="<<int(predicted.coefficients[row])<<" ROM="<<wanted
                        <<" offset="<<predicted.offsets[row]<<" ROM-offset="<<uint16_t(~mi.low)<<'\n';
                    require(false,"complete native control settings differ from firmware");
                }
            }
            for(const auto& slot:data.controls.slots) if(static_kind(slot.kind)) for(unsigned g=0;g<slot.groups;++g)
                for(unsigned t=0;t<slot.group[g].count;++t) {
                    const auto& target=slot.group[g].targets[t];
                    const unsigned count=slot.kind==ControlKind::diffusion || slot.kind==ControlKind::definition?3:slot.kind==ControlKind::mid_decay && (target.meta&64)?2:1;
                    for(unsigned part=0;part<count;++part) {
                        const unsigned row=slot.kind==ControlKind::diffusion || slot.kind==ControlKind::definition?target.row+part:part?target.second_row:target.row;
                        const auto mi=lexicon224x::decode(host.dsp->wcs[row]);
                        const int wanted=mi.negative?-int(mi.coefficient):int(mi.coefficient);
                        const bool delay=slot.kind==ControlKind::predelay || slot.kind==ControlKind::delay;
                        const unsigned wanted_offset=uint16_t(~mi.low);
                        if(predicted.coefficients[row]!=wanted || (delay && predicted.offsets[row]!=wanted_offset)) {
                            std::cerr<<info.name<<" page="<<page+1<<" slider="<<slider<<" physical="<<raw<<" cell="<<cell
                                <<" kind="<<unsigned(slot.kind)<<" owner="<<unsigned(slot.cell)<<" group="<<g<<" target="<<t<<" row="<<row
                                <<" native="<<int(predicted.coefficients[row])<<" ROM="<<wanted<<" meta="<<unsigned(target.meta)
                                <<" raw="<<unsigned(values[slot.cell])<<" offset="<<predicted.offsets[row]<<" ROM-offset="<<wanted_offset<<'\n';
                            require(false,"native XL static compiler differs");
                        }
                    }
                }
            for(unsigned target=0;target<data.controls.feedback.count;++target) for(unsigned part=0;part<3;++part) {
                const unsigned row=data.controls.feedback.targets[target].row+part;
                const auto mi=lexicon224x::decode(host.dsp->wcs[row]);const int wanted=mi.negative?-int(mi.coefficient):int(mi.coefficient);
                if(predicted.coefficients[row]!=wanted) {
                    std::cerr<<info.name<<" feedback page="<<page+1<<" slider="<<slider<<" raw="<<raw<<" row="<<row
                        <<" native="<<int(predicted.coefficients[row])<<" ROM="<<wanted
                        <<" Definition cell="<<unsigned(data.controls.definition_cell)<<" limit="<<unsigned(data.controls.feedback_limit)
                        <<" ROM-limit="<<unsigned(host.memory[0x3e0b])<<" ROM-indices="<<unsigned(host.memory[0x3e3e])<<'/'<<unsigned(host.memory[0x3e3f])<<'\n';
                    require(false,"native feedback compiler differs");
                }
            }
            ++cases;
        }
    }
    std::cout<<info.name<<" native static compiler: "<<cases<<" composed fader fixtures exact\n"<<std::flush;
}
template<Graph graph>
static void check_size(Engine& engine,Machine& machine,LarcOperator& op,const Bank& bank) {
    constexpr auto info=graph_info(graph);const auto& data=bank.programs[unsigned(graph)];
    if(!data.controls.size.enabled) return;
    auto result=machine.run_task([&]{return xl_test::select(machine,op,info.bank,info.program);});
    require(!result.failed,"Size program selection failed");
    auto& host=engine.host();unsigned cases=0;
    for(unsigned page=0;page<data.page_count;++page) for(unsigned slider=0;slider<6;++slider) {
        if(std::strncmp(data.pages[page].names[slider].data(),"SIZE",4)!=0) continue;
        for(unsigned raw:{2u,66u,130u,250u}) {
            result=machine.run_task([&]()->Task<void>{co_await op.moveSlider(int(page+1),slider,raw);co_await machine.sleep(1);});
            require(!result.failed,"Size fixture failed");
            std::array<uint8_t,48> values;std::copy_n(host.memory.begin()+0x3ca3,values.size(),values.begin());
            const auto layout=data.controls.layout(values);
            auto word=[&](unsigned a){return unsigned(host.memory[a])|unsigned(host.memory[a+1])<<8;};
            if(layout.scales[0]!=host.memory[0x3ce9] || layout.scales[1]!=host.memory[0x3cea] ||
               layout.lengths[0]!=word(0x3ce5) || layout.lengths[1]!=word(0x3ce7)) {
                std::cerr<<info.name<<" Size page="<<page+1<<" raw="<<raw<<" factors="<<unsigned(layout.scales[0])<<'/'<<unsigned(layout.scales[1])
                    <<" ROM="<<unsigned(host.memory[0x3ce9])<<'/'<<unsigned(host.memory[0x3cea])<<" lengths="<<layout.lengths[0]<<'/'<<layout.lengths[1]
                    <<" ROM="<<word(0x3ce5)<<'/'<<word(0x3ce7)<<'\n';
                require(false,"native Size memory layout differs");
            }
            NetworkSettings<128> predicted;
            for(unsigned row=0;row<info.rows;++row) {
                const auto mi=lexicon224x::decode(host.dsp->wcs[row]);
                predicted.coefficients[row]=int8_t(mi.negative?-int(mi.coefficient):int(mi.coefficient));
                predicted.offsets[row]=uint16_t(~mi.low);
            }
            tracking=true;data.controls.apply_size(predicted,values);data.controls.apply_static(predicted,values);tracking=false;
            for(unsigned row=0;row<info.rows;++row) {
                const auto mi=lexicon224x::decode(host.dsp->wcs[row]);
                if(mi.op!=lexicon224x::MEMR && mi.op!=lexicon224x::MEMW) continue;
                const unsigned wanted=uint16_t(~mi.low);
                if(predicted.offsets[row]!=wanted) {
                    std::cerr<<info.name<<" Size page="<<page+1<<" raw="<<raw<<" row="<<row
                        <<" offset="<<predicted.offsets[row]<<" ROM="<<wanted<<'\n';
                    require(false,"native Size DSP address differs");
                }
            }
            ++cases;
        }
    }
    std::cout<<info.name<<" native Size: "<<cases<<" memory layout/address fixtures exact\n"<<std::flush;
}
int main(int argc,char** argv) {
    require(argc==3 || argc==4,"usage: cineol_xl_controls_check ROM_DIRECTORY PREPARED_BANK [PROGRAM_INDEX]");
    std::ifstream input(argv[2],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    auto bank=std::make_unique<Bank>();require(read_bank(bytes.data(),bytes.size(),*bank),"invalid controls bank");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);
    auto machine=std::make_unique<Machine>(*engine);LarcOperator op(*machine);
    auto result=machine->run_task([&]{return xl_test::boot(*machine,op);});require(!result.failed,"boot failed");
    const int selected=argc==4?std::atoi(argv[3]):-1;
    each_graph([&]<Graph graph>() {if(selected<0 || selected==int(graph)) {check_static<graph>(*engine,*machine,op,*bank);check_size<graph>(*engine,*machine,op,*bank);}});
    require(allocations==0 && releases==0,"native controls allocated/released memory");
    std::cout<<"Native control compiler allocation/release=0\n";
}
