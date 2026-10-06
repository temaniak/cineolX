#pragma once
#include "panel_service_clock_xl.hpp"
#include "panel_text_work_xl.hpp"

namespace cineol::xl {
struct PanelTitleContextXL {
    uint8_t control_flags=0;
    std::array<uint8_t,7> selection{};
};
struct PanelTitleMemoryXL {
    uint8_t view=0,last_slider=255,last_column=255,compiler_request=0;
    std::array<uint8_t,7> selection{};
};
// Normal program-title path. Configuration/edit menus and control-field
// formatting are separate workers. Text contents do not select its branches.
class PanelTitleClockXL {
public:
    enum class Kind:uint8_t {publish,format,finished,unsupported};
    struct Event {Kind kind;uint64_t work_state;};
    void reset(PanelTitleMemoryXL& memory,PanelServiceMemoryXL& service,
        PanelPublishMemoryXL& panel,SerialTxMemoryXL& serial,
        const PanelTitleContextXL& context,uint64_t entry) noexcept {
        memory_=&memory;panel_=&panel;serial_=&serial;at_=entry;
        if(memory.view==12 || memory.view==13 || (context.control_flags&4)) {
            kind_=Kind::unsupported;return;
        }
        constexpr unsigned clear_selection=4+5+13+13+10;
        constexpr unsigned clear_timers=4+3*13+7+13+10;
        unsigned snapshot=6*(13+13)+7+13+10+7+7;
        if(memory.view==8)snapshot+=11;
        else {
            snapshot+=5+10+13+7+10;
            const uint8_t mode=context.selection[0];
            if(mode==1){snapshot+=10+10;memory.view=5;}
            else if(mode==2){snapshot+=7+5+10+10;memory.view=4;}
            else {snapshot+=7+11;memory.view=0;}
        }
        memory.last_slider=memory.last_column=255;memory.compiler_request=0;
        memory.selection=context.selection;memory.selection[1]=255;
        service.menu_flags=0;service.timers={};
        at_+=7+13+7+10+7+10+5+13+17+clear_selection+17+clear_timers+17+snapshot;
        // Caller branch, title field template, and jumps to publication.
        at_+=13+7+10+17+13+17+template_line_work()+10+10;
        kind_=Kind::publish;
    }
    Event next()const noexcept{return {kind_,at_};}
    void complete_publish() noexcept {
        if(kind_!=Kind::publish)return;
        at_+=panel_text_publish_work(*panel_,*serial_,12).work_states;
        at_+=7+17;kind_=Kind::format;
    }
    void complete_format() noexcept {
        if(kind_!=Kind::format)return;
        const uint8_t mode=memory_->selection[0];
        unsigned work=11+7+17+template_line_work()+13+7+10;
        if(mode==1)work+=10+7+10+7+10+7+17+template_line_work()+3*(13+7+13)+10;
        else if(mode==2)work+=7+10+10+7+10+11+7+17+template_line_work()+4*(13+7+13)+10+7+11;
        else work+=7+10+10+10;
        // The second field extends the published text before its next byte.
        serial_->text_remaining=24;
        at_+=work+13+4+11+10;kind_=Kind::finished;
    }
private:
    static constexpr unsigned template_line_work() noexcept {
        return 7+10+10+11+17+204+10+7+17+panel_text_copy_work(12)+4+7+10;
    }
    PanelTitleMemoryXL* memory_=nullptr;PanelPublishMemoryXL* panel_=nullptr;
    SerialTxMemoryXL* serial_=nullptr;uint64_t at_=0;Kind kind_=Kind::unsupported;
};
} // namespace cineol::xl
