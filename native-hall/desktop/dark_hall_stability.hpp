#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cineol::xl {
// Dark Hall's added control limits excessive return gain, never removes its
// feedback network. Unity is the exact original arithmetic. The normal short-
// decay range already lies below the cap and remains unchanged at either end.
class DarkHallStability {
public:
    static constexpr uint32_t unity=1u<<24;
    void amount(float original) noexcept {original_=std::clamp(original,0.f,1.f);}
    template<class Settings> void update(const Settings& settings) noexcept {
        float maximum=0;
        for(unsigned row:{41u,94u}) {
            const float mid=settings.coefficients[row],low=mid+settings.coefficients[row+1];
            maximum=std::max({maximum,std::abs(mid),std::abs(low)});
        }
        const float stable=maximum>19?19/maximum:1;
        target_=uint32_t(std::llround((stable+(1-stable)*original_)*unity));
    }
    void reset() noexcept {gain_=target_;}
    void advance(unsigned rate_numerator,unsigned rate_denominator) noexcept {
        const int64_t step=(uint64_t(unity)*20*rate_denominator+rate_numerator-1)/rate_numerator;
        gain_=uint32_t(int64_t(gain_)+std::clamp(int64_t(target_)-gain_,-step,step));
    }
    int64_t apply(int64_t operand) const noexcept {
        return gain_==unity?operand:(operand*gain_)>>24;
    }
    uint32_t gain() const noexcept {return gain_;}
private:
    float original_=1;
    uint32_t gain_=unity,target_=unity;
};
}
