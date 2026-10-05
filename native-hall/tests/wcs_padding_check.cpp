#include "../core/hall.hpp"
#include <emulator/wcs_access.hpp>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

namespace {
using namespace native_hall;
using namespace lexicon224x;
using namespace lexicon224x::cpu;
void require(bool ok,const char* message) {
    if(!ok){std::cerr<<message<<'\n';std::exit(1);}
}
void update_wcs(Machine& machine,const Hall& hall) {
    for(unsigned row=0;row<100;++row) {
        const int c=hall.coefficients()[row];
        machine.wcs[row]=(machine.wcs[row]&~(0xfc000000u|0x800000u|0x3fffu)) |
            uint32_t(std::abs(c))<<26 | (c<0?0x800000u:0) | uint32_t(~hall.offsets()[row]&0x3fff);
    }
}
bool state_equal(const Machine& a,const Machine& b) {
    return a.pc==b.pc && a.cpc==b.cpc && a.ACC==b.ACC && a.RR==b.RR &&
        a.operand==b.operand && a.partial==b.partial && a.partial_negative==b.partial_negative &&
        a.xreg_to_cpu==b.xreg_to_cpu && std::equal(a.R,a.R+4,b.R) &&
        std::equal(a.memory,a.memory+16384,b.memory);
}
uint32_t next_random(uint32_t& random) {
    return random=random*1664525u+1013904223u;
}
void check_sensitivity(const Machine& source) {
    auto plain=std::make_unique<Machine>(source),fault=std::make_unique<Machine>(source);
    uint32_t random=224;
    for(unsigned at=0;at<16384;++at)
        plain->memory[at]=fault->memory[at]=uint16_t(next_random(random)>>16);
    // Deliberately displace a consumed MEMR outside the allowed slots.
    // This must fail the same pass-state comparison used by the real test.
    unsigned consumed=1;
    while(consumed<94 && decode(source.wcs[consumed],Model::Lexicon224).op!=MEMR)++consumed;
    require(consumed<94,"Missing sensitivity-test memory read");
    for(unsigned row=0;row<100;++row) {
        fetch(*plain);fetch(*fault,row==consumed);
        converter_clock(*plain);converter_clock(*fault);
        if(row==0)plain->fpc.input_sample=fault->fpc.input_sample=0x7654;
        if(row==50)plain->fpc.input_sample=fault->fpc.input_sample=0x9876;
        fault->operand_held[0]=row==consumed+1;
        fault->operand_held[1]=fault->operand_held[2]=row==consumed;
        execute(*plain);execute(*fault);
    }
    require(!state_equal(*plain,*fault),"Pass-state comparison missed a consumed-row displacement");
}
}

