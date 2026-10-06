#pragma once
#include "feedback_timing_xl.hpp"

namespace cineol::xl {
// Resumable native B000 compiler. Only write and write-return boundaries are
// yielded: all profile/indices/template metadata are stable for this call.
// The owner supplies independently calculated READY completion, and owns
// wall-clock IRQs, DSP rows and each byte's physical commit visibility.
class FeedbackClockXL {
public:
    enum class Kind:uint8_t {write,write_return,finished};
    struct Event {Kind kind;uint64_t work_state;ControlByteXL payload;};
    void reset(const DiffusionProfile& profile,const std::array<uint8_t,128>& low2,
        uint8_t normal_index,uint8_t stop_index,uint8_t reduction,FeedbackBytesXL& bytes,uint64_t entry=0) noexcept {
        profile_=&profile;low2_=&low2;bytes_=&bytes;at_=entry;stop_=stop_index;reduction_=reduction;target_=pair_=0;
        if(!profile.count){at_+=31;kind_=Kind::finished;return;}
        at_+=88;doubled_=uint8_t(normal_index*2);bytes_->tag=128;begin_target();
    }
    Event next()const noexcept {
        if(kind_!=Kind::write)return {kind_,at_,{}};
        const unsigned row=profile_->targets[target_].row+pair_;
        const uint8_t magnitude=pair_==1?bytes_->middle:bytes_->outer;
        return {kind_,at_,{row,3,uint8_t(uint8_t(uint8_t(~magnitude)*4)|((*low2_)[row]&3))}};
    }
    void complete_write(unsigned work) noexcept {
        if(kind_!=Kind::write)return;
        at_+=work;kind_=Kind::write_return;
    }
    void resume_after_write() noexcept {
        if(kind_!=Kind::write_return)return;
        at_+=20;
        if(++pair_<3){at_+=55+125;kind_=Kind::write;return;}
        at_+=40;
        if(profile_->separate_stop) {
            at_+=27;
            if(!profile_->half_scale) {
                at_+=21;
                if(profile_->count-target_<=3){doubled_=uint8_t(stop_*2);bytes_->tag=128;at_+=42;}
            }
        }
        at_+=15;
        if(++target_==profile_->count){at_+=10;kind_=Kind::finished;return;}
        pair_=0;begin_target();
    }
private:
    void begin_target() noexcept {
        const auto& target=profile_->targets[target_];at_+=65;
        if(target.scale_cap!=bytes_->tag) {
            bytes_->tag=target.scale_cap;const uint8_t scale=target.scale_cap&224;
            at_+=78+byte_product_work(scale)+52;
            unsigned scaled=(unsigned(scale)*doubled_)>>8;
            if(profile_->half_scale){scaled>>=1;at_+=14;}
            const unsigned cap=target.scale_cap&31;at_+=28;if(cap>=scaled)at_+=5;
            uint8_t outer=uint8_t(std::min(cap,scaled)-reduction_);at_+=60;
            if(outer&128){outer=0;at_+=7;}
            bytes_->outer=outer;at_+=68;const uint8_t operand=uint8_t(outer*2);
            at_+=byte_product_work(operand)+79;
            const uint16_t product=uint16_t(unsigned(operand)*operand);
            bytes_->middle=uint8_t(32-uint8_t(uint16_t(product*2+128)>>8));
        }
        at_+=40+125;kind_=Kind::write;
    }
    const DiffusionProfile* profile_=nullptr;
    const std::array<uint8_t,128>* low2_=nullptr;
    FeedbackBytesXL* bytes_=nullptr;
    uint64_t at_=0;
    uint8_t doubled_=0,stop_=0,reduction_=0,target_=0,pair_=0;
    Kind kind_=Kind::finished;
};
} // namespace cineol::xl
