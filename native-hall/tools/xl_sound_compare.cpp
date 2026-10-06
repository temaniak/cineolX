// Offline/private-ROM full-path comparison. No reference state is injected
// into the native runtime, and no firmware code enters plugin processing.
#include "../desktop/runtime.hpp"
#include "../tests/xl_reference.hpp"
#include <iomanip>
#include <memory>
#include <new>
#include <stdexcept>

static bool tracking=false;
static unsigned allocations=0,releases=0;
void* operator new(size_t n) {if(tracking) ++allocations;if(void* p=std::malloc(n?n:1)) return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {if(tracking && p) ++releases;std::free(p);}
void operator delete(void* p,size_t) noexcept {::operator delete(p);}
void* operator new[](size_t n) {return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}

using Audio=std::array<std::vector<float>,2>;
using namespace lexplug;
using namespace lexplug::op;
using namespace cineol::xl;
static void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
// Controlled offline fixture format emitted by prepare_xl_listening.py:
// canonical RIFF, stereo IEEE float32, 48 kHz. No file access in processing.
static Audio input_wav(const std::filesystem::path& path,unsigned maximum_frames) {
    std::ifstream file(path,std::ios::binary);
    check(bool(file),"Cannot open input WAV");
    file.seekg(0,std::ios::end);const auto size=file.tellg();
    check(size>=44 && uint64_t(size)<=44+uint64_t(maximum_frames)*8,"Input WAV length outside fixture limits");
    file.seekg(0);std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size()));
    check(bool(file),"Cannot read input WAV");
    auto u32=[&](unsigned at){return uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8|
        uint32_t(bytes[at+2])<<16|uint32_t(bytes[at+3])<<24;};
    check(!std::memcmp(bytes.data(),"RIFF",4) && !std::memcmp(bytes.data()+8,"WAVEfmt ",8) &&
        u32(16)==16 && u32(20)==0x00020003 && u32(24)==48000 && u32(28)==384000 &&
        u32(32)==0x00200008 && !std::memcmp(bytes.data()+36,"data",4) &&
        u32(4)==bytes.size()-8 && u32(40)==bytes.size()-44 && u32(40)%8==0,
        "Expected canonical 48 kHz stereo float32 input WAV");
    const unsigned frames=u32(40)/8;check(frames>0 && frames<=maximum_frames,"Invalid input WAV frame count");
    Audio audio;for(auto& channel:audio)channel.resize(frames);
    for(unsigned n=0;n<frames;++n)for(unsigned c=0;c<2;++c) {
        const uint32_t bits=u32(44+n*8+c*4);float sample;std::memcpy(&sample,&bits,4);
        check(std::isfinite(sample) && std::abs(sample)<=1,"Invalid input WAV sample");audio[c][n]=sample;
    }
    return audio;
}
struct FrozenWcs {
    virtual ~FrozenWcs()=default;
    virtual void process(float,float,float&,float&)=0;
};
template<Graph graph> struct FrozenGraph final:FrozenWcs {
    Native48<graph> audio;
    std::array<std::array<float,2>,4> alignment{};
    uint64_t position=0;
    explicit FrozenGraph(const lexicon224x::Machine& source,bool analog,int left,int right) {
        typename Native48<graph>::Settings settings;
        for(unsigned row=0;row<settings.rows;++row) {
            const auto mi=lexicon224x::decode(source.wcs[row]);
            settings.coefficients[row]=int8_t(mi.negative?-int(mi.coefficient):int(mi.coefficient));
            settings.offsets[row]=uint16_t(~mi.low);
        }
        check(audio.prepare(settings),"Invalid diagnostic reference WCS");
        audio.set_global(0,1,float(analog),left,right);
    }
    void process(float left,float right,float& l,float& r) override {
        audio.process(left,right,l,r,true);
        constexpr unsigned extra=Runtime::latency_samples-Runtime::graph_delay(graph_info(graph).rows);
        alignment[position%alignment.size()]={l,r};
        const auto value=alignment[(position+alignment.size()-extra)%alignment.size()];
        ++position;l=value[0];r=value[1];
    }
};
static std::unique_ptr<FrozenWcs> frozen_wcs(Graph graph,const lexicon224x::Machine& source,bool analog,int left,int right) {
    switch(graph) {
#define CINEOL_XL_GRAPH(id,name,rows,bank,program) case Graph::id:return std::make_unique<FrozenGraph<Graph::id>>(source,analog,left,right);
#include "../desktop/graph_list.inc"
#undef CINEOL_XL_GRAPH
    }
    throw std::runtime_error("Invalid diagnostic graph");
}
static void wav(const std::filesystem::path& path,const Audio& audio) {
    std::ofstream file(path,std::ios::binary);
    auto u16=[&](unsigned v){for(unsigned i=0;i<2;++i) file.put(char(v>>(8*i)));};
    auto u32=[&](unsigned v){for(unsigned i=0;i<4;++i) file.put(char(v>>(8*i)));};
    file.write("RIFF",4);u32(unsigned(audio[0].size()*8+36));file.write("WAVEfmt ",8);u32(16);
    u16(3);u16(2);u32(48000);u32(48000*8);u16(8);u16(32);file.write("data",4);u32(unsigned(audio[0].size()*8));
    for(unsigned n=0;n<audio[0].size();++n) for(unsigned c=0;c<2;++c) {
        uint32_t bits;std::memcpy(&bits,&audio[c][n],4);u32(bits);
    }
    check(bool(file),"Cannot write comparison WAV");
}
static Task<void> set_toggle(Machine& machine,LarcOperator& op,unsigned which,bool enabled) {
    const unsigned mask=which==0?1:which==1?64:128;
    for(unsigned attempt=0;attempt<8;++attempt) {
        co_await op.setToggle(int(which),enabled);
        if(bool(machine.peek(uint16_t(op.recordBase()+42))&mask)==enabled) co_return;
        co_await machine.sleep(0.2);
    }
    co_await fail("XL physical toggle did not settle");
}
static Task<void> setup(Machine& machine,LarcOperator& op,const ProgramData& data,unsigned index,unsigned mode,
    const std::array<uint8_t,48>& physical,const std::array<bool,48>& overridden) {
    co_await machine.sleep(16);
    const auto info=graphs[index];co_await xl_test::select(machine,op,info.bank,info.program);
    // Start from bank factory controls, using physical faders only when the
    // firmware's selected variation differs. Record settling is part of the
    // recipe; the native side starts after these preparation operations.
    for(unsigned p=0;p<data.page_count;++p) for(unsigned slot=0;slot<6;++slot) {
        const auto& page=data.pages[p];const unsigned cell=page.cells[slot];
        if(cell>=48) continue;
        if(machine.peek(uint16_t(op.recordBase()+cell))!=data.controls.factory[cell])
            co_await op.moveSlider(int(p+1),slot,data.controls.factory[cell]);
    }
    if(!co_await op.recordSettled()) co_await fail("XL factory controls did not settle");
    for(unsigned p=0;p<data.page_count;++p) for(unsigned slot=0;slot<6;++slot) {
        const unsigned cell=data.pages[p].cells[slot];
        if(cell<48 && overridden[cell]) co_await op.moveSlider(int(p+1),slot,physical[cell]);
    }
    if(std::any_of(overridden.begin(),overridden.end(),[](bool v){return v;}) && !(co_await op.recordSettled()))
        co_await fail("XL physical control overrides did not settle");
    co_await set_toggle(machine,op,0,bool(mode&4));
    co_await set_toggle(machine,op,1,bool(mode&1));
    co_await set_toggle(machine,op,2,bool(mode&2));
    const auto toggles=co_await op.readToggles();
    if(toggles.value[0]!=int(bool(mode&4)) || toggles.value[1]!=int(bool(mode&1)) || toggles.value[2]!=int(bool(mode&2)))
        co_await fail("XL physical toggles differ from requested mode");
    co_await machine.sleep(0.5);
}
static void reference_state(std::ostream& out,const char* stage,const lexicon224x::cpu::Host& host) {
    auto word=[&](unsigned at){return unsigned(host.memory[at])|unsigned(host.memory[at+1])<<8;};
    out<<stage<<",reference,"<<word(0x3e47)<<','<<unsigned(host.memory[0x3e44])<<','
       <<unsigned(host.memory[0x3e45])<<','<<unsigned(host.memory[0x3e46])<<'\n';
}
int main(int argc,char** argv) try {
    const char* usage="usage: cineol_xl_sound_compare ROM_DIRECTORY BANK OUTPUT XL_INDEX MODE FIXTURE SEED AMPLITUDE WARMUP_MS [DURATION_S=12] [BLOCK=256] [ANALOG=1] [OPTIONS]\n"
        "MODE: bit 0 Mod Enh, bit 1 Decay Opt, bit 2 Dynamic Decay. FIXTURE: noise, impulse, impulse-left, impulse-right, music, file.\n"
        "Independent native/reference clocks; wet A/C by default, 0 dB gain. Outputs remain private.\n"
        "--static-wcs: mode-0 reference WCS diagnostic, independent clocks/empty delay memory.\n"
        "--audible-levels: physical level faders at 128 (use for muted Inverse Room).\n"
        "--gate-controls: physical stop faders at 18 and stop-delay fader at 10, for reverbs.\n"
        "--control=CELL:RAW: physical fader override, calibrated logical value recorded.\n"
        "--input-wav=PATH: file fixture, canonical stereo float32 WAV at 48 kHz; AMPLITUDE scales the file.\n"
        "--outputs=LEFT:RIGHT: select DAC channels 0..3 on both paths. Options follow positionals.\n";
    // File fixtures require the controlled canonical WAV format above;
    // AMPLITUDE scales it, and at least one second of stopped input is retained.
    if(argc==2 && std::string(argv[1])=="--help") {std::cout<<usage;return 0;}
    bool diagnostic=false,audible=false,gate=false;unsigned left=0,right=2;
    std::filesystem::path source;
    std::array<uint8_t,48> physical{};std::array<bool,48> overridden{};
    while(argc>1) {
        const std::string option=argv[argc-1];
        if(option=="--static-wcs") diagnostic=true;
        else if(option=="--audible-levels") audible=true;
        else if(option=="--gate-controls") gate=true;
        else if(option.starts_with("--input-wav=")) {check(source.empty(),"Duplicate input WAV option");source=option.substr(12);check(!source.empty(),"Empty input WAV path");}
        else if(option.starts_with("--control=") || option.starts_with("--outputs=")) {
            constexpr unsigned begin=10;
            const auto colon=option.find(':',begin);check(colon!=std::string::npos,"Invalid control/output override");
            const unsigned first=unsigned(std::stoul(option.substr(begin,colon-begin))),second=unsigned(std::stoul(option.substr(colon+1)));
            if(option.starts_with("--outputs=")) {check(first<4 && second<4,"Invalid output channels");left=first;right=second;}
            else {check(first<48 && first!=42 && second<256 && !overridden[first],"Invalid/duplicate physical control override");physical[first]=uint8_t(second);overridden[first]=true;}
        } else break;
        --argc;
    }
    check(argc>=10 && argc<=13,usage);
    const unsigned index=unsigned(std::stoul(argv[4])),mode=unsigned(std::stoul(argv[5]));
    check(index<graphs.size() && mode<8,"Invalid XL graph/mode");
    check(!diagnostic || mode==0,"Static WCS diagnostic requires mode 0");
    const std::string fixture=argv[6];check(fixture=="noise" || fixture=="impulse" || fixture=="impulse-left" || fixture=="impulse-right" || fixture=="music" || fixture=="file","Invalid fixture");
    check((fixture=="file")==!source.empty(),"File fixture requires --input-wav=PATH exclusively");
    const uint32_t seed=uint32_t(std::stoul(argv[7]));uint32_t random=seed;
    const float amplitude=std::stof(argv[8]);const unsigned warmup_ms=unsigned(std::stoul(argv[9]));
    const unsigned seconds=argc>10?unsigned(std::stoul(argv[10])):12,block=argc>11?unsigned(std::stoul(argv[11])):256;
    const unsigned analog=argc>12?unsigned(std::stoul(argv[12])):1;
    check(std::isfinite(amplitude) && amplitude>0 && amplitude<=1 && warmup_ms<=10000 && seconds>=3 && seconds<=60 &&
          block>0 && block<=4096 && analog<=1,"Invalid level/warmup/duration/block/analog");
    const auto external=source.empty()?Audio{}:input_wav(source,(seconds-1)*48000);
    std::ifstream file(argv[2],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<Bank>();check(read_bank(bytes.data(),bytes.size(),*bank),"Invalid XL bank");
    const auto& data=bank->programs[index];const auto info=graphs[index];
    check(data.dynamics.enabled || !(mode&6),"Reverb dynamics requested for a non-reverb graph");
    if(audible) for(const auto& control:data.controls.slots)
        if(control.kind==ControlKind::level && !overridden[control.cell]) {physical[control.cell]=128;overridden[control.cell]=true;}
    if(gate) {
        check(data.dynamics.enabled,"Gate controls requested for a non-reverb graph");
        const unsigned stop=data.dynamics.shared_stop?6:12;
        for(unsigned cell:{stop,stop+1,45u}) if(!overridden[cell]) {physical[cell]=uint8_t(cell==45?10:18);overridden[cell]=true;}
    }
    for(unsigned cell=0;cell<48;++cell) if(overridden[cell]) check(data.control_active(cell),"Physical override targets inactive control");
    const bool custom=std::any_of(overridden.begin(),overridden.end(),[](bool v){return v;});
    const auto output=std::filesystem::path(argv[3]);check(!std::filesystem::exists(output/"fixture.csv"),"Output already contains a fixture; choose a new directory");
    std::filesystem::create_directories(output);
    auto reference=std::make_unique<Engine>(0);xl_test::load(*reference,argv[1]);reference->set_analog(bool(analog));
    auto machine=std::make_unique<Machine>(*reference);LarcOperator op(*machine);
    const auto done=machine->run_task([&]{return setup(*machine,op,data,index,mode,physical,overridden);});
    if(done.failed) throw std::runtime_error(done.error.text);
    auto& host=reference->host();xl_test::ShapeCheck shape{*host.dsp,Graph(index)};shape.run();check(shape.valid,"XL reference graph shape changed");
    auto raw=data.controls.factory;raw[42]=uint8_t((raw[42]&~0xc1u)|((mode&4)?1:0)|((mode&1)?64:0)|((mode&2)?128:0));
    // Logical fader values follow the actual physical calibration. This reads
    // operator control values only, never reference DSP/controller state.
    for(unsigned cell=0;cell<48;++cell) if(overridden[cell]) raw[cell]=machine->peek(uint16_t(op.recordBase()+cell));
    data.resolve_controls(raw);
    std::ofstream controls(output/"controls.csv");controls<<"cell,native,reference,active,physical_override\n";
    for(unsigned cell=0;cell<48;++cell) {
        const unsigned wanted=machine->peek(uint16_t(op.recordBase()+cell));
        controls<<cell<<','<<unsigned(raw[cell])<<','<<wanted<<','<<data.control_active(cell)<<',';
        if(overridden[cell]) controls<<unsigned(physical[cell]);controls<<'\n';
        if(data.control_active(cell)) check(raw[cell]==wanted,"Active XL factory control differs from firmware");
    }
    check((host.memory[op.recordBase()+42]&0xc1)==(raw[42]&0xc1),"Firmware switch record differs from requested mode");
    auto native=std::make_unique<Runtime>();native->select(*bank,index);
    native->controls(raw,bool(mode&1),0,1,float(analog),int(left),int(right),bool(mode&2));
    // This is a labelled diagnostic, not plugin behavior: freeze only WCS
    // settings at the reference's prepared state. Do not clone converter
    // clocks, graph memory, counters or the firmware's audio state.
    auto frozen=diagnostic?frozen_wcs(Graph(index),*host.dsp,bool(analog),int(left),int(right)):nullptr;
    std::ofstream wcs(output/"wcs.csv");wcs<<"row,bank_coefficient,reference_coefficient,bank_offset,reference_offset,interpolation_row\n";
    for(unsigned row=0;row<info.rows;++row) {
        const auto mi=lexicon224x::decode(host.dsp->wcs[row]);bool interpolation=false;
        for(unsigned tap=0;tap<(data.modulation.flags&15);++tap)
            interpolation|=row==data.modulation.rows[tap] || row==unsigned(data.modulation.rows[tap])+1;
        wcs<<row<<','<<int(data.coefficients[row])<<','<<(mi.negative?-int(mi.coefficient):int(mi.coefficient))<<','
            <<data.offsets[row]<<','<<uint16_t(~mi.low)<<','<<interpolation<<'\n';
    }
    check(bool(wcs),"Cannot write WCS audit");
    std::ofstream states(output/"startup.csv");states<<"stage,variant,index,divider,random_divider,random_hold\n";
    const auto& initial=data.initial_modulation;
    states<<"program_select,bank_seed,"<<initial.index<<','<<unsigned(initial.divider)<<','<<unsigned(initial.random_divider)<<','
        <<unsigned(initial.random_hold)<<'\n';
    reference_state(states,"program_select",host);
    std::array<float,4096> zero{};std::array<std::array<float,4096>,4> scratch{};
    float* ptr[]={scratch[0].data(),scratch[1].data(),scratch[2].data(),scratch[3].data()};
    for(unsigned n=0;n<warmup_ms*48;n+=block) {
        const unsigned count=std::min(block,warmup_ms*48-n);reference->render(zero.data(),zero.data(),ptr,int(count));
        tracking=true;for(unsigned i=0;i<count;++i) {float l,r;native->process(0,0,l,r,true);if(frozen) frozen->process(0,0,l,r);}tracking=false;
    }
    reference_state(states,"input_start",host);check(bool(states) && bool(controls),"Cannot write state/control metadata");
    const auto start_cycles=host.cycles;
    const unsigned frames=seconds*48000;Audio input,result,original,frozen_audio;
    for(auto* audio:{&input,&result,&original}) for(auto& channel:*audio) channel.resize(frames);
    if(frozen) for(auto& channel:frozen_audio) channel.resize(frames);
    if(fixture=="file") {
        for(unsigned c=0;c<2;++c)for(unsigned n=0;n<external[c].size();++n)input[c][n]=external[c][n]*amplitude;
    } else if(fixture.starts_with("impulse")) {
        if(fixture!="impulse-right") input[0][0]=amplitude;
        if(fixture!="impulse-left") input[1][fixture=="impulse"?480:0]=amplitude;
    }
    else if(fixture=="noise") {
        for(unsigned n=0;n<9600;++n) for(unsigned c=0;c<2;++c) {
            random=random*1664525u+1013904223u;input[c][n]=amplitude*float(int32_t(random))/2147483648.f;
        }
    } else {
        for(unsigned n=0;n<96000;++n) {
            random=random*1664525u+1013904223u;const float noise=float(int32_t(random))/2147483648.f;
            if(n<8000) input[0][n]+=amplitude*noise*std::exp(-float(n)/1500);
            if(n>=24000 && n<32000) input[1][n]+=amplitude*noise*std::exp(-float(n-24000)/1500);
            if(n>=48000) {
                const float t=(n-48000)/48000.f,e=std::min(1.f,t*20)*std::exp(-t*5);
                for(unsigned c=0;c<2;++c) input[c][n]+=amplitude*.29f*e*(std::sin(6.28318530718f*220*t+c*.4f)+
                    std::sin(6.28318530718f*277.1826f*t+c*.7f)+std::sin(6.28318530718f*329.6276f*t+c*.9f));
            }
        }
    }
    std::array<unsigned,3> calls{};host.pc_watches[0xad5c]=host.pc_watches[0x82cf]=host.pc_watches[0x81b6]=true;
    host.pc_observer=[&](uint64_t,lexicon224x::cpu::CpuSnapshot cpu) {
        const unsigned i=cpu.pc==0xad5c?0:cpu.pc==0x82cf?1:2;++calls[i];host.pc_watches[cpu.pc]=true;
    };
    for(unsigned n=0;n<frames;n+=block) {
        const unsigned count=std::min(block,frames-n);reference->render(input[0].data()+n,input[1].data()+n,ptr,int(count));
        tracking=true;
        for(unsigned i=0;i<count;++i) {
            native->process(input[0][n+i],input[1][n+i],result[0][n+i],result[1][n+i],true);
            if(frozen) frozen->process(input[0][n+i],input[1][n+i],frozen_audio[0][n+i],frozen_audio[1][n+i]);
        }
        tracking=false;
        std::copy_n(scratch[left].begin(),count,original[0].begin()+n);std::copy_n(scratch[right].begin(),count,original[1].begin()+n);
    }
    check(allocations==0 && releases==0,"Allocation/release during native processing");
    for(const auto* audio:{&original,&result,&frozen_audio}) for(const auto& channel:*audio) for(float sample:channel)
        check(std::isfinite(sample) && std::abs(sample)<4,"Nonfinite/runaway render");
    reference_state(states,"render_end",host);
    wav(output/"input.wav",input);wav(output/"reference.wav",original);wav(output/"native.wav",result);
    if(frozen) wav(output/"static_reference_wcs.wav",frozen_audio);
    std::ofstream meta(output/"fixture.csv");
    meta<<"schema,program,bank,physical_program,rows,mode,fixture,seed,amplitude,warmup_ms,duration_s,rate,block,analog,input_db,mix,left,right,bank_version,bank_sha256,protocol,reference_setup_frames,reference_start_cycles,reference_end_cycles,mod_calls,slow_calls,fast_calls,native_mod_rate_hz,native_slow_rate_hz,native_fast_rate_hz,native_alignment_samples,allocations,releases,static_wcs_diagnostic,control_fixture";
    if(fixture=="file")meta<<",input_stop_s";
    meta<<'\n';
    meta<<std::setprecision(12)<<"xl-independent-v1,"<<index<<','<<info.bank<<','<<info.program<<','<<info.rows<<','<<mode<<','<<fixture<<','
        <<seed<<','<<amplitude<<','<<warmup_ms<<','<<seconds<<",48000,"<<block<<','<<analog<<",0,1,"<<left<<','<<right<<','<<BankHeader{}.version<<','
        <<lexplug::roms::Sha256::of(bytes.data(),bytes.size())<<','<<(custom?"physical-factory-overrides-toggles-settle500":"physical-factory-toggles-settle500")<<','<<machine->frame()<<','
        <<start_cycles<<','<<host.cycles<<','<<calls[0]<<','<<calls[1]<<','<<calls[2]<<','<<data.modulation.rate_tenths*.1<<','
        <<data.dynamics.slow_rate_tenths*.1<<','<<data.dynamics.fast_rate_tenths*.1<<','<<Runtime::latency_samples<<','<<allocations<<','<<releases<<','<<diagnostic<<','<<(custom?"physical_overrides":"factory");
    if(fixture=="file")meta<<','<<external[0].size()/48000.0;
    meta<<'\n';
    check(bool(meta),"Cannot write fixture metadata");
    std::cout<<info.name<<" mode="<<mode<<" fixture="<<fixture<<" seed="<<seed<<" rendered; native allocation/release=0; independent clocks\n";
} catch(const std::exception& error) {tracking=false;std::cerr<<error.what()<<'\n';return 1;}
