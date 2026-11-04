#include "core.hpp"
#include "../desktop/engine22448.hpp"
#include <clean/desktop/engine22448.hpp>
#include "pitch_engine.hpp"
#include <vector>
#include <cstring>

namespace cineol::pitch_lab {
namespace {
using Stereo=std::array<float,2>;
struct Delay {
    std::vector<Stereo> data;unsigned position=0;
    void prepare(unsigned n){data.assign(n+1,{});position=0;}
    void clear()noexcept{std::fill(data.begin(),data.end(),Stereo{});position=0;}
    Stereo tick(Stereo x,unsigned samples)noexcept {
        const unsigned size=unsigned(data.size());data[position]=x;
        const auto y=data[(position+size-std::min(samples,size-1))%size];
        if(++position==size)position=0;return y;
    }
};
// Stable TPT one-pole cascades, with 2/4 sections for 12/24 dB/oct.
// No resonance is introduced into a diagnostic pitch-return filter.
struct Filter {
    std::array<Stereo,4> hp{},lp{};float ah=.005f,al=.3f,th=.005f,tl=.3f,blend=0,target=0,slope=1,slope_target=1;
    void setup(float h,float l,bool on,float steep)noexcept {
        const auto coef=[](float f){const float g=std::tan(3.14159265358979323846f*std::clamp(f,20.f,20000.f)/48000);return g/(1+g);};
        th=coef(h);tl=coef(l);target=on?1.f:0.f;slope_target=steep;
    }
    void clear()noexcept{hp={};lp={};ah=th;al=tl;blend=target;slope=slope_target;}
    Stereo tick(Stereo x)noexcept {
        ah+=.002f*(th-ah);al+=.002f*(tl-al);blend+=.002f*(target-blend);slope+=.002f*(slope_target-slope);
        Stereo y=x,half{};
        for(unsigned stage=0;stage<4;++stage) {
            for(unsigned c=0;c<2;++c) {
                const float v=(y[c]-hp[stage][c])*ah,low=v+hp[stage][c];hp[stage][c]=low+v;y[c]-=low;
                const float w=(y[c]-lp[stage][c])*al,filtered=w+lp[stage][c];lp[stage][c]=filtered+w;y[c]=filtered;
            }
            if(stage==1)half=y;
        }
        for(unsigned c=0;c<2;++c)y[c]=x[c]+blend*(half[c]+slope*(y[c]-half[c])-x[c]);
        return y;
    }
};
}
struct Core::Impl {
    std::unique_ptr<PitchEngine> pitch=make_signalsmith_reference();
    native_hall::DesktopEngine48 original;
    clean_native_hall::DesktopEngine48 cleaned;
    std::unique_ptr<clean_native_hall::ProgramBank> clean_bank;
    const native_hall::ProgramBank* bank=nullptr;
    Values values=defaults(),gains{},targets{};
    std::array<Filter,4> filters;
    std::array<Delay,node_count> aligned;
    Delay direct_alignment;
    std::vector<Stereo> returns;
    unsigned return_position=0,position=0,selection=4,max_pitch_delay=0,loop_samples=0,guard_count=0;
    std::array<Stereo,quantum> input{},output{},pitch_input{},pitch_raw{},pitch_filtered{},loop_return{},source{};
    std::array<std::array<float,quantum>,2> planar_in{},planar_out{};
    std::array<Meter,node_count> meter{};
    float shift=12,glide_alpha=1,reset_blend=1,loop_delay_current=0;
    bool ready=false,tripped=false;
    void hall_controls()noexcept {
        native_hall::Parameters p;p.program=2;p.input_db=0;p.mix=1;p.analog=true;p.dirt=0;
        p.hall.bass=int(values[bass]);p.hall.mid=int(values[mid]);p.hall.crossover=int(values[crossover]);p.hall.treble=int(values[treble]);
        p.hall.depth=int(values[depth]);p.hall.predelay_ms=int(values[predelay]);p.hall.diffusion=int(values[diffusion]);
        p.hall.mode_enhancement=values[mode_enh]>.5f;p.hall.decay_optimization=values[decay_opt]>.5f;
        p.hall.modulation_period=uint8_t(std::max(1,int(values[mod_rate])-1));p.hall.modulation_step=uint8_t(4*int(values[mod_depth]));
        p.hall.optimization_response=uint8_t(values[opt_response]);
        original.set_parameters(p);
        clean_native_hall::Parameters q;
        static_assert(sizeof(q)==sizeof(p) && std::is_trivially_copyable_v<native_hall::Parameters>);
        std::memcpy(&q,&p,sizeof(p));cleaned.set_parameters(q);
    }
    void clear()noexcept {
        if(!ready)return;
        // Reset preallocated state only; no preparation/reconfiguration here.
        pitch->reset();
        for(auto& f:filters)f.clear();for(auto& d:aligned)d.clear();direct_alignment.clear();
        std::fill(returns.begin(),returns.end(),Stereo{});return_position=position=guard_count=0;
        input={};output={};pitch_input={};pitch_raw={};pitch_filtered={};loop_return={};source={};meter={};tripped=false;
        if(bank){original.activate(*bank,2);cleaned.activate(*clean_bank,2);hall_controls();}
        shift=values[semitones]+values[cents]/100;gains=targets;loop_delay_current=values[loop_delay]*48;reset_blend=0;
    }
    void read_meter(unsigned n,Stereo x)noexcept {
        auto& m=meter[n];const float peak=std::max(std::abs(x[0]),std::abs(x[1]));
        m.peak=std::max(peak,m.peak*.9996f);m.power+=.0002f*(.5f*(x[0]*x[0]+x[1]*x[1])-m.power);
        if(peak>=1)++m.clips;
    }
    void render()noexcept {
        const unsigned pd=pitch->latency();
        for(unsigned j=0;j<quantum;++j) {
            for(unsigned i=0;i<count;++i)gains[i]+=.002f*(targets[i]-gains[i]);
            source[j]={input[j][0]*gains[input_db],input[j][1]*gains[input_db]};read_meter(input_node,source[j]);
            // Fixed quantum of causal buffering, never a host-block return.
            loop_delay_current+=std::clamp(values[loop_delay]*48-loop_delay_current,-.25f,.25f);
            const unsigned delay=quantum+unsigned(loop_delay_current),size=unsigned(returns.size());
            const unsigned at=(return_position+j+size-delay)%size;
            const auto ret=filters[3].tick(returns[at]);
            const float fg=values[feedback_source]<.5f || tripped?0:gains[feedback]*(values[feedback_invert]>.5f?-1:1);
            loop_return[j]={ret[0]*fg,ret[1]*fg};read_meter(loop_node,loop_return[j]);
            Stereo pi={source[j][0]*gains[input_pitch_db]+loop_return[j][0],source[j][1]*gains[input_pitch_db]+loop_return[j][1]};
            if(values[mono]>.5f)pi[0]=pi[1]=.5f*(pi[0]+pi[1]);
            pitch_input[j]=filters[0].tick(pi);read_meter(pitch_input_node,pitch_input[j]);
            for(unsigned c=0;c<2;++c)planar_in[c][j]=pitch_input[j][c];
        }
        shift+=glide_alpha*(values[semitones]+values[cents]/100-shift);
        pitch->settings(shift,values[formant_shift],values[formant_preserve]>.5f);
        const float* in[]={planar_in[0].data(),planar_in[1].data()};float* out[]={planar_out[0].data(),planar_out[1].data()};
        pitch->process(in,out,quantum);
        for(unsigned j=0;j<quantum;++j) {
            pitch_raw[j]={planar_out[0][j],planar_out[1][j]};read_meter(pitch_raw_node,pitch_raw[j]);
            pitch_filtered[j]=filters[1].tick(pitch_raw[j]);read_meter(pitch_post_node,pitch_filtered[j]);
            const auto direct=direct_alignment.tick(source[j],values[align_sends]>.5f?pd:0);
            Stereo hi={gains[hall_input_db]*(direct[0]*gains[direct_hall_db]+pitch_filtered[j][0]*gains[pitch_hall_db]),
                       gains[hall_input_db]*(direct[1]*gains[direct_hall_db]+pitch_filtered[j][1]*gains[pitch_hall_db])};
            hi=filters[2].tick(hi);read_meter(hall_input_node,hi);Stereo hw{};
            if(bank && values[hall_on]>.5f) {
                if(values[clean]>.5f)cleaned.process(hi[0],hi[1],hw[0],hw[1],true);
                else original.process(hi[0],hi[1],hw[0],hw[1],true);
            }
            read_meter(hall_wet_node,hw);
            const Stereo chosen=values[feedback_source]>1.5f?hw:pitch_filtered[j];
            if(values[feedback_source]>.5f && values[guard]>.5f) {
                if(std::max(std::abs(chosen[0]),std::abs(chosen[1]))>.8f)++guard_count;
                else if(guard_count) --guard_count;
                if(guard_count>=960)tripped=true;
            }
            returns[return_position]=tripped?Stereo{}:chosen;
            if(++return_position==returns.size())return_position=0;
            const unsigned full=max_pitch_delay+70;
            std::array<Stereo,node_count> raw={source[j],pitch_input[j],pitch_raw[j],pitch_filtered[j],hi,hw,loop_return[j],Stereo{}};
            std::array<Stereo,node_count> a{};
            for(unsigned n=0;n<node_count-1;++n) {
                const unsigned intrinsic=(n==pitch_raw_node || n==pitch_post_node || n==hall_input_node)?pd:n==hall_wet_node?pd+70:0;
                a[n]=aligned[n].tick(raw[n],full-intrinsic);
            }
            Stereo mixed={gains[dry_level]*a[input_node][0]+gains[pitch_level]*a[pitch_post_node][0]+gains[hall_level]*a[hall_wet_node][0],
                          gains[dry_level]*a[input_node][1]+gains[pitch_level]*a[pitch_post_node][1]+gains[hall_level]*a[hall_wet_node][1]};
            const unsigned selected=unsigned(values[monitor]);
            if(selected>0)mixed=a[selected-1];
            reset_blend=std::min(1.f,reset_blend+1.f/480);
            for(float& v:mixed) {
                v*=gains[output_db]*reset_blend;
                if(!std::isfinite(v))v=0;
                if(values[limiter]>.5f && std::abs(v)>.9f)v=std::copysign(.9f+.1f*std::tanh((std::abs(v)-.9f)*10),v);
            }
            output[j]=mixed;read_meter(output_node,mixed);
        }
    }
};
Core::Core():impl_(std::make_unique<Impl>()){}Core::~Core()=default;
void Core::prepare(const native_hall::ProgramBank* bank) {
    auto& s=*impl_;s.ready=false;s.bank=bank;s.max_pitch_delay=0;
    s.pitch->prepare(rate);s.max_pitch_delay=s.pitch->maximum_latency();
    for(auto& d:s.aligned)d.prepare(s.max_pitch_delay+70);s.direct_alignment.prepare(s.max_pitch_delay);
    s.returns.assign(96000+quantum+1,{});
    if(bank) {
        s.clean_bank=std::make_unique<clean_native_hall::ProgramBank>();static_assert(sizeof(*bank)==sizeof(*s.clean_bank));
        std::memcpy(s.clean_bank.get(),bank,sizeof(*bank));s.original.prepare(*bank,2);s.cleaned.prepare(*s.clean_bank,2);
    }
    s.ready=true;set_values(s.values);s.clear();
}
void Core::set_values(const Values& next)noexcept {
    auto& s=*impl_;const auto old=s.values;
    for(unsigned i=0;i<count;++i)s.values[i]=sanitize(i,next[i]);
    s.selection=unsigned(s.values[window])*3+unsigned(s.values[overlap]);
    s.pitch->select(unsigned(s.values[window]),unsigned(s.values[overlap]));
    s.targets.fill(0);
    for(unsigned i:{input_db,input_pitch_db,pitch_hall_db,direct_hall_db,hall_input_db,pitch_level,hall_level,dry_level,output_db})s.targets[i]=amplitude(s.values[i]);
    s.targets[feedback]=s.values[feedback];
    for(unsigned f=0;f<4;++f) {const unsigned at=pre_hp+4*f;s.filters[f].setup(s.values[at],s.values[at+1],s.values[at+2]>.5f,s.values[at+3]);}
    s.glide_alpha=s.values[glide]<.01f?1.f:1-std::exp(-float(quantum)/(48*s.values[glide]));
    bool hall_changed=false;for(unsigned i=bass;i<=opt_response;++i)hall_changed|=old[i]!=s.values[i];
    if(s.bank && hall_changed)s.hall_controls();
    if(old[window]!=s.values[window] || old[overlap]!=s.values[overlap] || old[clean]!=s.values[clean] || old[hall_on]!=s.values[hall_on] || old[reset_trigger]!=s.values[reset_trigger])s.clear();
}
void Core::clear()noexcept {impl_->clear();}
std::array<float,2> Core::process(float l,float r)noexcept {
    auto& s=*impl_;if(!s.ready)return {};
    l=std::isfinite(l)?std::clamp(l,-16.f,16.f):0;r=std::isfinite(r)?std::clamp(r,-16.f,16.f):0;
    const auto out=s.output[s.position];s.input[s.position]={l,r};
    if(++s.position==quantum){s.render();s.position=0;}
    return out;
}
unsigned Core::latency()const noexcept{return quantum+impl_->max_pitch_delay+70;}
unsigned Core::pitch_latency()const noexcept{return impl_->pitch->latency();}
bool Core::hall_available()const noexcept{return impl_->bank!=nullptr;}
bool Core::guard_tripped()const noexcept{return impl_->tripped;}
uint64_t Core::hall_saturations()const noexcept{return impl_->values[clean]>.5f?impl_->cleaned.hall().saturation_count():impl_->original.hall().saturation_count();}
const std::array<Meter,node_count>& Core::meters()const noexcept{return impl_->meter;}
} // namespace cineol::pitch_lab
