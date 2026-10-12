#pragma once
#include <array>

namespace cineol::floating {
// Fifth-order Lagrange read on samples whole-2 .. whole+3. Delay-line history
// supplies the samples; changing a fraction does not reset audio storage.
class FractionalDelay {
public:
    void set_fraction(float fraction) noexcept {
        fraction_=fraction;
        for(unsigned i=0;i<6;++i) {
            double weight=1;
            for(unsigned j=0;j<6;++j)if(i!=j)weight*=(2+double(fraction)-j)/(double(i)-j);
            weights_[i]=float(weight);
        }
    }
    bool integral() const noexcept {return fraction_==0;}
    float weight(unsigned i) const noexcept {return weights_[i];}
private:
    std::array<float,6> weights_{};
    float fraction_=0;
};
} // namespace cineol::floating
