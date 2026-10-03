#!/usr/bin/env python3
"""Prepare a private profile as a compile-time asset; generated data stays in build/."""
import argparse
import struct
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('profile', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
data = args.profile.read_bytes()
if len(data) < 20:
    raise SystemExit('Truncated Hall profile')
magic, version, size, checksum = struct.unpack_from('<8sIII', data)
h = 2166136261
for byte in data[20:]:
    h = ((h ^ byte) * 16777619) & 0xffffffff
valid_format = (magic == b'HALL224\0' and version == 2 and size == 11674) or (magic == b'BANK224\0' and version == 1)
if not valid_format or len(data) != size+20 or h != checksum:
    raise SystemExit('A current, valid native Hall profile is required')
asset = 'bank_profile_data' if magic == b'BANK224\0' else 'hall_profile_data'
lines = ['#pragma once', '// Generated private ROM-derived asset. Do not commit.',
         f'alignas(4) inline constexpr unsigned char {asset}[] = {{']
for at in range(0, len(data), 24):
    lines.append('    '+','.join(str(x) for x in data[at:at+24])+',')
lines.append('};\n')
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text('\n'.join(lines))
print(f'Embedded {len(data)}-byte Hall profile')
