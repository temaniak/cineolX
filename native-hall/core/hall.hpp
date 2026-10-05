#pragma once
#include "program_bank.hpp"
#include <algorithm>
#include <array>
#include <cstdint>

namespace native_hall {
struct Controls {
    int bass=17, mid=14, crossover=5, treble=23, depth=21, predelay_ms=24, diffusion=1;
    bool mode_enhancement=true, decay_optimization=true;
};

// In-memory diagnostic snapshot only; not a bank, plugin parameter or session ABI.
struct ModulationState {
    std::array<uint8_t,10> descriptors{};
    std::array<int8_t,4> coefficients{};
    std::array<uint16_t,4> offsets{};
    uint16_t index=0;
    uint8_t divider=1,random_divider=8,hold=1;
};

// Fixed native networks for the original 224 v4.4; legacy Hall profiles
// remain supported. program_networks.inc has no WCS interpreter. Coefficient and delay
// address data vary. The original ARU's three-stage rounding/saturation order
// is retained; replacing it with a float multiply changes quiet tails.
class Hall {
public:
    static constexpr int sample_rate=20480, delay_words=16384;
    static constexpr bool separate_headroom=false;
    void prepare(const Profile& p) noexcept { bank_=nullptr;algorithm_=nullptr;profile_=&p;reset();set_controls(Controls{}); }
    void prepare(const ProgramBank& bank,unsigned program=2) noexcept {bank_=&bank;profile_=nullptr;select_program(program);}
    // Hard switch: reset the network and clear its fixed 32 KiB delay once.
    void select_program(unsigned program) noexcept;
    void reset() noexcept;
    void set_controls(const Controls& c) noexcept;
    void process(int16_t left,int16_t right,int16_t outputs[4],unsigned detectors=0) noexcept;
    // Integer network only: desktop may supply its own control scan clock.
    void process_uncontrolled(int16_t left,int16_t right,int16_t outputs[4]) noexcept;
    void observe_decay(int16_t left,int16_t right,unsigned detectors=0) noexcept {
        decay_.observe(left,right,detectors);
    }
    void poll_decay() noexcept {
        if(!profile_ && !algorithm_)return;
        if(decay_.poll(uint8_t((controls_.bass+controls_.mid)/2),controls_.decay_optimization,
                       algorithm_?algorithm_->decay_amount:profile_->decay_amount))
            set_loop_diffusion(decay_.state().amount);
    }
    uint64_t saturation_count() const noexcept {return saturations_;}
    const std::array<int8_t,100>& coefficients() const noexcept {return c_;}
    const std::array<uint16_t,100>& offsets() const noexcept {return offsets_;}
    const std::array<int16_t,delay_words>& memory() const noexcept {return memory_;}
    int32_t accumulator() const noexcept {return acc_;}
    int16_t result() const noexcept {return rr_;}
    // Diagnostic/control-rate entry, also usable by an external deterministic
    // control scheduler. Normal process() supplies the prototype's clock.
    void advance_modulation() noexcept {update_modulation();}
    ModulationState modulation_state() const noexcept;
    void restore_modulation(const ModulationState&) noexcept;
    const DecayState& decay_state() const noexcept {return decay_.state();}
    void restore_decay(const DecayState& state) noexcept {
        decay_.reset(state);level_clock_=0;set_loop_diffusion(state.amount);
    }
    void advance_decay(uint8_t level) noexcept {
        if(decay_.step(level,uint8_t((controls_.bass+controls_.mid)/2),controls_.decay_optimization,(algorithm_?algorithm_->decay_amount:profile_->decay_amount)))
            set_loop_diffusion(decay_.state().amount);
    }
private:
    static int32_t part(int32_t x,unsigned pair) noexcept {
        return ((pair&2)?x:0)+((pair&1)?(x>>1):0);
    }
    int32_t add(int32_t value) noexcept {
        int32_t sum=acc_+value;
        int32_t limited=std::clamp<int32_t>(sum,-262144,262143);
        // Quiet audio must not read/write a 64-bit diagnostic at every ARU edge.
        if(limited!=sum) ++saturations_;
        return limited;
    }
    void edge(unsigned pair,bool neg,bool load,int32_t value,bool zero) noexcept {
        int32_t sum=add(partial_);
        int32_t p=part(operand_,pair);
        partial_=neg?-p:p;
        operand_=load?value:(operand_>>2);
        acc_=zero?0:sum;
    }
    // op: 0 undriven, 1 memory read, 2 memory write, 3 input, 4 result.
    // Template arguments let the compiler remove operation dispatch entirely.
    template<unsigned Row,unsigned Op,unsigned RA,unsigned WA,bool Transfer,bool Zero,unsigned Outputs=0>
    [[gnu::always_inline]] inline void node(int16_t input=0) noexcept {
        node_at<Op,RA,WA,Transfer,Zero,Outputs>(Row,input);
    }
    // Share arithmetic by operation shape, keeping row addresses as data.
    // This fits -O3 in ITCM without outlining edge()/add() at -Os. Each native
    // graph still calls its fixed sequence; there is no opcode interpreter.
    template<unsigned Op,unsigned RA,unsigned WA,bool Transfer,bool Zero,unsigned Outputs>
    [[gnu::noinline]] void node_at(unsigned Row,int16_t input) noexcept {
        edge((previous_magnitude_>>2)&3,previous_negative_,false,0,false);
        int16_t bus=0;
        if constexpr(Op==1) bus=memory_[(position_-offsets_[Row])&0x3fff];
        if constexpr(Op==2 || Op==4) bus=rr_;
        if constexpr(Op==3) bus=input;
        if constexpr(Op==2) memory_[(position_-offsets_[Row])&0x3fff]=bus;
        for(unsigned i=0;i<4;++i) if(Outputs&(1u<<i)) output_[i]=bus;
        registers_[WA]=bus;
        edge(previous_magnitude_&3,previous_negative_,true,int32_t(registers_[RA])*8,false);
        if constexpr(Transfer) rr_=int16_t(add(partial_)>>3);
        const int coef=c_[Row];
        const unsigned magnitude=unsigned(coef<0?-coef:coef);
        edge((magnitude>>4)&3,coef<0,false,0,Zero);
        previous_magnitude_=magnitude; previous_negative_=coef<0;
    }
    void update_modulation() noexcept;
    void update_decay() noexcept;
    void set_loop_diffusion(unsigned reduction) noexcept;
    const Profile* profile_=nullptr;
    const ProgramBank* bank_=nullptr;
    const ProgramProfile* algorithm_=nullptr;
    std::array<int8_t,4> loop_base_{};
    Controls controls_{};
    std::array<int16_t,delay_words> memory_{};
    std::array<int16_t,4> registers_{}, output_{};
    std::array<int8_t,100> c_{};
    std::array<uint16_t,100> offsets_{};
    // CPU-format address/phase state for the two modulated interpolation taps.
    std::array<uint8_t,10> mod_{};
    uint16_t position_=1, mod_index_=0;
    uint8_t mod_divider_=1, random_divider_=8, random_hold_=1;
    uint32_t mod_clock_=0, level_clock_=0;
    int32_t operand_=0,partial_=0,acc_=-1;
    int16_t rr_=0;
    unsigned previous_magnitude_=0;
    bool previous_negative_=false, first_=true;
    uint64_t saturations_=0;
    DecayController decay_;
};
} // namespace native_hall
