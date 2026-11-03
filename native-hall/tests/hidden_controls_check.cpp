// Offline physical-panel v4.4 oracle. ROM state alignment below is diagnostic;
// independent audio comparisons live beside the retained validation report.
#include "../tools/reference224.hpp"
#include "../desktop/level_timing224.hpp"
#include <juce-plugin/source/operator/panel224_operator.hpp>
#include <iostream>
using namespace native_hall;using namespace sound_validation;using namespace lexplug;using namespace lexplug::op;
static Task<void> boot(Machine& m){co_await m.sleep(10);}
static Task<void> select(Panel224Operator& op,unsigned p) {
    require(co_await op.selectProgram(p==0?4:1),"alternate select");
    require(co_await op.selectProgram(1u<<p),"target select");
    co_await op.setToggle(1,true);co_await op.setToggle(2,true);
}
static Task<void> hidden(Machine& m,Panel224Operator& op,unsigned key,unsigned setting) {
    require(co_await op.cancelShift(),"cancel shift");require(co_await op.hold(1,panel224::SHIFT),"shift");
    require(co_await op.hold(0,1u<<(key-1)),"hidden key");
    require(co_await op.setPot(5,253),"preposition");co_await m.sleep(.04);
    require(co_await op.setPot(5,(setting-1)*16),"hidden pot");co_await m.sleep(.12);
    co_await op.hold(0,0);co_await op.hold(1,0);co_await op.cancelShift();
}
static Task<void> settle(Machine& m,double seconds){co_await m.sleep(seconds);}
int main(int argc,char** argv)try {
    require(argc==4,"usage: native_224_hidden_controls_check ROM_DIRECTORY BANK CSV");
    auto bank=read_bank(argv[2]);auto e=std::make_unique<Engine>(1);load_roms(*e,argv[1]);
    auto m=std::make_unique<Machine>(*e);Panel224Operator op(*m,LAYOUT_V4);
    auto run=[&](auto fn){auto result=m->run_task(fn);require(!result.failed,"physical operation failed");};
    run([&]{return boot(*m);});auto& h=e->host();
    std::ofstream csv(argv[3]);require(bool(csv),"CSV open");
    csv<<"program,rate,depth,opt,depth_last,mod_calls,mod_state_errors,mod_cost_errors,level_calls,level_state_errors,level_cost_errors\n";
    unsigned total_mod=0,total_level=0,errors=0,fixtures=0;
    for(unsigned p=0;p<6;++p)for(unsigned opt:{1u,5u,16u})for(unsigned rate:{1u,2u,16u})for(unsigned depth:{1u,8u,16u})for(unsigned depth_last:{0u,1u}) {
        // Cover ordering with the normal response; other responses only need
        // the two opposite modulation corners and the factory setting.
        if(opt!=5 && !((rate==1 && depth==16) || (rate==16 && depth==1) || (rate==2 && depth==1)) )continue;
        run([&]{return select(op,p);});
        if(depth_last){run([&]{return hidden(*m,op,7,rate);});run([&]{return hidden(*m,op,6,depth);});}
        else {run([&]{return hidden(*m,op,6,depth);});run([&]{return hidden(*m,op,7,rate);});}
        run([&]{return hidden(*m,op,8,opt);});
        auto native=std::make_unique<Hall>();native->prepare(*bank,p);
        Controls controls;controls.bass=h.memory[0x3f67];controls.mid=h.memory[0x3f66];
#ifndef CINEOL_TEST_LEGACY_HIDDEN
        controls.modulation_period=uint8_t(std::max(1u,rate-1));
        controls.modulation_step=uint8_t(depth_last?4*depth:rate==1?8:4);
        controls.optimization_response=uint8_t(opt);
#endif
        native->set_controls(controls);
        unsigned step=bank->programs[p].modulation_step,response=bank->programs[p].decay_amount;
#ifndef CINEOL_TEST_LEGACY_HIDDEN
        step=native->modulation_step();response=native->optimization_response();
        require(native->modulation_period()==h.memory[0x3f72] && step==h.memory[0x3f74] && response==h.memory[0x3f71],"effective hidden setting differs from physical ROM");
#endif
        unsigned calls=0,state_bad=0,cost_bad=0,levels=0,level_bad=0,level_cost_bad=0;
        bool seeded=false,mod_active=false,level_active=false;uint64_t mod_start=0,level_start=0,last=0;
        unsigned previous=0,mod_waits=0,level_waits=0,mod_expected=0,level_expected=0;
        DecayState initial{};
        for(unsigned pc=0xc7c;pc<=0xde5;++pc)h.pc_watches[pc]=true;
        for(unsigned pc=0x755;pc<=0x8c8;++pc)h.pc_watches[pc]=true;
        for(unsigned pc=0xe15;pc<=0xf88;++pc)h.pc_watches[pc]=true;
        for(unsigned pc=0x4e2;pc<=0x4ed;++pc)h.pc_watches[pc]=true;
        for(unsigned pc:{0xbbu,0x1b5u,0x228u})h.pc_watches[pc]=true;
        h.pc_observer=[&](uint64_t t,lexicon224x::cpu::CpuSnapshot cpu){
            h.pc_watches[cpu.pc]=true;
            if(mod_active && (previous==0xdd8 || previous==0xdd9 || previous==0xda5 || previous==0xdb7))mod_waits+=unsigned(t-last)-7;
            if(level_active && previous==0xe3a)level_waits+=unsigned(t-last)-7;
            if(cpu.pc==0xc7c){
                auto state=modulation(h);
                if(!seeded){native->restore_modulation(state);seeded=true;}
                else state_bad+=!equal(native->modulation_state(),state);
                mod_expected=modulation_cycles224(*bank,p,state,true,[](unsigned){return 7u;}
#ifndef CINEOL_TEST_LEGACY_HIDDEN
                    ,uint8_t(step)
#endif
                );
                native->advance_modulation();++calls;mod_active=true;mod_start=t;mod_waits=0;
            } else if(mod_active && (cpu.pc==0xbb || cpu.pc==0x1b5)) {
                mod_active=false;cost_bad+=unsigned(t-mod_start)-mod_waits!=mod_expected;
            }
            if(cpu.pc==0x755){std::memcpy(&initial,h.memory.data()+0x3e32,sizeof initial);level_active=true;level_start=t;level_waits=0;}
            else if(level_active && cpu.pc==0x779){
                require((controls.bass+controls.mid)/2==h.memory[0x3e61],"unexpected ROM mean");
                const bool half=h.memory[0x3e2f]&1;require(!half,"unexpected half-gain context");
                level_expected=level_cycles224(*bank,p,initial,cpu.hl,h.memory[0x3e61],true,uint8_t(response),h.memory[0x3f4c]==32,false,[](unsigned){return 7u;});
                native->restore_decay(initial);native->advance_decay(DecayController::level_from_word(cpu.hl));
            } else if(level_active && cpu.pc==0x228){
                level_active=false;++levels;level_bad+=std::memcmp(&native->decay_state(),h.memory.data()+0x3e32,sizeof(DecayState))!=0;
                level_cost_bad+=unsigned(t-level_start)-level_waits!=level_expected;
            }
            previous=cpu.pc;last=t;
        };
        // Excite the input detector, stop, then retrigger. No ROM state writes.
        h.set_audio(2047,0,1536,0);h.set_level_detectors(0,31);h.set_level_detectors(1,15);
        run([&]{return settle(*m,.12);});h.set_audio(0,0,0,0);h.set_level_detectors(0,0);h.set_level_detectors(1,0);
        run([&]{return settle(*m,.35);});h.set_audio(1024,0,512,0);h.set_level_detectors(0,7);h.set_level_detectors(1,3);
        run([&]{return settle(*m,.08);});h.set_audio(0,0,0,0);h.set_level_detectors(0,0);h.set_level_detectors(1,0);
        run([&]{return settle(*m,.15);});h.pc_observer={};h.pc_watches.fill(false);
        require(calls>100 && levels>10,"insufficient observed transitions");
        csv<<p<<','<<rate<<','<<depth<<','<<opt<<','<<depth_last<<','<<calls<<','<<state_bad<<','<<cost_bad<<','<<levels<<','<<level_bad<<','<<level_cost_bad<<'\n';csv.flush();
        total_mod+=calls;total_level+=levels;errors+=state_bad+cost_bad+level_bad+level_cost_bad;++fixtures;
    }
    std::cout<<fixtures<<" physical fixtures, "<<total_mod<<" modulation calls, "<<total_level<<" level calls, state/cycle errors="<<errors<<'\n';
    require(!errors,"hidden native controls/state/timing differ from physical ROM");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
