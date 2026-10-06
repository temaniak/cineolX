#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <utility>

namespace cineol::xl {
// Native event law of the X/XL FPC output latch. A request replaces the
// waiting word; an idle converter takes it on the next row. A conversion
// selects its DAC holds after nine rows and releases its slot after 23.
// Every request boundary is processed, including repeated channels; a later
// request can replace a waiting word exactly as the physical latch does.
// No ROM, CPU, opcode dispatch or per-row emulator runs here.
class EventDac48 {
public:
    static constexpr int32_t frame_ticks=12000000,row_ticks=168750;
    static constexpr int32_t converter_origin=351483,propagation_ticks=34560;
    static constexpr int32_t capture_origin=converter_origin+propagation_ticks;
    struct Frame {std::array<float,4> analog{},raw{};};
    void prepare() noexcept {kernels_=&kernels();reset();}
    void reset(int32_t clock_origin=0) noexcept {
        assert(clock_origin>=0 && clock_origin%grid_ticks==0);
        channels_={};events_={};read_=count_=0;pending_=false;clock_base_=0;
        // The independent FPC starts busy at age zero before its first row
        // clock. Row 21 finishes that initial conversion; row 22 is free.
        available_=clock_origin+converter_origin+22*row_ticks;pending_start_=0;
        pending_channels_=0;pending_value_=0;
    }
    void request(int32_t clock,unsigned channels,float value) noexcept {
        assert(clock>=0 && clock<=32*frame_ticks);
        clock+=clock_base_;
        advance(clock);
        pending_=true;pending_channels_=channels;pending_value_=value;
        pending_start_=std::max(clock+row_ticks,available_);
    }
    Frame sample(unsigned outputs=15) noexcept {
        const int32_t end=clock_base_+frame_ticks;
        advance(end);Frame result;
        while(count_ && events_[read_].time<=end) {
            const auto event=events_[read_];
            const int32_t time=event.time-clock_base_;assert(time>=0);
            for(unsigned c=0;c<4;++c) if((event.channels>>c)&1) {
                auto& channel=channels_[c];
                if(channel.started) advance_to(channel,time);
                channel.held=event.value;channel.time=time;channel.started=true;
            }
            read_=(read_+1)%events_.size();--count_;
        }
        for(unsigned c=0;c<4;++c) {
            auto& channel=channels_[c];
            if(!channel.started) continue;
            // Retain state at a capture, rather than updating all four chains
            // at every host frame. Sample only the requested analog outputs;
            // every channel's capture history remains valid for instant recall.
            if(frame_ticks-channel.time>2*frame_ticks) {
                evolve(channel,frame_ticks);channel.time+=frame_ticks;
            }
            float sum=0;
            if((outputs>>c)&1) {
                const auto& delta=interval(frame_ticks-channel.time);
                [&]<size_t... K>(std::index_sequence<K...>) noexcept {
                    ((sum+=contribution<K>(channel,delta[K])),...);
                }(std::make_index_sequence<modes>{});
                result.analog[c]=(sum+float(2.741597927047747e-09)*channel.held)*float(1.5941996081501124);
            }
            result.raw[c]=channel.held;
            channel.time-=frame_ticks;
        }
        clock_base_=end;
        // Keep clocks bounded without shifting the forecast queue every frame.
        // Even the 32-frame horizon stays inside signed ticks before this rebase.
        if(clock_base_==96*frame_ticks) {
            for(unsigned n=0;n<count_;++n)events_[(read_+n)%events_.size()].time-=clock_base_;
            available_=std::max(available_-clock_base_,-row_ticks);
            if(pending_)pending_start_-=clock_base_;
            clock_base_=0;
        }
        return result;
    }
private:
    static constexpr unsigned modes=7,phases=1281;
    static constexpr int32_t grid_ticks=18750,event_remainder=capture_origin%grid_ticks;
    struct Complex {float re=0,im=0;};
    struct Channel {std::array<Complex,modes> state{};float held=0;int32_t time=0;bool started=false;};
    struct Event {int32_t time=0;unsigned channels=0;float value=0;};
    struct Kernels {
        // Grid intervals, frame->capture intervals and capture->frame
        // intervals. Capture-to-capture/output intervals use at most two host
        // frames; dormant channels advance one whole frame to retain that bound.
        std::array<std::array<std::array<Complex,modes>,phases>,3> delta{};
        std::array<Complex,modes> weights{};
        Kernels() noexcept;
    };
    static const Kernels& kernels() noexcept {
        // Called only by prepare(), outside processing. The immutable table
        // is shared by all 22 graphs and Runtime objects.
        static const Kernels value;return value;
    }
    void advance(int32_t until) noexcept {
        if(!pending_ || pending_start_>until) return;
        if(pending_channels_) {
            assert(count_<events_.size());
            events_[(read_+count_)%events_.size()]={pending_start_+9*row_ticks+propagation_ticks,pending_channels_,pending_value_};
            ++count_;
        }
        available_=pending_start_+23*row_ticks;pending_=false;
    }
    const std::array<Complex,modes>& interval(int32_t ticks) const noexcept {
        assert(ticks>=0 && ticks<=2*frame_ticks && kernels_);
        const int32_t remainder=ticks%grid_ticks;
        const unsigned family=remainder==0?0:remainder==event_remainder?1:2;
        assert(remainder==0 || remainder==event_remainder || remainder==grid_ticks-event_remainder);
        const unsigned phase=unsigned(ticks/grid_ticks);assert(phase<phases);
        return kernels_->delta[family][phase];
    }
    void advance_to(Channel& channel,int32_t time) noexcept {
        const int32_t ticks=time-channel.time;
        assert(ticks>=0 && ticks<=3*frame_ticks);
        if(ticks>2*frame_ticks) {evolve(channel,frame_ticks);channel.time+=frame_ticks;}
        evolve(channel,time-channel.time);
    }
    void evolve(Channel& channel,int32_t ticks) noexcept {
        const auto& delta=interval(ticks);
        [&]<size_t... K>(std::index_sequence<K...>) noexcept {
            (evolve_mode<K>(channel,delta[K]),...);
        }(std::make_index_sequence<modes>{});
    }
    template<unsigned K> static void evolve_mode(Channel& channel,Complex a) noexcept {
        auto& s=channel.state[K];
        if constexpr(K==0 || K>=4) {
            // Four circuit poles/residues are real: their imaginary state
            // stays zero from reset. Keep all poles, omit only zero arithmetic.
            s.re=s.re+a.re*(s.re+channel.held);
        } else {
            const float re=s.re+a.re*(s.re+channel.held)-a.im*s.im;
            const float im=s.im+a.im*(s.re+channel.held)+a.re*s.im;s={re,im};
        }
    }
    template<unsigned K> float contribution(const Channel& channel,Complex a) const noexcept {
        const auto& w=kernels_->weights[K];const auto& s=channel.state[K];
        const float re=s.re+a.re*(s.re+channel.held);
        if constexpr(K==0 || K>=4) return w.re*re;
        else return w.re*(re-a.im*s.im)-w.im*(s.im+a.im*(s.re+channel.held)+a.re*s.im);
    }
    const Kernels* kernels_=nullptr;
    std::array<Channel,4> channels_{};
    // Include the existing output compatibility transport in event times.
    // At most four requests per pass, 25 host frames ahead: <80 queued captures.
    std::array<Event,128> events_{};
    unsigned read_=0,count_=0,pending_channels_=0;
    int32_t available_=0,pending_start_=0,clock_base_=0;
    float pending_value_=0;bool pending_=false;
};
inline EventDac48::Kernels::Kernels() noexcept {
    using C=std::complex<double>;
    const double rows[modes][4]={
        {-499999.9196412927,0,-0.014290779565073639,0},
        {-3866.5136046706643,98714.32890413406,-285.32850305430486,-775.0280427752442},
        {-14298.548643106906,88288.04903327773,2870.8837308856587,1408.6510272456123},
        {-30935.31584934812,57688.25681141755,-6993.1770747341725,3525.780513937593},
        {-42164.59294570152,0,-14122.997295322293,0},
        {-2.0328722602013864,0,-1.2762189938663815,0},
        {-19841.28472220363,0,23093.899981653653,0}};
    for(unsigned k=0;k<modes;++k) {
        const C p(rows[k][0],rows[k][1]),r(rows[k][2],rows[k][3]);
        const C weight=r/p*(p.imag()==0?1.0:2.0);weights[k]={float(weight.real()),float(weight.imag())};
        for(unsigned family=0;family<3;++family) for(unsigned phase=0;phase<phases;++phase) {
            const int32_t remainder=family==0?0:family==1?event_remainder:grid_ticks-event_remainder;
            const C x=p*(double(int32_t(phase)*grid_ticks+remainder)/576000000000.0);
            const C em1=std::abs(x)<1e-3?x*(1.0+x/2.0+x*x/6.0+x*x*x/24.0):std::exp(x)-C(1);
            delta[family][phase][k]={float(em1.real()),float(em1.imag())};
        }
    }
}
}
