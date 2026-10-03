#include "../src/Controls.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <cstdlib>
using namespace cineol::controls;
static void require(bool ok,const char* message) {if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
static void hold(Mapper& mapper,InputFrame& frame,bool one,bool two) {
    frame.button1=one;frame.button2=two;
    for(unsigned i=0;i<12;++i) mapper.update(frame);
}
static void position(InputFrame& frame,Target target,float normalized) {
    for(unsigned i=0;i<controller_count;++i) if(control_config[i].target==target) {
        const auto& config=control_config[i];
        const float p=config.inverted?1-normalized:normalized;
        frame.values[i]=config.minimum+p*(config.maximum-config.minimum);
    }
}
static void bounded(const native_hall::Parameters& p) {
    require(p.program<6 && p.hall.bass>=1 && p.hall.bass<=31 && p.hall.mid>=0 && p.hall.mid<=31
            && p.hall.depth>=0 && p.hall.depth<=71 && p.hall.diffusion>=1 && p.hall.diffusion<=63
            && p.input_db>=-36 && p.input_db<=12 && p.mix>=0 && p.mix<=1,"mapped parameter limits");
}
int main(int argc,char** argv) {
    require(argc==2,"usage: controls_check PROGRAMS.bank224");
    for(auto range:{ControlConfig{Target::Bass,0,3.3f,false},ControlConfig{Target::Bass,0,5,false},
                   ControlConfig{Target::Bass,-5,5,false},ControlConfig{Target::Bass,0,1,false}}) {
        require(normalize(range.minimum,range)==0 && normalize(range.maximum,range)==1,"range endpoints");
        require(std::abs(normalize((range.minimum+range.maximum)/2,range)-0.5f)<1e-6f,"range midpoint");
        range.inverted=true;
        require(normalize(range.minimum,range)==1 && normalize(range.maximum,range)==0,"range inversion");
        require(std::isfinite(normalize(NAN,range)),"nonfinite input escaped normalization");
    }
    Mapper mapper;auto frame=default_frame();mapper.update(frame);
    bounded(mapper.parameters());
    for(unsigned i=0;i<controller_count;++i) position(frame,Target(i),0);mapper.update(frame);bounded(mapper.parameters());
    for(unsigned i=0;i<controller_count;++i) position(frame,Target(i),1);mapper.update(frame);bounded(mapper.parameters());
    hold(mapper,frame,true,false);hold(mapper,frame,false,false);
    require(!mapper.parameters().hall.mode_enhancement && mapper.parameters().hall.decay_optimization,"button 1 release");
    hold(mapper,frame,false,true);hold(mapper,frame,false,false);
    require(!mapper.parameters().hall.decay_optimization,"button 2 release");
    hold(mapper,frame,true,true);hold(mapper,frame,false,true);hold(mapper,frame,false,false);
    require(!mapper.parameters().analog && !mapper.parameters().hall.mode_enhancement
            && !mapper.parameters().hall.decay_optimization,"chord must suppress single actions");
    for(unsigned i=0;i<6;++i) for(unsigned j=i+1;j<6;++j)
        require(program_rgb[i].red!=program_rgb[j].red || program_rgb[i].green!=program_rgb[j].green
                || program_rgb[i].blue!=program_rgb[j].blue,"algorithm colors must be distinct");
    std::ifstream file(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto bank=std::make_unique<native_hall::ProgramBank>();
    require(native_hall::read_bank(bytes.data(),bytes.size(),*bank),"invalid bank");
    auto engine=std::make_unique<native_hall::Engine48>();engine->prepare(*bank);
    float peak=0;
    for(unsigned n=0;n<96000;++n) {
        if(n%96==0) {
            frame=default_frame();position(frame,Target::Program,float((n/96)%6+0.5f)/6);
            mapper.update(frame);engine->set_parameters(mapper.parameters());
        }
        float l,r;engine->process(0.05f*std::sin(n*0.12f),0.03f*std::cos(n*0.09f),l,r);
        require(std::isfinite(l) && std::isfinite(r) && std::abs(l)<4 && std::abs(r)<4,"controls/DSP stress");
        peak=std::max({peak,std::abs(l),std::abs(r)});
    }
    require(peak>1e-5f,"silent processing");
    std::cout<<"Universal controls: 0..3.3V / 0..5V / -5..5V / normalized ranges, inversion, "
                "ten targets, two buttons/chord, six RGB colors and stereo DSP stress passed\n";
}
