#!/usr/bin/env python3
"""Bounded measurements of Pitch Lab in its isolated, running REAPER project."""
import argparse
import hashlib
import json
from pathlib import Path
import time
import numpy as np
from pitch_lab_control import request, MANIFEST, WORK, ROOT
from pitch_lab_fixtures import analyze, read_wave

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix',required=True,help='Unique filename prefix; existing renders are preserved')
    args=parser.parse_args()
    manifest=json.loads(MANIFEST.read_text());defs={d['id']:d for d in manifest['parameters']}
    cases=[
        ('down12',0,'stimulus.wav',{'semitones':-12}),
        ('unity',0,'stimulus.wav',{'semitones':0}),
        ('post-lp',0,'stimulus.wav',{'semitones':12,'post_on':1,'post_lp':1000}),
        ('input-impulse',0,'impulse.wav',{'monitor':1}),
        ('pitch-transients',0,'transients.wav',{'semitones':12}),
        ('pitch-hall',2,'noise-burst.wav',{'clean':0}),
        ('shimmer',4,'noise-burst.wav',{'clean':0}),
        ('pitch-delay',5,'transients.wav',{}),
    ]
    results=[];out=ROOT/'build/validation/pitch-lab-20261103';out.mkdir(parents=True,exist_ok=True)
    # Validate all IDs before changing the host or starting a render.
    for _,_,_,values in cases:
        for ident in values:assert ident in defs,ident
    for name,preset,fixture,values in cases:
        parameters={defs[k]['name']:v for k,v in manifest['presets'][preset]['normalized'].items()}
        for ident,raw in values.items():
            d=defs[ident];parameters[d['name']]=((raw-d['min'])/(d['max']-d['min']))**d['skew']
        request('set',{'reset':1},parameters)
        request('stimulus',{'path':str(WORK/fixture)})
        started=time.monotonic();host=request('render',{'name':args.prefix+'-'+name,'seconds':8})
        path=Path(host['path']);metrics=analyze(path)
        metrics.update(case=name,fixture=fixture,render_wall_seconds=time.monotonic()-started,
            sha256=hashlib.sha256(path.read_bytes()).hexdigest(),host=host)
        rate,a=read_wave(path)
        if name=='input-impulse':metrics['peak_frame']=int(np.argmax(np.max(np.abs(a),axis=1)))
        assert metrics['finite'] and metrics['peak_dbfs']<0,(name,metrics['peak_dbfs'])
        if name in ('down12','unity'):
            expected=np.array([440,660])*(.5 if name=='down12' else 1)
            metrics['pitch_error_cents']=(1200*np.log2(np.array(metrics['dominant_hz'])/expected)).tolist()
            assert np.max(np.abs(metrics['pitch_error_cents']))<15
        if name=='input-impulse':assert metrics['peak_frame']==24000
        results.append(metrics);(out/(args.prefix+'-reaper.json')).write_text(json.dumps(results,indent=2)+'\n')
        print(name,'peak',round(metrics['peak_dbfs'],2),'Hz',metrics['dominant_hz'],'late',round(metrics['last_2s_ac_rms_dbfs'],2),flush=True)
    # Leave a useful, quiet audition project rather than the last measurement routing.
    request('set',{'reset':1},{defs[k]['name']:v for k,v in manifest['presets'][0]['normalized'].items()})
    request('stimulus',{'path':str(WORK/'transients.wav')});request('save')

if __name__=='__main__':main()
