// Exercise the public adapter with simulated peripherals, no real GPIO pins.
#include <array>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <cstdlib>
namespace daisy {
struct Pin {
    int port=-1,pin=-1;
    constexpr Pin()=default;
    constexpr Pin(int p,int n):port(p),pin(n) {}
    constexpr bool IsValid() const {return port>=0 && pin>=0;}
};
struct GPIO {
    enum class Mode {INPUT,OUTPUT};enum class Pull {PULLUP,NOPULL};
    inline static bool state[3][10]{};
    inline static unsigned inputs=0,outputs=0,writes=0;
    Pin pin;
    void Init(Pin p,Mode mode,Pull) {pin=p;mode==Mode::INPUT?++inputs:++outputs;}
    bool Read() {return state[pin.port][pin.pin];}
    void Write(bool value) {state[pin.port][pin.pin]=value;++writes;}
};
struct AdcChannelConfig {
    enum {SPEED_64CYCLES_5};
    void InitSingle(Pin,int) {}
};
struct AdcHandle {
    enum {OVS_32};unsigned count=0;
    std::array<uint16_t,10> values{};
    void Stop() {}void Start() {}
    void Init(AdcChannelConfig*,unsigned n,int) {count=n;}
    uint16_t* GetPtr(unsigned i) {return &values[i];}
};
struct AnalogControl {
    uint16_t* raw=nullptr;
    void Init(uint16_t* r,float,bool,bool,float) {raw=r;}
    float Process() {return float(*raw)/65535;}
};
struct System {inline static unsigned delays=0;static void Delay(unsigned) {++delays;}};
}
#include "../src/HardwareAdapter.example.hpp"
struct MockConfig {
    inline static constexpr std::array<AnalogInput,10> analog_inputs={{
        {{0,0},3.3f,0}, {{0,1},5,0}, {{0,2},10,-5}, {{0,3}}, {{0,4}},
        {{0,5}}, {{0,6}}, {{0,7}}, {{0,8}}, {{0,9}}
    }};
    inline static constexpr std::array<ButtonInput,2> button_inputs={{
        {{1,0},false,daisy::GPIO::Pull::PULLUP},{{1,1},true,daisy::GPIO::Pull::NOPULL}
    }};
    inline static constexpr std::array<RgbOutput,3> rgb_outputs={{{{2,0},false},{{2,1},true},{{2,2},false}}};
};
static void require(bool ok,const char* text) {if(!ok){std::cerr<<text<<'\n';std::exit(1);}}
int main() {
    struct Board {daisy::AdcHandle adc;} board;
    board.adc.values[0]=65535;board.adc.values[1]=32768;board.adc.values[2]=0;
    BasicHardwareAdapter<MockConfig> adapter;adapter.Init(board);
    require(board.adc.count==10 && daisy::GPIO::inputs==2 && daisy::GPIO::outputs==3,"I/O counts; no button LED outputs");
    require(daisy::GPIO::state[2][0] && !daisy::GPIO::state[2][1] && daisy::GPIO::state[2][2],"RGB initially off by polarity");
    daisy::GPIO::state[1][0]=false;daisy::GPIO::state[1][1]=true;
    const auto delays=daisy::System::delays;
    auto frame=adapter.Read(board);
    require(std::abs(frame.values[0]-3.3f)<1e-6f && std::abs(frame.values[1]-2.5f)<1e-4f
            && frame.values[2]==-5,"ADC-to-voltage conversion");
    require(frame.button1 && frame.button2,"independent button polarity");
    adapter.WriteRgb(board,{0,1,1});
    require(daisy::GPIO::state[2][0] && daisy::GPIO::state[2][1] && !daisy::GPIO::state[2][2],"independent RGB polarity");
    require(delays==daisy::System::delays,"wait introduced into runtime I/O");
    std::cout<<"Hardware adapter: ten ADC inputs, voltage conversion, two button polarities, "
                "three RGB outputs, no button LEDs and no runtime waits passed\n";
}
