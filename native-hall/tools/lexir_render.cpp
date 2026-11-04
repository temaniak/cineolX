// Offline native XL renderer for private hardware-IR matching. No DSP changes.
// Build: clang++ -std=c++20 -O2 -ffp-contract=off native-hall/tools/lexir_render.cpp -o build/validation/lexirs-20261104/renderer
#include "../desktop/runtime.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <stdexcept>

int main(int argc,char** argv) try {
    if(argc<2) throw std::runtime_error("XL_BANK [--describe]");
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<cineol::xl::Bank>();
    if(!cineol::xl::read_bank(bytes.data(),bytes.size(),*bank)) throw std::runtime_error("Invalid current XL bank");
    if(argc==3 && std::string(argv[2])=="--display") {
        unsigned p;
        while(std::cin>>p) {
            if(p>=14)throw std::runtime_error("Invalid display program");
            auto raw=bank->programs[p].controls.factory;
            for(auto& v:raw){unsigned n;if(!(std::cin>>n) || n>255)throw std::runtime_error("Invalid display control");v=uint8_t(n);}
            const auto& d=bank->programs[p];d.resolve_controls(raw);std::cout<<'[';
            for(unsigned page=0;page<d.page_count;++page) {
                if(page)std::cout<<',';std::cout<<'[';
                for(unsigned slot=0;slot<6;++slot) {
                    if(slot)std::cout<<',';std::cout<<std::quoted(d.display_value(page,slot,raw).data());
                }
                std::cout<<']';
            }
            std::cout<<"]\n";
        }
        return 0;
    }
    if(argc==3 && std::string(argv[2])=="--describe") {
        std::cout<<"[";
        for(unsigned p=0;p<14;++p) {
            const auto& d=bank->programs[p];if(p)std::cout<<',';
            std::cout<<"{\"program\":"<<p<<",\"name\":"<<std::quoted(cineol::xl::graphs[p].name)<<",\"factory\":[";
            for(unsigned c=0;c<48;++c){if(c)std::cout<<',';std::cout<<unsigned(d.controls.factory[c]);}
            std::cout<<"],\"pages\":[";
            for(unsigned page=0;page<d.page_count;++page) {
                if(page)std::cout<<',';std::cout<<'[';
                for(unsigned slot=0;slot<6;++slot) {
                    if(slot)std::cout<<',';const auto& q=d.pages[page];unsigned c=q.cells[slot];
                    std::cout<<"{\"cell\":"<<c<<",\"name\":"<<std::quoted(q.names[slot].data())<<",\"active\":"<<(c<48 && d.control_active(c,cineol::xl::Graph(p))?"true":"false")<<",\"values\":[";
                    for(unsigned v=0;v<256;++v) {
                        auto raw=d.controls.factory;if(c<48)raw[c]=uint8_t(v);
                        if(v)std::cout<<',';std::cout<<std::quoted(d.display_value(page,slot,raw).data());
                    }
                    std::cout<<"]}";
                }
                std::cout<<']';
            }
            std::cout<<"]}";
        }
        std::cout<<"]\n";return 0;
    }
    auto engine=std::make_unique<cineol::xl::Runtime>();
    // Text recipe followed by interleaved input float32; output is stereo float32.
    // p frames warmup_frames mod opt dynamic left right stability + 48 raw cells.
    unsigned p,frames,warm,mod,opt,dynamic,left,right;float stability;
    while(std::cin>>p>>frames>>warm>>mod>>opt>>dynamic>>left>>right>>stability) {
        if(p>=14 || frames>48000*60 || warm>48000*10 || left>3 || right>3)throw std::runtime_error("Invalid recipe");
        std::array<uint8_t,48> raw{};
        for(auto& v:raw){unsigned n;if(!(std::cin>>n) || n>255)throw std::runtime_error("Invalid control");v=uint8_t(n);}
        if(std::cin.get()!='\n')throw std::runtime_error("Recipe terminator");
        std::vector<float> input(frames*2),output(frames*2);
        if(!std::cin.read(reinterpret_cast<char*>(input.data()),std::streamsize(input.size()*4)))throw std::runtime_error("Input truncated");
        raw[42]=uint8_t((raw[42]&~0xc1u)|(mod?64:0)|(opt?128:0)|(dynamic?1:0));
        bank->programs[p].resolve_controls(raw);
        engine->select(*bank,p);engine->controls(raw,mod,0,1,1,int(left),int(right),opt);
        engine->set_dark_stability(stability);engine->set_dirt(0);
        for(unsigned n=0;n<warm;++n){float l,r;engine->process(0,0,l,r,true);}
        for(unsigned n=0;n<frames;++n)engine->process(input[n*2],input[n*2+1],output[n*2],output[n*2+1],true);
        std::cout.write(reinterpret_cast<const char*>(output.data()),std::streamsize(output.size()*4));std::cout.flush();
        if(!std::cout)throw std::runtime_error("Output failed");
    }
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
