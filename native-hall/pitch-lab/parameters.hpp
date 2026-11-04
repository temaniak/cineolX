#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace cineol::pitch_lab {
enum Param : unsigned {
    input_db,input_pitch_db,pitch_hall_db,direct_hall_db,hall_input_db,pitch_level,hall_level,dry_level,output_db,
    feedback_source,feedback,loop_delay,monitor,semitones,cents,glide,window,overlap,mono,formant_preserve,formant_shift,
    pre_hp,pre_lp,pre_on,pre_slope,post_hp,post_lp,post_on,post_slope,
    hall_hp,hall_lp,hall_filter_on,hall_slope,loop_hp,loop_lp,loop_filter_on,loop_slope,
    bass,mid,crossover,treble,depth,predelay,diffusion,mode_enh,decay_opt,mod_rate,mod_depth,opt_response,
    clean,hall_on,guard,limiter,align_sends,reset_trigger,feedback_invert,count
};
enum class Kind {number,integer,toggle,choice};
struct Definition {const char* id;const char* name;float lo,hi,step,initial,skew;Kind kind;const char* choices;const char* unit;};
inline constexpr std::array<Definition,count> definitions={{
    {"input_db","Input Gain",-36,12,.1,0,1,Kind::number,"","dB"},
    {"input_pitch_db","Input to Pitch",-80,6,.1,0,1,Kind::number,"","dB"},
    {"pitch_hall_db","Pitch to Hall",-80,6,.1,-80,1,Kind::number,"","dB"},
    {"direct_hall_db","Input to Hall",-80,6,.1,-80,1,Kind::number,"","dB"},
    {"hall_input_db","Hall Input Trim",-36,12,.1,0,1,Kind::number,"","dB"},
    {"pitch_level","Pitch Output Level",-80,6,.1,0,1,Kind::number,"","dB"},
    {"hall_level","Hall Output Level",-80,6,.1,-80,1,Kind::number,"","dB"},
    {"dry_level","Dry Level",-80,6,.1,-80,1,Kind::number,"","dB"},
    {"output_db","Master Output",-36,6,.1,-6,1,Kind::number,"","dB"},
    {"feedback_source","Feedback Source",0,2,1,0,1,Kind::choice,"Off|Pitch|Hall",""},
    {"feedback","Feedback Return",0,.8,.001,.15,1,Kind::number,"","%"},
    {"loop_delay","Loop Delay",0,2000,.1,0,.4,Kind::number,"","ms"},
    {"monitor","Monitor",0,7,1,0,1,Kind::choice,"Mix|Input|Pitch Input|Pitch Raw|Pitch Filtered|Hall Input|Hall Wet|Loop Return",""},
    {"semitones","Pitch Semitones",-24,24,1,12,1,Kind::integer,"","st"},
    {"cents","Pitch Fine",-100,100,.1,0,1,Kind::number,"","ct"},
    {"glide","Pitch Glide",0,500,1,30,.5,Kind::number,"","ms"},
    {"window","Pitch Window",0,2,1,1,1,Kind::choice,"2048|4096|8192","samples"},
    {"overlap","Pitch Overlap",0,2,1,1,1,Kind::choice,"4x|8x|16x",""},
    {"mono","Pitch Mono Input",0,1,1,0,1,Kind::toggle,"",""},
    {"formant_preserve","Preserve Formants",0,1,1,0,1,Kind::toggle,"",""},
    {"formant_shift","Formant Shift",-12,12,.1,0,1,Kind::number,"","st"},
    {"pre_hp","Pitch Pre HP",20,4000,1,80,.3,Kind::number,"","Hz"},
    {"pre_lp","Pitch Pre LP",500,20000,1,12000,.4,Kind::number,"","Hz"},
    {"pre_on","Pitch Pre Filter",0,1,1,0,1,Kind::toggle,"",""},
    {"pre_slope","Pitch Pre Slope",0,1,1,1,1,Kind::choice,"12 dB/oct|24 dB/oct",""},
    {"post_hp","Pitch Post HP",20,4000,1,80,.3,Kind::number,"","Hz"},
    {"post_lp","Pitch Post LP",500,20000,1,10000,.4,Kind::number,"","Hz"},
    {"post_on","Pitch Post Filter",0,1,1,0,1,Kind::toggle,"",""},
    {"post_slope","Pitch Post Slope",0,1,1,1,1,Kind::choice,"12 dB/oct|24 dB/oct",""},
    {"hall_hp","Hall Input HP",20,4000,1,30,.3,Kind::number,"","Hz"},
    {"hall_lp","Hall Input LP",500,20000,1,14000,.4,Kind::number,"","Hz"},
    {"hall_filter_on","Hall Input Filter",0,1,1,0,1,Kind::toggle,"",""},
    {"hall_slope","Hall Input Slope",0,1,1,1,1,Kind::choice,"12 dB/oct|24 dB/oct",""},
    {"loop_hp","Loop HP",20,4000,1,180,.3,Kind::number,"","Hz"},
    {"loop_lp","Loop LP",500,20000,1,6500,.4,Kind::number,"","Hz"},
    {"loop_filter_on","Loop Filter",0,1,1,1,1,Kind::toggle,"",""},
    {"loop_slope","Loop Filter Slope",0,1,1,1,1,Kind::choice,"12 dB/oct|24 dB/oct",""},
    {"bass","Hall Bass Decay",1,31,1,17,1,Kind::integer,"",""},
    {"mid","Hall Mid Decay",0,31,1,14,1,Kind::integer,"",""},
    {"crossover","Hall Crossover",0,31,1,5,1,Kind::integer,"",""},
    {"treble","Hall Treble Decay",0,31,1,23,1,Kind::integer,"",""},
    {"depth","Hall Depth",0,71,1,21,1,Kind::integer,"",""},
    {"predelay","Hall Pre-delay",24,152,1,24,1,Kind::integer,"","ms"},
    {"diffusion","Hall Diffusion",1,63,1,1,1,Kind::integer,"",""},
    {"mode_enh","Hall Mod Enhancement",0,1,1,1,1,Kind::toggle,"",""},
    {"decay_opt","Hall Decay Optimization",0,1,1,1,1,Kind::toggle,"",""},
    {"mod_rate","Hall Mod Rate",1,16,1,2,1,Kind::integer,"",""},
    {"mod_depth","Hall Mod Depth",1,16,1,1,1,Kind::integer,"",""},
    {"opt_response","Hall Opt Response",1,16,1,5,1,Kind::integer,"",""},
    {"clean","Hall Precision",0,1,1,1,1,Kind::choice,"Original|Clean",""},
    {"hall_on","Hall Enabled",0,1,1,0,1,Kind::toggle,"",""},
    {"guard","Feedback Guard",0,1,1,1,1,Kind::toggle,"",""},
    {"limiter","Output Safety Ceiling",0,1,1,1,1,Kind::toggle,"",""},
    {"align_sends","Align Hall Input Sends",0,1,1,1,1,Kind::toggle,"",""},
    {"reset_trigger","Reset Histories Trigger",0,1,1,0,1,Kind::toggle,"",""},
    {"feedback_invert","Invert Feedback",0,1,1,0,1,Kind::toggle,"",""}
}};
using Values=std::array<float,count>;
inline Values defaults(){Values v{};for(unsigned i=0;i<count;++i)v[i]=definitions[i].initial;return v;}
inline float amplitude(float db){return db<=-79.99f?0.f:std::pow(10.f,db/20);}
inline float sanitize(unsigned i,float x){const auto& d=definitions[i];return std::isfinite(x)?std::clamp(x,d.lo,d.hi):d.initial;}
inline constexpr const char* preset_names[]={"Pitch Only","Hall Only","Pitch into Hall","Parallel","Shimmer Loop","Pitch Delay"};
inline Values preset(unsigned which,Values v=defaults()) {
    v[input_pitch_db]=v[pitch_hall_db]=v[direct_hall_db]=-80;
    v[dry_level]=v[pitch_level]=v[hall_level]=-80;v[feedback_source]=0;v[hall_on]=0;
    v[loop_delay]=0;v[monitor]=0;v[feedback]=.15f;
    switch(which) {
        case 0:v[input_pitch_db]=v[pitch_level]=0;break;
        case 1:v[direct_hall_db]=v[hall_level]=0;v[hall_on]=1;break;
        case 2:v[input_pitch_db]=v[pitch_hall_db]=v[hall_level]=0;v[hall_on]=1;break;
        case 3:v[input_pitch_db]=v[direct_hall_db]=0;v[pitch_level]=v[hall_level]=-6;v[hall_on]=1;break;
        case 4:v[direct_hall_db]=v[pitch_hall_db]=v[hall_level]=0;v[hall_on]=1;v[feedback_source]=2;break;
        case 5:v[input_pitch_db]=v[pitch_level]=0;v[feedback_source]=1;v[loop_delay]=250;break;
    }
    return v;
}
inline float normalized(unsigned i,float raw){const auto& d=definitions[i];return std::pow((sanitize(i,raw)-d.lo)/(d.hi-d.lo),d.skew);}
} // namespace cineol::pitch_lab
