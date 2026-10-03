// Offline ROM execution prepares the small, fixed-size native control data.
// The plugin and portable audio core never link this emulator.
#include "../core/profile.hpp"
#include "../core/hall.hpp"
#include <juce-plugin/source/roms/sha256.hpp>
#include <emulator/host.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace lexicon224x;

int main(int argc, char** argv) try {
    if (argc != 3) throw std::runtime_error("usage: extract_profile ROM_DIRECTORY OUTPUT.hall224");
    auto host = std::make_unique<cpu::Host>(Model::Lexicon224);
    auto& h = *host;
    const char* expected[]={
#include "../import/rom_hashes.inc"
    };
    unsigned loaded = 0;
    for (const auto& e : std::filesystem::directory_iterator(argv[1])) {
        if (!e.is_regular_file() || e.file_size()!=2048) continue;
        std::ifstream in(e.path(), std::ios::binary);
        std::array<uint8_t,2048> data{};
        if (!in.read(reinterpret_cast<char*>(data.data()), data.size())) throw std::runtime_error("ROM must contain 2048 bytes");
        const auto digest=lexplug::roms::Sha256::of(data.data(),data.size());
        unsigned chip=0;
        while(chip<4 && digest!=expected[chip]) ++chip;
        if(chip==4) continue;
        std::copy(data.begin(), data.end(), h.memory.begin()+chip*2048);
        loaded |= 1u << chip;
    }
    if (loaded != 15) throw std::runtime_error("ROM1 through ROM4 are required");
    // These compiler entry points are specifically the original 224 v4.4.
    if (h.memory[0xc7c] != 0x3a || h.memory[0x109d] != 0x21 || h.memory[0x10ca] != 0x21)
        throw std::runtime_error("unsupported firmware; use 224 v4_4");
    auto wait = [&](double ms) { h.run_until(h.cycles + uint64_t(ms*2048.0)); };
    auto button = [&](unsigned bank, unsigned mask) {
        h.panel.switches[bank] = uint8_t(~mask); wait(100);
        h.panel.switches[bank] = 255; wait(30);
    };
    wait(9000); button(1,4); button(0,4); wait(300);
    button(0,0x40); button(0,0x80); wait(300);
    if((h.memory[0x3f65]&63)!=4) throw std::runtime_error("Large Concert Hall B was not selected");
    native_hall::Profile p{};
    std::array<uint32_t,100> large_program{};
    std::copy_n(h.dsp->wcs,large_program.size(),large_program.begin());
    auto coeff = [&](unsigned r) {
        auto m = decode(h.dsp->wcs[r], Model::Lexicon224);
        return int8_t(m.negative ? -int(m.coefficient) : int(m.coefficient));
    };
    for(unsigned r=0;r<100;++r) {
        auto m=decode(h.dsp->wcs[r],Model::Lexicon224);
        p.coefficients[r]=coeff(r); p.offsets[r]=uint16_t(~m.low)&0x3fff;
    }
    // Ignored diagnostic oracle, never linked into the portable runtime.
    std::ofstream wcs(std::string(argv[2])+".wcs",std::ios::binary);
    for(unsigned a=0x4000;a<0x4200;++a) {char v=char(h.peek(uint16_t(a)));wcs.write(&v,1);}
    std::copy_n(h.memory.begin(),4096,p.modulation_sequence.begin());
    auto word = [&](unsigned a) {return unsigned(h.memory[a]) | unsigned(h.memory[a+1])<<8;};
    std::copy_n(h.memory.begin()+word(0x3e27),10,p.modulation_descriptors.begin());
    p.modulation_index=uint16_t(word(0x3e69));
    p.modulation_period=h.memory[0x3f72]; p.modulation_hold=h.memory[0x3f73];
    p.modulation_step=h.memory[0x3f74]; p.decay_amount=h.memory[0x3f71];
    p.modulation_mask=h.memory[0x3f75]; p.initial_mod_divider=h.memory[0x3e66];
    p.initial_random_divider=h.memory[0x3e67]; p.initial_random_hold=h.memory[0x3e68];
    std::memcpy(&p.initial_decay,h.memory.data()+0x3e32,sizeof p.initial_decay);
    auto compile = [&](int bass,int mid,int cross,int treble,int depth,int diffusion,unsigned program=4) {
        h.memory[0x3f65]=uint8_t(program);
        h.memory[0x3f66]=uint8_t(mid); h.memory[0x3f67]=uint8_t(bass);
        h.memory[0x3f68]=uint8_t(cross); h.memory[0x3f69]=uint8_t(treble);
        h.memory[0x3f6a]=uint8_t(depth);
        // 72*raw /256 is the display index; the panel retains 3*raw
        // as its fine interpolation coordinate (05F8..0613).
        int raw=(depth*256+71)/72; raw=std::min(raw,255);
        if(depth==21) raw=78; // Large Hall's factory fine coordinate, 0x00EA.
        h.memory[0x3f6b]=uint8_t(raw*3); h.memory[0x3f6c]=uint8_t(raw*3>>8);
        h.memory[0x3f6e]=uint8_t(diffusion);
        h.memory[0x3e35]=0;
        std::fill(h.memory.begin()+0x3f56,h.memory.begin()+0x3f64,0x7f);
        // Trigger the real panel dispatcher: changing RAM alone does not
        // call the firmware's compiler. Make BASS's accepted reading fall,
        // with soft pickup already live, without disturbing the other pots.
        h.panel.pots[0]=uint8_t(bass*8);
        h.memory[0x3f20]=255;
        h.memory[0x3f2b]=0x80;
        wait(35);
    };
    const unsigned tails[]={37,38,87,88,6,7};
    for(int b=0;b<32;++b) for(int m=0;m<32;++m) {
        compile(b,m,5,23,21,1);
        for(unsigned j=0;j<6;++j) p.tail[b][m][j]=coeff(tails[j]);
    }
    const unsigned cr[]={35,36,85,86}, tr[]={40,41,90,91};
    for(int i=0;i<32;++i) {
        compile(17,14,i,23,21,1);
        for(unsigned j=0;j<4;++j) p.crossover[i][j]=coeff(cr[j]);
        compile(17,14,5,i,21,1);
        for(unsigned j=0;j<4;++j) p.treble[i][j]=coeff(tr[j]);
    }
    const unsigned dep[]={42,43,44,45,92,93,94,95};
    for(int i=0;i<72;++i) {
        compile(17,14,5,23,i,1);
        for(unsigned j=0;j<8;++j) p.depth[i][j]=coeff(dep[j]);
    }
    const unsigned dif[]={28,29,32,33};
    for(int i=0;i<64;++i) {
        compile(17,14,5,23,21,std::max(1,i));
        for(unsigned j=0;j<4;++j) p.diffusion[i][j]=coeff(dif[j]);
    }
    if(p.tail[17][14][0] != p.coefficients[37] || p.tail[17][14][4] != p.coefficients[6] ||
       p.treble[23][0] != p.coefficients[40] || p.depth[21][0] != p.coefficients[42])
        throw std::runtime_error("factory control tables failed to reproduce the captured program");
    if(p.tail[5][5] == p.tail[17][14] || p.crossover[14] == p.crossover[5] || p.diffusion[25] == p.diffusion[1])
        throw std::runtime_error("firmware control compiler did not change the expected coefficients");
    compile(17,14,5,23,21,1);
    native_hall::Hall oracle;
    oracle.prepare(p);
    native_hall::Controls off;off.mode_enhancement=off.decay_optimization=false;oracle.set_controls(off);
    for(unsigned r=0;r<100;++r) if(oracle.coefficients()[r]!=p.coefficients[r])
        throw std::runtime_error("native factory coefficients differ at row "+std::to_string(r));
    for(int n=0;n<24;++n) {
        native_hall::Controls c;
        c.bass=1+(n*7)%31;c.mid=(n*11)%32;c.crossover=(n*13)%32;
        c.treble=(n*17)%32;c.depth=(n*19)%72;c.diffusion=1+(n*23)%63;
        c.mode_enhancement=c.decay_optimization=false;
        compile(c.bass,c.mid,c.crossover,c.treble,c.depth,c.diffusion);oracle.set_controls(c);
        for(unsigned r=0;r<100;++r) if(oracle.coefficients()[r]!=coeff(r))
            throw std::runtime_error("control table composition differs from ROM, set "+std::to_string(n)+
                " row "+std::to_string(r));
    }
    std::cout<<"Control table composition: 24 varied parameter sets exact\n";
    compile(17,14,5,23,21,1);oracle.reset();oracle.set_controls(off);
    // Clock the native modulation controller at the ROM's actual routine
    // entries; this separates the step law from the prototype's fixed clock.
    unsigned comparisons=0;
    h.memory[0x3f65]=h.memory[0x3f55]=0x44;
    h.pc_watches[0xc7c]=true;
    h.pc_observer=[&](uint64_t,cpu::CpuSnapshot cpu) {
        if(cpu.pc!=0xc7c) return;
        if(comparisons) for(unsigned row:{17u,18u,67u,68u}) {
            auto mi=decode(h.dsp->wcs[row],Model::Lexicon224);
            if(oracle.coefficients()[row]!=coeff(row) || oracle.offsets()[row]!=(uint16_t(~mi.low)&0x3fff))
                throw std::runtime_error("modulation mismatch at update "+std::to_string(comparisons)+
                    " row "+std::to_string(row)+" native="+std::to_string(oracle.coefficients()[row])+"/"+
                    std::to_string(oracle.offsets()[row])+" ROM="+std::to_string(coeff(row))+"/"+
                    std::to_string(uint16_t(~mi.low)&0x3fff));
        }
        oracle.advance_modulation();++comparisons;h.pc_watches[0xc7c]=true;
    };
    wait(2000);
    h.pc_observer={};h.pc_watches[0xc7c]=false;
    std::cout<<"Modulation step law: "<<comparisons-1<<" ROM-clocked updates exact\n";
    // All 17 controller RAM bytes and affected AP coefficients against ROM.
    unsigned decay_checks=0;
    for(auto pair:{std::pair{17,14},std::pair{30,31},std::pair{1,0},std::pair{1,1},std::pair{6,6}}) {
        h.set_audio(0,0,0,0);h.set_level_detectors(0,0);h.set_level_detectors(1,0);
        h.memory[0x3e35]=0;compile(pair.first,pair.second,5,23,21,1);
        h.memory[0x3f65]=h.memory[0x3f55]=0x84;
        native_hall::DecayController decay;
        bool first=true,pending=false;
        h.pc_watches[0x0779]=h.pc_watches[0x0228]=true;
        h.pc_observer=[&](uint64_t,cpu::CpuSnapshot cpu) {
            h.pc_watches[cpu.pc]=true;
            if(cpu.pc==0x0779) {
                if(first) {
                    native_hall::DecayState state;
                    std::memcpy(&state,h.memory.data()+0x3e32,sizeof state);decay.reset(state);first=false;
                }
                decay.step(native_hall::DecayController::level_from_word(cpu.hl),h.memory[0x3e61],
                    (h.memory[0x3f55]&128)!=0,h.memory[0x3f71]);pending=true;
            }
            if(cpu.pc==0x0228 && pending) {
                pending=false;++decay_checks;
                if(std::memcmp(&decay.state(),h.memory.data()+0x3e32,sizeof(native_hall::DecayState))) {
                    const auto* bytes=reinterpret_cast<const uint8_t*>(&decay.state());
                    for(unsigned i=0;i<17;++i) if(bytes[i]!=h.memory[0x3e32+i])
                        throw std::runtime_error("decay controller mismatch at RAM "+std::to_string(0x3e32+i)+
                            " native="+std::to_string(bytes[i])+" ROM="+std::to_string(h.memory[0x3e32+i]));
                }
                int g=std::max(4,int(p.tail[pair.first][pair.second][4])-int(decay.state().amount));
                for(unsigned row:{6u,10u,56u,60u})
                    if(coeff(row)!=g || coeff(row+1)!=32-g*g/32 || coeff(row+2)!=-g)
                        throw std::runtime_error("decay allpass recompilation differs from ROM");
            }
        };
        for(unsigned n=0;n<48;++n) {
            int amplitude=(n%24<4)?800:(n%24==15)?2047:0;
            if(n%24==20) h.memory[0x3f65]=h.memory[0x3f55]=4;
            if(n%24==23) h.memory[0x3f65]=h.memory[0x3f55]=0x84;
            h.set_audio(unsigned(amplitude)&4095,0,unsigned(-amplitude)&4095,0);
            unsigned detectors=amplitude==2047?31:amplitude?7:0;
            h.set_level_detectors(0,detectors);h.set_level_detectors(1,detectors);wait(150);
        }
        h.pc_observer={};h.pc_watches[0x0779]=h.pc_watches[0x0228]=false;
    }
    std::cout<<"Decay state + allpass coefficients: "<<decay_checks<<" ROM-clocked updates exact\n";
    // 02EF maps program 3 to program 1's template; 0363 chooses a different
    // factory control block. Verify the consequence with the actual loader,
    // rather than assuming both "B" names describe the same network.
    button(0,1);wait(300);button(0,0x40);button(0,0x80);wait(300);
    if((h.memory[0x3f65]&63)!=1) throw std::runtime_error("Small Concert Hall B was not selected");
    compile(17,14,5,23,21,1,1);
    for(unsigned r=0;r<large_program.size();++r) if(h.dsp->wcs[r]!=large_program[r])
        throw std::runtime_error("Small/Large Hall B normalized programs differ at row "+std::to_string(r));
    std::cout<<"Large / Small Concert Hall B: all 100 program words identical at matching controls\n";
    // Characterize the panel scan clock without introducing an 8080 into
    // the portable runtime. This is a nominal factory/quiet clock; signal
    // branches and coefficient writes still cause smaller timing variation.
    compile(17,14,5,23,21,1);
    h.set_audio(0,0,0,0);h.set_level_detectors(0,0);h.set_level_detectors(1,0);
    for(unsigned mode=0;mode<4;++mode) {
        h.memory[0x3f65]=h.memory[0x3f55]=uint8_t(4|((mode&1)?64:0)|((mode&2)?128:0));
        wait(300);
        unsigned count=0;uint64_t first=0,last=0;
        h.pc_watches[0x0755]=true;
        h.pc_observer=[&](uint64_t cycles,cpu::CpuSnapshot cpu) {
            if(cpu.pc!=0x0755) return;
            if(!count++) first=cycles;last=cycles;h.pc_watches[0x0755]=true;
        };
        wait(2000);h.pc_observer={};h.pc_watches[0x0755]=false;
        if(count<20 || last<=first) throw std::runtime_error("cannot measure controller clock");
        p.level_rate_tenths[mode]=uint16_t(std::lround((count-1)*20480000.0/double(last-first)));
        std::cout<<"Nominal level clock, mode "<<mode<<": "<<p.level_rate_tenths[mode]/10.0<<" Hz\n";
    }
    native_hall::ProfileHeader header;
    header.checksum=native_hall::profile_checksum(&p,sizeof p);
    std::ofstream out(argv[2],std::ios::binary);
    out.write(reinterpret_cast<const char*>(&header),sizeof header);
    out.write(reinterpret_cast<const char*>(&p),sizeof p);
    if(!out) throw std::runtime_error("profile write failed");
    std::cout << std::dec << "Prepared Hall v4.4 profile: " << sizeof p << " bytes\n";
} catch(const std::exception& e) {std::cerr << e.what() << '\n'; return 1;}
