#!/usr/bin/env python3
"""Small deterministic fixtures and WAV measurements; no user audio is modified."""
import argparse
import json
import struct
from pathlib import Path
import numpy as np

RATE=48000
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/pitch-lab/reaper-bench'

def write_wave(path,a):
    a=np.asarray(a,dtype='<f4');channels=a.shape[1]
    data=a.tobytes();fmt=struct.pack('<HHIIHH',3,channels,RATE,RATE*channels*4,channels*4,32)
    Path(path).write_bytes(b'RIFF'+struct.pack('<I',36+len(data))+b'WAVEfmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',len(data))+data)

def read_wave(path):
    data=Path(path).read_bytes();assert data[:4] in (b'RIFF',b'RF64') and data[8:12]==b'WAVE'
    position=12;fmt=None;raw=None
    while position+8<=len(data):
        key=data[position:position+4];size=struct.unpack_from('<I',data,position+4)[0];chunk=data[position+8:position+8+size]
        if key==b'fmt ':fmt=chunk
        if key==b'data':raw=chunk
        position+=8+size+(size&1)
    encoding,channels,rate,_,_,bits=struct.unpack_from('<HHIIHH',fmt)
    if encoding==65534:encoding=struct.unpack_from('<H',fmt,24)[0]
    if encoding==3:a=np.frombuffer(raw,dtype='<f4' if bits==32 else '<f8').astype(float)
    elif bits==16:a=np.frombuffer(raw,dtype='<i2').astype(float)/32768
    elif bits==24:
        b=np.frombuffer(raw,dtype=np.uint8).reshape(-1,3);v=b[:,0].astype(np.int32)|(b[:,1].astype(np.int32)<<8)|(b[:,2].astype(np.int32)<<16);a=((v^0x800000)-0x800000)/8388608
    else:a=np.frombuffer(raw,dtype='<i4').astype(float)/2147483648
    return rate,a.reshape(-1,channels)

def fixtures():
    WORK.mkdir(parents=True,exist_ok=True);t=np.arange(RATE*8)/RATE
    tone=np.zeros((len(t),2));active=(t>=.5)&(t<2.5)
    envelope=np.zeros_like(t);envelope[active]=np.minimum(1,(t[active]-.5)/.01)*np.minimum(1,(2.5-t[active])/.01)
    tone[:,0]=.08*np.sin(2*np.pi*440*t)*envelope;tone[:,1]=.06*np.sin(2*np.pi*660*t)*envelope
    write_wave(WORK/'stimulus.wav',tone)
    rng=np.random.default_rng(17);a=np.zeros_like(tone)
    for onset in (.5,1.5,2.5):
        u=t-onset;mask=(u>=0)&(u<.18);phase=2*np.pi*(180*u+35*(1-np.exp(-30*np.maximum(u,0))))
        a[mask,0]+=.14*np.exp(-u[mask]*40)*(.6*np.sin(phase[mask])+.4*rng.uniform(-1,1,int(mask.sum())))
        a[mask,1]=a[mask,0]*.65
    write_wave(WORK/'transients.wav',a)
    impulse=np.zeros_like(tone);impulse[int(.5*RATE)]=(.1,.07);write_wave(WORK/'impulse.wav',impulse)
    noise=np.zeros_like(tone);m=(t>=.5)&(t<1);noise[m,0]=rng.uniform(-.08,.08,int(m.sum()));noise[m,1]=rng.uniform(-.06,.06,int(m.sum()));write_wave(WORK/'noise-burst.wav',noise)
    print('Fixtures:',WORK)

def analyze(path):
    rate,a=read_wave(path);duration=len(a)/rate
    def db(x):return float(20*np.log10(max(float(x),1e-15)))
    def rms(part):
        return float(np.sqrt(np.mean(part*part))) if part.size else 0.
    active=a[int(.8*rate):int(2.2*rate)]
    fft_size=1 << (len(active)*8-1).bit_length()
    spectrum=np.abs(np.fft.rfft(active*np.hanning(len(active))[:,None],n=fft_size,axis=0))
    freqs=np.fft.rfftfreq(fft_size,1/rate);dominant=[]
    for c in range(a.shape[1]):dominant.append(float(freqs[np.argmax(spectrum[:,c])]))
    late=a[-min(rate*2,len(a)):];late=late-late.mean(axis=0)
    windows=[]
    for start in np.arange(0,duration-.1,.1):windows.append({'start':round(float(start),2),'rms_dbfs':db(rms(a[int(start*rate):int((start+.1)*rate)]))})
    return {'path':str(Path(path).resolve()),'sample_rate':rate,'channels':a.shape[1],'seconds':duration,'finite':bool(np.isfinite(a).all()),
            'peak_dbfs':db(np.max(np.abs(a))),'active_rms_dbfs':db(rms(active)),'dominant_hz':dominant,'last_2s_ac_rms_dbfs':db(rms(late)),'envelope_100ms':windows}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('action',choices=['create','analyze']);p.add_argument('path',nargs='?');p.add_argument('--output');args=p.parse_args()
    if args.action=='create':fixtures()
    else:
        result=analyze(args.path);text=json.dumps(result,indent=2)
        if args.output:Path(args.output).write_text(text+'\n')
        print(text)
