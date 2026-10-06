#pragma once
#include "feedback_timing_xl.hpp"

namespace cineol::xl {
// AFE4's native feedback invocation has a ten-state dispatch before B000.
// Resume maps the enclosing fast routine's work offsets to wall CPU states.
// Coarse indices and template bits belong to its prepared control context.
template<class Resume,class Emit>
TimedFeedbackXL feedback_from_fast(const DiffusionProfile& profile,const std::array<uint8_t,128>& low2,
    uint8_t normal_index,uint8_t other_index,uint8_t reduction,FeedbackBytesXL& bytes,
    unsigned fast_work_at,WcsTimingXL& bus,Resume&& resume,Emit&& emit) noexcept {
    const auto result=timed_feedback(profile,low2,normal_index,other_index,reduction,bytes,bus,
        [&](unsigned at)noexcept{return resume(fast_work_at+10+at);},
        [&](unsigned at,uint64_t start,ControlByteXL payload,const WcsTimingXL::Access& access)noexcept {
            emit(fast_work_at+10+at,start,payload,access);
        });
    return {10+result.work_states,result.finished_state};
}
} // namespace cineol::xl
