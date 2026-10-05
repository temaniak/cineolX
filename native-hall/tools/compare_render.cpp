// Offline listening fixture. The reference alone executes the original ROM;
// no emulator code is linked to the plugin or portable core.
#include "../desktop/engine22448.hpp"
#include <juce-plugin/source/engine.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <iostream>
#include <vector>
#include <stdexcept>

using Audio=std::array<std::vector<float>,2>;
static void wav(const std::filesystem::path& path,const Audio& a) {
    std::ofstream f(path,std::ios::binary);
    auto u16=[&](unsigned v){for(int i=0;i<2;++i) f.put(char(v>>(i*8)));};
    auto u32=[&](unsigned v){for(int i=0;i<4;++i) f.put(char(v>>(i*8)));};
    unsigned bytes=unsigned(a[0].size()*8);
    f.write("RIFF",4);u32(36+bytes);f.write("WAVEfmt ",8);u32(16);
    u16(3);u16(2);u32(48000);u32(48000*8);u16(8);u16(32);
    f.write("data",4);u32(bytes);
    for(size_t n=0;n<a[0].size();++n) for(int c=0;c<2;++c) {
        uint32_t bits;std::memcpy(&bits,&a[c][n],4);u32(bits);
    }
    if(!f) throw std::runtime_error("WAV write failed");
}
static void stats(std::ostream& report,const char* name,const Audio& a) {
    float peak=0;double total=0,tail=0;
    for(int c=0;c<2;++c) for(size_t n=0;n<a[c].size();++n) {
        float v=a[c][n];if(!std::isfinite(v)) throw std::runtime_error("nonfinite render");
        peak=std::max(peak,std::abs(v));total+=double(v)*v;
        if(n>=3*48000) tail+=double(v)*v;
    }
    report<<name<<": peak="<<peak<<", RMS="<<std::sqrt(total/(a[0].size()*2))
          <<", tail RMS (3..10s)="<<std::sqrt(tail/((a[0].size()-3*48000)*2))<<'\n';
    if(total<1e-4 || peak>4) throw std::runtime_error("silent or runaway render");
}
int main(int argc,char** argv) try {
    if(argc!=4) throw std::runtime_error("usage: native_hall_compare ROM_DIRECTORY BANK_OR_PROFILE OUTPUT_DIRECTORY");
    std::ifstream in(argv[2],std::ios::binary);
    std::vector<char> data((std::istreambuf_iterator<char>(in)),{});
    auto bank=std::make_unique<native_hall::ProgramBank>();native_hall::Profile profile;
    const bool have_bank=native_hall::read_bank(data.data(),data.size(),*bank);
    if(!have_bank && !native_hall::read_profile(data.data(),data.size(),profile)) throw std::runtime_error("bad bank or profile");
    auto output=std::filesystem::path(argv[3]);std::filesystem::create_directories(output);
    constexpr int frames=10*48000;
    Audio input;for(auto& c:input) c.resize(frames);
    uint32_t rng=17;
    // A left strike, a right strike, then an equal-power stereo chord. Same
    // samples and levels in every render; no normalization of either result.
    for(int n=0;n<2*48000;++n) {
        rng=rng*1664525u+1013904223u;float noise=float(int32_t(rng))/2147483648.0f;
        if(n<8000) input[0][n]+=0.12f*noise*std::exp(-n/1500.0f);
        if(n>=24000 && n<32000) input[1][n]+=0.12f*noise*std::exp(-(n-24000)/1500.0f);
        if(n>=48000) {
            float t=(n-48000)/48000.0f;
            float envelope=std::min(1.0f,t*20)*std::exp(-t*5);
            for(int c=0;c<2;++c) input[c][n]+=0.035f*envelope*(
                std::sin(6.28318530718f*220*t+c*0.4f)+
                std::sin(6.28318530718f*277.1826f*t+c*0.7f)+
                std::sin(6.28318530718f*329.6276f*t+c*0.9f));
        }
    }
    wav(output/"input.wav",input);
    std::ofstream report(output/"comparison.txt");
    report<<"224 v4.4 Large Concert Hall B; 48 kHz float WAV; wet only, outputs A/C.\n"
          <<"Factory: Bass 3.4s, Mid 2.6s, Crossover 540Hz, Treble 4000Hz, Depth 21, Predelay 24ms, Diffusion 1.\n"
          <<"No gain normalization. SRC/analog latency differs; no sample null claimed.\n"
          <<"Native: desktop scan with cleared startup diffusion period. Native aligned: diagnostic 17-byte decay state only; scan/modulation phase remains nominal.\n";
    struct Modes {const char* name;bool enhancement,decay,analog=true;};
    for(auto modes:{Modes{"modes-off",false,false},Modes{"modulation-only",true,false},
                    Modes{"decay-only",false,true},Modes{"modes-on",true,true},
                    Modes{"filters-off",false,false,false}}) {
        auto reference=std::make_unique<lexplug::Engine>(1);
        // Silent firmware boot has no consuming audio frames; do not queue
        // its DAC events into the bounded analog output buffers.
        auto audio_observer=std::move(reference->host().audio_observer);
        reference->host().audio_observer={};
        unsigned loaded=0;
        for(const auto& entry:std::filesystem::directory_iterator(argv[1])) {
            auto name=entry.path().filename().string();auto at=name.find("ROM");
            if(at==std::string::npos || name.size()<=at+3 || name[at+3]<'1' || name[at+3]>'4') continue;
            unsigned chip=unsigned(name[at+3]-'1');std::array<uint8_t,2048> rom;
            std::ifstream f(entry.path(),std::ios::binary);
            if(!f.read(reinterpret_cast<char*>(rom.data()),rom.size())) throw std::runtime_error("bad ROM");
            reference->load(rom.data(),rom.size(),chip*2048);loaded|=1u<<chip;
        }
        if(loaded!=15) throw std::runtime_error("ROM1..4 required");
        auto wait=[&](double ms){reference->host().run_until(reference->host().cycles+uint64_t(ms*2048));};
        auto button=[&](unsigned bank,unsigned mask) {
            reference->button(bank,mask);wait(100);reference->button(bank,0);wait(30);
        };
        wait(9000);button(1,4);button(0,4);wait(300);
        if(!modes.enhancement) button(0,0x40);
        if(!modes.decay) button(0,0x80);
        wait(300);
        reference->set_analog(modes.analog);
        reference->host().audio_observer=std::move(audio_observer);
        auto native=std::make_unique<native_hall::DesktopEngine48>();
        auto aligned=std::make_unique<native_hall::DesktopEngine48>();
        if(have_bank){native->prepare(*bank,2);aligned->prepare(*bank,2);}
        else{native->prepare(profile);aligned->prepare(profile);}
        native_hall::Parameters params;
        params.hall.mode_enhancement=modes.enhancement;params.hall.decay_optimization=modes.decay;
        params.analog=modes.analog;
        native->set_parameters(params);
        aligned->set_parameters(params);
        // Allow the analog state and fixed SRC delay to settle before input.
        std::array<float,256> zero{},scratch[4];float* ptr[4];
        for(int c=0;c<4;++c) ptr[c]=scratch[c].data();
        for(int n=0;n<48000;n+=256) {
            int count=std::min(256,48000-n);reference->render(zero.data(),zero.data(),ptr,count);
            for(int i=0;i<count;++i) {float l,r;native->process(0,0,l,r);aligned->process(0,0,l,r);}
        }
        // The free-running counters are not reset by a program load. A
        // different number of silent boot/menu ticks changes the first tail.
        // Align the decay bytes for this diagnostic A/B only; scan and
        // modulation timing remain nominal. The plugin uses its prepared bank.
        native_hall::DecayState initial;
        std::memcpy(&initial,reference->host().memory.data()+0x3e32,sizeof initial);
        aligned->hall().restore_decay(initial);
        unsigned reference_steps=0;uint8_t reference_level=0;
        reference->host().pc_watches[0x0779]=true;
        reference->host().pc_observer=[&](uint64_t,lexicon224x::cpu::CpuSnapshot cpu) {
            if(cpu.pc==0x0779) {
                ++reference_steps;reference_level=native_hall::DecayController::level_from_word(cpu.hl);
                reference->host().pc_watches[0x0779]=true;
            }
        };
        std::ofstream trace(output/(std::string("controllers-")+modes.name+".csv"));
        trace<<"frame,reference_steps,reference_level,reference_held,native_held,reference_stop,native_stop,reference_amount,native_amount,reference_period,native_period,aligned_held,aligned_amount,aligned_period\n";
        Audio original,result,aligned_result;
        for(auto* audio:{&original,&result,&aligned_result}) for(auto& c:*audio) c.resize(frames);
        for(int n=0;n<frames;n+=256) {
            int count=std::min(256,frames-n);
            reference->render(input[0].data()+n,input[1].data()+n,ptr,count);
            for(int i=0;i<count;++i) {
                original[0][n+i]=scratch[0][i];original[1][n+i]=scratch[2][i];
                native->process(input[0][n+i],input[1][n+i],result[0][n+i],result[1][n+i]);
                aligned->process(input[0][n+i],input[1][n+i],aligned_result[0][n+i],aligned_result[1][n+i]);
            }
            const auto& state=native->hall().decay_state();const auto& ram=reference->host().memory;
            const auto& aligned_state=aligned->hall().decay_state();
            trace<<n+count<<','<<reference_steps<<','<<unsigned(reference_level)<<','<<unsigned(ram[0x3e33])<<','
                 <<unsigned(state.held)<<','<<unsigned(ram[0x3e34])<<','<<unsigned(state.stopped)<<','
                 <<unsigned(ram[0x3e35])<<','<<unsigned(state.amount)<<','<<unsigned(ram[0x3e37])<<','
                 <<unsigned(state.diffusion_period)<<','<<unsigned(aligned_state.held)<<','
                 <<unsigned(aligned_state.amount)<<','<<unsigned(aligned_state.diffusion_period)<<'\n';
        }
        std::string suffix=modes.name;
        wav(output/("reference-"+suffix+".wav"),original);
        wav(output/("native-"+suffix+".wav"),result);
        wav(output/("native-aligned-"+suffix+".wav"),aligned_result);
        stats(report,("Reference "+suffix).c_str(),original);stats(report,("Native "+suffix).c_str(),result);
        stats(report,("Native aligned "+suffix).c_str(),aligned_result);
        report<<"Reference decay controller: "<<reference_steps/10.0<<" updates/s over this fixture\n";
        std::cout<<"Rendered matched input: "<<suffix<<'\n';
    }
    std::cout<<"Listening files: "<<output<<'\n';
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
