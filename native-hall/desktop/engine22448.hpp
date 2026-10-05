#pragma once
#include "../core/engine48.hpp"
#include "dac22448.hpp"
#include "adc22448.hpp"
#include "controllers224.hpp"

namespace native_hall {
// Desktop-only output boundary. The existing 70-sample reported/dry/bypass
// latency remains unchanged. A fixed 38-frame
// transport replaces the former output FIR's nominal 37.5-frame delay; the
// channel-specific converter phases are now represented explicitly.
class DesktopOutput48 {
public:
    static constexpr unsigned transport_frames=38;
    void prepare() noexcept {dac_.prepare();reset();}
    void reset() noexcept {dac_.reset();delay_.fill({});position_=0;}
    void select_network(unsigned network) noexcept {dac_.select_network(network);delay_.fill({});position_=0;}
    void push(const float* values) noexcept {dac_.push(values);}
    std::array<float,4> sample() noexcept {
        auto result=delay_[position_];delay_[position_]=dac_.sample();
        position_=(position_+1)%delay_.size();return result;
    }
private:
    Dac48 dac_;
    std::array<std::array<float,4>,transport_frames> delay_{};
    unsigned position_=0;
};
using DesktopEngine48=Engine48WithOutput<DesktopOutput48,Adc48,DesktopHall>;
} // namespace native_hall
