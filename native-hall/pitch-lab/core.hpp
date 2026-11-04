#pragma once
#include "parameters.hpp"
#include "../core/program_bank.hpp"
#include <memory>

namespace cineol::pitch_lab {
enum Node : unsigned {input_node,pitch_input_node,pitch_raw_node,pitch_post_node,hall_input_node,hall_wet_node,loop_node,output_node,node_count};
inline constexpr const char* node_names[]={"Input","Pitch input","Pitch raw","Pitch filtered","Hall input","Hall wet","Loop return","Output"};
struct Meter {float peak=0,power=0;uint64_t clips=0;};
class Core {
public:
    static constexpr unsigned rate=48000,quantum=64;
    Core();~Core();
    void prepare(const native_hall::ProgramBank*);
    void set_values(const Values&) noexcept;
    void clear() noexcept;
    std::array<float,2> process(float,float) noexcept;
    unsigned latency() const noexcept;
    unsigned pitch_latency() const noexcept;
    bool hall_available() const noexcept;
    bool guard_tripped() const noexcept;
    uint64_t hall_saturations() const noexcept;
    const std::array<Meter,node_count>& meters() const noexcept;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
} // namespace cineol::pitch_lab
