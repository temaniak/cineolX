#include "../desktop/space_clock.hpp"
#include <cstdlib>
#include <iostream>
static void require(bool ok,const char* message) {if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
int main() {
    cineol::SpaceClock clock;clock.prepare();
    for(float ratio:{1.0f,.5f,.73f}) {
        clock.reset(ratio);std::array<cineol::SpaceClock::Quad,70> delay{};unsigned pos=0,calls=0,peak_at=0;float peak=0;
        for(unsigned n=0;n<4096;++n) {
            auto y=clock.process(n==0?1:0,0,ratio,[&](float left,float) {
                ++calls;auto value=delay[pos];delay[pos]={left,2*left,-left,-2*left};pos=(pos+1)%delay.size();return value;
            });
            if(std::abs(y[0])>peak){peak=std::abs(y[0]);peak_at=n;}
            require(y[1]==2*y[0] && y[2]==-y[0] && y[3]==-2*y[0],"quad phases or channel isolation differ");
        }
        std::cout<<"ratio="<<ratio<<" peak_at="<<peak_at<<" peak="<<peak<<" calls="<<calls<<'\n';
        require(std::abs(int(peak_at)-int(cineol::SpaceClock::latency))<=1,"Space changed compensated onset");
        require(std::abs(double(calls)-4096*ratio)<1.1,"clock ratio accumulated drift");
        if(ratio==1)require(peak==1,"nominal clock did not retain exact unity path");
    }
    clock.reset();std::array<cineol::SpaceClock::Quad,70> delay{};unsigned pos=0;double energy=0;
    for(unsigned n=0;n<96000;++n) {
        const float ratio=n<24000?1:n<72000?1-.5f*float(n-24000)/48000:.5f;
        const float x=.2f*std::sin(float(n)*.13f);
        const auto y=clock.process(x,-x,ratio,[&](float l,float r) {
            auto value=delay[pos];delay[pos]={l,r,l,r};pos=(pos+1)%delay.size();return value;
        });
        for(float v:y)require(std::isfinite(v) && std::abs(v)<.3f,"moving clock produced an unstable sample");
        energy+=y[0]*y[0];
    }
    require(energy>100,"moving clock lost the sustained signal");
    std::cout<<"Clock ratios, fixed onset, nominal unity, quad coherence and moving-clock signal pass\n";
}
