#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace cineol {
// Shared immutable circuit tables are built by prepare(), never by processing.
// Eleven octaves cover Dirt's 1..1024 bandwidth and Space's 1..2 compensation.
template<class Kernel> class CircuitControl {
public:
    static constexpr unsigned steps=128;
    void prepare() {
        static const auto table=[] {
            std::vector<Kernel> result;result.reserve(steps+1);
            for(unsigned i=0;i<=steps;++i) result.emplace_back(std::exp2(11.0*i/steps));
            return result;
        }();
        table_=&table;update();
    }
    void set_dirt(float value) noexcept {
        value=std::clamp(value,0.0f,1.0f);if(value==dirt_)return;dirt_=value;update();
    }
    void set_clock_factor(float value) noexcept {
        value=std::clamp(value,1.0f,2.0f);if(value==clock_)return;clock_=value;update();
    }
    template<class F> auto value(F&& f) const noexcept {
        const auto a=f((*table_)[lo_]);
        if(fraction_==0)return a;
        return a+(f((*table_)[lo_+1])-a)*fraction_;
    }
    float terminal() const noexcept {return terminal_;}
    float dirt() const noexcept {return dirt_;}
    double low_factor() const noexcept {return clock_*(1.0-dirt_)*(1.0-dirt_);}
    bool original_low() const noexcept {return clock_==1 && dirt_==0;}
    // The DC-block pole is real and so slow that a fourth-order expm1 series
    // has negligible error over all converter intervals (|x| < 0.001).
    float low_delta(double pole,double seconds) const noexcept {
        const double x=pole*low_factor()*seconds;
        return float(x*(1+x/2+x*x/6+x*x*x/24));
    }
private:
    void update() noexcept {
        const float at=std::clamp((10*dirt_+std::log2(clock_))*float(steps)/11,0.0f,float(steps));
        lo_=std::min(unsigned(at),steps-1);fraction_=at-float(lo_);
        const float x=std::clamp((dirt_-.85f)/.15f,0.0f,1.0f);terminal_=x*x*(3-2*x);
    }
    const std::vector<Kernel>* table_=nullptr;
    unsigned lo_=0;float fraction_=0,dirt_=0,clock_=1,terminal_=0;
};
}
