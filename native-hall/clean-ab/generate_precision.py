#!/usr/bin/env python3
"""Build an isolated, auditable fixed-point precision variant of the native core.
No production or dependency files are modified. Every semantic substitution
fails configuration if its expected source boundary has changed.
"""
from pathlib import Path
import argparse, re, json, hashlib
p=argparse.ArgumentParser();p.add_argument('source');p.add_argument('output');a=p.parse_args()
src=Path(a.source);out=Path(a.output)
def change(s,old,new,count=1):
    if s.count(old)!=count:raise RuntimeError(f'Expected {count} occurrences of {old!r}, got {s.count(old)}')
    return s.replace(old,new)
manifest={}
for folder in ('core','desktop'):
 for path in sorted((src/folder).iterdir()):
  if path.suffix not in ('.hpp','.cpp','.inc'):continue
  s=path.read_text();manifest[str(path.relative_to(src))]=hashlib.sha256(path.read_bytes()).hexdigest()
  s=re.sub(r'\bnative_hall\b','clean_native_hall',s);s=re.sub(r'\bcineol\b','clean_cineol',s)
  name=str(path.relative_to(src))
  if name in ('core/hall.hpp','core/hall.cpp','desktop/concert.hpp'):
   s=re.sub(r'\bint16_t\b','precision::Word',s);s=re.sub(r'\bint32_t\b','int64_t',s)
   if path.suffix=='.hpp':s=change(s,'#pragma once','#pragma once\n#include "'+('../core/' if folder=='desktop' else '')+'precision.hpp"')
   s=s.replace('-262144,262143','-262144*precision::scale,262144*precision::scale-1')
   s=s.replace('precision::Word(add(partial_)>>3)','precision::wrap(add(partial_)>>3)')
   s=s.replace('precision::Word(sum()>>3)','precision::wrap(sum()>>3)')
   if name=='core/hall.hpp':s=change(s,'decay_.observe(left,right,detectors);','decay_.observe(precision::control(left),precision::control(right),detectors);')
   if name=='core/hall.cpp':s=change(s,'decay_.observe(left,right,detectors);','decay_.observe(precision::control(left),precision::control(right),detectors);')
   if name=='desktop/concert.hpp':
    s=change(s,'namespace clean_cineol::xl {','namespace clean_cineol::xl {\nnamespace precision=clean_native_hall::precision;')
    s=change(s,'precision::Word control_output() const noexcept {return xreg_to_control_;}', 'int16_t control_output() const noexcept {return precision::control(xreg_to_control_);}')
    s=change(s,'void set_control_input(precision::Word value) noexcept {xreg_from_control_=value;}', 'void set_control_input(int16_t value) noexcept {xreg_from_control_=precision::Word(value)*precision::scale;}')
  if name=='desktop/controllers224.hpp':
   s=change(s,'void process(int16_t left,int16_t right,int16_t outputs[4],unsigned detectors=0)', 'void process(precision::Word left,precision::Word right,precision::Word outputs[4],unsigned detectors=0)')
   s=change(s,'previous_input_={{left,right}}','previous_input_={{precision::control(left),precision::control(right)}}')
   s=change(s,'const int16_t value=2u*(sample_rate*10-before)<rate?left:right;', 'const precision::Word value=2u*(sample_rate*10-before)<rate?left:right;')
  if name in ('desktop/adc22448.hpp','desktop/adc_xl48.hpp'):
   start=s.index('    static int16_t adc(double sample) noexcept {');end=s.index('    static unsigned detectors',start)
   s=s[:start]+'''    static clean_native_hall::precision::Word adc(double sample) noexcept {
        return clean_native_hall::precision::adc(sample);
    }
'''+s[end:]
   s=change(s,'#pragma once','#pragma once\n#include "../core/precision.hpp"')
  if name=='core/engine48.hpp':
   start=s.index('    static int16_t adc(float sample,bool gain_ranging=true) noexcept {');end=s.index('    static unsigned detectors',start)
   s=s[:start]+'''    static precision::Word adc(float sample,bool gain_ranging=true) noexcept {
        return precision::adc(sample,gain_ranging);
    }
'''+s[end:]
   s=s.replace('int16_t words[2]','precision::Word words[2]').replace('int16_t clean=','precision::Word clean=').replace('words[c]=int16_t(std::lround','words[c]=precision::Word(std::llround').replace('int16_t four[4]','precision::Word four[4]')
   s=s.replace('dac(four[c])/32768.0f','float(dac(four[c])/(32768.0*precision::scale))')
   s=change(s,'static int16_t adc(float sample,bool gain_ranging=true) noexcept {','static precision::Word adc(float sample,bool gain_ranging=true) noexcept {')
   start=s.index('    static int16_t dac(int16_t sample) noexcept {');end=s.index('\nprivate:',start)
   s=s[:start]+'''    static precision::Word dac(precision::Word sample) noexcept {
        return precision::dac(sample);
    }
'''+s[end:]
  if name=='desktop/input_xl48.hpp':
   s=change(s,'std::array<int16_t,2> clean{},bare{};', 'std::array<clean_native_hall::precision::Word,2> clean{},bare{};')
  if name=='desktop/concert48.hpp':
   s=change(s,'int16_t words[2];','clean_native_hall::precision::Word words[2];')
   s=change(s,'words[c]=int16_t(std::lround','words[c]=clean_native_hall::precision::Word(std::llround')
   s=change(s,'(int16_t bus) noexcept {','(clean_native_hall::precision::Word bus) noexcept {')
   s=change(s,'clean_native_hall::Engine48::dac(bus)/32768.0f','float(clean_native_hall::Engine48::dac(bus)/(32768.0*clean_native_hall::precision::scale))')
  dst=out/name;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_text(s)
(out/'core/precision.hpp').write_text(r'''#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#ifndef CINEOL_CLEAN_FRACTION_BITS
#define CINEOL_CLEAN_FRACTION_BITS 12
#endif
namespace clean_native_hall::precision {
using Word=int64_t;
inline constexpr unsigned bits=CINEOL_CLEAN_FRACTION_BITS;
static_assert(bits==0 || bits==12);
inline constexpr Word scale=Word(1)<<bits;
inline Word wrap(Word value) noexcept {
    constexpr uint64_t width=uint64_t(1)<<(16+bits),half=width/2;
    const uint64_t u=uint64_t(value)&(width-1);
    return u>=half?Word(u)-Word(width):Word(u);
}
inline int16_t control(Word value) noexcept {
    return int16_t(std::clamp<Word>(value>>bits,-32768,32767));
}
inline Word adc(double sample,bool ranging=true) noexcept {
    const double a=std::abs(sample)*5;
    const unsigned range=!ranging?0:a<.56?3:a<1.12?2:a<2.24?1:0;
    const double volts=sample*5.0;
    const double v=std::clamp(volts*double(1u<<range)/5.0*2047.0,-2048.,2047.);
    return wrap(Word(std::llround(v*scale))*Word(1u<<(4-range)));
}
inline Word dac(Word sample) noexcept {
    if constexpr(bits!=0)return sample;
    unsigned range=0;Word value=sample;
    while(range<3 && value>=-16384 && value<=16383){value=value*2+1;++range;}
    return (value>>4)*Word(1u<<(4-range));
}
}
''')
(out/'source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
