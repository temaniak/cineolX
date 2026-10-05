#pragma once
#include "scan_timing224.hpp"

namespace native_hall {
// A fixed native event clock for stable original-224 controls. This stores
// controller stages, never CPU registers, opcodes or firmware memory.
// The audio graph still applies changed coefficient groups at call completion;
// displaced DSP fetches and held ARU clocks are outside this scheduler.
class ControlScan224 {
public:
    enum class Event {Modulation,TransferHigh,TransferLow,Level,LevelWord};
    void reset(unsigned network,uint64_t cycle=0,unsigned next_row=0) noexcept {
        next_=cycle;stage_=Stage::First;channel_=0;peak_=level_word_=0;
        bus_.reset(network,cycle,next_row);
    }
    // Canonical settled physical-panel scan: six pots retained two ADC codes
    // below their current value and three released switch banks. Offline
    // diagnostics may supply a different once-captured stable panel context.
    void set_panel_costs(const std::array<unsigned,9>& costs) noexcept {panel_costs_=costs;}
    uint64_t next_event() const noexcept {return next_;}
    template<class Word,class Detectors,class Observe>
    void run_until(uint64_t limit,const ProgramBank& bank,unsigned program,Hall& hall,
                   const Controls& controls,Word&& word,Detectors&& detectors,Observe&& observe) noexcept {
        while(next_<limit) {
            switch(stage_) {
            case Stage::First:
            case Stage::Second: {
                const bool first=stage_==Stage::First;
                observe(Event::Modulation,next_,uint16_t(0));
                mod_enabled_=controls.mode_enhancement;
                const uint64_t entry=next_;
                next_+=modulation_cycles224(bank,program,hall.modulation_state(),mod_enabled_,
                    [&](unsigned at){return bus_.write(entry+at);});
                stage_=first?Stage::FirstDone:Stage::SecondDone;break;
            }
            case Stage::FirstDone:
                if(mod_enabled_)hall.advance_modulation();
                next_+=transfer_gap224(bank.programs[program].modulation_flags)+10;
                stage_=Stage::High;break;
            case Stage::High: {
                const uint16_t sample=uint16_t(word(next_));
                observe(Event::TransferHigh,next_,sample);
                uint8_t high=uint8_t(uint16_t(~sample)>>8);
                const bool negative=high&128;
                if(negative)high^=255;
                const unsigned high_cycles=46+(negative?8:0);
                if(high) {
                    const uint16_t magnitude=uint16_t(high)<<8;
                    next_+=high_cycles-10+98+(magnitude>peak_?20:0);
                    peak_=std::max(peak_,magnitude);stage_=Stage::Second;
                } else {next_+=high_cycles;stage_=Stage::Low;}
                break;
            }
            case Stage::Low: {
                const uint16_t sample=uint16_t(word(next_));
                observe(Event::TransferLow,next_,sample);
                uint8_t low=uint8_t(~sample);
                const bool negative=low&128;
                if(negative)low^=255;
                next_+=29+(negative?4:0)-10+98+(low>peak_?20:0);
                peak_=std::max(peak_,uint16_t(low));stage_=Stage::Second;break;
            }
            case Stage::SecondDone:
                if(mod_enabled_)hall.advance_modulation();
                next_+=147+panel_costs_[channel_]+37;
                if(++channel_==9) {channel_=0;next_+=17;stage_=Stage::Level;}
                else {next_+=189;stage_=Stage::First;}
                break;
            case Stage::Level:
                observe(Event::Level,next_,uint16_t(0));
                level_entry_=next_;next_+=26;stage_=Stage::HeadroomLeft;break;
            case Stage::HeadroomLeft:
                level_word_=uint16_t((detectors(0,next_)&31)<<11);
                next_+=24;stage_=Stage::HeadroomRight;break;
            case Stage::HeadroomRight:
                level_word_|=uint16_t((detectors(1,next_)&31)<<11);
                next_+=323;stage_=Stage::LevelWord;break;
            case Stage::LevelWord: {
                level_word_|=peak_;peak_=0;
                observe(Event::LevelWord,next_,level_word_);
                level_mean_=uint8_t((controls.bass+controls.mid)/2);
                level_enabled_=controls.decay_optimization;
                level_period_=bank.programs[program].decay_amount;
                const unsigned duration=level_cycles224(program,hall.decay_state(),level_word_,
                    level_mean_,level_enabled_,level_period_,display_check_,false,
                    [&](unsigned at){return bus_.write(level_entry_+at);});
                next_=level_entry_+duration;stage_=Stage::LevelDone;break;
            }
            case Stage::LevelDone: {
                DecayController controller;controller.reset(hall.decay_state());
                controller.step(DecayController::level_from_word(level_word_),level_mean_,level_enabled_,level_period_);
                hall.restore_decay(controller.state());
                next_+=226;stage_=Stage::First;break;
            }
            }
        }
    }
    void set_display_check(bool enabled) noexcept {display_check_=enabled;}
private:
    enum class Stage {First,FirstDone,High,Low,Second,SecondDone,Level,HeadroomLeft,HeadroomRight,LevelWord,LevelDone};
    WcsTiming224 bus_;
    std::array<unsigned,9> panel_costs_{{112,112,112,112,112,112,78,48,48}};
    uint64_t next_=0,level_entry_=0;
    uint16_t peak_=0,level_word_=0;
    unsigned channel_=0;
    uint8_t level_mean_=0,level_period_=0;
    Stage stage_=Stage::First;
    bool mod_enabled_=false,level_enabled_=false,display_check_=false;
};
} // namespace native_hall
