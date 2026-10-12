#pragma once
#include "../core/hall.hpp"
#include "fractional_delay.hpp"
#include <cmath>
#include <vector>

namespace cineol::floating {
// Hall B's signal graph, independent of the ARU and converter models.
// Rate=20480 is an offline algebra check; the product uses Hall48 only.
template<unsigned Rate> class Hall {
public:
    static constexpr unsigned sample_rate=Rate, program=2;
    static constexpr float rate_ratio=float(Rate)/native_hall::Hall::sample_rate;
    static constexpr std::array<unsigned,34> writers={
        3,4,7,8,11,12,15,16,19,23,26,30,33,34,37,38,46,
        53,54,57,58,61,62,65,66,69,73,76,80,83,84,87,88,96};
    static constexpr std::array<unsigned,48> readers={
        1,2,5,6,9,10,13,14,17,18,20,24,25,27,28,31,32,35,36,39,40,42,43,44,45,
        51,52,55,56,59,60,63,64,67,68,70,74,75,77,78,81,82,85,86,89,90,92,93};
    // Allocation and bank validation are setup operations, never audio operations.
    bool prepare(const native_hall::ProgramBank& bank) {
        ready_=false;
        if(!bank.programs[program].valid(program)) return false;
        bank_=&bank;compiler_.prepare(bank,program);
        const auto& p=bank.programs[program];
        std::array<unsigned,34> maximum{};
        // Reserve every possible predelay and modulation address before audio.
        // Coefficient-only controls cannot change a delay-line capacity.
        for(unsigned pre=0;pre<129;++pre) {
            auto offsets=p.offsets;
            for(unsigned j=0;j<p.predelay_count;++j) offsets[p.predelay_rows[j]]=p.predelay[pre][j];
            for(unsigned row:all_readers) {
                unsigned variations=1;
                for(unsigned j=0;j<(p.modulation_flags&15);++j) {
                    const unsigned a=unsigned(p.modulation_descriptors[5*j])|(unsigned(p.modulation_descriptors[5*j+1])<<8);
                    const unsigned mod_row=127-(a-0x4000)/4;
                    if(row==mod_row || row==mod_row+1) variations=256;
                }
                for(unsigned low=0;low<variations;++low) {
                    const unsigned offset=variations==1?offsets[row]:(offsets[row]&0x3f00)|low;
                    const auto binding=resolve(row,offset,offsets);
                    maximum[binding.source]=std::max(maximum[binding.source],unsigned(std::ceil(binding.delay*rate_ratio))+2);
                }
            }
        }
        for(unsigned i=0;i<writers.size();++i) lines_[i].samples.assign(std::max(2u,maximum[i]+1),0);
        ready_=true;reset();return true;
    }
    void reset() noexcept {
        for(auto& line:lines_) {std::fill(line.samples.begin(),line.samples.end(),0);line.position=0;}
        if(bank_) {compiler_.prepare(*bank_,program);compiler_.set_controls(controls_);}
        modulation_clock_=decay_clock_=0;
        if(ready_) compile();
    }
    void set_controls(const native_hall::Controls& controls) noexcept {
        controls_=controls;
        if(!ready_)return;
        compiler_.set_controls(controls);compile();
    }
    bool ready() const noexcept {return ready_;}
    size_t storage_bytes() const noexcept {
        size_t bytes=sizeof(*this);for(const auto& line:lines_)bytes+=line.samples.capacity()*sizeof(float);return bytes;
    }
    std::array<float,4> process(float left,float right) noexcept {
        if(!ready_)return {};
        if(!std::isfinite(left))left=0;
        if(!std::isfinite(right))right=0;
        std::array<float,4> output{};
        // Each group below is a sum/allpass/delay connection, collapsed from
        // the fixed Hall B row graph. There are no opcodes or ARU bit edges.
        write<3>(c_[0]*left);
        // Both captured inputs are available for causal high-order reads of
        // the short input cross-taps. Integral/native-rate reads are unchanged.
        write<53>(c_[50]*right);
        const float t1=tap<1>(),t2=tap<2>();
        write<4>(c_[1]*t1+c_[2]*t2);
        write<7>(c_[3]*t2+c_[4]*t1);
        const float t5=tap<5>(),t6=tap<6>();
        write<8>(c_[5]*t5+c_[6]*t6);
        write<11>(c_[7]*t6+c_[8]*t5);
        const float t9=tap<9>(),t10=tap<10>();
        write<12>(c_[9]*t9+c_[10]*t10);
        write<15>(c_[11]*t10+c_[12]*t9);
        const float t13=tap<13>(),t14=tap<14>();
        write<16>(c_[13]*t13+c_[14]*t14);
        const float x17=c_[15]*t14+c_[16]*t13;write<19>(x17);
        const float x20=c_[17]*tap<17>()+c_[18]*tap<18>()+c_[19]*x17;
        const float t20=tap<20>();write<23>(c_[20]*t20+c_[21]*x20);
        const float x24=c_[22]*x20+c_[23]*t20;write<26>(x24);
        output[1]=c_[24]*tap<24>()+c_[25]*tap<25>()+c_[26]*x24;
        const float t27=tap<27>(),t28=tap<28>();
        write<30>(c_[27]*t27+c_[28]*t28);write<33>(c_[29]*t28+c_[30]*t27);
        const float t31=tap<31>(),t32=tap<32>();
        write<34>(c_[31]*t31+c_[32]*t32);write<37>(c_[33]*t32+c_[34]*t31);
        const float t35=tap<35>();
        const float x37=feedback<36,38>(c_[35]*t35,c_[36]);
        write<38>(x37);
        const float x40=c_[37]*t35+c_[38]*x37+c_[39]*tap<39>();
        write<46>(feedback<40,46>(c_[41]*x40,c_[40]));
        output[0]=c_[42]*tap<42>()+c_[43]*tap<43>()+c_[44]*tap<44>()+c_[45]*tap<45>();

        const float t51=tap<51>(),t52=tap<52>();
        write<54>(c_[51]*t51+c_[52]*t52);write<57>(c_[53]*t52+c_[54]*t51);
        const float t55=tap<55>(),t56=tap<56>();
        write<58>(c_[55]*t55+c_[56]*t56);write<61>(c_[57]*t56+c_[58]*t55);
        const float t59=tap<59>(),t60=tap<60>();
        write<62>(c_[59]*t59+c_[60]*t60);write<65>(c_[61]*t60+c_[62]*t59);
        const float t63=tap<63>(),t64=tap<64>();
        write<66>(c_[63]*t63+c_[64]*t64);
        const float x67=c_[65]*t64+c_[66]*t63;write<69>(x67);
        const float x70=c_[67]*tap<67>()+c_[68]*tap<68>()+c_[69]*x67;
        const float t70=tap<70>();write<73>(c_[70]*t70+c_[71]*x70);
        const float x74=c_[72]*x70+c_[73]*t70;write<76>(x74);
        output[3]=c_[74]*tap<74>()+c_[75]*tap<75>()+c_[76]*x74;
        const float t77=tap<77>(),t78=tap<78>();
        write<80>(c_[77]*t77+c_[78]*t78);write<83>(c_[79]*t78+c_[80]*t77);
        const float t81=tap<81>(),t82=tap<82>();
        write<84>(c_[81]*t81+c_[82]*t82);write<87>(c_[83]*t82+c_[84]*t81);
        const float t85=tap<85>();
        const float x87=feedback<86,88>(c_[85]*t85,c_[86]);
        write<88>(x87);
        const float x90=c_[87]*t85+c_[88]*x87+c_[89]*tap<89>();
        write<96>(feedback<90,96>(c_[91]*x90,c_[90]));
        output[2]=c_[92]*tap<92>()+c_[93]*tap<93>()+c_[94]*tap<94>()+c_[95]*tap<95>();
        for(auto& line:lines_) if(++line.position==line.samples.size())line.position=0;
        update_control_clock(left,right);
        return output;
    }
private:
    // The last two reads are included separately to keep the connection list explicit.
    static constexpr std::array<unsigned,50> all_readers=[] {
        std::array<unsigned,50> result{};for(unsigned i=0;i<readers.size();++i)result[i]=readers[i];
        result[48]=94;result[49]=95;return result;
    }();
    struct Binding {unsigned source=0,delay=0;};
    struct Tap {unsigned source=0,whole=0;FractionalDelay fractional;};
    struct Line {std::vector<float> samples;unsigned position=0;};
    static constexpr unsigned writer_index(unsigned row) noexcept {
        for(unsigned i=0;i<writers.size();++i)if(writers[i]==row)return i;
        return 0;
    }
    static Binding resolve(unsigned row,unsigned offset,const std::array<uint16_t,100>& offsets) noexcept {
        Binding best{0,16385};
        for(unsigned i=0;i<writers.size();++i) {
            const unsigned writer=writers[i];
            unsigned distance=(offset-offsets[writer])&0x3fff;
            if(distance==0 && writer>=row)distance=16384;
            // Equal-address writes are resolved by their actual execution order.
            if(distance<best.delay || (distance==best.delay && writer>writers[best.source]))best={i,distance};
        }
        return best;
    }
    void bind(unsigned row) noexcept {
        const auto binding=resolve(row,compiler_.offsets()[row],compiler_.offsets());
        const float delay=binding.delay*rate_ratio;
        auto& tap=taps_[row];tap.source=binding.source;tap.whole=unsigned(delay);
        tap.fractional.set_fraction(delay-std::floor(delay));
    }
    template<unsigned Row> float tap() noexcept {
        const auto& t=taps_[Row];const auto& line=lines_[t.source];
        if(t.fractional.integral())return read(line,t.whole);
        float sum=0;
        for(unsigned i=0;i<6;++i)sum+=t.fractional.weight(i)*read(line,t.whole-2+i);
        return sum;
    }
    static float read(const Line& line,unsigned delay) noexcept {
        const unsigned size=unsigned(line.samples.size());
        const unsigned index=line.position>=delay?line.position-delay:line.position+size-delay;
        return line.samples[index];
    }
    template<unsigned Row,unsigned Writer> float feedback(float input,float gain) noexcept {
        const auto& t=taps_[Row];
        if(t.source==writer_index(Writer) && !t.fractional.integral() && t.whole==2) {
            // A 2.34375-frame recurrence includes a small current-sample term.
            // Solve it analytically; do not read an unwritten circular slot.
            const auto& line=lines_[t.source];float past=0;
            for(unsigned i=1;i<6;++i)past+=t.fractional.weight(i)*read(line,i);
            return (input+gain*past)/(1-gain*t.fractional.weight(0));
        }
        return input+gain*tap<Row>();
    }
    template<unsigned Row> void write(float value) noexcept {
        auto& line=lines_[writer_index(Row)];line.samples[line.position]=value;
    }
    void compile() noexcept {
        for(unsigned r=0;r<100;++r)c_[r]=compiler_.coefficients()[r]/32.0f;
        for(unsigned row:all_readers)bind(row);
    }
    void update_control_clock(float left,float right) noexcept {
        const auto& p=bank_->programs[program];
        const unsigned mode=unsigned(controls_.mode_enhancement)|(unsigned(controls_.decay_optimization)<<1);
        // Word conversion is a control-envelope observation only: audio never
        // goes through this quantizer or the integer signal network.
        const auto level=[](float v){return int16_t(std::lround(std::clamp(v,-1.0f,0.999969f)*32768));};
        compiler_.observe_decay(level(left),level(right),0);
        modulation_clock_+=p.modulation_rate_tenths[mode];
        if(modulation_clock_>=Rate*10) {
            modulation_clock_-=Rate*10;
            if(controls_.mode_enhancement) {
                compiler_.advance_modulation();
                for(unsigned j=0;j<(p.modulation_flags&15);++j) {
                    const unsigned a=unsigned(p.modulation_descriptors[5*j])|(unsigned(p.modulation_descriptors[5*j+1])<<8);
                    const unsigned row=127-(a-0x4000)/4;
                    for(unsigned k=0;k<2;++k) {c_[row+k]=compiler_.coefficients()[row+k]/32.0f;bind(row+k);}
                }
            }
        }
        decay_clock_+=p.level_rate_tenths[mode];
        if(decay_clock_>=Rate*10) {
            decay_clock_-=Rate*10;compiler_.poll_decay();
            for(unsigned j=0;j<p.loop_count;++j)for(unsigned k=0;k<3;++k) {
                const unsigned row=p.loop_rows[j]+k;c_[row]=compiler_.coefficients()[row]/32.0f;
            }
        }
    }
    const native_hall::ProgramBank* bank_=nullptr;
    native_hall::Hall compiler_; // Existing control tables/laws only; never processes audio.
    native_hall::Controls controls_{};
    std::array<Line,34> lines_;
    std::array<Tap,100> taps_{};
    std::array<float,100> c_{};
    unsigned modulation_clock_=0,decay_clock_=0;
    bool ready_=false;
};
using Hall48=Hall<48000>;
} // namespace cineol::floating
