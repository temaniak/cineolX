#include "../desktop/bank.hpp"
#include "xl_reference.hpp"
#include "../import/xl_display.hpp"
#include <memory>
#include <string>
using namespace cineol::xl;
using namespace lexplug;using namespace lexplug::op;using xl_test::require;
static double numeric(const char* text) {
    double value=std::strtod(text,nullptr);
    if(std::strstr(text,"kHz")) value*=1000;
    return value;
}
static void check_page_values(Engine& engine,Machine& machine,LarcOperator& op,const ProgramData& data,unsigned& cases) {
    auto& host=engine.host();
    std::array<uint8_t,48> raw;std::copy_n(host.memory.begin()+0x3ca3,raw.size(),raw.begin());
    auto resolved=raw;data.resolve_controls(resolved);
    for(unsigned page=0;page<data.page_count;++page) {
        auto result=machine.run_task([&]()->Task<void>{co_await op.gotoPage(int(page+1));});
        require(!result.failed,"display page fixture failed");
        for(unsigned slot=0;slot<6;++slot) {
            const unsigned cell=data.pages[page].cells[slot];
            if(cell>=48 || !data.control_active(cell)) continue;
            require(resolved[cell]==raw[cell],"native logical bounds changed a calibrated firmware value");
            const auto actual=data.display_value(page,slot,resolved);
            const auto expected=cineol::xl::import::display_value(host.memory,slot,cell,raw[cell]);
            const bool infinity=std::strncmp(expected.data(),"--",2)==0;
            const double tolerance=std::strstr(expected.data()," s")?0.051:0.00001;
            if(infinity?std::strncmp(actual.data(),"--",2)!=0:std::abs(numeric(actual.data())-numeric(expected.data()))>tolerance) {
                std::cerr<<"XL display "<<data.pages[page].names[slot].data()<<" native="<<actual.data()<<" ROM="<<expected.data()<<'\n';
                require(false,"native display value differs from firmware");
            }
            ++cases;
        }
    }
}
int main(int argc,char** argv) {
    require(argc==3 || argc==4,"usage: cineol_xl_display_check ROM_DIRECTORY PREPARED_BANK [PROGRAM_INDEX]");
    std::ifstream input(argv[2],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    auto bank=std::make_unique<Bank>();require(read_bank(bytes.data(),bytes.size(),*bank),"invalid display bank");
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);
    auto machine=std::make_unique<Machine>(*engine);LarcOperator op(*machine);
    auto result=machine->run_task([&]{return xl_test::boot(*machine,op);});require(!result.failed,"boot failed");
    const int selected=argc==4?std::atoi(argv[3]):-1;
    for(unsigned g=0;g<graphs.size();++g) {
        if(selected>=0 && selected!=int(g)) continue;
        const auto& data=bank->programs[g];const auto info=graphs[g];unsigned cases=0;
        result=machine->run_task([&]{return xl_test::select(*machine,op,info.bank,info.program);});require(!result.failed,"display program selection failed");
        check_page_values(*engine,*machine,op,data,cases);
        if(data.controls.size.enabled) for(unsigned page=0;page<data.page_count;++page) for(unsigned slot=0;slot<6;++slot) {
            if(data.pages[page].cells[slot]!=43 && data.pages[page].cells[slot]!=44) continue;
            for(unsigned raw:{2u,66u,130u,250u}) {
                result=machine->run_task([&]()->Task<void>{co_await op.moveSlider(int(page+1),slot,raw);co_await machine->sleep(1);});
                require(!result.failed,"display Size fixture failed");check_page_values(*engine,*machine,op,data,cases);
            }
        }
        std::cout<<info.name<<" native display/bounds: "<<cases<<" logical values exact\n"<<std::flush;
    }
}
