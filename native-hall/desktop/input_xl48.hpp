#pragma once
#include "graphs.hpp"
#include "adc_xl48.hpp"
#include "../core/engine48.hpp"
#include <numeric>

namespace cineol::xl {
struct InputFrame {
    std::array<double,2> circuit{};
    std::array<float,2> raw{};
    std::array<int16_t,2> clean{},bare{};
    unsigned detectors=0;
};
// Native periodic FPC input boundary: independently timed analog holds and
// completed bypass loads, with conversion/comparator precision retained.
template<Graph graph> class Input48 {
public:
    static constexpr unsigned divisor=std::gcd(640u,9*graph_info(graph).rows);
    static constexpr unsigned numerator=640/divisor,denominator=9*graph_info(graph).rows/divisor;
    void prepare() noexcept {adc_.prepare(graph_info(graph).rows);}
    void reset() noexcept {adc_.reset();}
    template<class Emit> void process(const float* raw,Emit&& emit) noexcept {
        adc_.process_capture(raw,[&](const double* input,const float* held) noexcept {
            InputFrame frame;
            for(unsigned c=0;c<2;++c) {
                frame.circuit[c]=input[c];frame.raw[c]=held[c];
                frame.clean[c]=EventAdc48::adc(input[c]);
                frame.bare[c]=native_hall::Engine48::adc(held[c],false);
                frame.detectors|=EventAdc48::detectors(input[c]);
            }
            emit(frame);
        });
    }
private:
    EventAdc48 adc_;
};
}
