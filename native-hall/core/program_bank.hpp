#pragma once
#include "profile.hpp"

namespace native_hall {
// Six original 224 v4.4 programs, in panel order. ROM5 supplies Hall A.
inline constexpr unsigned program_count=6;
inline constexpr std::array<uint8_t,program_count> program_identities={1,2,4,8,16,32};
inline constexpr std::array<uint8_t,program_count> program_networks={0,1,0,2,1,3};
inline constexpr const char* program_names[program_count]={"Small Hall B","Vocal Plate","Large Hall B",
    "Acoustic Chamber","Percussion Plate A","Small Hall A"};
inline constexpr std::array<int,program_count> predelay_minima={24,0,24,25,0,24};
// A column may feed several rows. These are coefficient data, never opcodes.
template<unsigned Values,unsigned Columns,unsigned Rows> struct CoefficientTable {
    uint8_t columns=0,count=0;
    std::array<uint8_t,Rows> rows{},column{};
    std::array<std::array<int8_t,Columns>,Values> values{};
    bool valid() const noexcept {
        if(columns>Columns || count>Rows) return false;
        for(unsigned i=0;i<count;++i) if(rows[i]>=100 || column[i]>=columns) return false;
        for(const auto& v:values) for(unsigned i=0;i<columns;++i) if(v[i]<-63 || v[i]>63) return false;
        return true;
    }
    void apply(unsigned index,std::array<int8_t,100>& coefficients) const noexcept {
        const auto& v=values[index];
        for(unsigned i=0;i<count;++i) coefficients[rows[i]]=v[column[i]];
    }
};
struct ProgramProfile {
    std::array<int8_t,100> coefficients{};
    std::array<uint16_t,100> offsets{};
    CoefficientTable<1024,16,24> tail;
    CoefficientTable<32,4,4> crossover,treble;
    CoefficientTable<72,8,8> depth;
    CoefficientTable<64,18,18> diffusion;
    std::array<std::array<uint16_t,4>,129> predelay{};
    std::array<uint8_t,4> predelay_rows{},loop_rows{};
    std::array<uint8_t,10> modulation_descriptors{};
    std::array<uint16_t,4> level_rate_tenths{},modulation_rate_tenths{};
    DecayState initial_decay{};
    uint16_t modulation_index=0;
    uint8_t identity=1,network=0,predelay_count=0,loop_count=0,modulation_flags=0;
    uint8_t modulation_period=1,modulation_hold=32,modulation_step=4,modulation_mask=0;
    uint8_t initial_mod_divider=1,initial_random_divider=8,initial_random_hold=1,decay_amount=5;
    bool valid(unsigned index) const noexcept {
        if(identity!=program_identities[index] || network!=program_networks[index] || predelay_count>4 || loop_count>4
           || (modulation_flags&15)>2 || (modulation_flags&0x40) || modulation_index>=4096
           || !modulation_period || !modulation_hold || !modulation_step || !initial_mod_divider
           || !initial_random_divider || !initial_random_hold || !decay_amount) return false;
        if(!tail.valid() || !crossover.valid() || !treble.valid() || !depth.valid() || !diffusion.valid()) return false;
        for(auto c:coefficients) if(c<-63 || c>63) return false;
        for(auto o:offsets) if(o>=16384) return false;
        for(const auto& taps:predelay) for(auto o:taps) if(o>=16384) return false;
        for(unsigned i=0;i<predelay_count;++i) if(predelay_rows[i]>=100) return false;
        for(unsigned i=0;i<loop_count;++i) if(loop_rows[i]>=98) return false;
        for(auto r:level_rate_tenths) if(r<100 || r>2000) return false;
        for(auto r:modulation_rate_tenths) if(r<1000 || r>30000) return false;
        for(unsigned i=0;i<(modulation_flags&15);++i) {
            unsigned a=unsigned(modulation_descriptors[i*5]) | unsigned(modulation_descriptors[i*5+1])<<8;
            if(a<0x4077 || a>0x41ff || (a&3)!=3) return false;
            unsigned row=127-(a-0x4000)/4;
            if(row+1>=100) return false;
        }
        return true;
    }
};
struct ProgramBank {
    std::array<ProgramProfile,program_count> programs{};
    std::array<uint8_t,4096> modulation_sequence{};
};
static_assert(sizeof(ProgramProfile)==19894 && sizeof(ProgramBank)==123460,"bank ABI changed; bump format version");
struct BankHeader {
    char magic[8]={'B','A','N','K','2','2','4',0};
    uint32_t version=1,size=sizeof(ProgramBank),checksum=0;
};
static_assert(sizeof(BankHeader)==20,"bank header ABI changed");
inline bool read_bank(const void* data,size_t size,ProgramBank& result) noexcept {
    if(!data || size!=sizeof(BankHeader)+sizeof(ProgramBank)) return false;
    BankHeader h;std::memcpy(&h,data,sizeof h);const BankHeader expected;
    const auto* payload=static_cast<const uint8_t*>(data)+sizeof h;
    if(std::memcmp(h.magic,expected.magic,8) || h.version!=expected.version || h.size!=sizeof(ProgramBank)
        || h.checksum!=profile_checksum(payload,sizeof(ProgramBank))) return false;
    std::memcpy(&result,payload,sizeof result);
    for(unsigned i=0;i<program_count;++i) if(!result.programs[i].valid(i)) return false;
    return true;
}
} // namespace native_hall
