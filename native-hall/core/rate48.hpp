#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace native_hall {
// Fixed storage, exact rational phase clock. Coefficients are prepared once;
// process() accepts one sample and emits a bounded number without allocating.
// Down: 32/75, 64 taps. Up: 75/32, 32 taps. History is causal; combined filter
// delay is 32 host + 16 native = 69.5 samples at 48 kHz.
template<int Up,int Down,int Taps,int Channels>
class RationalFilter {
public:
    void prepare() noexcept {
        constexpr double pi=3.14159265358979323846;
        double cutoff=0.94*std::min(1.0,double(Up)/Down);
        for(int p=0;p<Up;++p) {
            double sum=0;
            for(int j=0;j<Taps;++j) {
                double d=double(j)-double(p)/Up-double(Taps/2-1);
                double a=cutoff*d;
                double sinc=a==0?1:std::sin(pi*a)/(pi*a);
                double r=d/(Taps/2), window=0;
                if(std::abs(r)<1) window=i0(8*std::sqrt(1-r*r))/i0(8);
                weights_[p][j]=float(cutoff*sinc*window);sum+=weights_[p][j];
            }
            for(float& v:weights_[p]) v=float(v/sum);
        }
        reset();
    }
    void reset() noexcept {for(auto& h:history_) h.fill(0);slot_=phase_=0;}
    template<class Emit> void process(const float* input,Emit&& emit) noexcept {
        const unsigned n=slot_;
        for(int c=0;c<Channels;++c) history_[c][n]=input[c];
        // phase_ is the next output time relative to this input, in 1/Up
        // units. It stays bounded; phase_<Up matches the original absolute
        // clock's next_/Up<=input_index condition.
        while(phase_<Up) {
            const auto& weights=weights_[phase_];
            float out[Channels]{};
            for(int j=0;j<Taps;++j) {
                unsigned slot=unsigned((n+1+j)%Taps);
                for(int c=0;c<Channels;++c) out[c]+=history_[c][slot]*weights[j];
            }
            emit(out);phase_+=Down;
        }
        phase_-=Up;slot_=(n+1)%Taps;
    }
private:
    static double i0(double x) noexcept {double sum=1,t=1;for(int k=1;k<32;++k){t*=x*x/(4*k*k);sum+=t;}return sum;}
    std::array<std::array<float,Taps>,Up> weights_{};
    std::array<std::array<float,Taps>,Channels> history_{};
    unsigned slot_=0,phase_=0;
};
} // namespace native_hall
