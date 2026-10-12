#include "fractional_delay.hpp"
#include <cmath>
#include <complex>
#include <iostream>
#include <stdexcept>
#include <algorithm>

static void require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
int main() try {
    constexpr double pi=3.14159265358979323846;
    double worst_loss=0,worst_delay_error=0;
    for(unsigned phase=0;phase<32;++phase) {
        const float fraction=phase/32.0f;
        cineol::floating::FractionalDelay delay;delay.set_fraction(fraction);
        double sum=0,moment=0;
        for(unsigned i=0;i<6;++i) {sum+=delay.weight(i);moment+=(int(i)-2)*double(delay.weight(i));}
        require(std::abs(sum-1)<2e-7,"fractional-delay DC gain");
        worst_delay_error=std::max(worst_delay_error,std::abs(moment-fraction));
        require(std::abs(moment-fraction)<2e-7,"fractional-delay time units");
        for(unsigned bin=0;bin<=2400;++bin) {
            const double frequency=10.0*bin;
            std::complex<double> response{};
            for(unsigned i=0;i<6;++i)response+=double(delay.weight(i))*std::polar(1.0,-2*pi*frequency*(int(i)-2)/48000);
            const double gain=std::abs(response);
            require(gain<=1.000001,"fractional read amplifies feedback");
            if(frequency<=8000) {
                worst_loss=std::max(worst_loss,1-gain);
                require(gain>.992,"fractional read loses too much upper-band energy");
            }
        }
        if(phase==0)for(unsigned i=0;i<6;++i)require(delay.weight(i)==(i==2?1.0f:0.0f),"integral read");
    }
    const double old_gain=std::sqrt(1-4*.34375*(1-.34375)*std::pow(std::sin(pi*8000/48000),2));
    require(old_gain<.992,"darkness fixture does not discriminate old interpolation");
    std::cout<<"32 fractional phases: DC gain/time, integer read, full-band gain bound and 0-8 kHz passband passed\n"
        <<"maximum passband loss="<<-20*std::log10(1-worst_loss)<<" dB; maximum DC delay error="<<worst_delay_error
        <<" frames; old 8 kHz read loss="<<20*std::log10(old_gain)<<" dB\n";
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
