// Independent T&C component oracle. Actual physical layouts are selected
// before a fresh reference board is clocked with a ready-controlled bus actor.
// This does not inject firmware event timing into the native audio engine.
#undef NDEBUG
#include "xl_reference.hpp"
#include "../desktop/wcs_timing_xl.hpp"
#include <emulator/wcs_access.hpp>
#include <limits>
#include <chrono>
#include <new>

static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n){if(tracking)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{if(tracking && p)++releases;std::free(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
using namespace cineol::xl;using namespace lexplug;using namespace lexplug::op;
using xl_test::require;namespace hw=lexicon224x::cpu;

struct Reference {
    hw::Scheduler scheduler;
    lexicon224x::Machine machine;
    hw::WcsAccess bus{scheduler,machine};
    uint64_t executed=0;
    explicit Reference(const std::array<uint32_t,128>& wcs) {
        std::copy(wcs.begin(),wcs.end(),std::begin(machine.wcs));
        scheduler.set_cpu_time(std::numeric_limits<uint64_t>::max());
    }
    void until(hw::Tick time) {
        while(scheduler.step_one(time,[&](hw::RowPhase phase,uint64_t row){
            if(phase==hw::RowPhase::Begin)bus.at_marker(machine.microinstruction,true);
            if(phase==hw::RowPhase::Fetch) {
                const auto old=machine.microinstruction;
                lexicon224x::fetch(machine,bus.displaced(scheduler.timing.marker(row)));
                bus.at_fetch(old,machine.microinstruction);++executed;
            }
        },[]{})){}
    }
};
static void match(uint64_t wanted,uint64_t actual,const char* name,unsigned program,unsigned iteration) {
    if(wanted!=actual){std::cerr<<"program="<<program<<" access="<<iteration<<" "<<name<<" native="<<wanted<<" reference="<<actual<<'\n';require(false,"XL native T&C boundary differs");}
}
static void check(unsigned program,const std::array<uint32_t,128>& wcs) {
    const auto graph=Graph(program);const auto info=graph_info(graph);
    const auto& slots=wcs_unprotected_rows[program];
    for(unsigned row=0;row<info.rows;++row) {
        const auto mi=lexicon224x::decode(wcs[row]);
        require(mi.protect==(row!=slots[0] && row!=slots[1]),"native XL protection slots differ from selected physical graph");
        require(mi.reset==(row+2==info.rows),"native XL RESET property differs");
    }
    Reference reference(wcs);WcsTimingXL native;native.reset(graph);
    uint64_t t1_state=0,reads=0,writes=0;uint32_t random=17;
    unsigned maximum_work=0;uint64_t checksum=0;double seconds=0;
    for(unsigned iteration=0;iteration<4096;++iteration) {
        random=random*1664525u+1013904223u;
        const bool reading=(iteration%7==0 || iteration%7==1);
        const unsigned row=(random>>12)%(info.rows-2);
        // Coefficient byte writes retain protect/reset shape. Low address
        // byte writes select a non-OPER row, so they cannot create RESET.
        unsigned target=row,lane=reading?(random>>24)&3:3;
        if(!reading && iteration%5==0) {
            for(unsigned n=0;n<info.rows;++n) {
                target=(row+n)%(info.rows-2);
                if(lexicon224x::decode(wcs[target]).op!=lexicon224x::OPER){lane=0;break;}
            }
        }
        const uint8_t value=uint8_t(random>>16);
        const auto t1=t1_state*hw::cpu_period;
        const auto request=t1+(reading?hw::cpu_period+hw::phi2_rise:2*hw::cpu_period);
        hw::WcsAccess::Request* actual=nullptr;
        reference.scheduler.at(request,[&]{actual=&reference.bus.on_request(reading,target,lane,value,t1);},!reading);
        const auto start=std::chrono::steady_clock::now();tracking=true;
        const auto predicted=native.access(t1_state,reading);tracking=false;
        seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        maximum_work=std::max(maximum_work,native.last_operations());
        reference.until(predicted.grant+hw::row*3);
        require(actual && actual->scheduled,"reference access did not complete within predicted window");
        // The reference board fills these directly at its grant event.
        match(predicted.grant,actual->displaced_start-hw::row,"grant",program,iteration);
        match(predicted.ack,actual->ack_time,"XACK",program,iteration);
        match(predicted.sample,actual->sampled_at,"CPU data sample",program,iteration);
        match(predicted.displaced_begin,actual->displaced_start,"displacement begin",program,iteration);
        match(predicted.displaced_end,actual->displaced_end,"displacement end",program,iteration);
        match(predicted.held_begin,actual->first_held,"first operand hold",program,iteration);
        match(predicted.held_end,actual->last_held,"last operand hold",program,iteration);
        if(reading) {
            ++reads;match(predicted.read_drive,actual->read_enable,"read bus drive",program,iteration);
            match(predicted.read_release,actual->read_release,"read bus release",program,iteration);
        } else {++writes;match(predicted.commit,actual->commit_at,"write commit",program,iteration);}
        const uint64_t finished=(actual->sampled_at-hw::phi2_rise)/hw::cpu_period+1;
        match(predicted.finished_cpu_state,finished,"instruction completion",program,iteration);
        checksum+=predicted.grant+predicted.commit+predicted.displaced_end;
        const unsigned gaps[]={0,1,3,17,97,503,2048,100003};
        t1_state=finished+gaps[(random>>27)&7];
    }
    std::cout<<info.name<<": 4096 read/write accesses, reads="<<reads<<", writes="<<writes
        <<", all boundaries exact, maximum native phase operations="<<maximum_work<<", timed native seconds="<<seconds
        <<", checksum="<<checksum<<'\n'<<std::flush;
}
int main(int argc,char** argv) {
    require(argc==1 || argc==2,"usage: cineol_xl_wcs_clock_check [ROM_DIRECTORY]");
    std::unique_ptr<Engine> engine;std::unique_ptr<Machine> machine;std::unique_ptr<LarcOperator> op;
    if(argc==2) {
        engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);machine=std::make_unique<Machine>(*engine);op=std::make_unique<LarcOperator>(*machine);
        require(!machine->run_task([&]{return xl_test::boot(*machine,*op);}).failed,"XL T&C oracle boot failed");
    }
    for(unsigned program=0;program<graphs.size();++program) {
        std::array<uint32_t,128> wcs{};const auto info=graphs[program];
        if(engine) {
            require(!machine->run_task([&]{return xl_test::select(*machine,*op,info.bank,info.program);}).failed,"XL T&C oracle physical selection failed");
            xl_test::ShapeCheck shape{*engine->host().dsp,Graph(program)};shape.run();require(shape.valid,"XL T&C oracle graph shape differs");
            std::copy(std::begin(engine->host().dsp->wcs),std::end(engine->host().dsp->wcs),wcs.begin());
        } else {
            for(unsigned row=0;row<info.rows;++row)wcs[row]=(row!=wcs_unprotected_rows[program][0] && row!=wcs_unprotected_rows[program][1])?(1u<<22):0;
            wcs[info.rows-2]|=(1u<<16)|8;
        }
        check(program,wcs);
    }
    require(allocations==0 && releases==0,"native XL T&C access touched heap");
    std::cout<<"22 XL T&C component layouts pass; native heap new/delete=0/0; physical firmware scan/cold selection phase not injected or validated\n";
}
