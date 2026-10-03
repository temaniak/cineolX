#include "../core/engine48.hpp"
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

static void require(bool ok,const char* message) {
    if(!ok) {std::cerr<<message<<'\n';std::exit(1);}
}

// Frozen absolute-clock implementation from before the phase optimization.
// Keep its scheduling and tap indexing independent of the production version.
template<int Up,int Down,int Taps,int Channels> class ReferenceFilter {
public:
    void prepare() {
        constexpr double pi=3.14159265358979323846;
        double cutoff=0.94*std::min(1.0,double(Up)/Down);
        for(int p=0;p<Up;++p) {
            double sum=0;
            for(int j=0;j<Taps;++j) {
                double d=double(j)-double(p)/Up-double(Taps/2-1);
                double a=cutoff*d;
                double sinc=a==0?1:std::sin(pi*a)/(pi*a);
                double r=d/(Taps/2),window=0;
                if(std::abs(r)<1) window=i0(8*std::sqrt(1-r*r))/i0(8);
                weights_[p][j]=float(cutoff*sinc*window);sum+=weights_[p][j];
            }
            for(float& v:weights_[p]) v=float(v/sum);
        }
        reset();
    }
    void reset() {for(auto& h:history_) h.fill(0);count_=next_=0;}
    template<class Emit> void process(const float* input,Emit&& emit) {
        uint64_t n=count_++;
        for(int c=0;c<Channels;++c) history_[c][n%Taps]=input[c];
        while(next_/Up<=n) {
            const auto& weights=weights_[next_%Up];float out[Channels]{};
            for(int j=0;j<Taps;++j) {
                unsigned slot=unsigned((n+1+j)%Taps);
                for(int c=0;c<Channels;++c) out[c]+=history_[c][slot]*weights[j];
            }
            emit(out);next_+=Down;
        }
    }
private:
    static double i0(double x) {
        double sum=1,t=1;for(int k=1;k<32;++k) {t*=x*x/(4*k*k);sum+=t;}return sum;
    }
    std::array<std::array<float,Taps>,Up> weights_{};
    std::array<std::array<float,Taps>,Channels> history_{};
    uint64_t count_=0,next_=0;
};

static float random_sample(uint32_t& state) {
    state=state*1664525u+1013904223u;
    return float(int16_t(state>>16))/32768.0f;
}

template<int Up,int Down,int Taps,int Channels> static void check_filter() {
    ReferenceFilter<Up,Down,Taps,Channels> reference;
    native_hall::RationalFilter<Up,Down,Taps,Channels> actual;
    reference.prepare();actual.prepare();uint32_t random=17;
    constexpr unsigned maximum_outputs=(Up+Down-1)/Down;
    for(unsigned n=0;n<1000000;++n) {
        std::array<float,Channels> input{};
        for(float& value:input) if(n<700000) value=random_sample(random);
        std::array<std::array<float,Channels>,maximum_outputs> expected{};
        unsigned count=0,emitted=0;
        reference.process(input.data(),[&](const float* output) {
            require(count<maximum_outputs,"reference output bound exceeded");
            std::copy_n(output,Channels,expected[count++].begin());
        });
        actual.process(input.data(),[&](const float* output) {
            require(emitted<count,"resampler emitted an extra frame");
            require(!std::memcmp(output,expected[emitted++].data(),sizeof(float)*Channels),
                    "resampler output differs from absolute-clock reference");
        });
        require(emitted==count,"resampler dropped a frame");
        if(n==123456 || n==789123) {reference.reset();actual.reset();}
    }
    std::cout<<"SRC "<<Up<<'/'<<Down<<": 1000000 inputs, reset/silence, bit exact\n";
}

static void check_controls(const native_hall::ProgramBank& bank) {
    auto reference=std::make_unique<native_hall::Engine48>();
    auto actual=std::make_unique<native_hall::Engine48>();
    uint32_t random=31;
    for(unsigned program=0;program<native_hall::program_count;++program) {
        native_hall::Parameters p;p.program=program;
        for(unsigned pass=0;pass<2;++pass) {
            // Repreparing with the previous settings must invalidate the cache.
            reference->prepare(bank,program);actual->prepare(bank,program);
            for(unsigned n=0;n<144000;++n) {
                if(n%96==0) {
                    unsigned k=n/96;
                    if(k%125==0) {
                        p.hall.bass=1+(k*7)%31;p.hall.mid=(k*11)%32;
                        p.hall.crossover=(k*13)%32;p.hall.treble=(k*17)%32;
                        p.hall.depth=(k*19)%72;p.hall.diffusion=1+(k*23)%63;
                        p.hall.predelay_ms=native_hall::predelay_minima[program]+int((k*29)%129);
                        p.hall.mode_enhancement=k&1;p.hall.decay_optimization=k&2;
                        p.analog=k&4;p.input_db=float(int(k%49)-36);
                        p.mix=(k%11)*0.1f;p.output_left=k%4;p.output_right=(k+2)%4;
                    }
                    // Change each Hall field on its own, so a missing cache
                    // comparison cannot hide behind another changed field.
                    switch(k%125) {
                        case 25:p.hall.bass=p.hall.bass%31+1;break;
                        case 26:p.hall.mid=(p.hall.mid+1)%32;break;
                        case 27:p.hall.crossover=(p.hall.crossover+1)%32;break;
                        case 28:p.hall.treble=(p.hall.treble+1)%32;break;
                        case 29:p.hall.depth=(p.hall.depth+1)%72;break;
                        case 30:++p.hall.predelay_ms;break;
                        case 31:p.hall.diffusion=p.hall.diffusion%63+1;break;
                        case 32:p.hall.mode_enhancement=!p.hall.mode_enhancement;break;
                        case 33:p.hall.decay_optimization=!p.hall.decay_optimization;break;
                    }
                    if(k>1200 && k%13==0) p.program=(p.program+1)%6;
                    reference->set_parameters(p);actual->set_parameters(p);
                    // Reproduce the previous engine's unconditional Hall write.
                    reference->hall().set_controls(p.hall);
                }
                float left=n<48000?random_sample(random):0;
                float right=n<48000?random_sample(random):0;
                std::array<float,2> expected{},output{};
                const bool wet_only=(n/96)&1;
                reference->process(left,right,expected[0],expected[1],wet_only);
                actual->process(left,right,output[0],output[1],wet_only);
                require(!std::memcmp(expected.data(),output.data(),sizeof(expected)),
                        "cached controls changed audio");
            }
            require(reference->hall().memory()==actual->hall().memory(),"cached controls changed delay memory");
            require(reference->hall().coefficients()==actual->hall().coefficients(),"cached controls changed coefficients");
            require(reference->hall().offsets()==actual->hall().offsets(),"cached controls changed offsets");
            require(reference->hall().saturation_count()==actual->hall().saturation_count(),"cached controls changed saturation count");
        }
        std::cout<<"Program "<<program<<": repeated controls, tails, switches and reprepare bit exact\n";
    }
}

int main(int argc,char** argv) {
    require(argc==2,"usage: processing_check PROGRAMS.bank224");
    check_filter<32,75,64,2>();check_filter<75,32,32,4>();
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<native_hall::ProgramBank>();
    require(native_hall::read_bank(bytes.data(),bytes.size(),*bank),"invalid bank");
    check_controls(*bank);
}
