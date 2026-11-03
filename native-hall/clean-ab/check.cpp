#include "engines.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <new>
#include <cstdlib>
static bool tracking=false;static unsigned allocations=0;
void* operator new(size_t n){if(tracking)++allocations;if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
static void require(bool v,const char*s){if(!v)throw std::runtime_error(s);}
template<class B>auto load(const char*path){std::ifstream f(path,std::ios::binary);std::vector<char>bytes((std::istreambuf_iterator<char>(f)),{});auto b=std::make_unique<B>();require(read_bank(bytes.data(),bytes.size(),*b),"invalid bank");return b;}
struct Stats {double sum[2]{},energy=0;unsigned n=0;void add(float l,float r){sum[0]+=l;sum[1]+=r;energy+=double(l)*l+double(r)*r;++n;}double ac()const {return std::sqrt(std::max(0.,energy/(2*n)-(sum[0]*sum[0]+sum[1]*sum[1])/(2.*n*n)));}};
int main(int argc,char**argv)try{
 require(argc==3 || argc==4,"usage: check ORIGINAL_BANK XL_BANK [METRICS_CSV]");auto a=load<native_hall::ProgramBank>(argv[1]);auto b=load<cineol::xl::Bank>(argv[2]);auto e=std::make_unique<cineol::clean_ab::Engines>();e->prepare(a.get(),b.get());
 std::ofstream csv;if(argc==4){csv.open(argv[3]);csv<<"program,mode,recipe,window,a_ac_dbfs,b_ac_dbfs\n";}
 double maximum=0;
 for(unsigned p=0;p<7;++p)for(unsigned mode:{0u,3u})for(unsigned recipe:{0u,1u}){
  tracking=true;e->select(p);tracking=false;
  if(p<3){native_hall::Controls c;c.mode_enhancement=mode&1;c.decay_optimization=mode&2;c.predelay_ms=native_hall::predelay_minima[cineol::clean_ab::indices[p]];c.modulation_period=2;c.modulation_step=1;c.optimization_response=5;e->original_controls(c);}
  else e->xl_controls(b->programs[cineol::clean_ab::indices[p]].controls.factory,mode&1,mode&2);
  tracking=true;for(unsigned n=0;n<48000;++n)e->process(0,0);tracking=false;
  uint32_t rng=recipe?991:17;std::array<Stats,3> sa{},sb{};
  const unsigned frames=argc==4?1464000:96000;
  tracking=true;
  for(unsigned n=0;n<frames;++n){float l=0,r=0;if(n<24000){rng=rng*1664525u+1013904223u;l=(recipe?.008f:.08f)*float(int32_t(rng))/2147483648.f;rng=rng*1664525u+1013904223u;r=(recipe?.008f:.08f)*float(int32_t(rng))/2147483648.f;}
   const auto v=e->process(l,r);for(float x:v)require(std::isfinite(x)&&std::abs(x)<4,"nonfinite or runaway output");
   maximum=std::max({maximum,double(std::abs(v[0]-v[2])),double(std::abs(v[1]-v[3]))});
   int w=n<24000?0:n>=984000&&n<1080000?1:n>=1368000?2:-1;if(w>=0){sa[w].add(v[0],v[1]);sb[w].add(v[2],v[3]);}
  }
  tracking=false;require(allocations==0,"audio/select allocated");
  if(argc==4)for(unsigned w=0;w<3;++w)csv<<p<<','<<mode<<','<<recipe<<','<<w<<','<<20*std::log10(std::max(sa[w].ac(),1e-18))<<','<<20*std::log10(std::max(sb[w].ac(),1e-18))<<'\n';
 }
 if constexpr(clean_native_hall::precision::bits==0)require(maximum==0,"zero-bit precision export differs from original");
 else require(maximum>0,"clean processing did not change audio");
 std::cout<<"Seven algorithms, four recipes each; finite output; selection/process allocations="<<allocations<<"; extra bits="<<clean_native_hall::precision::bits<<"; max A/B error="<<std::setprecision(12)<<maximum<<'\n';
 return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}
