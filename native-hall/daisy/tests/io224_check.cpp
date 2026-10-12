// Compare only boundaries with the actual current Cineol-X desktop policies.
// No Reflexion oracle or changes to Hall/controller behavior.
#undef NDEBUG
#include "../src/Io224.hpp"
#include "../../desktop/engine22448.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <new>
#include <vector>

static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n){if(tracking)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {if(tracking && p)++releases;std::free(p);}
void operator delete(void* p,size_t) noexcept {::operator delete(p);}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
static void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
constexpr double pi=3.14159265358979323846;
struct Metrics {
    double target=0,actual=0,old=0,error=0,peak=0;
    unsigned count=0,words=0,detectors=0;
    void add(double t,double a,double o) {
        require(std::isfinite(t)&&std::isfinite(a)&&std::isfinite(o),"nonfinite boundary output");
        target+=t*t;actual+=a*a;old+=o*o;error+=(a-t)*(a-t);peak=std::max(peak,std::abs(a-t));++count;
    }
    void write(std::ostream& out,const char* boundary,int network,int frequency,int channel) {
        out<<boundary<<','<<network<<','<<frequency<<','<<channel<<','<<count<<','
           <<10*std::log10(actual/target)<<','<<10*std::log10(old/target)<<','
           <<10*std::log10(error/target)<<','<<peak<<','<<words<<','<<detectors<<'\n';
    }
};
static void wav(const std::string& path,const std::vector<std::array<float,2>>& values) {
    std::ofstream out(path,std::ios::binary);require(bool(out),"cannot create WAV");
    auto word=[&](uint32_t n,unsigned bytes){for(unsigned k=0;k<bytes;++k)out.put(char(n>>(8*k)));};
    out.write("RIFF",4);word(36+values.size()*8,4);out.write("WAVEfmt ",8);word(16,4);
    word(3,2);word(2,2);word(48000,4);word(48000*8,4);word(8,2);word(32,2);
    out.write("data",4);word(values.size()*8,4);out.write(reinterpret_cast<const char*>(values.data()),values.size()*8);
}

template<class Engine> static double benchmark(Engine& engine,const native_hall::ProgramBank& bank,
                                               unsigned program,unsigned mode,double& checksum) {
    engine.prepare(bank,program);native_hall::Parameters p;p.program=program;
    p.hall.predelay_ms=native_hall::predelay_minima[program];p.analog=mode!=2;
    p.hall.mode_enhancement=p.hall.decay_optimization=mode!=0;engine.set_parameters(p);
    for(unsigned i=0;i<4800;++i){float l,r;engine.process(0,0,l,r);}
    uint32_t random=17;checksum=0;tracking=true;
    const auto begin=std::chrono::steady_clock::now();
    for(unsigned i=0;i<96000;++i) {
        random=random*1664525u+1013904223u;
        const float x=i<48000?.1f*float(int32_t(random))/2147483648.f:0;
        float l,r;engine.process(x,-.7f*x,l,r);checksum+=l+r;
    }
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    tracking=false;require(std::isfinite(checksum),"nonfinite engine benchmark");return seconds;
}

int main(int argc,char** argv) {
    using namespace native_hall;
    require(argc==3||(argc==4&&std::string(argv[3])=="--quick"),"usage: io224_check BANK OUTPUT_DIRECTORY [--quick]");
    const bool quick=argc==4;
#if defined(__aarch64__)
    uint64_t fpcr=0;asm volatile("mrs %0, fpcr":"=r"(fpcr));fpcr|=uint64_t(1)<<24;
    asm volatile("msr fpcr, %0"::"r"(fpcr));
#endif
    const std::string directory=argv[2];
    std::ofstream csv(directory+"/boundaries.csv");require(bool(csv),"cannot write metrics");
    csv<<std::setprecision(12)<<"boundary,network,frequency,channel,count,new_gain_error_db,old_gain_error_db,new_relative_error_db,peak_error,adc_word_differences,detector_differences\n";
    auto desktop=std::make_unique<Adc48>();auto embedded=std::make_unique<DaisyInput224>();
    auto legacy=std::make_unique<LegacyInput48>();
    const int frequencies[]={-2,-1,0,40,100,1000,4000,6000,8000,9000,9600,10000,12000,16000,20000};
    for(int f:frequencies) {
        desktop->prepare();embedded->prepare();legacy->prepare();uint32_t random=17;unsigned passes=0;
        Metrics metrics[2],silent[2];
        for(unsigned frame=0;frame<96000;++frame) {
            float raw[2];
            for(unsigned c=0;c<2;++c) {
                random=random*1664525u+1013904223u;
                if(f>0)raw[c]=float(.1*std::sin(2*pi*f*frame/48000+c*.9));
                else if(f==0)raw[c]=frame<48000?.5f*float(int32_t(random))/2147483648.f:0;
                else if(f==-1)raw[c]=(frame==c+1 || frame==c+33)?(c?-.75f:1.f):0;
                else raw[c]=frame<48000?float((int(frame/137)%13-6)*.112):0;
            }
            double t[2]{};float a[2]{},o[2]{};unsigned nt=0,na=0,no=0;tracking=true;
            desktop->process(raw,[&](const double* v){++nt;t[0]=v[0];t[1]=v[1];});
            embedded->process(raw,[&](const float* v){++na;a[0]=v[0];a[1]=v[1];});
            legacy->process(raw,[&](const float* v){++no;o[0]=v[0];o[1]=v[1];});tracking=false;
            require(nt==na&&nt==no,"input conversion cadence differs");passes+=nt;
            if(nt)for(unsigned c=0;c<2;++c) {
                // Tone gain after warmup; transient fixtures include startup.
                if(f<=0 || frame>=12000) {
                    metrics[c].add(t[c],a[c],o[c]);
                    metrics[c].words+=Adc48::adc(t[c])!=DaisyInput224::adc(a[c]);
                    metrics[c].detectors+=Adc48::detectors(t[c])!=DaisyInput224::detectors(a[c]);
                }
                if(f<=0&&frame>=72000)silent[c].add(t[c],a[c],o[c]);
            }
        }
        require(passes==40960,"input phase drift");
        for(unsigned c=0;c<2;++c) {
            metrics[c].write(csv,"input",0,f,c);require(metrics[c].peak<2e-6,"input approximation too large");
            if(f<=0) {silent[c].write(csv,"input_silent",0,f,c);require(silent[c].peak<1e-7,"input silent residual error");}
        }
    }
    for(unsigned network=0;network<4;++network)for(int f:frequencies) {
        DesktopOutput48 target;DaisyOutput224 actual;LegacyOutput48 old;
        target.prepare();actual.prepare();old.prepare();target.select_network(network);actual.select_network(network);
        Metrics metrics[4],silent[4];int32_t phase=0;unsigned pass=0;uint32_t random=17;
        for(unsigned frame=0;frame<96000;++frame) {
            tracking=true;
            if(phase<12000000) {
                float v[4];
                for(unsigned c=0;c<4;++c) {
                    random=random*1664525u+1013904223u;
                    if(f>0)v[c]=float(.1*std::sin(2*pi*f*pass/20480+c*.9));
                    else if(f==0)v[c]=frame<48000?.5f*float(int32_t(random))/2147483648.f:0;
                    else if(f==-1)v[c]=(pass==c+1 || pass==c+33)?(c&1?-.75f:1.f):0;
                    else v[c]=frame<48000?float((int(pass/137)%13-6)*.112):0;
                }
                target.push(v);actual.push(v);old.push(v);phase+=28125000;++pass;
            }
            phase-=12000000;
            const auto t=target.sample(),a=actual.sample(),o=old.sample();tracking=false;
            for(unsigned c=0;c<4;++c) {
                if(f<=0||frame>=12000)metrics[c].add(t[c],a[c],o[c]);
                if(f<=0&&frame>=72000)silent[c].add(t[c],a[c],o[c]);
            }
        }
        require(pass==40960,"output cadence drift");
        for(unsigned c=0;c<4;++c) {
            metrics[c].write(csv,"output",network,f,c);
            require(metrics[c].peak<2e-6,"output approximation too large");
            if(f<=0) {silent[c].write(csv,"output_silent",network,f,c);require(silent[c].peak<1e-7,"output silent residual error");}
        }
    }
    // Long-run resets and network switches on arbitrary host phases.
    desktop->reset();embedded->reset();DesktopOutput48 target;DaisyOutput224 actual;
    target.prepare();actual.prepare();int32_t phase=0;
    for(unsigned n=0;n<1000000;++n) {
        tracking=true;
        if(n%170003==0) {desktop->reset();embedded->reset();target.reset();actual.reset();phase=0;}
        if(n%719==0) {target.select_network((n/719)%4);actual.select_network((n/719)%4);}
        const float raw[]={float(n%17)*.03f,-float(n%31)*.02f};double t[2]{};unsigned nt=0,na=0;
        desktop->process(raw,[&](const double* v){++nt;t[0]=v[0];t[1]=v[1];});
        embedded->process(raw,[&](const float* v){++na;for(unsigned c=0;c<2;++c)require(std::abs(v[c]-t[c])<2e-6,"input reset error");});
        require(nt==na,"input reset cadence");
        if(phase<12000000) {float v[]={raw[0],raw[1],-raw[0],-raw[1]};target.push(v);actual.push(v);phase+=28125000;}
        phase-=12000000;const auto t4=target.sample(),a4=actual.sample();
        for(unsigned c=0;c<4;++c)require(std::abs(t4[c]-a4[c])<2e-6,"output reset/switch error");
        tracking=false;
    }
    std::ifstream bank_file(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(bank_file)),{});
    auto bank=std::make_unique<ProgramBank>();require(read_bank(bytes.data(),bytes.size(),*bank),"invalid bank");
    // Use the exact desktop boundaries with the SAME Hall core on both sides.
    // The plugin's desktop controller scheduler is outside this filter check.
    using FilterReference=Engine48WithOutput<DesktopOutput48,Adc48>;
    auto reference_engine=std::make_unique<FilterReference>();auto daisy_engine=std::make_unique<DaisyEngine224>();
    auto old_engine=std::make_unique<Engine48>();
    std::ofstream bench(directory+"/cpu.csv");bench<<std::setprecision(12)<<"program,mode,repeat,variant,seconds,checksum,state_bytes\n";
    if(!quick)for(unsigned program=0;program<6;++program)for(unsigned mode=0;mode<3;++mode)for(unsigned repeat=0;repeat<3;++repeat) {
        // Alternate ordering to reduce simple temperature/order bias.
        for(unsigned step=0;step<3;++step) {
            const unsigned variant=(step+repeat)%3;double sum=0,seconds=0;size_t size=0;
            if(variant==0){seconds=benchmark(*old_engine,*bank,program,mode,sum);size=sizeof(*old_engine);}
            if(variant==1){seconds=benchmark(*daisy_engine,*bank,program,mode,sum);size=sizeof(*daisy_engine);}
            if(variant==2){seconds=benchmark(*reference_engine,*bank,program,mode,sum);size=sizeof(*reference_engine);}
            bench<<program<<','<<mode<<','<<repeat<<','<<variant<<','<<seconds<<','<<sum<<','<<size<<'\n';
        }
    }
    // Narrow listening pairs: same Hall and controls, unnormalized + matched gain.
    if(!quick)for(unsigned program:{2u,4u}) {
        reference_engine->prepare(*bank,program);daisy_engine->prepare(*bank,program);
        Parameters p;p.program=program;p.hall.predelay_ms=predelay_minima[program];
        reference_engine->set_parameters(p);daisy_engine->set_parameters(p);
        for(unsigned n=0;n<4800;++n){float l,r;reference_engine->process(0,0,l,r);daisy_engine->process(0,0,l,r);}
        std::vector<std::array<float,2>> ref(288000),emb(288000);uint32_t random=17;double rp=0,ep=0;
        tracking=true;
        for(unsigned n=0;n<ref.size();++n) {
            random=random*1664525u+1013904223u;
            const float envelope=n<48000?float(std::exp(-double(n%12000)/1800)):0;
            const float x=envelope*(.15f*float(int32_t(random))/2147483648.f+.1f*std::sin(float(n)*.071f));
            reference_engine->process(x,-.7f*x,ref[n][0],ref[n][1]);daisy_engine->process(x,-.7f*x,emb[n][0],emb[n][1]);
            for(unsigned c=0;c<2;++c){require(std::isfinite(ref[n][c])&&std::isfinite(emb[n][c]),"nonfinite tail");rp+=double(ref[n][c])*ref[n][c];ep+=double(emb[n][c])*emb[n][c];}
        }
        tracking=false;
        const auto stem=directory+"/program-"+std::to_string(program+1);
        wav(stem+"-cineol-boundaries.wav",ref);wav(stem+"-daisy.wav",emb);
        const float match=float(std::sqrt(rp/ep));for(auto& frame:emb)for(auto& v:frame)v*=match;
        wav(stem+"-daisy-matched.wav",emb);
    }
    require(allocations==0&&releases==0,"allocation/release in processing");
    std::cout<<"Input/output frequencies, transients, silence, four networks, 1000000-frame resets/switches pass; audio allocations/releases=0\n"
             <<"State bytes: legacy="<<sizeof(Engine48)<<" new="<<sizeof(DaisyEngine224)<<" desktop-boundary="<<sizeof(FilterReference)<<'\n';
}
