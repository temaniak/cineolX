#pragma once
#include <array>
#include <cmath>
#include <complex>

namespace native_hall {
// Sampled continuous-time modal filters from analog/filters_224.hpp. Complex
// conjugate poles are stored once as real 2x2 recurrences. Setup computes the
// first-order-hold coefficients; processing uses float arithmetic only.
// This approximates the reference's event-timed analog model at 48 kHz.
class Analog48 {
public:
    void prepare(bool input) noexcept;
    float process(float sample) noexcept {
        float sum=direct_*sample;
        for(auto& s:sections_) {
            float re=s.ar*s.xr-s.ai*s.xi+s.hr*previous_+s.jr*(sample-previous_);
            float im=s.ai*s.xr+s.ar*s.xi+s.hi*previous_+s.ji*(sample-previous_);
            s.xr=re;s.xi=im;
            sum+=s.rr*re-s.ri*im;
        }
        previous_=sample;
        return sum*scale_;
    }
    void reset() noexcept {for(auto& s:sections_) s.xr=s.xi=0;previous_=0;}
private:
    struct Section {float ar=0,ai=0,hr=0,hi=0,jr=0,ji=0,rr=0,ri=0,xr=0,xi=0;};
    std::array<Section,6> sections_{};
    float previous_=0,direct_=0,scale_=1;
};
inline void Analog48::prepare(bool input) noexcept {
    using C=std::complex<double>;
    // Four conjugate pairs + two real poles on AIN; three pairs + three
    // real poles on AOUT. Each pair or real pole has its own section.
    const double in[][4]={
        {-30303.16922232196,86598.07360509051,-1535.566601095282,-303.913252204529},
        {-2079.01966805843,52657.713571139124,-747.0265644510486,11863.594897528039},
        {-7591.824267460196,47083.68694027513,-18171.93448936985,-43128.62144416562},
        {-16515.629520320686,30674.000189746792,46300.222444901585,86311.59842586887},
        {-39606.34650559455,0,-142926.05668907412,0},
        {-22721.559324947593,0,91234.66712059393,0}};
    // Negligible -500 kHz pole is omitted (DC contribution <7e-11).
    const double out[][4]={
        {-2078.9309743031404,52656.156156557365,-154.75983466442128,-521.1939205999234},
        {-7593.720389659135,47085.12790844678,1802.026900585217,1082.4614462112024},
        {-16514.177328627768,30674.64428732316,-4855.783307439754,1742.6454306668868},
        {-22720.857808301902,0,-9549.660499129033,0},
        {-2.0328722285876513,0,-1.274430980922891,0},
        {-12550.2093589853,0,16073.03221843225,0}};
    const auto* rows=input?in:out;
    constexpr double dt=1.0/48000;
    for(unsigned i=0;i<6;++i) {
        C p(rows[i][0],rows[i][1]),r(rows[i][2],rows[i][3]);
        C a=std::exp(p*dt), h=(a-C(1))/p,j=(a-C(1)-p*dt)/(p*p*dt);
        auto& s=sections_[i];
        s={float(a.real()),float(a.imag()),float(h.real()),float(h.imag()),float(j.real()),float(j.imag()),
           float(r.real()*(p.imag()==0?1:2)),float(r.imag()*(p.imag()==0?1:2)),0,0};
    }
    direct_=input?float(-1.9027652288899368e-11):float(3.5966000160032862e-12);
    scale_=input?float(0.6714903705341165):float(1.5975062688498582);
    previous_=0;
}
} // namespace native_hall
