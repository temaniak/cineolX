#pragma once
#include "decay_compiler_xl.hpp"

namespace cineol::xl {
struct ParameterCompilerMemoryXL {
    uint16_t remaining_mask=0;
    uint8_t page=0,logical_cell=0,secondary_low=0,secondary_mid=0,secondary_feedback=0;
    std::array<uint8_t,5> cached_auxiliary{};
    uint8_t primary_auxiliary=0,secondary_auxiliary=0;
};
struct ParameterCompilerContextXL {
    uint16_t program_mask=0;
    uint8_t control_flags=0,secondary_limit=0;
    std::array<uint8_t,72> logical{},cached{};
    std::array<uint8_t,5> auxiliary{};
    bool variable_size=false;
};
// Outer coherent parameter compilation. The program-preparation, main and
// six-slot group workers supply their own durations. Their payload/compiler
// laws are separate; this clock owns page dispatch, secondary bookkeeping
// and the final five auxiliary snapshots. All events use local work states.
class ParameterCompilerClockXL {
public:
    enum class Kind:uint8_t {program,main,group,secondary_group,finished};
    struct Event {Kind kind;uint64_t work_state;};
    void reset(ParameterCompilerMemoryXL& memory,DynamicsState& dynamics,
        const ParameterCompilerContextXL& context,uint64_t entry) noexcept {
        memory_=&memory;dynamics_=&dynamics;context_=context;at_=entry;
        if(context.control_flags&4){at_+=31;kind_=Kind::finished;return;}
        at_+=13+7+5+10+10+17;kind_=Kind::program;
    }
    Event next()const noexcept{return {kind_,at_};}
    void complete_program(unsigned work) noexcept {
        if(kind_!=Kind::program)return;
        at_+=work+17;kind_=Kind::main;
    }
    void complete_main(unsigned work) noexcept {
        if(kind_!=Kind::main)return;
        uint16_t mask=context_.program_mask;
        if(!(mask&0x0800))mask&=uint16_t(~0x0400);
        memory_->remaining_mask=uint16_t(mask<<1);memory_->page=1;
        memory_->logical_cell=(mask&0x8000)?0:6;
        at_+=work+10;
        if(mask&0x8000)skip_group();
        else at_+=10;
        advance();
    }
    void complete_group(unsigned work,uint8_t secondary_limit) noexcept {
        if(kind_!=Kind::secondary_group && kind_!=Kind::group)return;
        // Enabled groups consume six compact logical cells. A masked page
        // skips its descriptor group but consumes no logical/cache cells.
        for(unsigned i=0;i<6;++i) {
            const unsigned cell=memory_->logical_cell+i;
            context_.cached[cell]=context_.logical[cell];
            if(cell==6)memory_->secondary_low=context_.logical[cell];
            if(cell==7)memory_->secondary_mid=context_.logical[cell];
            if(cell>=42 && cell<47)memory_->cached_auxiliary[cell-42]=context_.logical[cell];
        }
        memory_->logical_cell=uint8_t(memory_->logical_cell+6);
        if(kind_==Kind::secondary_group) {
            at_+=work+11+10+17;
            // The secondary group's semantic owner supplies its limit field.
            // Index/amount and work of the feedback-index suffix are native.
            at_+=secondary_feedback_work(secondary_limit);
            at_+=13+10+10;
        } else at_+=work+10;
        advance();
    }
    void complete_group(unsigned work) noexcept {complete_group(work,context_.secondary_limit);}
private:
    ParameterCompilerMemoryXL* memory_=nullptr;DynamicsState* dynamics_=nullptr;
    ParameterCompilerContextXL context_{};uint64_t at_=0;Kind kind_=Kind::finished;
    void skip_group() noexcept {at_+=11+10+10+4+10;}
    unsigned secondary_feedback_work(uint8_t limit) noexcept {
        const uint8_t raw=memory_->secondary_mid;
        const unsigned index=std::max(1u,unsigned(raw)>>3);
        unsigned work=7+13+11+7+17+decay_compiler_detail::index_work(raw)+5+13+4+10;
        dynamics_->amount=1;
        if(limit){work+=5+4+10;if(index<=unsigned(limit-1))work+=5;}
        else work+=5;
        memory_->secondary_feedback=uint8_t(limit?std::min(unsigned(limit-1),index):index);
        return work+10+10;
    }
    void advance() noexcept {
        // At most eleven masked pages plus the final auxiliary suffix.
        for(unsigned i=0;i<12;++i) {
            at_+=13+5+13+7+10;++memory_->page;
            if(memory_->page==13) {
                at_+=10+5*(17+93)+4*5;
                memory_->cached_auxiliary=context_.auxiliary;
                at_+=16+10+10+7+7+10;
                if(context_.variable_size) {
                    at_+=10+7+13+10;memory_->primary_auxiliary=context_.auxiliary[4];
                } else {
                    at_+=10+7+13+10+7+13+10;
                    memory_->primary_auxiliary=context_.auxiliary[2];
                    memory_->secondary_auxiliary=context_.auxiliary[4];
                }
                kind_=Kind::finished;return;
            }
            const bool skip=(memory_->remaining_mask&0x8000)!=0;
            memory_->remaining_mask=uint16_t(memory_->remaining_mask<<1);
            at_+=11+16+10+16+10+10;
            if(skip){skip_group();continue;}
            at_+=7+10;
            if(memory_->page==2) {
                at_+=5+17+93+5+10;
                const unsigned mid=memory_->logical_cell+1;
                const bool changed=context_.cached[mid]!=context_.logical[mid];
                context_.cached[mid]=context_.logical[mid];
                if(changed) {
                    memory_->secondary_low=memory_->secondary_mid=255;at_+=7+13+13;
                    context_.cached[6]=context_.cached[7]=255;
                }
                at_+=13+13+7+13+17;kind_=Kind::secondary_group;
            } else {at_+=7+13+17;kind_=Kind::group;}
            return;
        }
    }
};
// Normal accepted fader transaction copies the 48-byte program/control record after
// the enclosing compiler, then reaches the separately derived refresh suffix.
constexpr unsigned parameter_record_copy_work_xl=11+10+10+7+17+48*39+10+10+10;
class FaderTransactionClockXL {
public:
    enum class Kind:uint8_t {calibration,compiler,copy,finished};
    struct Event {Kind kind;uint64_t work_state;};
    void reset(uint64_t entry) noexcept {
        // Logical-cell and pickup-cell address preparation precede calibration.
        constexpr unsigned logical=13+4+5+4+4+4+5+7+10+10+10;
        constexpr unsigned pickup=11+5+7+10+10+4+10+10;
        at_=entry+11+17+logical+17+pickup+17;kind_=Kind::calibration;
    }
    Event next()const noexcept{return {kind_,at_};}
    void complete_calibration(unsigned work,bool recompile) noexcept {
        if(kind_!=Kind::calibration)return;
        at_+=work+10;
        if(recompile){at_+=17;kind_=Kind::compiler;}
        else {at_+=40;kind_=Kind::finished;}
    }
    void complete_compiler(unsigned work) noexcept {
        if(kind_!=Kind::compiler)return;
        at_+=work+17;kind_=Kind::copy;
    }
    void complete_copy() noexcept {
        if(kind_!=Kind::copy)return;
        at_+=parameter_record_copy_work_xl+40;kind_=Kind::finished;
    }
private:
    uint64_t at_=0;Kind kind_=Kind::finished;
};
} // namespace cineol::xl
