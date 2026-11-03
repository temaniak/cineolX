#include "engines.hpp"
#include <fstream>
#include <vector>
#include <iostream>
#include <iomanip>
#include <cstdlib>
static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n){if(tracking)++allocations;if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{if(tracking&&p)++releases;std::free(p);}
void operator delete[](void*p)noexcept{::operator delete(p);}
void operator delete(void*p,size_t)noexcept{::operator delete(p);}
void operator delete[](void*p,size_t)noexcept{::operator delete(p);}
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
template<class E,class B>void setup(E&e,const B&b,unsigned lf,unsigned mid,unsigned treble,unsigned size,unsigned mode,float amount){
 const auto&d=b.programs[2];e.activate(d.template settings<E::graph_id>());e.set_modulation(d.modulation,d.initial_modulation,true);e.set_dynamics(d.dynamics,d.initial_dynamics);e.set_chorus_reference(uint8_t(d.chorus*8));
 auto raw=d.controls.factory;raw[0]=lf;raw[1]=mid;raw[3]=treble;raw[43]=size;raw[42]=uint8_t((raw[42]&~0xc1u)|(mode&1?64:0)|(mode&2?128:0)|(mode&4?1:0));
 d.resolve_controls(raw);e.set_native_controls(d.template settings<E::graph_id>(),d.controls,raw,mode&2);e.set_chorus(raw[8]);e.enable_modulation(mode&1);e.set_global(0,1,1,0,2);e.set_dark_stability(amount);
}
template<class E,class B>void campaign(E&e,const B&b,std::ostream&csv,unsigned precision){
 unsigned cases=0;
 for(auto decay:std::array<std::array<unsigned,2>,6>{{{192,176},{208,184},{240,232},{248,248},{248,32},{32,248}}})
 for(unsigned tone:{48u,208u})for(unsigned size:{0u,128u,254u})for(unsigned mode:{0u,3u}){
  tracking=true;setup(e,b,decay[0],decay[1],tone,size,mode,0);float a,c;for(unsigned n=0;n<48000;++n)e.process(0,0,a,c,true);tracking=false;
  uint32_t rng=17;double first=0,last=0;
  for(unsigned sec=0;sec<45;++sec){long double sum[2]{},sq[2]{};tracking=true;
   for(unsigned n=0;n<48000;++n){float l=0,r=0;if(sec==0&&n<9600){rng=rng*1664525u+1013904223u;l=.03f*float(int32_t(rng))/2147483648.f;rng=rng*1664525u+1013904223u;r=.03f*float(int32_t(rng))/2147483648.f;}e.process(l,r,a,c,true);check(std::isfinite(a)&&std::isfinite(c)&&std::abs(a)<4&&std::abs(c)<4,"nonfinite/overflow");sum[0]+=a;sum[1]+=c;sq[0]+=double(a)*a;sq[1]+=double(c)*c;}
   tracking=false;const double db=10*std::log10(std::max(1e-36L,(sq[0]/48000-std::pow(sum[0]/48000,2)+sq[1]/48000-std::pow(sum[1]/48000,2))/2));if(sec==10)first=db;if(sec==44)last=db;
   csv<<precision<<','<<decay[0]<<','<<decay[1]<<','<<tone<<','<<size<<','<<mode<<",0,"<<sec<<','<<db<<','<<e.dark_stability_gain()<<'\n';
  }
  check(last<-80 || last<first-3,"Stable endpoint still grows/holds");++cases;csv.flush();
 }
 std::cout<<"precision="<<precision<<" stable long-tail cases="<<cases<<" passed\n";std::cout.flush();
 // Listen-test settings: a continuous approach to the original runaway.
 for(float amount:{0.f,.25f,.5f,.75f,1.f}){
  tracking=true;setup(e,b,192,176,208,128,0,amount);float a,c;for(unsigned n=0;n<48000;++n)e.process(0,0,a,c,true);tracking=false;uint32_t rng=17;
  for(unsigned sec=0;sec<45;++sec){long double sum[2]{},sq[2]{};tracking=true;for(unsigned n=0;n<48000;++n){float l=0,r=0;if(sec==0&&n<9600){rng=rng*1664525u+1013904223u;l=.03f*float(int32_t(rng))/2147483648.f;rng=rng*1664525u+1013904223u;r=.03f*float(int32_t(rng))/2147483648.f;}e.process(l,r,a,c,true);check(std::isfinite(a)&&std::isfinite(c),"slider output");sum[0]+=a;sum[1]+=c;sq[0]+=double(a)*a;sq[1]+=double(c)*c;}tracking=false;const auto db=10*std::log10(std::max(1e-36L,(sq[0]/48000-std::pow(sum[0]/48000,2)+sq[1]/48000-std::pow(sum[1]/48000,2))/2));csv<<precision<<",192,176,208,128,0,"<<amount<<','<<sec<<','<<db<<','<<e.dark_stability_gain()<<'\n';}csv.flush();
 }
}
template<class E,class B>void edges(E&e,const B&b){
 auto reference=std::make_unique<E>();reference->prepare({});setup(e,b,32,32,208,128,0,0);setup(*reference,b,32,32,208,128,0,1);
 tracking=true;for(unsigned n=0;n<96000;++n){const float input=n<24000?.03f*std::sin(float(n)*.117f):0;float a,c,x,y;e.process(input,-input,a,c,true);reference->process(input,-input,x,y,true);check(a==x&&c==y,"Stable changed ordinary short-decay sound");}tracking=false;
 check(e.dark_stability_gain()==(1u<<24),"ordinary settings did not bypass");setup(e,b,192,176,208,128,3,1);uint32_t previous=e.dark_stability_gain(),largest=0;double tail=0;
 tracking=true;for(unsigned n=0;n<480000;++n){if(n%12000==0){const auto memory=e.concert().memory();e.set_dark_stability(float((n/12000)%5)/4);check(e.concert().memory()==memory,"stability setter cleared tail");}const float input=n<240000?.03f*std::sin(float(n)*.117f):0;float a,c;e.process(input,-input,a,c,true);check(std::isfinite(a)&&std::isfinite(c),"moving slider nonfinite");if(n>240000)tail+=double(a)*a+double(c)*c;const auto gain=e.dark_stability_gain();largest=std::max(largest,gain>previous?gain-previous:previous-gain);previous=gain;}tracking=false;
 check(largest<=11000&&tail>1e-7,"slew discontinuity or lost tail");std::cout<<"ordinary identity, moving slider memory retention, Q24 max step="<<largest<<" passed\n";
}
int main(int argc,char**argv)try{
 check(argc==3,"XL_BANK CSV|--edges");std::ifstream f(argv[1],std::ios::binary);std::vector<char>bytes((std::istreambuf_iterator<char>(f)),{});auto b=std::make_unique<cineol::xl::Bank>();check(cineol::xl::read_bank(bytes.data(),bytes.size(),*b),"bank");auto clean=std::make_unique<clean_cineol::xl::Bank>();std::memcpy(clean.get(),b.get(),sizeof(*b));
 auto a=std::make_unique<cineol::xl::Native48<cineol::xl::Graph::dark_hall>>();auto c=std::make_unique<clean_cineol::xl::Native48<clean_cineol::xl::Graph::dark_hall>>();a->prepare({});c->prepare({});
 if(std::string(argv[2])=="--edges"){edges(*a,*b);edges(*c,*clean);check(!allocations&&!releases,"edge audio allocations");std::cout<<"audio allocations=0 releases=0\n";return 0;}
 std::ofstream csv(argv[2]);csv<<std::setprecision(12)<<"precision,lf,mid,treble,size,mode,original,second,dbfs,gain_q24\n";
 campaign(*a,*b,csv,0);campaign(*c,*clean,csv,12);check(!allocations&&!releases,"audio allocations");std::cout<<"audio allocations=0 releases=0\n";
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}
