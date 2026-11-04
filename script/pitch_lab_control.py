#!/usr/bin/env python3
"""Control only the dedicated Pitch Lab REAPER project through its ReaScript bridge."""
import argparse
import json
import math
import os
from pathlib import Path
import time
import uuid

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/pitch-lab/reaper-bench'
MANIFEST=ROOT/'build/pitch-lab/parameters.json'

def request(action,fields=None,parameters=None,timeout=30):
    WORK.mkdir(parents=True,exist_ok=True)
    identity=uuid.uuid4().hex
    lines=[f'request_id\t{identity}',f'action\t{action}']
    for k,v in (fields or {}).items():
        if any(x in str(v) for x in '\n\r\t'):raise ValueError('Invalid control field')
        lines.append(f'{k}\t{v}')
    for name,value in (parameters or {}).items():lines.append(f'param\t{name}\t{value:.12g}')
    temporary=WORK/'request.tmp';temporary.write_text('\n'.join(lines)+'\n');os.replace(temporary,WORK/'request.tsv')
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        try:
            result=json.loads((WORK/'response.json').read_text())
            if result.get('request_id')==identity:
                if not result.get('ok'):raise RuntimeError(result.get('error',result))
                return result
        except (FileNotFoundError,json.JSONDecodeError):pass
        time.sleep(.05)
    raise TimeoutError('REAPER bridge is not responding. Run script/reaper_pitch_lab.lua in its dedicated project tab.')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=['status','set','preset','monitor','panic','render','stimulus','save','play','stop','close_bridge'])
    p.add_argument('argument',nargs='?');p.add_argument('value',nargs='?');p.add_argument('--seconds',type=float,default=8);p.add_argument('--reset',action='store_true')
    args=p.parse_args();manifest=json.loads(MANIFEST.read_text());lookup={x['id']:x for x in manifest['parameters']}
    if args.action in ('set','monitor'):
        ident='monitor' if args.action=='monitor' else args.argument
        raw=float(args.argument if args.action=='monitor' else args.value)
        d=lookup[ident];raw=min(d['max'],max(d['min'],raw));value=((raw-d['min'])/(d['max']-d['min']))**d['skew']
        result=request('set',{'reset':int(args.reset)},{d['name']:value})
    elif args.action=='preset':
        n=int(args.argument);preset=manifest['presets'][n]
        result=request('set',{'reset':1},{lookup[k]['name']:v for k,v in preset['normalized'].items()})
    elif args.action=='render':result=request('render',{'name':args.argument,'seconds':args.seconds})
    elif args.action=='stimulus':result=request('stimulus',{'path':str(Path(args.argument).resolve())})
    else:result=request(args.action)
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
