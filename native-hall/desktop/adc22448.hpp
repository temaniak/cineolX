#pragma once
#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>

namespace native_hall {
// Exact linear boundary of Reflexion's original-224 AIN: 4x sinc interpolation,
// 192 kHz first-order hold and the circuit sampled at independent ADC holds.
// Combine the finite impulse response with its modal tail during preparation;
// processing needs only a 33-tap FIR per conversion and six host-rate modes.
// FOH uses the next point in its last quarter-frame; all queried input times
// are at least 14 frames behind the host, so that point is already available.
class Adc48 {
public:
    static constexpr int32_t frame_ticks=12000000, pass_ticks=28125000, row_ticks=281250;
    static constexpr int32_t event_origin_ticks=585805;
    // RD_AD at rows 0/50 reads the completed left/right conversion. Left's
    // load was in the preceding pass; the CH1 hold edge is 36 rows earlier.
    inline static constexpr int hold_rows[2]={-46,4};
    static constexpr int latency_frames=16;
    // Match the reference's voltage-domain range/rounding operations, including
    // negative full scale and half-code ties. Keep precision until quantization.
    static int16_t adc(double sample) noexcept {
        const double volts=sample*5.0,a=std::abs(volts);
        const unsigned range=a<0.56?3:a<1.12?2:a<2.24?1:0;
        const int code=int(std::lround(std::clamp(volts*double(1u<<range)/5.0*2047.0,-2048.0,2047.0)));
        return int16_t(code*int(1u<<(4-range)));
    }
    static unsigned detectors(double sample) noexcept {
        constexpr double thresholds[]={0.28,0.56,1.12,2.24,5.0};
        const double volts=std::abs(sample*5.0);unsigned mask=0;
        for(unsigned k=0;k<5;++k)if(volts>=thresholds[k])mask|=1u<<k;
        return mask;
    }
    void prepare() noexcept;
    void reset() noexcept {raw_={};states_={};raw_slot_=state_slot_=seen_=0;next_pass_=0;}
    template<class Emit> void process(const float* input,Emit&& emit) noexcept {
        const auto delayed=raw_[(raw_slot_+raw_.size()-tail_age)%raw_.size()];
        raw_[raw_slot_]={input[0],input[1]};
        const auto& previous=states_[(state_slot_+states_.size()-1)%states_.size()];
        auto& current=states_[state_slot_];
        for(unsigned c=0;c<2;++c)for(unsigned k=0;k<modes;++k) {
            const auto p=previous[c][k],a=whole_[k],b=tail_[k];
            current[c][k]={a.re*p.re-a.im*p.im+b.re*delayed[c],
                           a.im*p.re+a.re*p.im+b.im*delayed[c]};
            // At power-up the reference starts at point zero, without the
            // preceding FOH ramp that ordinary later input impulses have.
            if(seen_==0) {
                current[c][k].re-=startup_[k].re*input[c];
                current[c][k].im-=startup_[k].im*input[c];
            }
        }
        if(next_pass_<frame_ticks) {
            double converted[2];
            for(unsigned c=0;c<2;++c)
                converted[c]=sample(c,next_pass_+hold_rows[c]*row_ticks+event_origin_ticks-latency_frames*frame_ticks);
            emit(converted);next_pass_+=pass_ticks;
        }
        next_pass_-=frame_ticks;
        raw_slot_=(raw_slot_+1)%raw_.size();state_slot_=(state_slot_+1)%states_.size();
        seen_=std::min(seen_+1,unsigned(raw_.size()));
    }
private:
    static constexpr unsigned modes=6,taps=33,tail_age=32,phases=64;
    static constexpr int32_t grid_ticks=187500;
    static constexpr int32_t remainder_ticks=event_origin_ticks%grid_ticks;
    struct Complex {double re=0,im=0;};
    double sample(unsigned channel,int32_t relative_time) const noexcept {
        assert(relative_time<=0);
        const unsigned ago=unsigned((-relative_time+frame_ticks-1)/frame_ticks);
        assert(ago<states_.size());
        if(ago>seen_)return 0;
        const int32_t fraction=relative_time+int32_t(ago)*frame_ticks;
        assert(fraction>=remainder_ticks && (fraction-remainder_ticks)%grid_ticks==0);
        const unsigned phase=unsigned((fraction-remainder_ticks)/grid_ticks);
        assert(phase<phases);
        double result=0;
        const auto& weights=fir_[phase];
        for(unsigned j=0;j<taps;++j)
            result+=weights[j]*raw_[(raw_slot_+raw_.size()-ago+1-j)%raw_.size()][channel];
        const auto& state=states_[(state_slot_+states_.size()-ago)%states_.size()][channel];
        for(unsigned k=0;k<modes;++k) {
            const auto a=fractional_[phase][k],p=state[k];result+=a.re*p.re-a.im*p.im;
        }
        return result;
    }
    std::array<std::array<double,taps>,phases> fir_{};
    std::array<std::array<Complex,modes>,phases> fractional_{};
    std::array<Complex,modes> whole_{},tail_{},startup_{};
    std::array<std::array<float,2>,64> raw_{};
    std::array<std::array<std::array<Complex,modes>,2>,20> states_{};
    unsigned raw_slot_=0,state_slot_=0,seen_=0;
    int32_t next_pass_=0;
};
inline void Adc48::prepare() noexcept {
    using C=std::complex<double>;
    constexpr double pi=3.14159265358979323846,scale=0.6714903705341165;
    constexpr double direct=-1.9027652288899368e-11,tick_seconds=1.0/576000000000.0;
    const double rows[modes][4]={
        {-30303.16922232196,86598.07360509051,-1535.566601095282,-303.913252204529},
        {-2079.01966805843,52657.713571139124,-747.0265644510486,11863.594897528039},
        {-7591.824267460196,47083.68694027513,-18171.93448936985,-43128.62144416562},
        {-16515.629520320686,30674.000189746792,46300.222444901585,86311.59842586887},
        {-39606.34650559455,0,-142926.05668907412,0},
        {-22721.559324947593,0,91234.66712059393,0}};
    auto bessel=[](double x) {
        double sum=1,term=1;for(int k=1;k<30;++k){term*=x*x/(4*k*k);sum+=term;}return sum;
    };
    std::array<double,129> h{};
    constexpr double fc=22000.0/192000.0;
    for(unsigned n=0;n<128;++n) {
        double t=double(n)-63.5,r=2.0*n/127.0-1;
        double sinc=std::sin(2*pi*fc*t)/(pi*t);
        h[n]=sinc*bessel(8*std::sqrt(std::max(0.0,1-r*r)))/bessel(8)*4;
    }
    struct Coefficients {C decay,step,ramp;};
    const auto coefficients=[](C p,double seconds) {
        C x=p*seconds,e=std::exp(x),em1,em1x;
        if(std::abs(x)<1e-3) {
            em1=x*(1.0+x/2.0+x*x/6.0+x*x*x/24.0);
            em1x=x*x*(0.5+x/6.0+x*x/24.0+x*x*x/120.0);
        } else {em1=e-C(1);em1x=em1-x;}
        return Coefficients{e,em1/p,em1x/(p*p)};
    };
    std::array<C,modes> poles{},residues{};
    std::array<std::array<C,modes>,129> z{};
    constexpr double step_seconds=1.0/192000.0;
    for(unsigned k=0;k<modes;++k) {
        C p(rows[k][0],rows[k][1]),r(rows[k][2],rows[k][3]);
        poles[k]=p;residues[k]=r*(p.imag()==0?1.0:2.0)*scale;
        const auto a=coefficients(p,step_seconds);
        z[0][k]=h[0]/step_seconds*a.ramp;
        C startup=residues[k]*z[0][k];
        startup_[k]={startup.real(),startup.imag()};
        for(unsigned n=0;n<128;++n)
            z[n+1][k]=a.decay*z[n][k]+h[n]*a.step+(h[n+1]-h[n])/step_seconds*a.ramp;
        C decay=std::exp(p/48000.0),tail=residues[k]*z[128][k];
        whole_[k]={decay.real(),decay.imag()};
        tail_[k]={tail.real(),tail.imag()};
    }
    for(unsigned phase=0;phase<phases;++phase) {
        const int32_t fraction=remainder_ticks+int32_t(phase)*grid_ticks;
        for(unsigned k=0;k<modes;++k) {
            C e=std::exp(poles[k]*(double(fraction)*tick_seconds));
            fractional_[phase][k]={e.real(),e.imag()};
        }
        for(unsigned j=0;j<taps;++j) {
            const int32_t time=(int32_t(j)-1)*frame_ticks+fraction;
            if(time<0) {
                double y=0;
                if(time>=-frame_ticks/4) {
                    const double partial=double(time+frame_ticks/4)*tick_seconds;
                    for(unsigned k=0;k<modes;++k)
                        y+=(residues[k]*coefficients(poles[k],partial).ramp*h[0]/step_seconds).real();
                    y+=direct*scale*h[0]*partial/step_seconds;
                }
                fir_[phase][j]=y;continue;
            }
            const unsigned segment=unsigned(time/(frame_ticks/4));
            const double partial=double(time% (frame_ticks/4))*tick_seconds;
            double y=0;
            for(unsigned k=0;k<modes;++k) {
                const auto a=coefficients(poles[k],partial);
                C value=a.decay*z[segment][k]+h[segment]*a.step
                    +(h[segment+1]-h[segment])/step_seconds*a.ramp;
                y+=(residues[k]*value).real();
            }
            y+=direct*scale*(h[segment]+(h[segment+1]-h[segment])*partial/step_seconds);
            fir_[phase][j]=y;
        }
    }
    reset();
}
} // namespace native_hall
