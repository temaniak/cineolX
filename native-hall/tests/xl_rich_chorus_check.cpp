#include "../desktop/runtime.hpp"
#include "../desktop/control_timing_xl.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <cstdlib>

using namespace cineol::xl;
static void require(bool ok,const char* message) {
    if(!ok) {std::cerr<<message<<'\n';std::exit(1);}
}
static void check_work() {
    ModulationProfile profile;profile.flags=32;
    for(unsigned index=0;index<32;++index) {
        profile.set_chorus(uint8_t(index*8));
        Modulator mod;mod.reset(ModulationState{});NetworkSettings<128> settings;
        const unsigned calls=unsigned(profile.period)*8*profile.hold;
        uint64_t work=0;
        for(unsigned n=0;n<calls;++n) {
            work+=modulation_work_cycles(profile,mod.state(),true,[](unsigned,ControlByteXL){return 0u;});
            mod.step(profile,settings);
        }
        require(std::abs(double(work)/calls-rich_chorus_work(uint8_t(index*8),true,profile.hold))<1e-9,
            "Rich scan budget differs from independently checked AD5C work law");
        require(rich_chorus_work(uint8_t(index*8),false,profile.hold)==1127,"Mod OFF is not a fixed delay");
    }
}
static std::vector<float> render(const Bank& bank,unsigned program,unsigned chorus,bool mod) {
    auto runtime=std::make_unique<Runtime>();runtime->select(bank,program);
    const auto& data=bank.programs[program];auto raw=data.controls.factory;
    raw[data.pages[data.chorus_page-1].cells[data.chorus_slot]]=uint8_t(chorus);
    const unsigned stop=data.dynamics.shared_stop?6:12;
    raw[stop]=raw[stop+1]=18;raw[45]=10;raw[42]|=1;
    runtime->controls(raw,mod,0,1,1,0,2,true);
    std::vector<float> audio;audio.reserve(48000*6*2);uint32_t random=17;
    for(unsigned n=0;n<48000*6;++n) {
        random=random*1664525+1013904223;
        const float x=n>=48000 && n<3*48000?(float(random>>8)/16777216.f*2-1)*.03f:0;
        float l,r;runtime->process(x,x*.7f,l,r,true);
        require(std::isfinite(l)&&std::isfinite(r),"non-finite Rich output");audio.push_back(l);audio.push_back(r);
    }
    return audio;
}
int main(int argc,char** argv) {
    require(argc==2,"usage: cineol_xl_rich_chorus_check XL_BANK");check_work();
    std::ifstream file(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<Bank>();require(read_bank(bytes.data(),bytes.size(),*bank),"invalid XL bank");
    for(unsigned program:{13u,21u}) {
        const auto& data=bank->programs[program];const auto graph=Graph(program);
        const unsigned cell=data.pages[data.chorus_page-1].cells[data.chorus_slot];
        require(data.control_active(cell,graph)&&data.mode_enhancement_active(graph),"Rich Chorus/Mod hidden");
        auto low=render(*bank,program,0,true),high=render(*bank,program,128,true);
        double delta=0;for(unsigned n=3*48000*2;n<low.size();++n)delta+=std::abs(low[n]-high[n]);
        require(delta>1e-4,"Chorus still does not affect Dynamic/Optimization tail timing");
        require(high==render(*bank,program,248,true),"upper-half step invented moving taps in Rich program");
        require(render(*bank,program,0,false)==render(*bank,program,248,false),"Chorus changes timing with Mod OFF");
        std::cout<<"Rich program "<<program<<": active controls, finite tail, Chorus-dependent timing, fixed upper half and Mod-OFF invariance pass\n";
    }
    std::cout<<"32-position AD5C work budget check passed\n";
}
