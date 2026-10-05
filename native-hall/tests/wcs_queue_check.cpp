#include "../desktop/control_write_queue224.hpp"
#include <isa-level-cpp/lexicon224x.hpp>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

using namespace native_hall;
using namespace lexicon224x;
namespace {
void require(bool ok,const char* message) {if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
struct Event {uint64_t visible=0;ControlWrite224 write;};
void apply(Machine& m,ControlWrite224 write) {
    auto& word=m.wcs[write.row];
    if(write.kind==ControlWrite224::Kind::AddressLow)
        word=(word&~255u)|uint32_t(uint8_t(~write.value));
    else {
        const int value=int8_t(write.value);
        word=(word&~(0xfc000000u|0x800000u))|uint32_t(std::abs(value))<<26|(value<0?0x800000u:0);
    }
}
}
int main(int argc,char** argv) {
    require(argc==2,"usage: native_224_wcs_queue_check PROGRAMS.bank224");
    std::ifstream file(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<ProgramBank>();require(read_bank(bytes.data(),bytes.size(),*bank),"Invalid bank");
    uint64_t total_writes=0;
    for(unsigned program=0;program<program_count;++program) {
        auto hall=std::make_unique<Hall>();hall->prepare(*bank,program);
        Controls controls;controls.predelay_ms=predelay_minima[program];hall->set_controls(controls);
        auto reference=std::make_unique<Machine>();reference->model=Model::Lexicon224;
        std::ifstream image(std::string(argv[1])+"."+std::to_string(program)+".wcs",std::ios::binary);
        std::array<uint8_t,512> data{};require(bool(image.read(reinterpret_cast<char*>(data.data()),512)),"Missing WCS fixture");
        load_wcs(*reference,data.data());
        for(unsigned row=0;row<100;++row) {
            const int c=hall->coefficients()[row];
            reference->wcs[row]=(reference->wcs[row]&~(0xfc000000u|0x800000u|0x3fffu)) |
                uint32_t(std::abs(c))<<26|(c<0?0x800000u:0)|uint32_t(~hall->offsets()[row]&0x3fff);
        }
        ControlWriteQueue224 queue;std::array<Event,12> events{};unsigned index=events.size(),peak=0;
        uint32_t random=224;const auto next=[&](){return random=random*1664525u+1013904223u;};
        for(unsigned frame=0;frame<12000;++frame) {
            if(frame%4==0) {
                require(queue.size()==0 && index==events.size(),"Pending fixture group did not finish");
                for(unsigned i=0;i<events.size();++i) {
                    auto& e=events[i];e.write.kind=i%4==0?ControlWrite224::Kind::AddressLow:ControlWrite224::Kind::Coefficient;
                    e.write.row=uint8_t(bank->programs[program].loop_rows[i/3]+i%3);
                    if(e.write.kind==ControlWrite224::Kind::AddressLow) {
                        const auto& d=bank->programs[program].modulation_descriptors;
                        e.write.row=uint8_t(127-((unsigned(d[0])|(unsigned(d[1])<<8))-0x4000)/4+(i&1));
                        e.write.value=uint8_t(next()>>16);
                    } else {
                        const int magnitude=1+int((next()>>16)%31);
                        e.write.value=uint8_t(hall->coefficients()[e.write.row]<0?-magnitude:magnitude);
                    }
                    // Explicitly before, at and after the target's fetch;
                    // multiple fields/updates can share one audio pass.
                    e.visible=uint64_t(frame)*100+100+e.write.row+i%3-1;
                }
                std::sort(events.begin(),events.end(),[](const Event& a,const Event& b){return a.visible<b.visible;});
                for(const auto& e:events)queue.push(e.visible,e.write);
                peak=std::max(peak,queue.size());index=0;total_writes+=events.size();
            }
            const auto left=int16_t(next()>>16),right=int16_t(next()>>16);
            int16_t expected[4]{},actual[4]{};
            for(unsigned row=0;row<100;++row) {
                const uint64_t fetch=uint64_t(frame)*100+row;
                while(index<events.size() && events[index].visible<=fetch)apply(*reference,events[index++].write);
                lexicon224x::fetch(*reference);converter_clock(*reference);
                if(row==0)reference->fpc.input_sample=uint16_t(left);
                if(row==50)reference->fpc.input_sample=uint16_t(right);
                if(reference->mi.wr_da)for(unsigned channel=0;channel<4;++channel)
                    if(reference->mi.channels&(1u<<channel))expected[channel]=int16_t(source_value(*reference,reference->mi));
                execute(*reference);
            }
            queue.render(uint64_t(frame)*100,*hall,[&](){hall->process_uncontrolled(left,right,actual);});
            require(std::equal(actual,actual+4,expected),"Fetch-visible queue changed a DAC output");
            require(hall->accumulator()==reference->ACC && hall->result()==reference->RR,"Fetch-visible queue changed ARU state");
            require(std::equal(hall->memory().begin(),hall->memory().end(),reference->memory,
                [](int16_t a,uint16_t b){return uint16_t(a)==b;}),"Fetch-visible queue changed delay memory");
        }
        require(!queue.size() && peak==ControlWriteQueue224::capacity,"Queue boundary/capacity coverage missing");
        std::cout<<program_names[program]<<": 12000 full-scale passes, 36000 mixed writes, all fetch boundaries bit exact\n";
    }
    std::cout<<"72000 passes and "<<total_writes<<" writes: fixed graph matches independent row-by-row application\n";
}
