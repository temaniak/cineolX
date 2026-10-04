#include "xl_import.hpp"
#include <juce-plugin/source/operator/larc_operator.hpp>
#include <juce-plugin/source/roms/firmware_sets_data.hpp>
#include <juce-plugin/source/roms/sha256.hpp>
#include <cmath>
#include <stdexcept>
#include "xl_display.hpp"

namespace cineol::xl::import {
using namespace lexplug;using namespace lexplug::op;
static const auto& firmware=lexplug::roms::data::known_sets[7];
static void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
static ControlKind control_kind(unsigned entry) {
    switch(entry) {
        case 0xae9d:return ControlKind::low_decay;
        case 0xaf4e:case 0xaf68:return ControlKind::mid_decay;
        case 0xb0ff:return ControlKind::filter;
        case 0xb193:return ControlKind::depth;
        case 0xb2dd:case 0xb4a3:return ControlKind::predelay;
        case 0xb3ba:return ControlKind::chorus;
        case 0xb2a3:return ControlKind::diffusion;
        case 0xb277:return ControlKind::definition;
        case 0xb3e3:return ControlKind::level;
        case 0xb3fa:return ControlKind::delay;
        case 0xb4c3:return ControlKind::pan;
        case 0xa9ae:case 0xb53b:return ControlKind::none;
        default:throw std::runtime_error("Unknown XL native control compiler.");
    }
}
int rom_chip(const uint8_t* data,size_t size) {
    if(!data || (size!=2048 && size!=4096)) return -1;
    const auto hash=lexplug::roms::Sha256::of(data,size);
    for(int i=0;i<firmware.chip_count;++i) if(hash==firmware.chips[i].sha256) return i;
    return -1;
}
// Reject a graph mismatch during preparation, before making it playable.
struct ShapeCheck {
    const lexicon224x::Machine& source;Graph graph;bool valid=true;
    template<unsigned Row,unsigned Op,unsigned RA,unsigned WA,bool Transfer,bool Zero,unsigned Outputs,bool Shift,bool WriteX=false>
    void node(int16_t=0) {
        const auto mi=lexicon224x::decode(source.wcs[Row]);
        const unsigned operation=mi.op==lexicon224x::MEMR?1:mi.op==lexicon224x::MEMW?2:
            mi.op==lexicon224x::OPER && mi.source==lexicon224x::FromADC?3:
            mi.op==lexicon224x::OPER && mi.source==lexicon224x::FromRR?4:
            mi.op==lexicon224x::OPER && mi.source==lexicon224x::FromXREG?5:0;
        valid &= operation==Op && mi.ra==RA && mi.wa==WA && mi.xfer==Transfer && mi.zero==Zero &&
            (mi.wr_da?mi.channels:0)==Outputs && mi.keep_shifting==Shift &&
            mi.wr_xreg==WriteX && mi.reset==(Row+2==graph_info(graph).rows);
    }
    void run() {int16_t left=0,right=0;
        using cineol::xl::Graph;
        switch(graph) {
#include "../desktop/graph_dispatch.inc"
        }
    }
};
static Task<void> report_progress(double progress,const char* stage,const native_hall::import::Callbacks& callbacks) {
    if(callbacks.progress && !callbacks.progress(progress,stage)) co_await fail("Import cancelled.");
}
static Task<void> prepare_displays(Engine& engine,Machine& machine,LarcOperator& op,ProgramData& data,
    std::array<uint8_t,65536>& display_memory) {
    auto& host=engine.host();
    auto word=[&](unsigned a){return unsigned(host.memory[a])|unsigned(host.memory[a+1])<<8;};
    for(unsigned p=0;p<data.page_count;++p) {
        co_await op.gotoPage(int(p+1));
        auto& page=data.pages[p];page.type=host.memory[0x3c33];
        for(unsigned slot=0;slot<6;++slot) {
            if(std::strncmp(page.names[slot].data(),"INACTIVE",8)==0) {page.cells[slot]=255;continue;}
            const unsigned cell=page.type==13?43+slot:page.column*6+slot;
            if(cell>=48) co_await fail("Invalid XL logical display binding.");
            page.cells[slot]=uint8_t(cell);
            if((page.type==1 || page.type==2) && slot==5)
                page.variable_predelay[slot]=host.memory[uint16_t(word(0x3e05)+0x3ca3+cell+0x2b8)]==255;
            unsigned limit=host.memory[uint16_t(word(0x3e03)+1+cell)];
            if(page.type==13 && slot<2) limit=255;
            else if(page.type==10 || page.type==5 || ((page.type==1 || page.type==2) && slot==5)) {
                if(limit==255) limit=255;else limit=(limit&31)*8;
            }
            page.maximum[slot]=uint8_t(limit);
            bool failed=false;
            try {
                for(unsigned raw=0;raw<256;++raw) page.values[slot][raw]=display_value(host.memory,slot,cell,raw,display_memory);
            } catch(const std::exception&) {failed=true;}
            if(failed) co_await fail("XL display scale preparation failed.");
        }
        co_await machine.sleep(0.01);
    }
    co_await op.gotoPage(1);
}
// Bound compiler-specific coroutine frames by separating selection/control
// capture from dynamics capture. The preparation order and firmware timing stay
// identical; all tasks still use the existing fixed pool.
static Task<void> prepare_program_controls(Engine& engine,Machine& machine,LarcOperator& op,Bank& result,
    const native_hall::import::Callbacks& callbacks,std::array<uint8_t,65536>& display_memory,PagesReading& pages,unsigned index) {
    const auto info=graphs[index];const double progress=double(index)/graphs.size();const char* stage=info.name;
    auto& data=result.programs[index];auto& host=engine.host();
    auto word=[&](unsigned a){return unsigned(host.memory[a])|unsigned(host.memory[a+1])<<8;};
    host.pc_watches[0x11b5]=true;
    host.pc_observer=[&](uint64_t,lexicon224x::cpu::CpuSnapshot cpu) {
        host.pc_watches[0x11b5]=true;
        if(cpu.de<0xa91e || cpu.de>=0xa9ae || ((cpu.de-0xa91e)&1) || cpu.hl<0x3ca3 || cpu.hl>=0x3cd3) return;
        auto& slot=data.controls.slots[(cpu.de-0xa91e)/2];slot.kind=control_kind(word(cpu.de));
        slot.cell=uint8_t(cpu.hl-0x3ca3);
        slot.groups=slot.kind==ControlKind::none?0:slot.kind==ControlKind::filter?2:slot.kind==ControlKind::depth?4:1;
        for(unsigned g=0;g<slot.groups;++g) {
            auto& group=slot.group[g];const unsigned descriptor=cpu.bc+g*3;
            group.flags=host.memory[descriptor+2];group.count=group.flags&15;
            unsigned target_address=word(descriptor);
            if(slot.kind==ControlKind::delay || slot.kind==ControlKind::predelay)
                group.timing=host.memory[uint16_t(word(0x3e03)+cpu.hl+6+0xc358)];
            for(unsigned t=0;t<group.count;++t) {
                auto& target=group.targets[t];const unsigned address=word(target_address);
                check(address>=0x4000 && address<0x4200,"Invalid XL native control target.");
                target.row=uint8_t(127-(address-0x4000)/4);target.meta=host.memory[target_address+2];
                target.signs=lexicon224x::decode(host.dsp->wcs[target.row]).negative?1:0;
                if(slot.kind==ControlKind::diffusion || slot.kind==ControlKind::definition)
                    for(unsigned pair=1;pair<3;++pair) if(lexicon224x::decode(host.dsp->wcs[target.row+pair]).negative) target.signs|=uint8_t(1u<<pair);
                if(slot.kind==ControlKind::mid_decay && (target.meta&64)) {
                    const unsigned second=word(target_address+3);
                    check(second>=0x4000 && second<0x4200,"Invalid XL paired decay target.");
                    target.second_row=uint8_t(127-(second-0x4000)/4);target.second_meta=host.memory[target_address+5];
                    // The paired decay compiler derives its sign from LF/MID.
                    target_address+=6;
                } else if(slot.kind==ControlKind::delay || slot.kind==ControlKind::predelay) {
                    target.offset=uint16_t(word(target_address+3));target_address+=5;
                } else target_address+=3;
            }
        }
    };
    if(!(co_await op.selectProgram(int(info.bank),int(info.program)))) co_await fail("XL program selection failed.");
    // Program selection may briefly compile the previous program while fading
    // it out. Discard those descriptors before compiling the selected one.
    data.controls.slots.fill(ControlSlot{});
    co_await op.setToggle(0,false);co_await op.setToggle(2,false);co_await op.setToggle(1,true);
    co_await machine.sleep(1);co_await report_progress(progress,stage,callbacks);
    co_await op.readPages(pages);co_await report_progress(progress,stage,callbacks);
    for(unsigned slider=0;slider<6;++slider) {
        const auto& source=pages.pages[0].sliders[slider];
        if(std::strncmp(source.shown.name,"INACTIVE",8)==0) continue;
        co_await op.moveSlider(1,slider,source.raw^128);
        co_await op.moveSlider(1,slider,source.raw);
        co_await machine.sleep(0.1);
        break;
    }
    host.pc_watches[0x11b5]=false;host.pc_observer={};
    std::copy_n(host.memory.begin()+0x3ca3,data.controls.factory.size(),data.controls.factory.begin());
    data.controls.time_scale=host.memory[word(0x3e03)];
    data.predelay_base=host.memory[uint16_t(word(0x3e03)-1)];
    for(unsigned t=0;t<32;++t) data.controls.decay_times[t]=uint16_t(host.memory[0x8ac4+t]|
        (t>=30?unsigned(host.memory[0x8ac4+t+2])<<8:0));
    for(unsigned c=0;c<data.controls.decay_curves.size();++c)
        std::copy_n(host.memory.begin()+0xb0d2+c*5,5,data.controls.decay_curves[c].begin());
    for(unsigned c=0;c<data.controls.depth_curves.size();++c) for(unsigned g=0;g<4;++g)
        std::copy_n(host.memory.begin()+0xb247+c*16+g*4,4,data.controls.depth_curves[c][g].begin());
    for(unsigned r=0;r<4;++r) data.controls.addresses.regions[r]=uint16_t(word(0x3cd8+r*2));
    data.controls.addresses.lengths[0]=uint16_t(word(0x3ce5));data.controls.addresses.lengths[1]=uint16_t(word(0x3ce7));
    data.controls.addresses.scales={host.memory[0x3ce9],host.memory[0x3cea]};
    auto& size=data.controls.size;
    size.enabled=host.memory[uint16_t(word(0x3e03)+0x2c)]==254;
    size.coupling=host.memory[0x3ce0];
    std::copy_n(host.memory.begin()+0x3ce1,4,size.ranges.begin());
    for(unsigned row=0;row<128;++row) {
        const uint16_t source=uint16_t(word(0x3e01)+0x2a6-row*4);
        size.offsets[row]=uint16_t(word(source));size.scale[row]=(host.memory[source+2]&3)!=2;
    }
    data.page_count=uint8_t(pages.count);
    for(unsigned p=0;p<data.page_count;++p) {
        const auto& source=pages.pages[p];auto& page=data.pages[p];page.column=uint8_t(source.column);
        for(unsigned slot=0;slot<6;++slot) {
            const auto& slider=source.sliders[slot];page.raw[slot]=slider.raw;
            std::copy_n(slider.shown.name,13,page.names[slot].begin());
            std::copy_n(slider.shown.value,25,page.factory_values[slot].begin());
            if(std::strncmp(slider.shown.name,"CHORUS",6)==0) {data.chorus_page=uint8_t(p+1);data.chorus_slot=uint8_t(slot);data.chorus=slider.raw>>3;}
            if(std::strncmp(slider.shown.name,"INACTIVE",8)!=0)
                for(const auto& control:data.controls.slots)
                    if(control.kind==ControlKind::definition && control.cell==page.column*6+slot)
                        data.controls.definition_cell=control.cell;
            if(std::strncmp(slider.shown.name,"DIFFUSION",9)==0) {data.diffusion_page=uint8_t(p+1);data.diffusion_slot=uint8_t(slot);data.diffusion_index=slider.raw>>2;}
        }
    }
    auto& feedback=data.controls.feedback;
    feedback.count=host.memory[0x3cf9]&15;feedback.separate_stop=(host.memory[0x3df9]&64)==0;
    data.controls.feedback_limit=host.memory[0x3e0b];data.controls.reduction=host.memory[0x3e12];
    const unsigned feedback_targets=word(0x3cf7);
    for(unsigned t=0;t<feedback.count;++t) {
        const unsigned address=word(feedback_targets+t*3);
        if(address<0x4003 || address>=0x4200 || (address&3)!=3) co_await fail("Invalid XL feedback descriptor.");
        auto& target=feedback.targets[t];target.row=uint8_t(127-(address-0x4000)/4);
        target.scale_cap=host.memory[feedback_targets+t*3+2];
        for(unsigned pair=0;pair<3;++pair) target.negative[pair]=lexicon224x::decode(host.dsp->wcs[target.row+pair]).negative;
    }
    co_await prepare_displays(engine,machine,op,data,display_memory);
}
static Task<void> prepare_program_dynamics(Engine& engine,Machine& machine,LarcOperator& op,Bank& result,
    const native_hall::import::Callbacks& callbacks,std::array<uint8_t,65536>& display_memory,PagesReading& pages,unsigned index) {
    const auto info=graphs[index];const double progress=double(index)/graphs.size();const char* stage=info.name;
    auto& data=result.programs[index];auto& host=engine.host();
    auto word=[&](unsigned a){return unsigned(host.memory[a])|unsigned(host.memory[a+1])<<8;};
    data.diffusion.half_scale=true;
    unsigned diffusion_record=0;
    if(data.diffusion_page) {
    const auto& page=data.pages[data.diffusion_page-1];const auto raw_diffusion=page.raw[data.diffusion_slot];
    host.pc_watches[0xb2a3]=true;
    host.pc_observer=[&](uint64_t,lexicon224x::cpu::CpuSnapshot cpu) {diffusion_record=cpu.bc;};
    co_await op.moveSlider(data.diffusion_page,data.diffusion_slot,raw_diffusion);
    host.pc_watches[0xb2a3]=false;host.pc_observer={};
    if(!(diffusion_record!=0)) co_await fail("XL Diffusion compiler record not found.");
    data.diffusion.count=host.memory[diffusion_record+2]&15;data.diffusion.half_scale=true;
    const unsigned diffusion_targets=word(diffusion_record);
    for(unsigned i=0;i<data.diffusion.count;++i) {
        const unsigned address=word(diffusion_targets+i*3);
        if(!(address>=0x4003 && address<0x4200 && (address&3)==3)) co_await fail("Invalid XL Diffusion descriptor.");
        auto& target=data.diffusion.targets[i];target.row=uint8_t(127-(address-0x4000)/4);
        target.scale_cap=host.memory[diffusion_targets+i*3+2];
        if(!(unsigned(target.row)+2<info.rows)) co_await fail("XL Diffusion target is outside the graph.");
        for(unsigned j=0;j<3;++j) target.negative[j]=lexicon224x::decode(host.dsp->wcs[target.row+j]).negative;
    }
    }
    auto& modulation=data.modulation;
    std::copy_n(host.memory.begin()+0x8000,4096,modulation.sequence.begin());
    modulation.flags=host.memory[0x3cf6];modulation.period=host.memory[0x3cd2];modulation.hold=host.memory[0x3cd3];
    modulation.step=host.memory[0x3cd4];modulation.mask=host.memory[0x3cd5];
    const unsigned descriptors=word(0x3cf4),taps=modulation.flags&15;
    for(unsigned i=0;i<taps;++i) {
        const unsigned address=word(descriptors+i*5);
        if(!(address>=0x4003 && address<0x4200 && (address&3)==3)) co_await fail("Invalid XL modulation descriptor.");
        modulation.rows[i]=uint8_t(127-(address-0x4000)/4);modulation.caps[i]=host.memory[descriptors+i*5+2];
        for(unsigned pair=0;pair<2;++pair) modulation.negative[i][pair]=
            lexicon224x::decode(host.dsp->wcs[modulation.rows[i]+pair]).negative;
    }
    unsigned calls=0;uint64_t first=0,last=0;
    std::array<unsigned,2> dynamics_calls{};std::array<uint64_t,2> dynamics_first{},dynamics_last{};
    host.pc_watches[0xad5c]=host.pc_watches[0x82cf]=host.pc_watches[0x81b6]=true;
    host.pc_observer=[&](uint64_t cycles,lexicon224x::cpu::CpuSnapshot cpu) {
        if(cpu.pc!=0xad5c) {
            const unsigned i=cpu.pc==0x82cf?0:1;
            if(!dynamics_calls[i]++) dynamics_first[i]=cycles;
            dynamics_last[i]=cycles;host.pc_watches[cpu.pc]=true;return;
        }
        if(!calls++) first=cycles;last=cycles;host.pc_watches[0xad5c]=true;
    };
    for(unsigned n=0;n<10;++n) {co_await report_progress(progress,stage,callbacks);co_await machine.sleep(0.1);}
    host.pc_watches[0xad5c]=host.pc_watches[0x82cf]=host.pc_watches[0x81b6]=false;host.pc_observer={};
    for(unsigned i=0;i<2;++i) if(dynamics_calls[i]<2 || dynamics_last[i]<=dynamics_first[i])
        co_await fail("XL level-following clock measurement failed.");
    data.dynamics.slow_rate_tenths=uint32_t(std::lround(double(dynamics_calls[0]-1)*20480000.0/double(dynamics_last[0]-dynamics_first[0])));
    data.dynamics.fast_rate_tenths=uint32_t(std::lround(double(dynamics_calls[1]-1)*20480000.0/double(dynamics_last[1]-dynamics_first[1])));
    data.dynamics.base_period=host.memory[0x3c5b];data.dynamics.shared_stop=(host.memory[0x3df9]&64)!=0;
    data.dynamics.enabled=(host.memory[0x3e07]&1)==0;
    auto& initial=data.initial_dynamics;
    initial.held=uint16_t(word(0x3e0f));initial.average=host.memory[0x3c50];initial.flags=host.memory[0x3c51];
    initial.low=host.memory[0x3c52];initial.mid=host.memory[0x3c53];initial.trigger_peak=host.memory[0x3c54];
    initial.stop_counter=host.memory[0x3c5f];initial.stopped=host.memory[0x3e11];initial.amount=host.memory[0x3e12];
    initial.divider=host.memory[0x3e13];initial.period=host.memory[0x3e14];initial.peak_divider=host.memory[0x3c38];
    initial.peak_input=host.memory[0x3c61];std::copy_n(host.memory.begin()+0x3e15,11,initial.history.begin());
    initial.feedback_mid=uint8_t(host.memory[0x3e3e]<<3);initial.feedback_amount=initial.amount;
    if(!(!taps || (calls>100 && last>first))) co_await fail("XL modulation clock measurement failed.");
    modulation.rate_tenths=taps?uint32_t(std::lround(double(calls-1)*20480000.0/double(last-first))):1;
    auto& state=data.initial_modulation;
    state.divider=host.memory[0x3e44];state.random_divider=host.memory[0x3e45];state.random_hold=host.memory[0x3e46];
    state.index=uint16_t(word(0x3e47)&4095);
    for(unsigned i=0;i<taps;++i) {state.address_low[i]=host.memory[descriptors+i*5+3];state.phase[i]=host.memory[descriptors+i*5+4];}
    if(!taps) {modulation.period=1;modulation.hold=32;modulation.step=4;state={};}
    ShapeCheck shape{*host.dsp,Graph(index)};shape.run();if(!(shape.valid)) co_await fail("Unsupported XL native graph variation.");
    for(unsigned r=0;r<info.rows;++r) {
        const auto mi=lexicon224x::decode(host.dsp->wcs[r]);
        data.coefficients[r]=int8_t(mi.negative?-int(mi.coefficient):int(mi.coefficient));data.offsets[r]=uint16_t(~mi.low);
    }
    if(!data.controls.valid(info.rows)) co_await fail("Invalid XL control profile for %s (%u rows).",info.name,info.rows);
    if(!(data.valid(Graph(index)))) co_await fail("Invalid prepared XL program: %s.",info.name);
}
static Task<void> prepare_programs(Engine& engine,Machine& machine,LarcOperator& op,Bank& result,
    const native_hall::import::Callbacks& callbacks,std::array<uint8_t,65536>& display_memory,PagesReading& pages,bool silent_preparation=true) {
    double progress=0;const char* stage="Starting 224XL v8.21";
    for(unsigned n=0;n<160;++n) {co_await report_progress(progress,stage,callbacks);co_await machine.sleep(0.1);}
    if(silent_preparation) engine.prepare_silence();
    for(unsigned index=0;index<graphs.size();++index) {
        progress=double(index)/graphs.size();stage=graphs[index].name;co_await report_progress(progress,stage,callbacks);
        co_await prepare_program_controls(engine,machine,op,result,callbacks,display_memory,pages,index);
        co_await prepare_program_dynamics(engine,machine,op,result,callbacks,display_memory,pages,index);
    }
    progress=1;stage="Saving prepared XL bank";co_await report_progress(progress,stage,callbacks);
}
PreparationRuntimeStats check_preparation_runtime(const native_hall::import::Callbacks& callbacks) {
    auto engine=std::make_unique<Engine>(0);
    Machine machine(*engine);LarcOperator op(machine);auto result=std::make_unique<Bank>();
    auto display_memory=std::make_unique<std::array<uint8_t,65536>>();
    auto pages=std::make_unique<PagesReading>();
    auto probe=[&](const char* stage,auto make) {
        if(callbacks.progress) callbacks.progress(0,stage);
        {
            PoolScope scope(machine.pool());auto task=make();
            // Clang may elide these short-lived frames into this function.
            check(bool(task.handle()) && !task.handle().done(),"XL runtime probe did not create a suspended task.");
        }
        check(machine.pool().in_use()==0,"XL runtime probe leaked a coroutine frame.");
    };
    probe("prepare_programs",[&]{return prepare_programs(*engine,machine,op,*result,callbacks,*display_memory,*pages);});
    probe("prepare_program_controls",[&]{return prepare_program_controls(*engine,machine,op,*result,callbacks,*display_memory,*pages,0);});
    probe("prepare_program_dynamics",[&]{return prepare_program_dynamics(*engine,machine,op,*result,callbacks,*display_memory,*pages,0);});
    probe("prepare_displays",[&]{return prepare_displays(*engine,machine,op,result->programs[0],*display_memory);});
    probe("selectProgram",[&]{return op.selectProgram(1,1);});
    probe("readPages",[&]{return op.readPages(*pages);});
    probe("moveSlider",[&]{return op.moveSlider(1,0,0);});
    probe("setToggle",[&]{return op.setToggle(0,false);});
    probe("gotoPage",[&]{return op.gotoPage(1);});
    if(callbacks.progress) callbacks.progress(0,"cancel before firmware execution");
    native_hall::import::Callbacks cancel;cancel.progress=[](double,const char*){return false;};
    const auto done=machine.run_task([&]{return prepare_programs(*engine,machine,op,*result,cancel,*display_memory,*pages);});
    check(done.failed && std::string(done.error.text)=="Import cancelled.","XL runtime cancellation failed.");
    check(machine.frame()==0 && machine.pool().in_use()==0,"XL runtime cancellation rendered firmware or leaked frames.");
    if(callbacks.progress) callbacks.progress(0,"silent preparation CPU/bus equivalence");
    for(unsigned port=3;port<=9;++port) {
        auto normal=std::make_unique<Engine>(0),silent=std::make_unique<Engine>(0);
        // Synthetic IN/STA/MVI/STA/HLT: read a DSP port and write a WCS byte.
        // Exercise monitors, transfers, headroom and protected bus timing.
        const uint8_t code[]={0xdb,uint8_t(port),0x32,0x00,0x3c,0x3e,0x5a,0x32,0x00,0x40,0x76};
        normal->load(code,sizeof code,0);silent->load(code,sizeof code,0);silent->prepare_silence();
        std::array<float,511> zero{};std::array<std::array<float,511>,4> output{};
        float* out[]={output[0].data(),output[1].data(),output[2].data(),output[3].data()};
        for(int frames:{1,7,128,511}) {
            normal->render(zero.data(),zero.data(),out,frames);silent->render(zero.data(),zero.data(),out,frames);
            check(normal->host().cycles==silent->host().cycles && normal->host().memory==silent->host().memory &&
                normal->host().wcs_writes()==silent->host().wcs_writes(),"XL silent preparation changed CPU/bus state.");
            for(unsigned row=0;row<128;++row) check(normal->host().dsp->wcs[row]==silent->host().dsp->wcs[row],"XL silent preparation changed WCS.");
        }
        zero[0]=1;bool rejected=false;
        try {silent->render(zero.data(),zero.data(),out,1);} catch(const std::logic_error&) {rejected=true;}
        check(rejected,"XL silent preparation accepted non-silent input.");
    }
    return {machine.pool().max_request(),machine.pool().high_water()};
}
std::unique_ptr<Bank> prepare_bank(const RomSet& roms,const native_hall::import::Callbacks& callbacks) {
    check(std::string(firmware.name)=="224XL v8.21","XL firmware catalog changed");
    auto engine=std::make_unique<Engine>(0);
    for(unsigned chip=0;chip<roms.size();++chip) {
        check(rom_chip(roms[chip].data(),roms[chip].size())==int(chip),"A complete original 224XL v8.21 ROM set is required.");
        engine->load(roms[chip].data(),roms[chip].size(),firmware.chips[chip].base);
    }
    Machine machine(*engine);LarcOperator op(machine);auto result=std::make_unique<Bank>();
    // Keep the formatter's 64 KiB scratch image outside coroutine frames.
    // Compiler inlining must not overflow the operator's fixed 16 KiB blocks
    // (whose failure handler aborts the host process).
    auto display_memory=std::make_unique<std::array<uint8_t,65536>>();
    // MSVC spills PagesReading into the coroutine frame even in Release.
    // Allocate import-only scratch before starting any task, outside audio.
    auto pages=std::make_unique<PagesReading>();
    const auto done=machine.run_task([&]{return prepare_programs(*engine,machine,op,*result,callbacks,*display_memory,*pages,!callbacks.full_emulation);});
    check(!done.failed,done.error.text);return result;
}
} // namespace cineol::xl::import
