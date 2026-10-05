#pragma once
#include "../core/hall.hpp"

namespace native_hall {
// Original-224 desktop control scan. Firmware reads nine panel channels;
// each scan calls modulation twice but samples the XREG peak only once.
// Headroom comparators hold every ADC peak until the level read. This is
// a native scheduler, not an 8080 interpreter; nominal bank rates still
// approximate the signal-dependent instruction timing.
class DesktopHall : public Hall {
public:
    void prepare(const Profile& profile) noexcept {
        bank_=nullptr;profile_=&profile;Hall::prepare(profile);initialize();
    }
    void prepare(const ProgramBank& bank,unsigned program=2) noexcept {
        bank_=&bank;profile_=nullptr;program_=std::min(program,program_count-1);
        Hall::prepare(bank,program_);initialize();
    }
    void reset() noexcept {Hall::reset();initialize();}
    void select_program(unsigned program) noexcept {
        if(!bank_)return;
        const auto state=decay_state();
        const auto modulation=modulation_state();
        program_=std::min(program,program_count-1);Hall::select_program(program_);
        // The firmware's level/history dividers live outside the program.
        restore_decay(state);
        // The ROM program compiler resets its random-table index and tap
        // descriptors, but leaves these three free-running counters alone.
        auto next=modulation_state();next.divider=modulation.divider;
        next.random_divider=modulation.random_divider;next.hold=modulation.hold;
        restore_modulation(next);
    }
    void set_controls(const Controls& controls) noexcept {
        controls_=controls;Hall::set_controls(controls);
    }
    void process(int16_t left,int16_t right,int16_t outputs[4],unsigned detectors=0) noexcept {
        process_uncontrolled(left,right,outputs);
        if(!bank_ && !profile_)return;
        observe_decay(0,0,detectors);
        const unsigned mode=unsigned(controls_.mode_enhancement)|unsigned(controls_.decay_optimization)<<1;
        const unsigned rate=bank_?bank_->programs[program_].modulation_rate_tenths[mode]
                                 :unsigned(profile_->level_rate_tenths[mode])*18;
        const unsigned before=clock_;clock_+=rate;
        if(clock_<sample_rate*10)return;
        clock_-=sample_rate*10;
        if(scan_&1) {
            // XREG holds the latest channel (input rows 0 and 50). Resolve
            // the nominal scan edge within this completed network pass.
            const int16_t value=2u*(sample_rate*10-before)<rate?left:right;
            observe_decay(value,value);
        }
        if(controls_.mode_enhancement)advance_modulation();
        if(++scan_==18){scan_=0;poll_decay();}
    }
private:
    void initialize() noexcept {
        clock_=0;scan_=0;
        auto state=decay_state();
        // Import sweeps can leave period 15 after testing optimization.
        // Normal silent firmware startup has an unset (zero) period.
        state.diffusion_period=0;restore_decay(state);
    }
    const ProgramBank* bank_=nullptr;
    const Profile* profile_=nullptr;
    Controls controls_{};
    unsigned program_=2,clock_=0,scan_=0;
};
} // namespace native_hall
