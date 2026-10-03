#pragma once
// Copy to ControlConfig.local.hpp for a private configuration. No pins here.
inline constexpr std::array<ControlConfig,10> control_config={{
    {Target::Bass,      0.0f,1.0f,false},
    {Target::Mid,       0.0f,1.0f,false},
    {Target::Crossover, 0.0f,1.0f,false},
    {Target::Treble,    0.0f,1.0f,false},
    {Target::Depth,     0.0f,1.0f,false},
    {Target::Predelay,  0.0f,1.0f,false},
    {Target::Diffusion, 0.0f,1.0f,false},
    {Target::InputGain, 0.0f,1.0f,false},
    {Target::Mix,       0.0f,1.0f,false},
    {Target::Program,   0.0f,1.0f,false},
}};
// Change ranges to the units returned by your adapter, e.g. {Target::Bass,
// 0,3.3f,false}, {Target::Mid,-5,5,true}, or {Target::Mix,0,5,false}.
// Optional curve functions operate on the normalized 0..1 position.
// Each function must be bounded, allocation-free and nonblocking.
inline constexpr std::array<Rgb,6> program_rgb={{
    {0,1,1}, {1,0,1}, {0,0,1}, {0,1,0}, {1,0,0}, {1,1,0}
}};
