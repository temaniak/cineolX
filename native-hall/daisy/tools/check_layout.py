#!/usr/bin/env python3
"""Check the existing QSPI loader contract and the Hall RAM runtime layout."""
import struct
import subprocess
import sys
from pathlib import Path

elf, binary = map(Path, sys.argv[1:])
data = binary.read_bytes()
stack, reset = struct.unpack_from('<II', data)
assert 0x20000000 < stack <= 0x20020000, 'Invalid Cortex-M7 stack vector'
assert 0x90040000 <= (reset & ~1) < 0x90040000+len(data) and reset & 1, 'Expected a QSPI/Thumb reset vector'
assert len(data) <= 7936*1024, 'Image exceeds the Daisy QSPI application region'
symbols = subprocess.check_output(['arm-none-eabi-nm', '-n', str(elf)], text=True)
found = {}
for line in symbols.splitlines():
    fields = line.split()
    if len(fields) == 3:
        found[fields[2]] = int(fields[0], 16)
for name in ('_hall_itcm_start', '_hall_itcm_end', '_hall_itcm_load', '_ebss', '_estack',
             '_hall_vector_start', '_hall_vector_end', '_hall_vector_load',
             '_hall_runtime_start', '_hall_runtime_end', '_hall_runtime_load',
             '_hall_bank_start', '_hall_bank_end', 'HallQspiBootReset'):
    assert name in found, f'Missing layout marker: {name}'
assert 0 <= found['_hall_itcm_start'] < found['_hall_itcm_end'] <= 65536, 'ITCM overflow/empty'
assert (reset & ~1) == found['HallQspiBootReset'], 'Reset does not enter the QSPI bootstrap'
for section in ('itcm', 'vector', 'runtime'):
    load=found[f'_hall_{section}_load'];start=found[f'_hall_{section}_start'];end=found[f'_hall_{section}_end']
    assert 0x90040000 <= load < load+end-start <= 0x90040000+len(data), f'{section}: copy source outside QSPI image'
assert 0x38000000 <= found['_hall_vector_start'] < found['_hall_vector_end'] <= 0x38010000, 'Interrupt vectors outside D3 RAM'
assert 0x24000000 <= found['_hall_runtime_start'] < found['_hall_runtime_end'] <= 0x24078000, 'Runtime outside AXI SRAM'
assert 0x24000000 <= found['_hall_bank_start'] < found['_hall_bank_end'] <= 0x24078000, 'Bank outside AXI SRAM'
assert found['_hall_bank_start'] >= found['_hall_runtime_end'], 'Bank overlaps runtime'
assert found['_ebss'] <= found['_estack']-16384, 'Insufficient stack margin'
hot = [name for name in found if 'AudioCallbackE' in name or '4Hall7processE' in name or '4Hall7node_atI' in name]
assert len(hot) >= 2, 'Missing Hall/callback symbols'
for name in hot:
    assert found[name] < 65536, f'Hot audio code outside ITCM: {name}'
arithmetic = [name for name in found if '4Hall4edgeE' in name or '4Hall3addE' in name]
assert not arithmetic, f'ARU arithmetic was outlined; check DSP optimization flags: {arithmetic}'
rows = [name for name in hot if '4Hall7node_atI' in name]
assert rows, 'Missing shared native row helpers'
print(f'Native row helpers: {len(rows)}; edge/add inlined (no nested ARU calls)')
print(f'QSPI boot image @ 0x90040000: {len(data)} bytes; ITCM: {found["_hall_itcm_end"]-found["_hall_itcm_start"]} bytes')
print(f'AXI control bank: {found["_hall_bank_end"]-found["_hall_bank_start"]} bytes; AXI free: {0x24078000-found["_hall_bank_end"]} bytes')
print(f'DTCM runtime end: 0x{found["_ebss"]:08x}; stack margin: {found["_estack"]-found["_ebss"]} bytes')
