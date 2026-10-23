#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace cineol {
// A bounded 48 kHz <-> variable 24..48 kHz clock domain. All coefficient
// construction is confined to prepare(). One engine call at most per frame.
// Four wet channels share one phase clock; they can never drift apart.
class SpaceClock {
public:
    static constexpr unsigned latency=256,taps=32,phases=256,rates=32;
    using Quad=std::array<float,4>;
    void prepare() {static const Kernels kernels;table_=&kernels;reset();}
    void reset(float ratio=1) noexcept {
        input_={};output_={};delay_={};input_slot_=output_slot_=delay_slot_=0;
        phase_=0;ratio_=std::clamp(ratio,.5f,1.0f);
    }
    float ratio() const noexcept {return ratio_;}
    template<class Render> Quad process(float left,float right,float target,Render&& render) noexcept {
        ratio_+=.0005f*(std::clamp(target,.5f,1.0f)-ratio_);
        if(std::abs(ratio_-target)<1e-6f) ratio_=target;
        input_[input_slot_]={left,right};phase_+=ratio_;
        if(phase_>=1) {
            phase_-=1;
            const double age=phase_/ratio_;
            const float rate=(ratio_-.5f)*float(2*rates);
            const unsigned ri=std::min(unsigned(rate),rates-1);const float ru=rate-float(ri);
            const double at=age*phases;const unsigned pi=std::min(unsigned(at),phases-1);
            const float pu=float(at-pi);std::array<float,2> input{};
            for(unsigned j=0;j<taps;++j) {
                const auto& a=table_->down[ri];const auto& b=table_->down[ri+1];
                const float w0=a[pi][j]+(a[pi+1][j]-a[pi][j])*pu;
                const float w1=b[pi][j]+(b[pi+1][j]-b[pi][j])*pu;
                const float w=w0+(w1-w0)*ru;
                const auto& frame=input_[(input_slot_+input_.size()-j)%input_.size()];
                input[0]+=w*frame[0];input[1]+=w*frame[1];
            }
            output_slot_=(output_slot_+1)%output_.size();
            output_[output_slot_]=render(input[0],input[1]);
        }
        const double at=phase_*phases;const unsigned pi=std::min(unsigned(at),phases-1);const float u=float(at-pi);
        Quad wet{};
        for(unsigned j=0;j<taps;++j) {
            const float w=table_->up[pi][j]+(table_->up[pi+1][j]-table_->up[pi][j])*u;
            const auto& frame=output_[(output_slot_+output_.size()-j)%output_.size()];
            for(unsigned c=0;c<4;++c)wet[c]+=w*frame[c];
        }
        delay_[delay_slot_]=wet;
        // Both engines are aligned to 70 virtual frames. Keep the declared
        // host-domain delay fixed across Space: 16 + (70+16)/ratio + padding.
        const double extra=latency-16-86.0/ratio_;
        const unsigned whole=unsigned(extra);const float f=float(extra-whole);
        Quad result{};
        for(unsigned c=0;c<4;++c) {
            const float a=delay_[(delay_slot_+delay_.size()-whole)%delay_.size()][c];
            const float b=delay_[(delay_slot_+delay_.size()-whole-1)%delay_.size()][c];
            const float before=delay_[(delay_slot_+delay_.size()-whole+1)%delay_.size()][c];
            const float after=delay_[(delay_slot_+delay_.size()-whole-2)%delay_.size()][c];
            result[c]=before*(-f*(f-1)*(f-2)/6)+a*((f+1)*(f-1)*(f-2)/2)
                     +b*(-(f+1)*f*(f-2)/2)+after*((f+1)*f*(f-1)/6);
        }
        input_slot_=(input_slot_+1)%input_.size();delay_slot_=(delay_slot_+1)%delay_.size();
        return result;
    }
private:
    using Weights=std::array<std::array<float,taps>,phases+1>;
    struct Kernels {
        std::array<Weights,rates+1> down{};Weights up{};
        Kernels() {
            const auto i0=[](double x) {double sum=1,v=1;for(unsigned k=1;k<32;++k){v*=x*x/(4*k*k);sum+=v;}return sum;};
            const auto fill=[&](auto& weights,double centre,double cutoff) {
                double total=0;
                for(unsigned j=0;j<taps;++j) {
                    const double d=double(j)-centre,r=d/16;
                    const double x=3.14159265358979323846*cutoff*d;
                    const double w=std::abs(r)<1?i0(8*std::sqrt(1-r*r))/i0(8):0;
                    weights[j]=float(cutoff*(x==0?1:std::sin(x)/x)*w);total+=weights[j];
                }
                for(float& v:weights)v=float(v/total);
            };
            for(unsigned r=0;r<=rates;++r) {
                const double ratio=.5+.5*double(r)/rates;
                for(unsigned p=0;p<=phases;++p) fill(down[r][p],16+double(p)/phases,ratio);
            }
            for(unsigned p=0;p<=phases;++p) fill(up[p],16-double(p)/phases,1);
            // Exact wires at the nominal clock, with only integer transport.
            down[rates][0].fill(0);down[rates][0][16]=1;
            up[0].fill(0);up[0][16]=1;
        }
    };
    const Kernels* table_=nullptr;
    std::array<std::array<float,2>,64> input_{};
    std::array<Quad,64> output_{};
    std::array<Quad,512> delay_{};
    unsigned input_slot_=0,output_slot_=0,delay_slot_=0;
    double phase_=0;float ratio_=1;
};
}
