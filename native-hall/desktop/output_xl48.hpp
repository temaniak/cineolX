#pragma once
#include "graphs.hpp"
#include "dac_xl48.hpp"
#include <numeric>

namespace cineol::xl {
// Every native WR_DA bus word reaches the waiting converter latch. Independent
// FPC/continuous-time checks cover cold and periodic captures on all 22 graphs.
template<Graph graph> class Output48 {
public:
    static constexpr unsigned rows=graph_info(graph).rows;
    static constexpr unsigned divisor=std::gcd(640u,9*rows);
    static constexpr unsigned numerator=9*rows/divisor,denominator=640/divisor;
    static constexpr unsigned latency_samples=unsigned(32+16.0*numerator/denominator+0.5);
    static constexpr unsigned raw_delay_samples=32;
    // The existing input adapter emits at ceil(pass_start / host_frame).
    // Forecast captures one frame later so none precede their emission; that
    // causal frame is included in the existing graph's compatibility delay.
    static constexpr unsigned transport_frames=latency_samples-32;
    void prepare() noexcept {dac_.prepare();reset();}
    void reset() noexcept {
        dac_.reset(int32_t(transport_frames)*EventDac48::frame_ticks);raw_.fill(0);next_pass_=0;
    }
    template<unsigned Row,unsigned Channels> void request(float value) noexcept {
        dac_.request(next_pass_+int32_t(transport_frames)*EventDac48::frame_ticks+int32_t(Row)*EventDac48::row_ticks+EventDac48::converter_origin,Channels,value);
    }
    void finish_pass() noexcept {next_pass_+=int32_t(rows)*EventDac48::row_ticks;}
    void push(const float*) noexcept {finish_pass();}
    std::array<float,4> sample(unsigned outputs=15) noexcept {
        const auto frame=dac_.sample(outputs);next_pass_-=EventDac48::frame_ticks;
        raw_=frame.raw;return frame.analog;
    }
    const std::array<float,4>& raw_sample() const noexcept {return raw_;}
private:
    EventDac48 dac_;
    std::array<float,4> raw_{};
    int32_t next_pass_=0;
};
}
