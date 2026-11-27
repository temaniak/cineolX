#pragma once
#include "profile.hpp"
#include <algorithm>
#include <cstddef>

namespace native_hall {
// CALL followed by the program's button or simultaneous button pair.
inline constexpr unsigned legacy_program_count=6;
inline constexpr unsigned program_count=9;
inline constexpr unsigned maximum_modulation_taps=9;
inline constexpr unsigned chorus_program=8;
inline constexpr std::array<uint8_t,program_count> program_identities={1,2,4,8,16,32,9,33,12};
inline constexpr std::array<uint8_t,program_count> program_networks={0,1,0,2,1,3,4,5,6};
inline constexpr const char* program_names[program_count]={"Small Hall B","Vocal Plate","Large Hall B",
    "Acoustic Chamber","Percussion Plate A","Small Hall A","Room A","Constant Density Plate A","Chorus A"};
inline constexpr std::array<int,program_count> predelay_minima={24,0,24,25,0,24,24,5,0};
// The v4.4 panel caps both Plates at 107 ms, before the input-delay read
// wraps into other delay lines. Retain the existing supported Hall/Chamber
// ranges and bank/host layouts; old Plate values above the cap resolve here.
inline constexpr std::array<int,program_count> predelay_spans={128,107,128,128,107,128,231,180,253};
inline int resolve_predelay(unsigned program,int milliseconds) noexcept {
    program=std::min(program,program_count-1);
    return std::clamp(milliseconds,predelay_minima[program],predelay_minima[program]+predelay_spans[program]);
}
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
// The v2 cache stores these members directly. A flat record gives the
// additional fields the same offsets with Apple Clang and MSVC.
struct ProgramProfile {
    std::array<int8_t,100> coefficients{};
    std::array<uint16_t,100> offsets{};
    CoefficientTable<1024,16,24> tail;
    CoefficientTable<32,6,6> crossover,treble;
    CoefficientTable<72,8,8> depth;
    CoefficientTable<64,18,18> diffusion;
    std::array<std::array<uint16_t,4>,256> predelay{};
    std::array<uint8_t,4> predelay_rows{},loop_rows{};
    std::array<uint8_t,5*maximum_modulation_taps> modulation_descriptors{};
    std::array<uint16_t,4> level_rate_tenths{},modulation_rate_tenths{};
    DecayState initial_decay{};
    uint16_t modulation_index=0;
    uint8_t identity=1,network=0,predelay_count=0,loop_count=0,modulation_flags=0;
    uint8_t modulation_period=1,modulation_hold=32,modulation_step=4,modulation_mask=0;
    uint8_t initial_mod_divider=1,initial_random_divider=8,initial_random_hold=1,decay_amount=5;
    // Bass/Mid/Crossover/Treble (voice levels in Chorus), Depth, Pre-delay, Diffusion.
    std::array<uint8_t,7> factory{};
    std::array<uint8_t,1024> decay_means{};
    std::array<uint8_t,4> loop_types{};
    std::array<uint8_t,2*maximum_modulation_taps> modulation_negative{};
    int8_t modulation_coefficient(unsigned tap,unsigned side,int magnitude) const noexcept {
        return int8_t(modulation_negative[2*tap+side]?-magnitude:magnitude);
    }
    bool valid(unsigned index) const noexcept {
        if(identity!=program_identities[index] || network!=program_networks[index] || predelay_count>4 || loop_count>4
           || (modulation_flags&15)>maximum_modulation_taps || (modulation_flags&0x40) || modulation_index>=4096
           || !modulation_period || !modulation_hold || !modulation_step || !initial_mod_divider
           || !initial_random_divider || !initial_random_hold || (loop_count && !decay_amount)) return false;
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
    unsigned available_programs=program_count;
};
static_assert(sizeof(ProgramProfile)==22134 && sizeof(ProgramBank)==203308,"bank ABI changed; bump format version");
static_assert(offsetof(ProgramProfile,factory)==21081 && offsetof(ProgramBank,available_programs)==203304,
              "v2 bank field offsets changed");
struct BankHeader {
    char magic[8]={'B','A','N','K','2','2','4',0};
    uint32_t version=2,size=sizeof(ProgramBank),checksum=0;
};
static_assert(sizeof(BankHeader)==20,"bank header ABI changed");
// The v1 cache keeps its original six-program record layout.
struct LegacyProgramProfile {
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
};
inline constexpr size_t legacy_bank_size=sizeof(LegacyProgramProfile)*legacy_program_count+4096;
static_assert(sizeof(LegacyProgramProfile)==19894 && legacy_bank_size==123460,"legacy bank ABI changed");

inline void upgrade_profile(const LegacyProgramProfile& source,ProgramProfile& dest) noexcept {
    dest={};
    dest.coefficients=source.coefficients;dest.offsets=source.offsets;
    dest.tail=source.tail;
    const auto upgrade_filter=[](const auto& from,auto& to) {
        to.columns=from.columns;to.count=from.count;
        std::copy(from.rows.begin(),from.rows.end(),to.rows.begin());
        std::copy(from.column.begin(),from.column.end(),to.column.begin());
        for(unsigned i=0;i<from.values.size();++i)
            std::copy(from.values[i].begin(),from.values[i].end(),to.values[i].begin());
    };
    upgrade_filter(source.crossover,dest.crossover);upgrade_filter(source.treble,dest.treble);
    dest.depth=source.depth;dest.diffusion=source.diffusion;
    std::copy(source.predelay.begin(),source.predelay.end(),dest.predelay.begin());
    dest.predelay_rows=source.predelay_rows;dest.loop_rows=source.loop_rows;
    std::copy(source.modulation_descriptors.begin(),source.modulation_descriptors.end(),dest.modulation_descriptors.begin());
    dest.level_rate_tenths=source.level_rate_tenths;dest.modulation_rate_tenths=source.modulation_rate_tenths;
    dest.initial_decay=source.initial_decay;dest.modulation_index=source.modulation_index;
    dest.identity=source.identity;dest.network=source.network;dest.predelay_count=source.predelay_count;
    dest.loop_count=source.loop_count;dest.modulation_flags=source.modulation_flags;
    dest.modulation_period=source.modulation_period;dest.modulation_hold=source.modulation_hold;
    dest.modulation_step=source.modulation_step;dest.modulation_mask=source.modulation_mask;
    dest.initial_mod_divider=source.initial_mod_divider;dest.initial_random_divider=source.initial_random_divider;
    dest.initial_random_hold=source.initial_random_hold;dest.decay_amount=source.decay_amount;
}
inline bool read_bank(const void* data,size_t size,ProgramBank& result) noexcept {
    if(!data || size<sizeof(BankHeader)) return false;
    BankHeader h;std::memcpy(&h,data,sizeof h);const BankHeader expected;
    const auto* payload=static_cast<const uint8_t*>(data)+sizeof h;
    const bool legacy=h.version==1 && h.size==legacy_bank_size;
    if(std::memcmp(h.magic,expected.magic,8) || (!legacy && (h.version!=expected.version || h.size!=sizeof(ProgramBank)))
        || h.size!=size-sizeof h || h.checksum!=profile_checksum(payload,h.size)) return false;
    if(legacy) {
        for(unsigned i=0;i<legacy_program_count;++i) {
            LegacyProgramProfile profile;
            std::memcpy(&profile,payload+i*sizeof profile,sizeof profile);
            if((profile.modulation_flags&15)>2 || !profile.crossover.valid() || !profile.treble.valid()) return false;
            upgrade_profile(profile,result.programs[i]);
        }
        std::memcpy(result.modulation_sequence.data(),payload+legacy_program_count*sizeof(LegacyProgramProfile),result.modulation_sequence.size());
        result.available_programs=legacy_program_count;
    } else {
        std::memcpy(&result,payload,sizeof result);
        if(result.available_programs!=program_count) return false;
    }
    for(unsigned i=0;i<result.available_programs;++i) if(!result.programs[i].valid(i)) return false;
    return true;
}
} // namespace native_hall
