#include "../desktop/runtime.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstdlib>

static void require(bool ok,const char* message) {if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
using namespace cineol::xl;
static void check_hf(const Bank& bank) {
    unsigned checked=0;
    for(unsigned program=0;program<graphs.size();++program) {
        const auto& data=bank.programs[program];
        for(unsigned page=0;page<data.page_count;++page) for(unsigned slot=0;slot<6;++slot) {
            const auto cell=data.pages[page].cells[slot];
            const std::string name=data.pages[page].names[slot].data();
            if(name.find("TREB")==std::string::npos && name.find("HF B")==std::string::npos)continue;
            auto raw=data.controls.factory;raw[cell]=255;data.resolve_controls(raw);
            std::cout<<graphs[program].name<<" "<<name<<" max="<<unsigned(raw[cell])<<std::endl;
            require(raw[cell]==254,"HF maximum still reaches the firmware's mute sentinel");
            raw[42]&=uint8_t(~1u);
            // Independently rendered upper values must be the same open filter.
            // All four outputs are checked so a split routing cannot hide silence.
            std::vector<std::array<float,4>> reference;
            for(unsigned value:{254u,255u}) {
                Runtime engine;engine.select(bank,program);raw[cell]=uint8_t(value);data.resolve_controls(raw);
                engine.controls(raw,false,0,1,1,0,2,false);
                uint32_t random=17;double active=0,tail=0;
                for(unsigned n=0;n<96000;++n) {
                    random=random*1664525+1013904223;
                    const float x=n>=24000 && n<72000 ? (float(random>>8)/16777216.f*2-1)*.03f:0;
                    float l,r;std::array<float,4> outputs{};engine.process(x,x,l,r,true,&outputs);
                    if(value==254)reference.push_back(outputs);
                    else require(outputs==reference[n],"HF host maximum differs from physical upper endpoint");
                    for(float y:outputs) {
                        require(std::isfinite(y) && std::abs(y)<2,"HF endpoint is nonfinite or unstable");
                        if(n>=48000 && n<72000)active+=double(y)*y;
                        if(n>=72000)tail+=double(y)*y;
                    }
                }
                std::cout<<" raw="<<value<<" signal_energy="<<active<<" silent_tail_energy="<<tail<<'\n';
                require(active>1e-8,"HF upper endpoint lost the expected signal");
            }
            ++checked;
        }
    }
    require(checked>=40,"HF regression did not cover the reverb catalog");
    std::cout<<"HF endpoint audio: "<<checked<<" controls, physical/host maximum identical\n";
}
static void check_access(const Bank& bank) {
    for(unsigned program:{14u,15u,16u}) {
        for(unsigned cell:{6u,7u})require(bank.programs[program].control_active(cell,Graph(program)),"effect voices 1/2 hidden by stop-decay alias");
        require(!bank.programs[program].control_active(45,Graph(program)),"effect incorrectly exposes Dynamic Decay stop delay");
    }
    require(bank.programs[9].control_active(1,Graph::inverse_room),"Inverse Room's dependent Mid Decay is hidden");
    for(unsigned program:{11u,12u})require(bank.programs[program].control_active(0,Graph(program)),"CD Plate's dependent LF Decay is hidden");
    // These two programs use Chorus for monitor cadence, without moving taps.
    require(bank.programs[13].control_active(8,Graph::rich_plate) && bank.programs[21].control_active(14,Graph::rich_split),"Rich Chorus remains hidden");
    require(!bank.programs[9].control_active(8,Graph::inverse_room),"unrelated zero-target Chorus changed");
    for(unsigned i=0;i<bank.programs.size();++i)require(!bank.programs[i].control_active(255,Graph(i)),"parked slot became active");
    std::cout<<"XL control eligibility: voices 1/2 and dependent LF/Mid, genuine unsupported slots pass\n";
}
static void check_depth(const Bank& bank) {
    unsigned checked=0;
    for(unsigned program=0;program<graphs.size();++program) {
        const auto& data=bank.programs[program];
        for(unsigned page=0;page<data.page_count;++page) for(unsigned slot=0;slot<6;++slot) {
            const unsigned cell=data.pages[page].cells[slot];
            bool depth=false;for(const auto& control:data.controls.slots)
                depth|=control.cell==cell && control.kind==ControlKind::depth;
            if(!depth || !data.control_active(cell,Graph(program)))continue;
            auto raw=data.controls.factory;raw[42]&=uint8_t(~0xc1u);
            require(data.control_maximum(page,slot,raw)==254,"Attack/Depth UI reaches the mute sentinel");
            for(unsigned value=0;value<256;++value) {
                auto resolved=raw;resolved[cell]=uint8_t(value);data.resolve_controls(resolved);
                require(resolved[cell]==std::min(value,254u),"Attack/Depth resolution changed a valid fader value");
            }
            std::vector<std::array<float,4>> reference;reference.reserve(192000);
            for(unsigned value:{254u,255u}) {
                Runtime engine;engine.select(bank,program);raw[cell]=uint8_t(value);data.resolve_controls(raw);
                engine.controls(raw,false,0,1,1,0,2,false);
                double signal=0,tail=0;uint32_t random=17;
                for(unsigned n=0;n<192000;++n) {
                    random=random*1664525u+1013904223u;
                    const float input=n>=24000 && n<120000?.03f*float(int32_t(random))/2147483648.f:0;
                    float l,r;std::array<float,4> quad{};engine.process(input,input*.7f,l,r,true,&quad);
                    if(value==254)reference.push_back(quad);
                    else require(quad==reference[n],"Attack/Depth host maximum differs from physical maximum");
                    for(float sample:quad) {
                        require(std::isfinite(sample) && std::abs(sample)<2,"Attack/Depth endpoint is nonfinite or unstable");
                        if(n>=72000 && n<120000)signal+=double(sample)*sample;
                        if(n>=120000)tail+=double(sample)*sample;
                    }
                }
                require(signal>1e-6 && tail>1e-8,"Attack/Depth maximum lost expected signal or tail");
            }
            ++checked;std::cout<<graphs[program].name<<" "<<data.pages[page].names[slot].data()
                <<": 256 resolutions, four-output 254/255 audio exact\n"<<std::flush;
        }
    }
    require(checked>=19,"Attack/Depth catalog coverage missing");
    std::cout<<"Attack/Depth endpoints: "<<checked<<" controls pass\n";
}
int main(int argc,char** argv) {
    require(argc==3,"usage: cineol_xl_control_regression_check BANK --hf|--depth|--access");
    std::ifstream file(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<Bank>();require(read_bank(bytes.data(),bytes.size(),*bank),"invalid XL bank");
    const std::string option=argv[2];require(option=="--hf" || option=="--depth" || option=="--access","invalid regression selection");
    if(option=="--hf")check_hf(*bank);else if(option=="--depth")check_depth(*bank);else check_access(*bank);
}
