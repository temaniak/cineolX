#pragma once
#include <array>
#include <complex>
#include <cmath>

namespace cineol::xl {
// First-order-hold sampling of Reflexion's X/XL circuit model at 48 kHz.
// Setup only uses complex math; process is fixed, allocation-free float DSP.
class Analog48 {
public:
    void prepare(bool input) noexcept {
        using C=std::complex<double>;
        const double in[][4]={
            {-45454.75388750916,129897.11080552098,599.773325965535,-9738.048963400382},
            {-4102.298527633177,96728.4657975426,-3115.5783052596125,33527.28561804381},
            {-14963.06197207162,85999.19138571685,-31892.933065639514,-132417.4931524734},
            {-34379.626801054466,52026.47659924561,15332.017538737677,300041.418123026},
            {-79535.43774462046,0.0,-613178.6793615303,0.0},
            {-7365.576561320724,0.0,-213.5380362124952,0.0},
            {-55623.822331865085,0.0,651545.6581728631,0.0}};
        const double out[][4]={
            {-3866.5136046706643,98714.32890413406,-285.32850305430486,-775.0280427752442},
            {-14298.548643106906,88288.04903327773,2870.8837308856587,1408.6510272456123},
            {-30935.31584934812,57688.25681141755,-6993.1770747341725,3525.780513937593},
            {-42164.59294570152,0.0,-14122.997295322293,0.0},
            {-2.0328722602013864,0.0,-1.2762189938663815,0.0},
            {-19841.28472220363,0.0,23093.899981653653,0.0}};
        const auto* rows=input?in:out;count_=input?std::size(in):std::size(out);
        constexpr double dt=1.0/48000;
        for(unsigned i=0;i<count_;++i) {
            C p(rows[i][0],rows[i][1]),r(rows[i][2],rows[i][3]);
            C a=std::exp(p*dt),h=(a-C(1))/p,j=(a-C(1)-p*dt)/(p*p*dt);
            const double multiplicity=p.imag()==0?1:2;
            sections_[i]={float(a.real()),float(a.imag()),float(h.real()),float(h.imag()),
                float(j.real()),float(j.imag()),float(r.real()*multiplicity),float(r.imag()*multiplicity),0,0};
        }
        direct_=input?float(2.4950753693704976e-10):float(2.741597927047747e-09);
        scale_=input?float(0.6768697179710966):float(1.5941996081501124);
        previous_=0;
    }
    float process(float input) noexcept {
        float sum=direct_*input;
        for(unsigned i=0;i<count_;++i) {
            auto& s=sections_[i];
            const float re=s.ar*s.xr-s.ai*s.xi+s.hr*previous_+s.jr*(input-previous_);
            const float im=s.ai*s.xr+s.ar*s.xi+s.hi*previous_+s.ji*(input-previous_);
            s.xr=re;s.xi=im;sum+=s.rr*re-s.ri*im;
        }
        previous_=input;return sum*scale_;
    }
    void reset() noexcept {previous_=0;for(auto& s:sections_) s.xr=s.xi=0;}
private:
    struct Section {float ar,ai,hr,hi,jr,ji,rr,ri,xr,xi;};
    std::array<Section,7> sections_{};
    unsigned count_=0;
    float previous_=0,direct_=0,scale_=1;
};
}
