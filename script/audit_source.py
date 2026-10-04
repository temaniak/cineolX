#!/usr/bin/env python3
"""Check staged source contents before creating a source archive or publishing."""
import argparse
import json
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rom-directory', type=Path, help='Optional private chips used only to scan for leaked bytes')
args = parser.parse_args()
records = subprocess.check_output(['git','-C',str(root),'ls-files','--stage','-z']).split(b'\0')
pins = json.loads((root/'dependencies.json').read_text())
private = []
if args.rom_directory:
    private = [p.read_bytes() for p in args.rom_directory.iterdir() if p.is_file() and p.stat().st_size in (2048,4096)]
count=0
for record in records:
    if not record: continue
    metadata,name=record.split(b'\t',1);mode,oid,_=metadata.split();name=name.decode()
    if mode==b'160000':
        assert name.startswith('deps/') and oid.decode()==pins[name.split('/')[1]]['commit'], 'Dependency pin mismatch'
        continue
    assert not name.startswith(('build/','firmware/','out/','deps/')), f'Private/generated/dependency contents staged: {name}'
    assert not name.endswith(('.bank224','.bankxl','.hall224','.wcs','.rom','.bin','.elf','.hex','.map','.ru.md','.local.hpp')), name
    data=subprocess.check_output(['git','-C',str(root),'show',':'+name])
    assert not any(chip in data for chip in private), f'ROM bytes staged in {name}'
    if Path(name).suffix not in ('.png',):
        text=data.decode('utf-8')
        assert not re.search('[\u0400-\u04ff]',text), f'Non-English source/documentation: {name}'
        # Split literals so this check does not match itself.
        forbidden=['fdn_'+'dreamage','FND_'+'Euro_Morph','Panel'+'Calibration',
                   'PanelControl'+'Config','/Users'+'/','/home'+'/']
        assert not any(word in text for word in forbidden), f'Private hardware/workstation detail: {name}'
    count+=1
print(f'Source audit passed: {count} own files, pinned submodule references, English documentation; '
      'no private hardware details, ROMs, generated banks, binaries or review translation staged.')