int main(int argc,char** argv) {
    require(argc==2,"usage: native_224_wcs_padding_check PROGRAMS.bank224");
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<ProgramBank>();
    require(read_bank(bytes.data(),bytes.size(),*bank),"Invalid prepared bank");
    uint64_t total_grants=0,total_displaced=0,total_held=0;
    for(unsigned program=0;program<program_count;++program) {
        auto plain=std::make_unique<Machine>(),accessed=std::make_unique<Machine>();
        plain->model=accessed->model=Model::Lexicon224;
        std::ifstream image(std::string(argv[1])+"."+std::to_string(program)+".wcs",std::ios::binary);
        std::array<uint8_t,512> data{};
        require(bool(image.read(reinterpret_cast<char*>(data.data()),data.size())),"Missing private WCS fixture");
        load_wcs(*plain,data.data());load_wcs(*accessed,data.data());
        check_sensitivity(*plain);
        auto hall=std::make_unique<Hall>();hall->prepare(*bank,program);
        Scheduler scheduler;scheduler.timing=timing_224;
        WcsAccess access(scheduler,*accessed);access.model=Model::Lexicon224;
        WcsAccess::Request* pending=nullptr;
        Tick request_after=timing_224.marker(2);
        uint32_t random=17;
        std::array<uint64_t,100> grants{};
        std::array<uint64_t,4> lanes{};
        uint64_t displaced=0,held=0,outputs=0;
        bool saw_operand_difference=false;
        int16_t left=0,right=0;
        // 12,000 20.48 kHz passes, including full-scale noise and a silent
        // tail. 47 coefficient/address sets and changing fractional taps.
        const auto phase=[&](RowPhase kind,uint64_t absolute_row) {
            const unsigned row=unsigned(absolute_row%100);
            const unsigned frame=unsigned(absolute_row/100);
            switch(kind) {
            case RowPhase::Begin: {
                if(row==0) {
                    if(frame%257==0) {
                        const unsigned n=frame/257;Controls c;
                        c.bass=1+(n*7)%31;c.mid=(n*11)%32;c.crossover=(n*13)%32;
                        c.treble=(n*17)%32;c.depth=(n*19)%72;c.diffusion=1+(n*23)%63;
                        c.predelay_ms=predelay_minima[program]+int((n*29)%129);
                        hall->set_controls(c);
                    }
                    if(frame%3==0)hall->advance_modulation();
                    update_wcs(*plain,*hall);update_wcs(*accessed,*hall);
                    left=int16_t(next_random(random)>>16);right=int16_t(next_random(random)>>16);
                    if(frame>=9000)left=right=0;
                }
                if(pending && pending->scheduled) {
                    request_after=pending->sampled_at+cpu_period*(7+next_random(random)%170);
                    pending=nullptr;
                }
                if(!pending && scheduler.now()>=request_after) {
                    // An unchanged padding word isolates arbitration from
                    // coefficient payload changes. All four byte lanes use
                    // the reference board's actual grant/commit windows.
                    const unsigned lane=(next_random(random)>>16)&3;
                    const uint8_t value=uint8_t(~(accessed->wcs[98]>>(8*lane)));
                    pending=&access.on_request(false,98,lane,value,scheduler.now()-2*cpu_period);
                    ++lanes[lane];
                }
                const bool was_scheduled=pending && pending->scheduled;
                access.at_marker(accessed->microinstruction,true);
                if(pending && pending->scheduled && !was_scheduled)++grants[row];
                break;
            }
            case RowPhase::ExecutePrevious: {
                const auto marker=timing_224.marker(absolute_row+1);
                for(unsigned edge=0;edge<3;++edge) {
                    accessed->operand_held[edge]=access.held(timing_224.aruck_edge(marker,edge));
                    held+=accessed->operand_held[edge];
                }
                execute(*plain);execute(*accessed);
                saw_operand_difference|=plain->operand!=accessed->operand;
                require(plain->ACC==accessed->ACC && plain->RR==accessed->RR,
                    "WCS arbitration changed arithmetic output at a row");
                require(std::equal(plain->R,plain->R+4,accessed->R) &&
                    plain->xreg_to_cpu==accessed->xreg_to_cpu,"WCS arbitration changed register/XREG data");
                if(row==99)require(state_equal(*plain,*accessed),"WCS arbitration changed pass-end DSP state");
                break;
            }
            case RowPhase::Fetch: {
                require(plain->pc==row && accessed->pc==row,"DSP scan lost its 100-row boundary");
                const bool skip=access.displaced(timing_224.marker(absolute_row));
                if(skip) {
                    ++displaced;
                    const auto mi=decode(accessed->wcs[row],Model::Lexicon224);
                    require(mi.op==NOP && mi.coefficient==0 && !mi.xfer && !mi.zero &&
                        !mi.reset && !mi.keep_shifting,"Grant displaced a consumed DSP operation");
                }
                const auto previous=accessed->microinstruction;
                fetch(*plain);fetch(*accessed,skip);
                access.at_fetch(previous,accessed->microinstruction);
                break;
            }
            case RowPhase::Converter:
                converter_clock(*plain);converter_clock(*accessed);
                if(row==0)plain->fpc.input_sample=accessed->fpc.input_sample=uint16_t(left);
                if(row==50)plain->fpc.input_sample=accessed->fpc.input_sample=uint16_t(right);
                require(plain->dac_channels==accessed->dac_channels &&
                    plain->dac_code==accessed->dac_code && plain->dac_gain==accessed->dac_gain,
                    "WCS arbitration changed a DAC capture");
                outputs+=plain->dac_channels!=0;
                break;
            case RowPhase::ResetDecode:break;
            }
        };
        const auto end=timing_224.marker(1200000)+timing_224.execute_offset;
        while(scheduler.step_one(end,phase,[]{})){}
        uint64_t count=0;
        for(unsigned row=0;row<100;++row) {
            if(!grants[row])continue;
            count+=grants[row];
            const unsigned network=program_networks[program];
            require(network==1?(row==47 || row==70 || row==94):network==2?row==95:(row==47 || row==96),
                "Unexpected WCS grant slot");
        }
        const unsigned network=program_networks[program];
        require(network==1?(grants[47] && grants[70] && grants[94]):network==2?grants[95]:(grants[47] && grants[96]),
            "An allowed WCS slot was not exercised");
        require(std::all_of(lanes.begin(),lanes.end(),[](uint64_t n){return n>100;}),"Byte-lane coverage too small");
        require(saw_operand_difference && held==displaced*3 && access.writes_committed()==count,
            "Arbitration perturbation or commit coverage missing");
        require(state_equal(*plain,*accessed),"Final delay/ARU state differs");
        total_grants+=count;total_displaced+=displaced;total_held+=held;
        std::cout<<program_names[program]<<": passes=12000, grants="<<count<<", displaced="<<displaced
            <<", held_edges="<<held<<", DAC captures="<<outputs<<", audio/state exact\n";
    }
    std::cout<<"All-six unchanged-byte writes: grants="<<total_grants<<", displaced="<<total_displaced
        <<", held_edges="<<total_held<<"; zero audio/arithmetic/pass-state differences\n";
}
