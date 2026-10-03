// Generic Daisy reference: stereo 48 kHz, 48-frame blocks, controls at 500 Hz.
#if defined(CINEOL_DAISY_PATCH_SM)
#include "daisy_patch_sm.h"
using Board=daisy::patch_sm::DaisyPatchSM;
#else
#include "daisy_seed.h"
using Board=daisy::DaisySeed;
#endif
#include "Controls.hpp"
#if __has_include("HardwareAdapter.local.hpp")
#include "HardwareAdapter.local.hpp"
#else
#include "HardwareAdapter.example.hpp"
#endif
#include "generated_profile.hpp"
#include <new>

namespace {
Board hardware;
HardwareAdapter adapter;
cineol::controls::Mapper mapper;
native_hall::Engine48 engine;
alignas(native_hall::ProgramBank) unsigned char bank_storage[sizeof(native_hall::ProgramBank)]
    __attribute__((section(".hall_bank_bss")));
unsigned blocks=0,last_program=native_hall::program_count;
void ProcessControls() noexcept {
    mapper.update(adapter.Read(hardware));
    engine.set_parameters(mapper.parameters());
    const unsigned active=engine.active_program();
    if(active!=last_program) {
        adapter.WriteRgb(hardware,cineol::controls::program_rgb[active]);last_program=active;
    }
}
void AudioCallback(daisy::AudioHandle::InputBuffer in,daisy::AudioHandle::OutputBuffer out,size_t count) {
    if((blocks++&1)==0) ProcessControls();
    for(size_t i=0;i<count;++i) engine.process(in[0][i],in[1][i],out[0][i],out[1][i]);
}
}
int main() {
    constexpr uint32_t fast_fp=(1u<<24)|(1u<<25);
    FPU->FPDSCR|=fast_fp;__set_FPSCR(__get_FPSCR()|fast_fp);
    hardware.Init();
    hardware.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_48KHZ);
    hardware.SetAudioBlockSize(48);
    adapter.Init(hardware);
    auto* bank=new(bank_storage) native_hall::ProgramBank;
    if(!native_hall::read_bank(bank_profile_data,sizeof bank_profile_data,*bank)) {
        for(;;) __WFI();
    }
    mapper.update(adapter.Read(hardware));
    engine.prepare(*bank,mapper.parameters().program);
    engine.set_parameters(mapper.parameters());
    ProcessControls();
    hardware.StartAudio(AudioCallback);
    for(;;) __WFI();
}
