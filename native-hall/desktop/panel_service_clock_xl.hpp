#pragma once
#include <array>
#include <cstdint>

namespace cineol::xl {
struct PanelServiceMemoryXL {
    uint8_t menu_flags=0;
    std::array<uint8_t,3> timers{};
};
// Slow-return panel dispatch. Text/status rendering and UART service belong
// to their enclosing clocks; this component never formats or sends text.
class PanelServiceClockXL {
public:
    enum class Kind:uint8_t {title,status,alternate_status,finished};
    struct Event {Kind kind;uint64_t work_state;};
    void reset(PanelServiceMemoryXL& memory,uint8_t panel_flags,uint64_t entry) noexcept {
        memory_=&memory;at_=entry+25;kind_=Kind::finished;
        if(panel_flags!=128){at_+=6;return;}
        at_+=34;
        if(memory.menu_flags&1) {
            // Clear the three panel timers and flags before title dispatch.
            at_+=17+73;memory.menu_flags=0;memory.timers={};
            memory.timers[1]=24;at_+=37;kind_=Kind::title;return;
        }
        at_+=24;
        if(!(memory.menu_flags&32)){at_+=10;return;}
        at_+=34+10+17;
        const bool alternate=memory.menu_flags&16;
        memory.menu_flags=alternate?8:16;
        kind_=alternate?Kind::alternate_status:Kind::status;
    }
    Event next()const noexcept {return {kind_,at_};}
    // A title worker may itself clear panel timers. The worker owns that
    // branch; the enclosing clock supplies its completion and clear action.
    void complete_worker(unsigned work,bool cleared_timers=false) noexcept {
        if(kind_==Kind::finished)return;
        at_+=work;
        if(cleared_timers){memory_->menu_flags=0;memory_->timers={};}
        if(kind_!=Kind::title) {
            memory_->timers[0]=kind_==Kind::status?32:16;
            at_+=30;
        }
        kind_=Kind::finished;
    }
private:
    PanelServiceMemoryXL* memory_=nullptr;
    uint64_t at_=0;
    Kind kind_=Kind::finished;
};
} // namespace cineol::xl
