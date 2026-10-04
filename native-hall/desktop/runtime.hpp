#pragma once
#include "bank.hpp"
#include "concert48.hpp"
#include <tuple>
#include <memory>

namespace cineol::xl {
class Runtime {
public:
    static constexpr unsigned graph_delay(unsigned rows) noexcept {return (32*1280+16*18*rows+640)/1280;}
    static constexpr unsigned latency_samples=57; // Maximum of the 22 graph-specific converter paths.
    Runtime():engines_(std::make_unique<Engines>()) {
        // Kernels are prepared outside audio; processing never allocates.
        std::apply([](auto&... engine){(engine.prepare(typename std::decay_t<decltype(engine)>::Settings{}),...);},*engines_);
    }
    void select(const Bank& bank,unsigned program) noexcept {
        program_=std::min(program,unsigned(graphs.size()-1));data_=&bank.programs[program_];
        visit([&](auto& engine){
            using Audio=std::decay_t<decltype(engine)>;
            typename Audio::Settings settings;
            std::copy_n(data_->coefficients.begin(),settings.rows,settings.coefficients.begin());
            std::copy_n(data_->offsets.begin(),settings.rows,settings.offsets.begin());
            engine.activate(settings);engine.set_modulation(data_->modulation,data_->initial_modulation,true);
        });
        alignment_.fill({});alignment_position_=0;
    }
    void controls(const std::array<uint8_t,48>& values,bool enhancement,float gain,float mix,float clean,int left,int right) noexcept {
        visit([&](auto& engine){
            using Audio=std::decay_t<decltype(engine)>;
            auto settings=data_->template settings<Audio::graph_id>();
            data_->controls.apply_size(settings,values);data_->controls.apply_static(settings,values);
            engine.set_controls(settings);
            if(data_->chorus_page) engine.set_chorus(values[data_->pages[data_->chorus_page-1].cells[data_->chorus_slot]]);
            engine.enable_modulation(enhancement);engine.set_global(gain,mix,clean,left,right);
        });
    }
    void process(float left,float right,float& l,float& r,bool wet_only) noexcept {
        visit([&](auto& engine){engine.process(left,right,l,r,wet_only);});
        const unsigned extra=latency_samples-graph_delay(graphs[program_].rows);
        alignment_[alignment_position_%alignment_.size()]={l,r};
        const auto value=alignment_[(alignment_position_+alignment_.size()-extra)%alignment_.size()];
        ++alignment_position_;l=value[0];r=value[1];
    }
private:
    template<class F> void visit(F&& f) noexcept {
        switch(Graph(program_)) {
            case Graph::concert:f(std::get<0>(*engines_));break;
            case Graph::bright_hall:f(std::get<1>(*engines_));break;
            case Graph::dark_hall:f(std::get<2>(*engines_));break;
            case Graph::plate:f(std::get<3>(*engines_));break;
            case Graph::room:f(std::get<4>(*engines_));break;
            case Graph::rich_chamber:f(std::get<5>(*engines_));break;
            case Graph::small_room:f(std::get<6>(*engines_));break;
            case Graph::chamber:f(std::get<7>(*engines_));break;
            case Graph::dark_chamber:f(std::get<8>(*engines_));break;
            case Graph::inverse_room:f(std::get<9>(*engines_));break;
            case Graph::small_plate:f(std::get<10>(*engines_));break;
            case Graph::cd_plate_a:f(std::get<11>(*engines_));break;
            case Graph::cd_plate_b:f(std::get<12>(*engines_));break;
            case Graph::rich_plate:f(std::get<13>(*engines_));break;
            case Graph::chorus_echo:f(std::get<14>(*engines_));break;
            case Graph::resonant_chords:f(std::get<15>(*engines_));break;
            case Graph::multiband_delay:f(std::get<16>(*engines_));break;
            case Graph::hall_hall:f(std::get<17>(*engines_));break;
            case Graph::plate_plate:f(std::get<18>(*engines_));break;
            case Graph::plate_hall:f(std::get<19>(*engines_));break;
            case Graph::plate_chorus:f(std::get<20>(*engines_));break;
            case Graph::rich_split:f(std::get<21>(*engines_));break;
        }
    }
    using Engines=std::tuple<Native48<Graph::concert>,
        Native48<Graph::bright_hall>,
        Native48<Graph::dark_hall>,
        Native48<Graph::plate>,
        Native48<Graph::room>,
        Native48<Graph::rich_chamber>,
        Native48<Graph::small_room>,
        Native48<Graph::chamber>,
        Native48<Graph::dark_chamber>,
        Native48<Graph::inverse_room>,
        Native48<Graph::small_plate>,
        Native48<Graph::cd_plate_a>,
        Native48<Graph::cd_plate_b>,
        Native48<Graph::rich_plate>,
        Native48<Graph::chorus_echo>,
        Native48<Graph::resonant_chords>,
        Native48<Graph::multiband_delay>,
        Native48<Graph::hall_hall>,
        Native48<Graph::plate_plate>,
        Native48<Graph::plate_hall>,
        Native48<Graph::plate_chorus>,
        Native48<Graph::rich_split>>;
    std::unique_ptr<Engines> engines_;
    const ProgramData* data_=nullptr;
    unsigned program_=0;
    uint64_t alignment_position_=0;
    std::array<std::array<float,2>,4> alignment_{};
};
} // namespace cineol::xl
