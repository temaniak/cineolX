#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <type_traits>
#include "modulation.hpp"
#include "graphs.hpp"
#include "diffusion.hpp"

// Keep the row-emission adapter transparent to the optimizer. Outlining this
// small proxy doubled Resonant Chords' calls per pass on AppleClang 21.
#if defined(_MSC_VER)
#define CINEOL_XL_ROW_PROXY_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define CINEOL_XL_ROW_PROXY_INLINE inline __attribute__((always_inline))
#else
#define CINEOL_XL_ROW_PROXY_INLINE inline
#endif

namespace cineol::xl {
// Prepared control data, deliberately separate from the native graph. This
// desktop-only type is not included by the Daisy engine or its bank format.
template<unsigned Rows> struct NetworkSettings {
    static constexpr unsigned rows=Rows;
    std::array<int8_t,rows> coefficients{};
    std::array<uint16_t,rows> offsets{};
    bool valid() const noexcept {
        for(auto c:coefficients) if(c < -63 || c > 63) return false;
        return true;
    }
};
template<Graph graph> class Network {
public:
    static constexpr unsigned rows=graph_info(graph).rows;
    using Settings=NetworkSettings<rows>;
    // 30.72 MHz / 9 slots / graph rows. Keep the clock rational; integer Hz
    // rounding would slowly change the relation between audio and controls.
    static constexpr unsigned rate_numerator=10240000,rate_denominator=3*rows;
    static constexpr unsigned delay_words=65536;
    bool prepare(const Settings& settings) noexcept {
        if(!settings.valid()) return false;
        settings_=base_settings_=compiler_settings_=settings;modulation_enabled_=false;initial_modulation_={};reset();return true;
    }
    // Updating coefficients/addresses retains the tail. A program/firmware
    // change must explicitly reset the instance instead.
    bool set_settings(const Settings& settings) noexcept {
        if(!settings.valid()) return false;
        settings_=base_settings_=settings;return true;
    }
    void set_controls(Settings settings) noexcept {
        compiler_settings_=settings;
        // A coefficient update preserves the current interpolation position.
        // Ordinary controls keep the running tap coefficients/low addresses.
        // The physical Mod/Size compiler edge explicitly restores them below.
        const unsigned taps=modulation_.flags&15;
        for(unsigned i=0;i<taps;++i) for(unsigned pair=0;pair<2;++pair) {
            const unsigned row=modulation_.rows[i]+pair;
            settings.coefficients[row]=settings_.coefficients[row];
            settings.offsets[row]=uint16_t((settings.offsets[row]&0xff00)|(settings_.offsets[row]&255));
        }
        set_settings(settings);
    }
    bool set_modulation(const ModulationProfile& profile,const ModulationState& state,bool enabled) noexcept {
        if(!profile.valid(rows) || !state.valid()) return false;
        modulation_=profile;modulator_.reset(state);initial_modulation_=state;
        modulation_enabled_=enabled;modulation_clock_=0;return true;
    }
    const ModulationState& modulation_state() const noexcept {return modulator_.state();}
    void set_chorus(uint8_t raw) noexcept {modulation_.set_chorus(raw);}
    void recompile_modulation() noexcept {
        // AB52 resets the sequence index, and the program compiler restores
        // the descriptors and WCS pairs. AD5C's three counters survive the
        // physical key/Size edge; delay memory and DSP registers survive too.
        auto state=modulator_.state();state.index=initial_modulation_.index;
        state.startup_lookup=initial_modulation_.startup_lookup;
        state.address_low=initial_modulation_.address_low;state.phase=initial_modulation_.phase;
        modulator_.reset(state);
        for(unsigned i=0;i<(modulation_.flags&15);++i) for(unsigned pair=0;pair<2;++pair) {
            const unsigned row=modulation_.rows[i]+pair;
            settings_.coefficients[row]=compiler_settings_.coefficients[row];
            settings_.offsets[row]=compiler_settings_.offsets[row];
        }
    }
    void enable_modulation(bool enabled) noexcept {
        if(enabled!=modulation_enabled_) recompile_modulation();
        modulation_enabled_=enabled;
    }
    void set_diffusion(const DiffusionProfile& profile,uint8_t index) noexcept {
        profile.apply(settings_,index,0,0);profile.apply(base_settings_,index,0,0);
        profile.apply(compiler_settings_,index,0,0);
    }
    const Settings& settings() const noexcept {return settings_;}
    void reset() noexcept {
        settings_=base_settings_;
        memory_.fill(0);registers_.fill(0);output_.fill(0);
        position_=1;operand_=partial_=0;acc_=-1;result_=0;
        xreg_to_control_=xreg_from_control_=0;
        previous_magnitude_=0;previous_negative_=false;first_=true;saturations_=0;
        modulation_clock_=0;modulator_.reset(initial_modulation_);
    }
    void process(int16_t left,int16_t right,int16_t outputs[4]) noexcept {
        NoOutput emit;process(left,right,outputs,emit);
    }
    template<class Emit> void process(int16_t left,int16_t right,int16_t outputs[4],Emit&& emit) noexcept {
        process_stream(left,right,emit);
        std::copy(output_.begin(),output_.end(),outputs);
    }
    // The event-DAC path consumes every row's bus word; its final four-word
    // copy is unnecessary. Keep the ordinary output API for offline checks.
    template<class Emit> void process_stream(int16_t left,int16_t right,Emit&& emit) noexcept {
        if(first_) first_=false;else ++position_;
        GraphExecutor<std::remove_reference_t<Emit>>{*this,emit}.run(left,right);
        if(modulation_enabled_) {
            modulation_clock_+=modulation_.rate_tenths*rate_denominator;
            if(modulation_clock_>=rate_numerator*10) {
                modulation_clock_-=rate_numerator*10;modulator_.step(modulation_,settings_);
            }
        }
    }
    const auto& memory() const noexcept {return memory_;}
    int32_t accumulator() const noexcept {return acc_;}
    int16_t result() const noexcept {return result_;}
    int16_t control_output() const noexcept {return xreg_to_control_;}
    void set_control_input(int16_t value) noexcept {xreg_from_control_=value;}
    uint64_t saturation_count() const noexcept {return saturations_;}
private:
    struct NoOutput {template<unsigned Row,unsigned Channels> void operator()(int16_t) const noexcept {}};
    template<class Emit> struct GraphExecutor {
        Network& network;Emit& emit;
        void run(int16_t left,int16_t right) noexcept {
            #include "native_graph_dispatch.inc"
        }
        template<unsigned Row,unsigned Op,unsigned RA,unsigned WA,bool Transfer,bool Zero,unsigned Outputs,bool Shift,bool WriteX=false>
        CINEOL_XL_ROW_PROXY_INLINE void node(int16_t input=0) noexcept {
            const auto bus=network.template node<Row,Op,RA,WA,Transfer,Zero,Outputs,Shift,WriteX>(input);
            if constexpr(Outputs) emit.template operator()<Row,Outputs>(bus);
        }
    };
    int32_t sum() noexcept {
        const int32_t value=acc_+partial_;
        const int32_t bounded=std::clamp(value,-262144,262143);
        if(value!=bounded) ++saturations_;
        return bounded;
    }
    void edge(unsigned pair,bool negative,bool load,int32_t value,bool zero) noexcept {
        const auto added=sum();
        int32_t product=((pair&2)?operand_:0)+((pair&1)?(operand_>>1):0);
        partial_=negative?-product:product;
        operand_=load?value:(operand_>>2);
        acc_=zero?0:added;
    }
    template<unsigned Row,unsigned Op,unsigned RA,unsigned WA,bool Transfer,bool Zero,unsigned Outputs,bool Shift,bool WriteX=false>
    inline int16_t node(int16_t input=0) noexcept {
        edge((previous_magnitude_>>2)&3,previous_negative_,false,0,false);
        int16_t bus=0;
        const uint16_t address=uint16_t(position_-settings_.offsets[Row]);
        if constexpr(Op==1) bus=memory_[address];
        if constexpr(Op==2 || Op==4) bus=result_;
        if constexpr(Op==3) bus=input;
        if constexpr(Op==5) bus=xreg_from_control_;
        if constexpr(Op==2) memory_[address]=bus;
        for(unsigned c=0;c<4;++c) if(Outputs&(1u<<c)) output_[c]=bus;
        if constexpr(WriteX) xreg_to_control_=bus;
        registers_[WA]=bus;
        edge(previous_magnitude_&3,previous_negative_,!Shift,int32_t(registers_[RA])*8,false);
        if constexpr(Transfer) result_=int16_t(sum()>>3);
        const int coefficient=settings_.coefficients[Row];
        const unsigned magnitude=unsigned(coefficient<0?-coefficient:coefficient);
        edge((magnitude>>4)&3,coefficient<0,false,0,Zero);
        previous_magnitude_=magnitude;previous_negative_=coefficient<0;
        return bus;
    }
    Settings settings_{},base_settings_{},compiler_settings_{};
    ModulationProfile modulation_{};
    ModulationState initial_modulation_{};
    Modulator modulator_;
    uint32_t modulation_clock_=0;
    bool modulation_enabled_=false;
    std::array<int16_t,delay_words> memory_{};
    std::array<int16_t,4> registers_{},output_{};
    uint16_t position_=1;
    int32_t operand_=0,partial_=0,acc_=-1;
    int16_t result_=0,xreg_to_control_=0,xreg_from_control_=0;
    unsigned previous_magnitude_=0;
    bool previous_negative_=false,first_=true;
    uint64_t saturations_=0;
};
using Concert=Network<Graph::concert>;
using ConcertSettings=Concert::Settings;
} // namespace cineol::xl
#undef CINEOL_XL_ROW_PROXY_INLINE
