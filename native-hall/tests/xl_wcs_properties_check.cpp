// Establish fixed T&C protect/reset graph properties through physical keys
// and Size faders. No WCS words, coefficients or addresses are exported here.
#include "xl_reference.hpp"
#include "../desktop/bank.hpp"
#include <memory>
using namespace lexplug;using namespace lexplug::op;using namespace cineol::xl;
using xl_test::require;
static Task<void> mod_key(Machine& m,LarcOperator& op,bool enabled) {
    for(unsigned n=0;n<8;++n) {
        co_await op.setToggle(1,enabled);co_await m.sleep(0.2);
        if(bool(m.peek(uint16_t(op.recordBase()+42))&64)==enabled)co_return;
    }
    co_await fail("XL graph-property Mod key did not settle");
}
static Task<void> size_keys(Machine& m,LarcOperator& op,const ProgramData& data,unsigned first,unsigned second) {
    unsigned found=0;
    for(unsigned p=0;p<data.page_count;++p)for(unsigned slot=0;slot<6;++slot) {
        const unsigned cell=data.pages[p].cells[slot];
        if(cell==43 || cell==44){co_await op.moveSlider(int(p+1),slot,cell==43?first:second);++found;}
    }
    if(!found || !(co_await op.recordSettled()))co_await fail("XL graph-property Size controls did not settle");
    co_await m.sleep(0.3);
}
int main(int argc,char** argv) {
    require(argc==4,"usage: cineol_xl_wcs_properties_check ROM_DIRECTORY BANK OUTPUT_CSV");
    const auto output=std::filesystem::weakly_canonical(argv[3]);
    const auto relative=output.lexically_relative(std::filesystem::weakly_canonical(std::filesystem::current_path()/"build"));
    require(!relative.empty() && !relative.is_absolute() && *relative.begin()!="..","private property evidence must stay under repository build/");
    require(!std::filesystem::exists(output),"property output already exists");
    std::ifstream file(argv[2],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<Bank>();require(read_bank(bytes.data(),bytes.size(),*bank),"invalid XL property bank");
    std::ofstream csv(output);require(bool(csv),"cannot open XL graph-property output");
    csv<<"program,fixture,row,protect,reset\n";
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);
    auto machine=std::make_unique<Machine>(*engine);LarcOperator op(*machine);
    require(!machine->run_task([&]{return xl_test::boot(*machine,op);}).failed,"XL property boot failed");
    unsigned fixtures=0,rows=0;
    for(unsigned index=0;index<graphs.size();++index) {
        const auto info=graphs[index];const auto& data=bank->programs[index];
        require(!machine->run_task([&]{return xl_test::select(*machine,op,info.bank,info.program);}).failed,"XL property selection failed");
        std::array<uint8_t,128> baseline{};bool initial=true;
        auto capture=[&](const char* fixture) {
            const auto& source=*engine->host().dsp;xl_test::ShapeCheck shape{source,Graph(index)};shape.run();
            require(shape.valid,"XL property graph shape differs");
            for(unsigned row=0;row<info.rows;++row) {
                const auto mi=lexicon224x::decode(source.wcs[row]);const uint8_t flags=uint8_t(mi.protect|(mi.reset<<1));
                if(initial)baseline[row]=flags;else require(baseline[row]==flags,"XL protect/reset changes with Mod/Size; fixed scheduling metadata unsafe");
                csv<<index<<','<<fixture<<','<<row<<','<<mi.protect<<','<<mi.reset<<'\n';++rows;
            }
            initial=false;++fixtures;
        };
        capture("factory_mod_off");
        require(!machine->run_task([&]{return mod_key(*machine,op,true);}).failed,"XL property Mod on failed");capture("factory_mod_on");
        if(data.controls.size.enabled)for(const auto endpoints:{std::array<unsigned,2>{2,254},{254,2},{130,130}}) {
            require(!machine->run_task([&]{return size_keys(*machine,op,data,endpoints[0],endpoints[1]);}).failed,"XL property Size edge failed");
            const auto label=std::to_string(endpoints[0])+"_"+std::to_string(endpoints[1]);capture(label.c_str());
        }
        std::cout<<info.name<<": protect/reset invariant across physical Mod and available Size fixtures\n"<<std::flush;
    }
    require(bool(csv),"cannot finish XL graph-property output");
    std::cout<<"22 physical programs, "<<fixtures<<" fixtures, "<<rows<<" row properties; no WCS payload export\n";
}
