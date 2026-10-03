#include "../core/engine48.hpp"
#include <isa-level-cpp/lexicon224x.hpp>
#include <analog/filters_224.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <new>
#include <cstdlib>

static thread_local bool count_allocations=false;
static unsigned allocations=0;
void* operator new(size_t n) {if(count_allocations) ++allocations;if(void* p=std::malloc(n?n:1)) return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,size_t) noexcept {std::free(p);}
void* operator new[](size_t n) {return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}

static void require(bool good,const char* message) {if(!good) {std::cerr<<message<<'\n';std::exit(1);}}
static void boundary_check() {
    // Compare float modal pairs with the independent double implementation,
    // driven by the same 48 kHz first-order hold (not the event-time board).
    for(bool input:{false,true}) {
        auto modal=input?lexicon224x::analog::ain_224_modal():lexicon224x::analog::aout_224_modal();
        lexicon224x::analog::Chain reference(modal);native_hall::Analog48 native;native.prepare(input);
        double previous=0,error=0;
        for(int n=0;n<48000;++n) {
            float value=n<12000?0.1f*std::sin(n*0.119f)+0.03f*std::sin(n*0.91f):0;
            reference.advance(12000000,previous,(value-previous)*48000);
            error=std::max(error,std::abs(native.process(value)-reference.output(value)));previous=value;
        }
        require(error<3e-5,"48 kHz modal filter differs from independent double reference");
        std::cout<<(input?"AIN":"AOUT")<<" 48 kHz FOH float/double peak error="<<error<<'\n';
    }
    // Streaming rational clock and stopband must survive arbitrary run length.
    for(int frequency:{1000,16000}) {
        native_hall::RationalFilter<32,75,64,2> down;
        native_hall::RationalFilter<75,32,32,2> up;down.prepare();up.prepare();
        int native_count=0,host_count=0;double power=0;
        for(int n=0;n<48000;++n) {
            float input[]={std::sin(float(n*6.283185307179586*frequency/48000)),0};
            down.process(input,[&](const float* sample) {
                ++native_count;
                up.process(sample,[&](const float* frame) {if(host_count++>1000) power+=double(frame[0])*frame[0];});
            });
        }
        require(native_count==20480 && host_count==48000,"rational SRC sample clock drifted");
        double amplitude=std::sqrt(power/(host_count-1001)*2);
        require(frequency==1000?std::abs(amplitude-1)<0.002:amplitude<0.001,"SRC passband or stopband failed");
        std::cout<<"SRC "<<frequency<<" Hz amplitude="<<amplitude<<", sample counts exact\n";
    }
    for(int n=-10000;n<=10000;++n) {
        float sample=n/9999.0f;auto code=lexicon224x::analog::convert(double(sample)*5);
        int signed_code=int(code.code);if(signed_code&0x800) signed_code-=4096;
        int expected=signed_code*int(1u<<(4-code.iga));
        // float and double rounding can differ by one code at half-LSB ties.
        require(std::abs(int(native_hall::Engine48::adc(sample))-expected)<=int(1u<<(4-code.iga)),
            "ADC thresholds/gain compensation differ");
        int bare=int(std::lround(std::clamp(sample,-1.0f,1.0f)*2047.0f))*16;
        require(native_hall::Engine48::adc(sample,false)==bare,"bypass ADC differs from reference nearest-sample path");
    }
    std::cout<<"ADC: 20001 amplitudes match within one mantissa LSB\n";
    std::cout<<"Bypass ADC: 20001 amplitudes match the fixed-gain reference exactly\n";
}
static void bypass_spectrum_check(const native_hall::Profile& p) {
    // A tone above the native Nyquist must fold and leave a DAC image in
    // reference bypass mode. Leaving either FIR active hid these artifacts.
    double amplitude[2][2]{};
    for(int mode=0;mode<2;++mode) {
        auto e=std::make_unique<native_hall::Engine48>();e->prepare(p);
        native_hall::Parameters parameters;parameters.analog=mode!=0;
        parameters.hall.mode_enhancement=parameters.hall.decay_optimization=false;e->set_parameters(parameters);
        double real[2]{},imaginary[2]{};
        constexpr double frequencies[]={8480,12000};
        for(int n=0;n<3*48000;++n) {
            float left,right,input=0.05f*std::sin(n*6.283185307179586*12000/48000);
            e->process(input,input,left,right);
            if(n<2*48000) continue;
            for(unsigned k=0;k<2;++k) {
                double phase=n*6.283185307179586*frequencies[k]/48000;
                real[k]+=left*std::cos(phase);imaginary[k]+=left*std::sin(phase);
            }
        }
        for(unsigned k=0;k<2;++k) amplitude[mode][k]=2*std::hypot(real[k],imaginary[k])/48000;
    }
    for(unsigned k=0;k<2;++k) {
        std::cout<<(k?"12 kHz DAC image":"8.48 kHz input alias")<<": bypass="<<amplitude[0][k]
                 <<", filtered="<<amplitude[1][k]<<'\n';
        require(amplitude[0][k]>1e-6 && amplitude[0][k]>amplitude[1][k]*10,
            "analog bypass still suppresses input aliasing or DAC images");
    }
}
int main(int argc,char** argv) {
    require(argc==2,"usage: core_check PROFILE.hall224");
    std::ifstream in(argv[1],std::ios::binary);
    std::vector<char> data((std::istreambuf_iterator<char>(in)),{});
    native_hall::Profile p;
    require(native_hall::read_profile(data.data(),data.size(),p),"profile validation");
    auto broken=data;broken.back()^=1;native_hall::Profile rejected;
    require(!native_hall::read_profile(broken.data(),broken.size(),rejected),"corrupt profile accepted");
    boundary_check();
    auto h=std::make_unique<native_hall::Hall>();h->prepare(p);
    native_hall::Controls controls;controls.mode_enhancement=controls.decay_optimization=false;
    h->set_controls(controls);
    // Independent row-machine reference, with original opcode topology decoded
    // from the exported program. Reconstruct op fields from the specialized
    // network fixture; the fixture is generated separately from captured WCS.
    auto m=std::make_unique<lexicon224x::Machine>();m->model=lexicon224x::Model::Lexicon224;
    std::ifstream image(std::string(argv[1])+".wcs",std::ios::binary);
    uint8_t bytes[512];require(bool(image.read(reinterpret_cast<char*>(bytes),512)),"missing reference WCS image");
    lexicon224x::load_wcs(*m,bytes);
    for(unsigned r=0;r<100;++r) {
        int c=h->coefficients()[r];
        m->wcs[r]=(m->wcs[r]&~(0xfc000000u|0x800000u)) | uint32_t(std::abs(c))<<26 | (c<0?0x800000u:0);
    }
    uint32_t random=7;
    for(int frame=0;frame<12000;++frame) {
        random=random*1664525u+1013904223u;int16_t left=int16_t(random>>16);
        random=random*1664525u+1013904223u;int16_t right=int16_t(random>>16);
        int16_t expected[4]{},actual[4];
        for(unsigned r=0;r<100;++r) {
            lexicon224x::fetch(*m);lexicon224x::converter_clock(*m);
            if(r==0) m->fpc.input_sample=uint16_t(left);
            if(r==50) m->fpc.input_sample=uint16_t(right);
            if(m->mi.wr_da) for(unsigned c=0;c<4;++c) if(m->mi.channels&(1u<<c))
                expected[c]=int16_t(lexicon224x::source_value(*m,m->mi));
            lexicon224x::execute(*m);
        }
        h->process(left,right,actual);
        require(std::equal(actual,actual+4,expected),"native network differs from independent DSP model");
        require(h->accumulator()==m->ACC && h->result()==m->RR,"ARU pipeline state differs");
    }
    require(std::equal(h->memory().begin(),h->memory().end(),m->memory,
        [](int16_t a,uint16_t b){return uint16_t(a)==b;}),"delay-memory state differs");
    std::cout<<"Native topology/partial-product saturation: 12000 full-scale stereo frames exact\n";
    for(int s=-32768;s<=32767;++s) {
        lexicon224x::Converter f;
        f.busy=false;f.new_data=true;f.waiting=uint16_t(s);f.waiting_channels=1;
        for(int k=0;k<11;++k) lexicon224x::clock_converter(f,0,0,0,0,false,false,0,0);
        int mantissa=int(f.dac_code()^0x800);if(mantissa&0x800) mantissa-=0x1000;
        int expected=mantissa*int(1u<<(4-(f.output_gain&3)));
        require(native_hall::Engine48::dac(int16_t(s))==expected,"DAC gain/quantization differs");
    }
    std::cout<<"DAC: all 65536 input words exact\n";
    bypass_spectrum_check(p);
    auto e=std::make_unique<native_hall::Engine48>();e->prepare(p);
    native_hall::Parameters settings;
    float peak=0;double energy=0;
    auto begin=std::chrono::steady_clock::now();
    count_allocations=true;
    for(int n=0;n<480000;++n) {
        if(n%8000==0) {
            settings.hall.bass=1+(n/8000)%31;settings.hall.mid=(n/8000)%32;
            settings.hall.depth=(n/8000)%72;settings.hall.diffusion=1+(n/8000)%63;
            settings.hall.predelay_ms=24+(n/8000)%129; settings.analog=(n/8000)%2;
            e->set_parameters(settings);
        }
        float l,r;e->process(n<24000?0.15f*std::sin(n*0.31f):0,n==0?0.5f:0,l,r);
        require(std::isfinite(l)&&std::isfinite(r),"nonfinite 48 kHz output");
        peak=std::max({peak,std::abs(l),std::abs(r)});energy+=double(l)*l+double(r)*r;
    }
    count_allocations=false;
    double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    require(allocations==0,"allocation during processing/control updates");
    require(energy>0.01 && peak<4,"unexpected silence or runaway output");
    std::cout<<"48 kHz stereo/control stress: finite, allocations=0, peak="<<peak
             <<", elapsed="<<seconds<<"s for 10s audio\n";
    e->prepare(p);settings=native_hall::Parameters{};settings.mix=0;e->set_parameters(settings);
    for(int n=0;n<16000;++n) {float l,r;e->process(0,0,l,r);}
    for(int n=0;n<128;++n) {
        float l,r;e->process(n==0?0.3f:0,n==0?-0.2f:0,l,r);
        require(std::abs(l-(n==70?0.3f:0))<1e-6f && std::abs(r-(n==70?-0.2f:0))<1e-6f,
            "dry signal altered or latency differs from reported 70 samples");
    }
    std::cout<<"Dry path: unity gain, exact 70-sample delay\n";
    std::cout<<"Storage: Hall="<<sizeof(native_hall::Hall)<<", Engine48="<<sizeof(native_hall::Engine48)
             <<", Profile="<<sizeof(p)<<" bytes\n";
}
