#pragma once
#include "../desktop/engine22448.hpp"
#include "../desktop/bank.hpp"
#include "../desktop/concert48.hpp"
#include <tuple>
#include <clean/core/engine48.hpp>
#include <clean/desktop/engine22448.hpp>
#include <clean/desktop/concert48.hpp>
#include <clean/desktop/bank.hpp>
#include <cstring>
#include <memory>

namespace cineol::clean_ab {
inline constexpr unsigned programs=7;
inline constexpr const char* names[]={"224 / Large Concert Hall B","224 / Small Concert Hall A","224 / Percussion Plate A",
    "224XL / Concert Hall","224XL / Bright Hall","224XL / Dark Hall","224XL / CD Plate B"};
inline constexpr unsigned indices[]={2,5,4,0,1,2,12};
// All engines, buffers and converter kernels are allocated/prepared off-thread.
// Selection clears preallocated storage. No ROM execution takes place here.
class Engines {
    template<cineol::xl::Graph G>struct XL {
        cineol::xl::Native48<G> a;
        clean_cineol::xl::Native48<static_cast<clean_cineol::xl::Graph>(G)> b;
    };
    using Slots=std::tuple<XL<xl::Graph::concert>,XL<xl::Graph::bright_hall>,XL<xl::Graph::dark_hall>,XL<xl::Graph::cd_plate_b>>;
public:
    Engines():original_(std::make_unique<native_hall::DesktopEngine48>()),clean_(std::make_unique<clean_native_hall::DesktopEngine48>()),
        slots_(std::make_unique<Slots>()),bank224_(std::make_unique<clean_native_hall::ProgramBank>()),bankxl_(std::make_unique<clean_cineol::xl::Bank>()) {}
    void prepare(const native_hall::ProgramBank* a,const xl::Bank* b) {
        a_=a;b_=b;
        static_assert(sizeof(*bank224_)==sizeof(*a));static_assert(sizeof(*bankxl_)==sizeof(*b));
        static_assert(std::is_trivially_copyable_v<clean_native_hall::ProgramBank> && std::is_trivially_copyable_v<clean_cineol::xl::Bank>);
        if(a){std::memcpy(bank224_.get(),a,sizeof(*a));original_->prepare(*a);clean_->prepare(*bank224_);}
        if(b){std::memcpy(bankxl_.get(),b,sizeof(*b));std::apply([](auto&... x){((x.a.prepare({}),x.b.prepare({})),...);},*slots_);}
        selected_=programs;
    }
    bool available(unsigned p)const noexcept{return p<programs && (p<3?a_!=nullptr:b_!=nullptr);}
    void select(unsigned p) noexcept {
        selected_=p;pad_.fill({});pad_pos_=0;
        if(!available(p))return;
        if(p<3){original_->activate(*a_,indices[p]);clean_->activate(*bank224_,indices[p]);}
        else visit([&](auto& x){activate(x.a,*b_);activate(x.b,*bankxl_);});
    }
    void original_controls(native_hall::Controls c) noexcept {
        if(selected_>=3 || !available(selected_))return;
        native_hall::Parameters p;p.program=indices[selected_];p.hall=c;original_->set_parameters(p);
        clean_native_hall::Parameters q;q.program=p.program;
        static_assert(sizeof(c)==sizeof(q.hall));static_assert(std::is_trivially_copyable_v<native_hall::Controls>);
        std::memcpy(&q.hall,&c,sizeof(c));clean_->set_parameters(q);
    }
    void xl_controls(std::array<uint8_t,48> raw,bool mod,bool opt,bool dynamic=false) noexcept {
        if(selected_<3 || !available(selected_))return;
        raw[42]=uint8_t((raw[42]&~0xc1u)|(mod?64:0)|(opt?128:0)|(dynamic?1:0));
        b_->programs[indices[selected_]].resolve_controls(raw);
        visit([&](auto& x){apply(x.a,*b_,raw,mod,opt);apply(x.b,*bankxl_,raw,mod,opt);});
    }
    // Both branches run on identical inputs and retain their own tails.
    std::array<float,4> process(float l,float r) noexcept {
        std::array<float,4> out{};if(!available(selected_))return out;
        if(selected_<3){original_->process(l,r,out[0],out[1],true);clean_->process(l,r,out[2],out[3],true);}
        else {
            visit([&](auto& x){x.a.process(l,r,out[0],out[1],true);x.b.process(l,r,out[2],out[3],true);});
            const unsigned delay=70-((32*1280+16*18*xl::graphs[indices[selected_]].rows+640)/1280);
            const auto value=pad_[(pad_pos_+pad_.size()-delay)%pad_.size()];pad_[pad_pos_]=out;
            pad_pos_=(pad_pos_+1)%pad_.size();out=value;
        }
        return out;
    }
private:
    template<class E,class B>void activate(E& e,const B& b)noexcept {
        const auto& d=b.programs[indices[selected_]];auto settings=d.template settings<E::graph_id>();
        e.activate(settings);e.set_modulation(d.modulation,d.initial_modulation,true);e.set_dynamics(d.dynamics,d.initial_dynamics);e.set_chorus_reference(uint8_t(d.chorus*8));
    }
    template<class E,class B>void apply(E& e,const B& b,const std::array<uint8_t,48>& raw,bool mod,bool opt)noexcept {
        const auto& d=b.programs[indices[selected_]];e.set_native_controls(d.template settings<E::graph_id>(),d.controls,raw,opt);
        if(d.chorus_page)e.set_chorus(raw[d.pages[d.chorus_page-1].cells[d.chorus_slot]]);
        e.enable_modulation(mod);e.set_global(0,1,1,0,2);
    }
    template<class F>void visit(F f)noexcept {
        switch(selected_){case 3:f(std::get<0>(*slots_));break;case 4:f(std::get<1>(*slots_));break;case 5:f(std::get<2>(*slots_));break;case 6:f(std::get<3>(*slots_));break;}
    }
    std::unique_ptr<native_hall::DesktopEngine48> original_;
    std::unique_ptr<clean_native_hall::DesktopEngine48> clean_;
    std::unique_ptr<Slots> slots_;
    std::unique_ptr<clean_native_hall::ProgramBank> bank224_;
    std::unique_ptr<clean_cineol::xl::Bank> bankxl_;
    const native_hall::ProgramBank* a_=nullptr;const xl::Bank* b_=nullptr;
    std::array<std::array<float,4>,32> pad_{};unsigned pad_pos_=0,selected_=programs;
};
}
