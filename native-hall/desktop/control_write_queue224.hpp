#pragma once
#include "control_write224.hpp"
#include "../core/hall.hpp"
#include <cassert>

namespace native_hall {
// A procedure predicts at most twelve writes. Its final commit precedes
// return, and the next procedure's writes follow a later audio boundary.
// Select old/new values by each target's one fetch, without a row interpreter.
class ControlWriteQueue224 {
public:
    static constexpr unsigned capacity=12;
    void reset() noexcept {count_=0;}
    void push(uint64_t visible,ControlWrite224 write) noexcept {
        assert(count_<capacity && (!count_ || events_[count_-1].visible<=visible));
        if(count_<capacity)events_[count_++]={visible,write};
    }
    unsigned size() const noexcept {return count_;}
    template<class Render> void render(uint64_t begin,Hall& hall,Render&& render) noexcept {
        unsigned due=0;
        while(due<count_ && events_[due].visible<begin+100) {
            const auto& e=events_[due++];
            if(e.visible<=begin+e.write.row)apply(hall,e.write);
        }
        render();
        // A post-fetch commit takes effect in this row's next pass. Replay
        // in commit order so multiple writes to one target retain the last.
        for(unsigned i=0;i<due;++i)apply(hall,events_[i].write);
        for(unsigned i=due;i<count_;++i)events_[i-due]=events_[i];
        count_=uint8_t(count_-due);
    }
private:
    static void apply(Hall& hall,ControlWrite224 write) noexcept {
        if(write.kind==ControlWrite224::Kind::Coefficient)hall.apply_control_coefficient(write.row,int8_t(write.value));
        else hall.apply_control_address_low(write.row,write.value);
    }
    struct Event {uint64_t visible=0;ControlWrite224 write;};
    std::array<Event,capacity> events_{};
    uint8_t count_=0;
};
} // namespace native_hall
