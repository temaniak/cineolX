#include "engines.hpp"
#include "../desktop/runtime.hpp"
#include <clean/desktop/runtime.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <new>
#include <cstdlib>
#include <vector>
static bool tracking=false;static unsigned allocations=0,releases=0;
void* operator new(size_t n){if(tracking)++allocations;if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{if(tracking&&p)++releases;std::free(p);}
void operator delete[](void*p)noexcept{::operator delete(p);}
void operator delete(void*p,size_t)noexcept{::operator delete(p);}
void operator delete[](void*p,size_t)noexcept{::operator delete(p);}
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
template<class B>auto load(const char*p){std::ifstream f(p,std::ios::binary);std::vector<char>bytes((std::istreambuf_iterator<char>(f)),{});auto bank=std::make_unique<B>();check(read_bank(bytes.data(),bytes.size(),*bank),"bank");return bank;}
struct Stats{long double sum[2]{},sq[2]{};unsigned n=0;void add(float a,float b){sum[0]+=a;sum[1]+=b;sq[0]+=double(a)*a;sq[1]+=double(b)*b;++n;}double db()const{return 10*std::log10(std::max(1e-36L,(sq[0]/n-std::pow(sum[0]/n,2)+sq[1]/n-std::pow(sum[1]/n,2))/2));}};
int main(int argc,char**argv)try{
 check(argc==3||argc==4,"ORIGINAL_BANK XL_BANK [METRICS_DIRECTORY]");auto original=load<native_hall::ProgramBank>(argv[1]);auto xl=load<cineol::xl::Bank>(argv[2]);auto clean=std::make_unique<clean_native_hall::ProgramBank>();auto cleanxl=std::make_unique<clean_cineol::xl::Bank>();static_assert(sizeof(*clean)==sizeof(*original)&&sizeof(*cleanxl)==sizeof(*xl));std::memcpy(clean.get(),original.get(),sizeof(*clean));std::memcpy(cleanxl.get(),xl.get(),sizeof(*cleanxl));
 auto a=std::make_unique<native_hall::DesktopEngine48>();auto b=std::make_unique<clean_native_hall::DesktopEngine48>();auto ax=std::make_unique<cineol::xl::Runtime>();auto bx=std::make_unique<clean_cineol::xl::Runtime>();a->prepare_audio();b->prepare_audio();
 std::filesystem::path folder;std::ofstream csv;if(argc==4){folder=argv[3];std::filesystem::create_directories(folder);csv.open(folder/"catalog.csv");csv<<std::setprecision(12)<<"program,mode,window,original_dbfs,clean_dbfs,max_difference\n";}
 double maximum=0;
 for(unsigned p=0;p<28;++p)for(unsigned mode:{0u,3u}){
  tracking=true;
  if(p<6){a->activate(*original,p);b->activate(*clean,p);native_hall::Parameters v;v.program=p;v.hall.mode_enhancement=mode&1;v.hall.decay_optimization=mode&2;v.hall.predelay_ms=native_hall::predelay_minima[p];v.hall.modulation_period=2;v.hall.modulation_step=1;v.hall.optimization_response=5;clean_native_hall::Parameters w;std::memcpy(&w,&v,sizeof(v));a->set_parameters(v);b->set_parameters(w);}
  else{ax->select(*xl,p-6);bx->select(*cleanxl,p-6);auto raw=xl->programs[p-6].controls.factory;ax->controls(raw,mode&1,0,1,1,0,2,mode&2);bx->controls(raw,mode&1,0,1,1,0,2,mode&2);}
  tracking=false;
  auto process=[&](float l,float r){std::array<float,4>v{};if(p<6){a->process(l,r,v[0],v[1],true);b->process(l,r,v[2],v[3],true);}else{ax->process(l,r,v[0],v[1],true);bx->process(l,r,v[2],v[3],true);}return v;};
  tracking=true;for(unsigned n=0;n<48000;++n)process(0,0);tracking=false;
  std::array<Stats,3>sa{},sb{};std::vector<std::array<float,4>>capture(argc==4?12000:0);uint32_t rng=17;const unsigned frames=argc==4?1464000:96000;double difference=0;
  tracking=true;
  for(unsigned n=0;n<frames;++n){float l=0,r=0;if(n<24000){rng=rng*1664525u+1013904223u;l=.08f*float(int32_t(rng))/2147483648.f;rng=rng*1664525u+1013904223u;r=.08f*float(int32_t(rng))/2147483648.f;}const auto v=process(l,r);for(float x:v)check(std::isfinite(x)&&std::abs(x)<4,"invalid output");difference=std::max({difference,std::abs(double(v[0])-v[2]),std::abs(double(v[1])-v[3])});const int w=n<24000?0:n>=984000&&n<1080000?1:n>=1368000?2:-1;if(w>=0){sa[w].add(v[0],v[1]);sb[w].add(v[2],v[3]);}if(argc==4&&n>=12000&&n<24000)capture[n-12000]=v;}
  tracking=false;check(allocations==0&&releases==0,"audio/select allocation");maximum=std::max(maximum,difference);
  if(argc==4){for(unsigned w=0;w<3;++w)csv<<p<<','<<mode<<','<<w<<','<<sa[w].db()<<','<<sb[w].db()<<','<<difference<<'\n';csv.flush();std::ofstream f(folder/(std::to_string(p)+"-"+std::to_string(mode)+".f32"),std::ios::binary);f.write(reinterpret_cast<const char*>(capture.data()),std::streamsize(capture.size()*sizeof(capture[0])));}
 }
 if constexpr(clean_native_hall::precision::bits==0)check(maximum==0,"zero-bit export differs");
 std::cout<<"28 algorithms x 2 modes; extra_bits="<<clean_native_hall::precision::bits<<" allocations="<<allocations<<" releases="<<releases<<" maximum_difference="<<std::setprecision(12)<<maximum<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}
