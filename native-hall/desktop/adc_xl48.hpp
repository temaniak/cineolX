#pragma once
#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>

namespace cineol::xl {
// Exact linear boundary of the X/XL AIN: 4x sinc interpolation,
// 192 kHz first-order hold and the circuit sampled at independent ADC holds.
// Combine the finite impulse response with its modal tail during preparation;
// processing needs only a 33-tap FIR per conversion and seven host-rate modes.
// FOH uses the next point in its last quarter-frame; all queried input times
// are at least 14 frames behind the host, so that point is already available.
class EventAdc48 {
public:
    static constexpr int32_t frame_ticks=12000000, row_ticks=168750;
    static constexpr int32_t event_origin_ticks=351483;
    // The periodic FPC scan is restarted at each graph's final row.
    // Final loads: left at 89-rows, right at 39; analog holds precede them
    // by 36 clocks. All 22 physical layouts are independently phase-probed.
    static constexpr int right_load_ticks=39*row_ticks+event_origin_ticks;
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
    void prepare(unsigned rows) noexcept {
        assert(rows>=100 && rows<=109);rows_=rows;pass_ticks_=int32_t(rows)*row_ticks;
        kernels_=&kernels();reset();
    }
    void reset() noexcept {raw_={};states_={};raw_slot_=state_slot_=seen_=0;next_pass_=0;}
    template<class Emit> void process(const float* input,Emit&& emit) noexcept {
        process_capture(input,[&](const double* circuit,const float*) noexcept {emit(circuit);});
    }
    template<class Emit> void process_capture(const float* input,Emit&& emit) noexcept {
        const auto delayed=raw_[(raw_slot_+raw_.size()-tail_age)%raw_.size()];
        raw_[raw_slot_]={input[0],input[1]};
        const auto& previous=states_[(state_slot_+states_.size()-1)%states_.size()];
        auto& current=states_[state_slot_];
        for(unsigned c=0;c<2;++c)for(unsigned k=0;k<modes;++k) {
            const auto p=previous[c][k],a=kernels_->whole_[k],b=kernels_->tail_[k];
            current[c][k]={a.re*p.re-a.im*p.im+b.re*delayed[c],
                           a.im*p.re+a.re*p.im+b.im*delayed[c]};
            // At power-up the reference starts at point zero, without the
            // preceding FOH ramp that ordinary later input impulses have.
            if(seen_==0) {
                current[c][k].re-=kernels_->startup_[k].re*input[c];
                current[c][k].im-=kernels_->startup_[k].im*input[c];
            }
        }
        if(next_pass_+right_load_ticks<frame_ticks) {
            double converted[2];float bare[2];
            for(unsigned c=0;c<2;++c) {
                converted[c]=sample(c,next_pass_+(c?3:53-int32_t(rows_))*row_ticks+event_origin_ticks-latency_frames*frame_ticks);
                // Circuit bypass uses held 48 kHz pins at the final FPC load,
                // without range gain. The callback waits for right load 39;
                // left's load 89-rows is already in the fixed input history.
                bare[c]=raw_sample(c,next_pass_+(c?39:89-int32_t(rows_))*row_ticks+event_origin_ticks);
            }
            emit(converted,bare);next_pass_+=pass_ticks_;
        }
        next_pass_-=frame_ticks;
        raw_slot_=(raw_slot_+1)%raw_.size();state_slot_=(state_slot_+1)%states_.size();
        seen_=std::min(seen_+1,unsigned(raw_.size()));
    }
private:
    static constexpr unsigned modes=7,taps=33,tail_age=32,phases=640;
    static constexpr int32_t grid_ticks=18750;
    static constexpr int32_t remainder_ticks=event_origin_ticks%grid_ticks;
    struct Complex {double re=0,im=0;};
    float raw_sample(unsigned channel,int32_t relative_time) const noexcept {
        assert(relative_time<frame_ticks);
        const unsigned ago=relative_time>=0?0:unsigned((-relative_time+frame_ticks-1)/frame_ticks);
        assert(ago<raw_.size());if(ago>seen_)return 0;
        return raw_[(raw_slot_+raw_.size()-ago)%raw_.size()][channel];
    }
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
        const auto& weights=kernels_->fir_[phase];
        for(unsigned j=0;j<taps;++j)
            result+=weights[j]*raw_[(raw_slot_+raw_.size()-ago+1-j)%raw_.size()][channel];
        const auto& state=states_[(state_slot_+states_.size()-ago)%states_.size()][channel];
        for(unsigned k=0;k<modes;++k) {
            const auto a=kernels_->fractional_[phase][k],p=state[k];result+=a.re*p.re-a.im*p.im;
        }
        return result;
    }
    struct Kernels {
        std::array<std::array<double,taps>,phases> fir_{};
        std::array<std::array<Complex,modes>,phases> fractional_{};
        std::array<Complex,modes> whole_{},tail_{},startup_{};
        Kernels() noexcept;
    };
    static const Kernels& kernels() noexcept {static const Kernels value;return value;}
    const Kernels* kernels_=nullptr;
    std::array<std::array<float,2>,64> raw_{};
    std::array<std::array<std::array<Complex,modes>,2>,20> states_{};
    unsigned raw_slot_=0,state_slot_=0,seen_=0;
    int32_t next_pass_=0,pass_ticks_=0;unsigned rows_=105;
};
inline EventAdc48::Kernels::Kernels() noexcept {
    using C=std::complex<double>;
    constexpr double pi=3.14159265358979323846,scale=0.6768697179710966;
    constexpr double direct=2.4950753693704976e-10,tick_seconds=1.0/576000000000.0;
    const double rows[modes][4]={
        {-45454.75388750916,129897.11080552098,599.773325965535,-9738.048963400382},
        {-4102.298527633177,96728.4657975426,-3115.5783052596125,33527.28561804381},
        {-14963.06197207162,85999.19138571685,-31892.933065639514,-132417.4931524734},
        {-34379.626801054466,52026.47659924561,15332.017538737677,300041.418123026},
        {-79535.43774462046,0,-613178.6793615303,0},
        {-7365.576561320724,0,-213.5380362124952,0},
        {-55623.822331865085,0,651545.6581728631,0}};
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
}
} // namespace cineol::xl
