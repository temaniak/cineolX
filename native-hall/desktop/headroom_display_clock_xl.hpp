#pragma once
#include <array>
#include <cstdint>

namespace cineol::xl {
// Display state affects the slow controller's duration, even though glyphs
// themselves do not affect DSP. Glyph tables and panel I/O stay outside this
// timing component; their values do not select any of these branches.
struct HeadroomDisplayChannelXL {
    uint8_t input=0,cache=0,peak=0,hold=0,changed=0;
};
struct HeadroomDisplayMemoryXL {
    std::array<HeadroomDisplayChannelXL,2> channels{};
    uint8_t divider=3,lamp=0;
};
struct PanelPublishMemoryXL {uint8_t flags=0,receive_mode=0;};
// A publication request executes with interrupts disabled. The enclosing
// scan owns serial service and samples its current panel state at this event.
// Costs include the request's return, excluding its caller and serial IRQs.
constexpr unsigned panel_publish_work(PanelPublishMemoryXL& memory) noexcept {
    unsigned work=78;
    if(memory.receive_mode==2)return work+54;
    work+=34;
    if(memory.flags&1)return work+54;
    work+=17;
    if(!(memory.flags&128)) {
        memory.flags|=8;
        return work+35+10+31+54;
    }
    memory.flags=uint8_t((memory.flags|1)&~uint8_t(128|8));
    return work+81+10+101+54;
}
class HeadroomDisplayClockXL {
public:
    enum class Kind:uint8_t {publish,dip,finished};
    struct Event {Kind kind;uint64_t work_state;};
    void reset(HeadroomDisplayMemoryXL& memory,uint64_t entry) noexcept {
        memory_=&memory;at_=entry;stage_=0;advance();
    }
    Event next()const noexcept{return {kind_,at_};}
    void complete_publish(unsigned work) noexcept {
        if(kind_!=Kind::publish)return;
        at_+=work;advance();
    }
    void complete_dip(uint8_t byte) noexcept {
        if(kind_!=Kind::dip)return;
        memory_->lamp=(byte&1)?0:192;
        at_+=10+4+7+4+4+13;
        advance();
    }
private:
    struct Branch {unsigned work;bool publish;};
    static Branch rise(HeadroomDisplayChannelXL& s) noexcept {
        unsigned work=41;const unsigned cached=s.cache&31;
        if(cached!=s.input) {
            if(cached>s.input)return {work+11,false};
            work+=5;
        }
        const bool rising=cached<s.input;
        s.cache=s.input;work+=61;
        if(s.peak!=s.input)work+=10;
        if(s.peak<=s.input){s.peak=s.input;s.hold=20;work+=32;}
        work+=10+(rising?5:11);
        if(!rising)return {work,false};
        s.changed=1;return {work+178,true};
    }
    static Branch fall(HeadroomDisplayChannelXL& s) noexcept {
        unsigned work=35;
        if(!--s.hold){s.peak=0;work+=20;}
        const bool changed=s.changed;s.changed=0;
        if(changed)return {work+37,false};
        work+=31+102;
        if(s.cache&32){s.cache&=31;work+=7+7+7;}
        else {s.cache=uint8_t(s.cache<<1|s.cache>>7);work+=7+4+7+10;}
        return {work+4+5+10,true};
    }
    void advance() noexcept {
        for(;;) {
            if(stage_==0 || stage_==1) {
                const unsigned channel=stage_++;
                const auto branch=rise(memory_->channels[channel]);
                at_+=10+17+branch.work+10;
                if(branch.publish){publish();return;}
            } else if(stage_==2) {
                ++stage_;at_+=10+10;
                if(!(uint8_t(--memory_->divider)&128)) {
                    at_+=11;kind_=Kind::finished;return;
                }
                memory_->divider=3;at_+=5+10;kind_=Kind::dip;return;
            } else if(stage_==3 || stage_==4) {
                const unsigned channel=stage_++-3;
                const auto branch=fall(memory_->channels[channel]);
                at_+=10+17+branch.work;
                if(!channel)at_+=10;
                else at_+=branch.publish?5:11;
                if(branch.publish){publish();return;}
                if(channel){kind_=Kind::finished;return;}
            } else {
                at_+=10;kind_=Kind::finished;return;
            }
        }
    }
    void publish() noexcept {
        // Caller address setup + CALL, glyph construction, and both jumps
        // to the interrupt-disabled publication request. No ROM table lookup.
        at_+=10+10+17+242;kind_=Kind::publish;
    }
    HeadroomDisplayMemoryXL* memory_=nullptr;
    uint64_t at_=0;
    uint8_t stage_=0;
    Kind kind_=Kind::finished;
};
} // namespace cineol::xl
