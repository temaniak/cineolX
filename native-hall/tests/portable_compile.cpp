// Cross-compile the complete 48 kHz consumer, including header-only SRC and
// analog filters. This is a compile gate, not an MCU timing benchmark.
#include "../core/engine48.hpp"
static_assert(sizeof(native_hall::Engine48)<64*1024,"DSP state exceeds the intended memory budget");
void prepare_native_hall(native_hall::Engine48& e,const native_hall::Profile& profile) {
    e.prepare(profile);
}
void set_native_hall(native_hall::Engine48& e,const native_hall::Parameters& p) {
    e.set_parameters(p);
}
void render_native_hall(native_hall::Engine48& e,const float* left,const float* right,
                        float* out_left,float* out_right,unsigned frames) {
    for(unsigned n=0;n<frames;++n) e.process(left[n],right[n],out_left[n],out_right[n]);
}
