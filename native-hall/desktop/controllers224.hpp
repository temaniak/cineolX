#pragma once
#include "../core/hall.hpp"
#include "control_scan224.hpp"

namespace native_hall {
// Original-224 desktop control scan. Firmware reads nine panel channels;
// each scan calls modulation twice but samples the XREG peak only once.
// Headroom comparators hold every ADC peak until the level read.
// Original banks use the validated native event clock; legacy single-Hall
// profiles retain their prepared nominal rates.
class DesktopHall : public Hall {
public:
    static constexpr bool separate_headroom=true;
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
        scan_clock_.reset(program_networks[program_],cycles_);
        held_detectors_.fill(0);previous_input_.fill(0);older_right_=0;
        previous_right_detector_=0;
    }
    void set_controls(const Controls& controls) noexcept {
        controls_=controls;Hall::set_controls(controls);
    }
    void process(int16_t left,int16_t right,int16_t outputs[4],unsigned detectors=0) noexcept {
        if(bank_) {
            // The left converter hold belongs to the preceding pass (row
            // -46); right belongs to row 4 of this pass. Resolve the previous
            // pass's control events before running this pass's audio graph.
            // No audio sample or advertised converter latency is added.
            if(cycles_) {
                const uint64_t begin=cycles_-100;
                bool left_added=false,right_added=false;
                const auto accumulate=[&](uint64_t t) {
                    if(!right_added && t>=begin+6){held_detectors_[1]|=previous_right_detector_;right_added=true;}
                    if(!left_added && t>=begin+56){held_detectors_[0]|=uint8_t(detectors&31);left_added=true;}
                };
                scan_clock_.run_until(cycles_,*bank_,program_,*this,controls_,
                    [&](uint64_t t){
                        const unsigned phase=unsigned(t-begin);
                        return phase<3?older_right_:previous_input_[phase<53?0:1];
                    },[&](unsigned channel,uint64_t t){
                        accumulate(t);const auto mask=held_detectors_[channel];held_detectors_[channel]=0;return mask;
                    },[](ControlScan224::Event,uint64_t,uint16_t){});
                accumulate(cycles_);
            } else held_detectors_[0]|=uint8_t(detectors&31);
            process_uncontrolled(left,right,outputs);
            older_right_=previous_input_[1];previous_input_={{left,right}};
            previous_right_detector_=uint8_t((detectors>>8)&31);cycles_+=100;
            return;
        }
        process_uncontrolled(left,right,outputs);
        if(!bank_ && !profile_)return;
        observe_decay(0,0,(detectors|(detectors>>8))&31);
        const unsigned mode=unsigned(controls_.mode_enhancement)|unsigned(controls_.decay_optimization)<<1;
        const unsigned rate=unsigned(profile_->level_rate_tenths[mode])*18;
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
        clock_=0;scan_=0;cycles_=0;held_detectors_.fill(0);
        previous_input_.fill(0);older_right_=0;previous_right_detector_=0;
        scan_clock_.reset(bank_?program_networks[program_]:0);
        auto state=decay_state();
        // Import sweeps can leave period 15 after testing optimization.
        // Normal silent firmware startup has an unset (zero) period.
        state.diffusion_period=0;restore_decay(state);
    }
    const ProgramBank* bank_=nullptr;
    const Profile* profile_=nullptr;
    Controls controls_{};
    ControlScan224 scan_clock_;
    std::array<int16_t,2> previous_input_{};
    std::array<uint8_t,2> held_detectors_{};
    uint64_t cycles_=0;
    int16_t older_right_=0;
    uint8_t previous_right_detector_=0;
    unsigned program_=2,clock_=0,scan_=0;
};
} // namespace native_hall
