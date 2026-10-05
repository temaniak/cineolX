#include "reference224.hpp"
#include <iostream>
#include <iomanip>

using Audio=std::array<std::vector<float>,2>;
static void wav(const std::filesystem::path& path,const Audio& audio) {
    std::ofstream file(path,std::ios::binary);
    auto u16=[&](unsigned v){for(unsigned i=0;i<2;++i)file.put(char(v>>(8*i)));};
    auto u32=[&](unsigned v){for(unsigned i=0;i<4;++i)file.put(char(v>>(8*i)));};
    file.write("RIFF",4);u32(unsigned(audio[0].size()*8+36));file.write("WAVEfmt ",8);u32(16);
    u16(3);u16(2);u32(48000);u32(48000*8);u16(8);u16(32);file.write("data",4);u32(unsigned(audio[0].size()*8));
    for(size_t n=0;n<audio[0].size();++n)for(unsigned c=0;c<2;++c) {
        uint32_t bits;std::memcpy(&bits,&audio[c][n],4);u32(bits);
    }
    sound_validation::require(bool(file),"Cannot write WAV");
}
int main(int argc,char** argv) try {
    using namespace native_hall;using namespace sound_validation;
    bool startup_probe=false,compiler_start=false;int arguments=argc;
    while(arguments>1 && std::string(argv[arguments-1]).starts_with("--")) {
        const std::string option=argv[--arguments];
        if(option=="--startup-probe"){require(!startup_probe,"Repeated startup flag");startup_probe=true;}
        else if(option=="--compiler-start"){require(!compiler_start,"Repeated compiler flag");compiler_start=true;}
        else require(false,"Unknown sound comparison option");
    }
    require(arguments==10 || arguments==15,"usage: native_224_sound_compare ROM_DIRECTORY BANK OUTPUT_DIRECTORY PROGRAM MODE noise|music SEED AMPLITUDE WARMUP_MS [BASS MID TREBLE DEPTH PREDELAY] [--startup-probe] [--compiler-start]");
    const unsigned program=unsigned(std::stoul(argv[4])),mode=unsigned(std::stoul(argv[5]));
    require(program<6 && mode<4,"Invalid program/mode");
    const std::string fixture=argv[6];require(fixture=="noise" || fixture=="music","Invalid fixture");
    uint32_t random=uint32_t(std::stoul(argv[7]));const float amplitude=std::stof(argv[8]);
    const unsigned warmup=unsigned(std::stoul(argv[9]))*48;
    require(amplitude>0 && amplitude<=2 && warmup<=480000,"Invalid amplitude/warmup");
    auto bank=read_bank(argv[2]);const auto output=std::filesystem::path(argv[3]);std::filesystem::create_directories(output);
    Controls controls;controls.predelay_ms=predelay_minima[program];
    if(arguments==15) {
        controls.bass=std::stoi(argv[10]);controls.mid=std::stoi(argv[11]);controls.treble=std::stoi(argv[12]);
        controls.depth=std::stoi(argv[13]);controls.predelay_ms=std::stoi(argv[14]);
    }
    require(controls.bass>=1 && controls.bass<=31 && controls.mid>=0 && controls.mid<=31 &&
            controls.treble>=0 && controls.treble<=31 && controls.depth>=0 && controls.depth<=71 &&
            controls.predelay_ms>=predelay_minima[program] && controls.predelay_ms<=predelay_minima[program]+128,"Invalid controls");
    std::ofstream phases;
    if(startup_probe) {
        phases.open(output/"startup.csv");require(bool(phases),"Cannot write startup CSV");
        phases<<"stage,variant,index,divider,random_divider,hold,first_fraction,second_fraction\n";
    }
    auto record_phase=[&](const char* stage,const char* variant,const ModulationState& state) {
        if(startup_probe)phases<<stage<<','<<variant<<','<<state.index<<','<<unsigned(state.divider)<<','
            <<unsigned(state.random_divider)<<','<<unsigned(state.hold)<<','<<unsigned(state.descriptors[4])<<','
            <<unsigned(state.descriptors[9])<<'\n';
    };
    auto reference=std::make_unique<lexplug::Engine>(1);auto& host=reference->host();
    auto observer=std::move(host.audio_observer);host.audio_observer={};load_roms(*reference,argv[1]);
    wait(host,9000);button(host,1,4);button(host,0,program_identities[program]);wait(host,400);
    if(compiler_start) {
        // Use the same physical-key seed capture as bank preparation. A
        // direct RAM flag clear alone freezes an already advanced phase.
        if(host.memory[0x3f65]&64)button(host,0,64);
        if(host.memory[0x3f65]&128)button(host,0,128);
        wait(host,300);
    }
    host.memory[0x3f65]=host.memory[0x3f55]=program_identities[program];
    controls.mode_enhancement=controls.decay_optimization=false;compile_controls(host,program,controls);
    // Capture only at mode enable, before the reference-only settling time.
    // The probe never restores controller state at input start.
    const auto load_modulation=modulation(host);
    record_phase("mode_enable","reference",load_modulation);
    DecayState load_decay;std::memcpy(&load_decay,host.memory.data()+0x3e32,sizeof load_decay);
    host.memory[0x3f65]=host.memory[0x3f55]=uint8_t(program_identities[program]|((mode&1)?64:0)|((mode&2)?128:0));
    wait(host,300);host.audio_observer=std::move(observer);
    controls.mode_enhancement=bool(mode&1);controls.decay_optimization=bool(mode&2);
    Parameters params;params.program=program;params.hall=controls;
    auto native=std::make_unique<DesktopEngine48>();auto aligned=std::make_unique<DesktopEngine48>();
    native->prepare(*bank,program);aligned->prepare(*bank,program);native->set_parameters(params);aligned->set_parameters(params);
    record_phase("mode_enable","native",native->hall().modulation_state());
    struct Probe {const char* name;std::unique_ptr<DesktopEngine48> engine;Audio audio;};
    std::array<Probe,4> probes{{{"warmup",{},{}},{"modulation",{},{}},{"decay",{},{}},{"load",{},{}}}};
    if(startup_probe) {
        for(auto& probe:probes) {
            probe.engine=std::make_unique<DesktopEngine48>();
            probe.engine->prepare(*bank,program);probe.engine->set_parameters(params);
        }
        probes[1].engine->hall().restore_modulation(load_modulation);
        probes[2].engine->hall().restore_decay(load_decay);
        probes[3].engine->hall().restore_modulation(load_modulation);probes[3].engine->hall().restore_decay(load_decay);
        // Give all probes the same extra 300 ms as the reference. They
        // retain independent converter, graph and canonical scan clocks.
        for(unsigned i=0;i<14400;++i)for(auto& probe:probes){float l,r;probe.engine->process(0,0,l,r);}
    }
    std::array<float,256> zero{},scratch[4];float* ptr[4];for(unsigned c=0;c<4;++c)ptr[c]=scratch[c].data();
    for(unsigned n=0;n<warmup;n+=256) {
        const unsigned count=std::min(256u,warmup-n);reference->render(zero.data(),zero.data(),ptr,int(count));
        for(unsigned i=0;i<count;++i){
            float l,r;native->process(0,0,l,r);aligned->process(0,0,l,r);
            if(startup_probe)for(auto& probe:probes)probe.engine->process(0,0,l,r);
        }
    }
    // Diagnostics only: align free-running controller state, never input,
    // converter clocks or delay memory. This is not shipped plugin behavior.
    DecayState decay;std::memcpy(&decay,host.memory.data()+0x3e32,sizeof decay);
    aligned->hall().restore_decay(decay);aligned->hall().restore_modulation(modulation(host));
    aligned->hall().restart_control_scan(); // No stale writes from its pre-snapshot procedure.
    record_phase("input_start","reference",modulation(host));record_phase("input_start","native",native->hall().modulation_state());
    if(startup_probe) {
        for(auto& probe:probes)record_phase("input_start",probe.name,probe.engine->hall().modulation_state());
        require(bool(phases),"Cannot write startup CSV");
    }
    Audio input,original,result,aligned_result;constexpr unsigned frames=12*48000;
    for(auto* audio:{&input,&original,&result,&aligned_result})for(auto& channel:*audio)channel.resize(frames);
    if(startup_probe)for(auto& probe:probes)for(auto& channel:probe.audio)channel.resize(frames);
    if(fixture=="noise") {
        for(unsigned n=0;n<9600;++n)for(unsigned c=0;c<2;++c) {
            random=random*1664525u+1013904223u;input[c][n]=amplitude*float(int32_t(random))/2147483648.f;
        }
    } else {
        for(unsigned n=0;n<96000;++n) {
            random=random*1664525u+1013904223u;float noise=float(int32_t(random))/2147483648.f;
            if(n<8000)input[0][n]+=amplitude*noise*std::exp(-float(n)/1500);
            if(n>=24000 && n<32000)input[1][n]+=amplitude*noise*std::exp(-float(n-24000)/1500);
            if(n>=48000) {
                const float t=(n-48000)/48000.f,e=std::min(1.f,t*20)*std::exp(-t*5);
                for(unsigned c=0;c<2;++c)input[c][n]+=amplitude*(.035f/.12f)*e*(
                    std::sin(6.28318530718f*220*t+c*.4f)+std::sin(6.28318530718f*277.1826f*t+c*.7f)+std::sin(6.28318530718f*329.6276f*t+c*.9f));
            }
        }
    }
    unsigned calls=0;host.pc_watches[0xc7c]=true;
    host.pc_observer=[&](uint64_t,lexicon224x::cpu::CpuSnapshot cpu){if(cpu.pc==0xc7c){++calls;host.pc_watches[cpu.pc]=true;}};
    for(unsigned n=0;n<frames;n+=256) {
        const unsigned count=std::min(256u,frames-n);reference->render(input[0].data()+n,input[1].data()+n,ptr,int(count));
        for(unsigned i=0;i<count;++i) {
            original[0][n+i]=scratch[0][i];original[1][n+i]=scratch[2][i];
            native->process(input[0][n+i],input[1][n+i],result[0][n+i],result[1][n+i]);
            aligned->process(input[0][n+i],input[1][n+i],aligned_result[0][n+i],aligned_result[1][n+i]);
            if(startup_probe)for(auto& probe:probes)
                probe.engine->process(input[0][n+i],input[1][n+i],probe.audio[0][n+i],probe.audio[1][n+i]);
        }
    }
    for(auto* audio:{&original,&result,&aligned_result})for(const auto& channel:*audio)for(float value:channel)
        require(std::isfinite(value) && std::abs(value)<4,"Nonfinite/runaway render");
    wav(output/"input.wav",input);wav(output/"reference.wav",original);wav(output/"native.wav",result);wav(output/"aligned.wav",aligned_result);
    if(startup_probe)for(const auto& probe:probes) {
        for(const auto& channel:probe.audio)for(float value:channel)
            require(std::isfinite(value) && std::abs(value)<4,"Nonfinite/runaway startup probe");
        wav(output/(std::string(probe.name)+".wav"),probe.audio);
    }
    std::ofstream meta(output/"fixture.csv");require(bool(meta),"Cannot write fixture metadata");
    meta<<"program,mode,fixture,seed,amplitude,warmup_ms,bass,mid,treble,depth,predelay_ms,reference_mod_calls,startup_probe,reference_extra_ms,probe_extra_ms,phase_protocol\n"
        <<program<<','<<mode<<','<<fixture<<','<<argv[7]<<','<<amplitude<<','<<argv[9]<<','<<controls.bass<<','<<controls.mid<<','<<controls.treble<<','<<controls.depth<<','<<controls.predelay_ms<<','<<calls<<','<<startup_probe<<",300,"<<(startup_probe?300:0)<<','<<(compiler_start?"compiler":"historical")<<'\n';
    std::cout<<program_names[program]<<" mode="<<mode<<" seed="<<argv[7]<<" rendered; ROM calls="<<calls<<'\n';
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
