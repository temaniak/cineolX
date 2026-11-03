#pragma once
// Coupled native work clock for the zero-interpolation Rich graphs.
// Coefficient compilation delays subsequent slow/fast polls and commits its
// writes progressively. UART entry is rounded to semantic boundaries; panel
// startup/text and physical fader transactions remain bounded approximations.
#include "slow_entry_clock_xl.hpp"
#include "slow_tail_clock_xl.hpp"
#include "decay_transition_xl.hpp"
#include "fast_control_clock_xl.hpp"
#include "monitor_pass_clock_xl.hpp"
#include "headroom_display_clock_xl.hpp"
#include "coefficient_structure_xl.hpp"
#include "fast_feedback_timing_xl.hpp"
#include "input_xl48.hpp"
#include "concert.hpp"
#include "serial_tx_work_xl.hpp"
#include "parameter_queue_xl.hpp"

namespace cineol::xl {
template<Graph graph> class RichControllerXL {
public:
    using Core=Network<graph>;using Settings=typename Core::Settings;
    struct Counters {uint64_t slow=0,fast=0,mod=0,decay=0,feedback=0,bytes=0,irqs=0,irq_states=0,parameters=0;};
    const Counters& counters() const noexcept {return counters_;}
    const DynamicsState& state() const noexcept {return memory.dynamics;}
    uint64_t cpu_state() const noexcept {return clock_/5;}
    const std::array<uint8_t,48>& active_parameters() const noexcept {return parameters_.active();}
    void modulation(const ModulationProfile& profile,const ModulationState& state) noexcept {
        period_=profile.period;hold_=profile.hold;mod_state_=state;
    }
    void chorus(uint8_t raw) noexcept {const unsigned index=raw>>3;period_=uint8_t(index<16?17-index:1);}
    void enhancement(bool enabled) noexcept {enhancement_=enabled;}
    void request(const std::array<uint8_t,48>& raw,bool optimization) noexcept {
        parameters_.request(raw);requested_optimization_=optimization;
    }
    void reset(const DynamicsProfile& dynamics,const ControlProfile& controls,const std::array<uint8_t,48>& raw,
        const DynamicsState& initial,const Settings& settings,unsigned descriptors,bool dynamic,bool optimization) noexcept {
        dynamics_=&dynamics;controls_=&controls;parameters_.reset(raw);raw_=&parameters_.active();
        memory={};memory.dynamics=initial;live_=base_=settings;counters_={};requested_optimization_=optimization;
        count_=used_=0;clock_=irq_offset_=irq_cursor_=blocked_until_=0;tx_deadline_=~uint64_t(0);serial_={};peak_=0;held_={};display_memory_={};tail_memory_={};panel_={128,0};
        compiler_={};compiler_.cached[0]=initial.low;compiler_.cached[1]=initial.mid;
        for(unsigned i=2;i<6;++i)compiler_.cached[i]=raw[controls.slots[i].cell];
        compiler_.special=initial.mid>=253?65:uint8_t(!dynamics.enabled);
        compiler_.decay_time=uint16_t(controls.decay_duration(uint8_t(std::max(8u,unsigned(initial.mid))),0,raw));
        compiler_.feedback_index=feedback_index(initial.mid);
        for(unsigned i=0;i<Core::rows;++i)negative_[i]=live_.coefficients[i]<0;
        for(const auto& t:controls.slots[1].group[0].targets)
            if(t.row<Core::rows && !live_.coefficients[t.row])negative_[t.row]=(t.signs&1)!=0;
        assert(descriptors==0 && "Rich controller requires zero interpolation destinations");
        descriptors_=descriptors;dynamic_=dynamic;optimization_=optimization;bus_.reset(graph);
        begin_slow(0);
    }
    bool process(const InputFrame& input,int16_t transfer) noexcept {
        clock_+=3*Core::rows;const uint64_t now=clock_/5;
        for(unsigned c=0;c<2;++c) {
            const unsigned bits=EventAdc48::detectors(input.circuit[c]);
            held_[c]|=uint8_t((bits&1)<<4|(bits&2)<<2|(bits&4)|(bits&8)>>2|(bits&16)>>4);
        }
        bool changed=flush(now);
        for(unsigned iteration=0;iteration<128;++iteration) {
            const uint64_t at=next_state();
            irq_offset_+=service(std::min(at+irq_offset_,now));
            if(at+irq_offset_>now)break;
            if(stage_==Stage::entry) {
                const auto e=entry_.next();using K=SlowEntryClockXL::Kind;
                if(e.kind==K::left || e.kind==K::right) {
                    if(at+irq_offset_+9>now)break;entry_.complete_read(read(e.kind==K::right));
                } else if(e.kind==K::feedback) {
                    entry_.complete_compiler(feedback(at),memory.dynamics.period);
                } else if(e.kind==K::feedback_return)entry_.resume_after_compiler();
                else {peak_=entry_.monitor_peak();ramp_.reset(memory.dynamics,at,dynamics_->shared_stop,*raw_);stage_=Stage::ramp;}
            } else if(stage_==Stage::ramp) {
                const auto e=ramp_.next();using K=DecayTransitionXL::Kind;
                if(e.kind==K::compile){ramp_.complete_compiler(decay(at));}
                else if(e.kind==K::compiler_return)ramp_.resume_after_compiler();
                else {tail_.reset(memory,tail_memory_,at,compiler_.decay_time);stage_=Stage::tail;}
            } else if(stage_==Stage::tail) {
                const auto e=tail_.next();using K=SlowTailClockXL::Kind;
                if(e.kind==K::feedback)tail_.complete_compiler(feedback(at),memory.dynamics.period,compiler_.decay_time);
                else if(e.kind==K::compiler_return)tail_.resume_after_compiler();
                else if(e.kind==K::display) {
                    display_memory_.channels[0].input=memory.headroom_left;display_memory_.channels[1].input=memory.headroom_right;
                    display_at_=at;display_.reset(display_memory_,at);stage_=Stage::display;
                } else if(e.kind==K::dip)tail_.complete_dip(255);
                else if(e.kind==K::finished) {
                    // At 85AF, an idle panel with no pending timer work returns
                    // in 93 states; a panel whose flags differ from 80 in 31.
                    assert(panel_.flags!=128 || !tail_memory_.menu_flags);
                    ordinal_=9;pass_.reset(at+(panel_.flags==128?93:31)+slow_to_first_pass_work,descriptors_,peak_);stage_=Stage::pass;
                } else {assert(false && "unexpected Rich panel transaction");break;}
            } else if(stage_==Stage::display) {
                const auto e=display_.next();using K=HeadroomDisplayClockXL::Kind;
                if(e.kind==K::publish) {
                    const uint8_t previous=panel_.flags;const unsigned work=panel_publish_work(panel_);
                    blocked_until_=at+irq_offset_+work;
                    if((previous&128) && panel_.receive_mode!=2 && !(previous&1)) {
                        serial_.remaining=4;tx_deadline_=at+irq_offset_+work-64+serial_character_states_xl;
                    }
                    display_.complete_publish(work);
                }
                else if(e.kind==K::dip) {if(at+irq_offset_+9>now)break;display_.complete_dip(255);}
                else {tail_.complete_display(unsigned(at-display_at_));stage_=Stage::tail;}
            } else if(stage_==Stage::pass) {
                const auto e=pass_.next();using K=MonitorPassClockXL::Kind;
                if(e.kind==K::mod_first || e.kind==K::mod_second){++counters_.mod;pass_.complete(modulation_work());}
                else if(e.kind==K::auxiliary) {
                    const unsigned idle=auxiliary_idle_work(0,compiler_.special,0);assert(idle);pass_.complete(idle);
                } else if(e.kind==K::monitor_high || e.kind==K::monitor_low) {
                    if(at+irq_offset_+9>now)break;
                    pass_.complete(e.kind==K::monitor_high?uint8_t(uint16_t(transfer)>>8):uint8_t(transfer));
                } else {
                    peak_=pass_.peak();
                    if(ordinal_&1) {
                        ++counters_.fast;
                        fast_.reset(memory,at+pass_to_fast_work,compiler_.special,dynamics_->base_period,*raw_);stage_=Stage::fast;
                    } else {--ordinal_;pass_.reset(at+even_pass_to_next_pass_work,descriptors_,peak_);}
                }
            } else if(stage_==Stage::reconcile) {
                reconcile_at_+=67+((panel_.flags&128)?24:0);
                if(++reconcile_cell_==10) {
                    if(parameters_.pending() || optimization_!=requested_optimization_) {
                        reconcile_at_=parameters(reconcile_at_);
                        changed=true;
                    }
                    begin_slow(reconcile_at_+27);
                }
            } else {
                const auto e=fast_.next();using K=FastControlClockXL::Kind;
                if(e.kind==K::left || e.kind==K::right) {
                    if(at+irq_offset_+9>now)break;fast_.complete_read(read(e.kind==K::right));
                } else if(e.kind==K::feedback)fast_.complete_compiler(feedback(at),memory.dynamics.period);
                else if(e.kind==K::restore){fast_.complete_compiler(decay(at),memory.dynamics.period);}
                else if(e.kind==K::feedback_return || e.kind==K::restore_return)fast_.resume_after_compiler();
                else if(--ordinal_) {pass_.reset(at+fast_to_next_pass_work,descriptors_,peak_);stage_=Stage::pass;}
                else {
                    // Last Fast return -> ten coherent panel cells -> Slow.
                    // Each unchanged cell costs 67, plus 24 if panel bit 7.
                    reconcile_at_=at+52;reconcile_cell_=0;stage_=Stage::reconcile;
                }
            }
            changed|=flush(now);
        }
        return changed;
    }
    const Settings& settings()const noexcept{return live_;}
private:
    enum class Stage:uint8_t {entry,ramp,tail,display,pass,fast,reconcile};Stage stage_=Stage::entry;
    struct Byte {uint64_t commit;ControlByteXL value;};std::array<Byte,128> bytes_{};
    FastControlMemoryXL memory{};
    Counters counters_{};
    ParameterQueueXL parameters_;
    ModulationState mod_state_{};
    uint8_t period_=1,hold_=32;
    bool enhancement_=true,requested_optimization_=false;
    unsigned count_=0,used_=0,descriptors_=0,ordinal_=9;
    uint64_t clock_=0,display_at_=0,irq_offset_=0,irq_cursor_=0,blocked_until_=0,tx_deadline_=~uint64_t(0),reconcile_at_=0;unsigned reconcile_cell_=0;SerialTxMemoryXL serial_{};uint16_t peak_=0;
    std::array<uint8_t,2> held_{};std::array<bool,Core::rows> negative_{};
    const DynamicsProfile* dynamics_=nullptr;const ControlProfile* controls_=nullptr;const std::array<uint8_t,48>* raw_=nullptr;
    bool dynamic_=false,optimization_=false;Settings live_{},base_{};
    SlowEntryClockXL entry_;DecayTransitionXL ramp_;SlowTailClockXL tail_;
    HeadroomDisplayClockXL display_;MonitorPassClockXL pass_;FastControlClockXL fast_;
    SlowTailMemoryXL tail_memory_{};HeadroomDisplayMemoryXL display_memory_{};PanelPublishMemoryXL panel_{};
    DecayCompilerMemoryXL compiler_{};FeedbackBytesXL feedback_bytes_{};WcsTimingXL bus_;
    uint64_t next_state()const noexcept {
        switch(stage_) {
            case Stage::entry:return entry_.next().work_state;case Stage::ramp:return ramp_.next().work_state;
            case Stage::tail:return tail_.next().work_state;case Stage::display:return display_.next().work_state;
            case Stage::pass:return pass_.next().work_state;case Stage::fast:return fast_.next().work_state;case Stage::reconcile:return reconcile_at_;
        }
        return ~uint64_t(0);
    }
    unsigned modulation_work() noexcept {
        if(!enhancement_)return 1127;
        unsigned work=100;
        if(!--mod_state_.random_divider) {
            mod_state_.random_divider=8;work+=35;
            if(!--mod_state_.random_hold){mod_state_.random_hold=hold_;work+=75;}
        }
        if(--mod_state_.divider)return work+1097;
        mod_state_.divider=period_;return work+91;
    }
    uint64_t parameters(uint64_t at) noexcept {
        assert(used_==count_);
        const auto previous=*raw_;parameters_.commit();++counters_.parameters;
        dynamic_=((*raw_)[42]&1)!=0;optimization_=requested_optimization_;
        bool update_decay=bool(previous[42]&1)!=dynamic_;
        for(unsigned cell:{0u,1u,6u,7u,12u,13u,43u,44u})update_decay|=previous[cell]!=(*raw_)[cell];
        Dynamics state;state.reset(memory.dynamics);
        state.parameters(*dynamics_,*controls_,*raw_,dynamic_,optimization_,update_decay);
        memory.dynamics=state.state();
        auto effective=*raw_;effective[0]=memory.dynamics.low;effective[1]=memory.dynamics.mid;
        live_=base_;controls_->apply_size(live_,effective);
        controls_->apply_static(live_,effective,memory.dynamics.amount);
        effective[1]=memory.dynamics.feedback_mid;
        controls_->apply_feedback(live_,effective,memory.dynamics.feedback_amount);
        if(previous[43]!=(*raw_)[43] || previous[44]!=(*raw_)[44])compiler_.cached[0]=compiler_.cached[1]=255;
        // Coherent host updates wait for the current scan/compiler. Refresh
        // only the static cache; the main compiler retains live LF/MID state.
        for(unsigned i=2;i<6;++i)compiler_.cached[i]=(*raw_)[controls_->slots[i].cell];
        return at+17+decay(at+17)+1957+40;
    }
    uint8_t read(unsigned channel) noexcept {const auto value=held_[channel];held_[channel]=0;return value;}
    void begin_slow(uint64_t at) noexcept {
        ++counters_.slow;stage_=Stage::entry;
        entry_.reset(memory,at,peak_,compiler_.special,dynamics_->base_period,(*raw_)[45],dynamic_,optimization_);
    }
    unsigned write(uint64_t start,ControlByteXL value) noexcept {
        const uint64_t interrupted=start+service(start);
        const auto access=bus_.access(interrupted+4,false);irq_cursor_=access.finished_cpu_state;assert(count_<bytes_.size());
        bytes_[count_++]={access.commit,value};++counters_.bytes;
        return unsigned(access.finished_cpu_state-start);
    }
    unsigned decay(uint64_t at) noexcept {
        assert(used_==count_);count_=used_=0;++counters_.decay;
        const unsigned work=decay_compile_work(*dynamics_,*controls_,*raw_,compiler_,memory.dynamics,
            coefficient_lane_low2[unsigned(graph)],coefficient_sign_lane_low7[unsigned(graph)],
            [&](unsigned offset,ControlByteXL value)noexcept{return write(at+irq_offset_+offset,value);});
        const unsigned extra=service(at+irq_offset_+work);irq_cursor_=at+irq_offset_+work+extra;
        return work+extra;
    }
    uint8_t feedback_index(uint8_t value)const noexcept {
        const unsigned limit=controls_->definition_cell==255?controls_->feedback_limit:32-(unsigned((*raw_)[controls_->definition_cell])>>3);
        const unsigned index=std::max(1u,unsigned(value)>>3);
        return uint8_t(limit?std::min(limit-1,index):index);
    }
    unsigned feedback(uint64_t at) noexcept {
        assert(used_==count_);count_=used_=0;++counters_.feedback;
        const unsigned work=10+feedback_work_cycles(controls_->feedback,coefficient_lane_low2[unsigned(graph)],compiler_.feedback_index,
            feedback_index((*raw_)[7]),memory.dynamics.amount,feedback_bytes_,
            [&](unsigned offset,ControlByteXL value)noexcept{return write(at+irq_offset_+10+offset,value);});
        const unsigned extra=service(at+irq_offset_+work);irq_cursor_=at+irq_offset_+work+extra;
        memory.dynamics.feedback_mid=memory.dynamics.mid;memory.dynamics.feedback_amount=memory.dynamics.amount;
        memory.dynamics.period=Dynamics::normal_period(*dynamics_,*controls_,*raw_,memory.dynamics.peak_input);
        return work+extra;
    }
    unsigned service(uint64_t target) noexcept {
        unsigned added=0;
        for(unsigned i=0;i<128 && tx_deadline_<=target+added;++i) {
            const uint64_t entry=std::max({tx_deadline_,irq_cursor_,blocked_until_});
            if(entry>target+added)break;
            const auto result=serial_tx_work(panel_,serial_);
            ++counters_.irqs;counters_.irq_states+=result.work_states;
            added+=result.work_states;irq_cursor_=entry+result.work_states;
            if(result.action==SerialTxWorkXL::Action::disable)tx_deadline_=~uint64_t(0);
            else tx_deadline_=entry+result.write_start+10+serial_character_states_xl;
        }
        return added;
    }
    bool flush(uint64_t now) noexcept {
        bool changed=false;
        while(used_<count_ && bytes_[used_].commit<=now*WcsTimingXL::cpu_period) {
            const auto value=bytes_[used_++].value;const unsigned row=value.row;
            if(value.lane==2) {
                negative_[row]=!(value.value&128);const int magnitude=std::abs(int(live_.coefficients[row]));
                live_.coefficients[row]=int8_t(negative_[row]?-magnitude:magnitude);
            } else if(value.lane==3) {
                const int magnitude=unsigned(uint8_t(~value.value))>>2;
                live_.coefficients[row]=int8_t(negative_[row]?-magnitude:magnitude);
            }
            changed=true;
        }
        return changed;
    }
};
} // namespace cineol::xl
