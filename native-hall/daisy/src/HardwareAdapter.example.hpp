#pragma once
#include "Controls.hpp"

// Copy to HardwareAdapter.local.hpp and assign ADC/button/RGB pins below.
// Default-constructed pins are disabled; no board wiring is published.
// ADC readings are smoothed 0..1, then transformed to your declared units.
// Examples: scale=3.3, offset=0 -> 0..3.3 V; scale=5, offset=0 -> 0..5 V;
// scale=10, offset=-5 -> -5..+5 V for a suitably conditioned bipolar input.
struct AnalogInput {
    daisy::Pin pin{};
    float scale=1,offset=0,slew_seconds=0.002f;
};
struct ButtonInput {
    daisy::Pin pin{};
    bool active_high=false;
    daisy::GPIO::Pull pull=daisy::GPIO::Pull::PULLUP;
};
struct RgbOutput {daisy::Pin pin{};bool active_high=true;};
inline constexpr std::array<AnalogInput,10> analog_inputs{};
inline constexpr std::array<ButtonInput,2> button_inputs{};
inline constexpr std::array<RgbOutput,3> rgb_outputs{}; // red, green, blue

struct DefaultHardwareConfig {
    inline static constexpr const auto& analog_inputs=::analog_inputs;
    inline static constexpr const auto& button_inputs=::button_inputs;
    inline static constexpr const auto& rgb_outputs=::rgb_outputs;
};

template<class Config=DefaultHardwareConfig> class BasicHardwareAdapter {
public:
    template<class Board> void Init(Board& board) {
        daisy::AdcChannelConfig channels[10];unsigned count=0;
        for(unsigned i=0;i<10;++i) {
            slots_[i]=255;
            if(Config::analog_inputs[i].pin.IsValid()) {
                channels[count].InitSingle(Config::analog_inputs[i].pin,daisy::AdcChannelConfig::SPEED_64CYCLES_5);
                slots_[i]=count++;
            }
        }
        if(count) {
            board.adc.Stop();board.adc.Init(channels,count,daisy::AdcHandle::OVS_32);
            for(unsigned i=0;i<10;++i) if(slots_[i]!=255)
                analog_[i].Init(board.adc.GetPtr(slots_[i]),500,false,false,Config::analog_inputs[i].slew_seconds);
            board.adc.Start();
            // ADC/smoothing warm-up is before audio starts, never in Read.
            daisy::System::Delay(5);
            for(unsigned n=0;n<16;++n) {
                for(unsigned i=0;i<10;++i) if(slots_[i]!=255) analog_[i].Process();
                daisy::System::Delay(2);
            }
        }
        for(unsigned i=0;i<2;++i) if(Config::button_inputs[i].pin.IsValid())
            buttons_[i].Init(Config::button_inputs[i].pin,daisy::GPIO::Mode::INPUT,Config::button_inputs[i].pull);
        for(unsigned i=0;i<3;++i) if(Config::rgb_outputs[i].pin.IsValid()) {
            rgb_[i].Init(Config::rgb_outputs[i].pin,daisy::GPIO::Mode::OUTPUT,daisy::GPIO::Pull::NOPULL);
            rgb_[i].Write(!Config::rgb_outputs[i].active_high);
        }
    }
    template<class Board> cineol::controls::InputFrame Read(Board&) noexcept {
        auto frame=cineol::controls::default_frame();
        for(unsigned i=0;i<10;++i) if(slots_[i]!=255)
            frame.values[i]=analog_[i].Process()*Config::analog_inputs[i].scale+Config::analog_inputs[i].offset;
        if(Config::button_inputs[0].pin.IsValid()) frame.button1=buttons_[0].Read()==Config::button_inputs[0].active_high;
        if(Config::button_inputs[1].pin.IsValid()) frame.button2=buttons_[1].Read()==Config::button_inputs[1].active_high;
        return frame;
    }
    template<class Board> void WriteRgb(Board&,cineol::controls::Rgb color) noexcept {
        // On/off GPIO palette. Replace with bounded PWM for variable brightness.
        const float values[]={color.red,color.green,color.blue};
        for(unsigned i=0;i<3;++i) if(Config::rgb_outputs[i].pin.IsValid())
            rgb_[i].Write(values[i]>0?Config::rgb_outputs[i].active_high:!Config::rgb_outputs[i].active_high);
    }
private:
    std::array<uint8_t,10> slots_{};
    daisy::AnalogControl analog_[10];
    daisy::GPIO buttons_[2],rgb_[3];
};

using HardwareAdapter=BasicHardwareAdapter<>;
