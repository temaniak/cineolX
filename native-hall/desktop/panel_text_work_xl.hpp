#pragma once
#include "serial_tx_work_xl.hpp"

namespace cineol::xl {
struct PanelTextWorkXL {
    unsigned work_states=0,command_start=0,write_start=0;
};
// Start a text job with its header byte. Character contents and panel I/O
// remain outside this clock; the enclosing native worker owns their length.
// The request disables interrupts and returns through the common restore.
constexpr PanelTextWorkXL panel_text_publish_work(PanelPublishMemoryXL& panel,
    SerialTxMemoryXL& serial,uint8_t characters) noexcept {
    constexpr unsigned command=10+4+44+5+7+7+16+7;
    constexpr unsigned write=command+10+7+13+10+16+7+5+16+4+10+7;
    panel.flags=4;serial.text_remaining=characters;
    return {write+10+10+54,command,write};
}
constexpr unsigned panel_text_copy_work(unsigned characters) noexcept {
    return characters*(7+7+5+5+5+10)+10;
}
// Both status workers construct a fixed 24-character field. Template
// lookup/copy work is independent of the ROM text and displayed digit.
constexpr PanelTextWorkXL panel_status_text_work(PanelPublishMemoryXL& panel,
    SerialTxMemoryXL& serial,bool alternate) noexcept {
    constexpr unsigned lookup=204; // Register exchange is four CPU states.
    constexpr unsigned line=11+17+lookup+10+7+17+panel_text_copy_work(12)+4+7+10;
    constexpr unsigned two_lines=7+10+17+line+7+17+panel_text_copy_work(12)+4+7+10;
    const unsigned prefix=alternate
        ?7+17+two_lines+13+7+13+4+10+10
        :10+10+7+17+panel_text_copy_work(24)+4+7+10+10;
    auto work=panel_text_publish_work(panel,serial,24);
    work.work_states+=prefix;work.command_start+=prefix;work.write_start+=prefix;
    return work;
}
} // namespace cineol::xl
