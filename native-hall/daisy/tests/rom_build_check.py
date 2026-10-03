#!/usr/bin/env python3
"""Exercise ROM checks without building or communicating with hardware."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


daisy = Path(__file__).resolve().parents[1]
root = daisy.parents[1]
verifier = daisy / 'tools/verify_roms.py'
source = Path(sys.argv[1])
hashes = json.loads('[' + (daisy.parent / 'import/rom_hashes.inc').read_text() + ']')
chips = {}
for path in source.iterdir():
    if path.is_file() and path.stat().st_size == 2048:
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest in hashes:
            chips[hashes.index(digest)] = data
assert len(chips) == 5, 'Tests need the original 224 v4.4 ROM1-ROM5'


def run(command, *, env=None):
    return subprocess.run(command, capture_output=True, text=True, env=env, timeout=20)


with tempfile.TemporaryDirectory(prefix='daisy-rom-check-') as temp:
    work = Path(temp)
    valid = work / 'renamed ROMs'
    valid.mkdir()
    for chip, data in chips.items():
        (valid / f'chip-{chip}.bin').write_bytes(data)
    (valid / 'unrelated.txt').write_text('ignored')
    result = run([sys.executable, str(verifier), str(valid)])
    assert result.returncode == 0, result.stderr

    empty = work / 'empty'
    empty.mkdir()
    incomplete = work / 'incomplete'
    incomplete.mkdir()
    corrupt = work / 'corrupt'
    corrupt.mkdir()
    wrong_size = work / 'wrong-size'
    wrong_size.mkdir()
    for chip, data in chips.items():
        if chip != 4:
            (incomplete / f'{chip}.bin').write_bytes(data)
        broken = bytes([data[0] ^ 1]) + data[1:] if chip == 4 else data
        (corrupt / f'{chip}.bin').write_bytes(broken)
        (wrong_size / f'{chip}.bin').write_bytes(data + b'\0' if chip == 4 else data)
    # Duplicate files cannot substitute for the missing chip.
    (incomplete / 'duplicate.bin').write_bytes(chips[0])

    # A previous header/image and explicit PROFILE must not bypass validation.
    cached = work / 'cached'
    cached.mkdir()
    (cached / 'generated_profile.hpp').write_text('// Previous generated asset\n')
    (cached / 'Hall224_Daisy.bin').write_bytes(b'previous firmware')
    previous = (cached / 'Hall224_Daisy.bin').read_bytes()
    for case in [work / 'absent', empty, incomplete, corrupt, wrong_size]:
        result = run([sys.executable, str(verifier), str(case)])
        assert result.returncode != 0 and 'ROM1-ROM5' in result.stderr, result
        env = dict(os.environ, NATIVE_HALL_ROM_DIR=str(case), LIBDAISY_DIR=str(work / 'no-library'))
        for mode in ['--build-only', '--flash']:
            result = run(['bash', str(root / 'script/build_daisy.sh'), mode], env=env)
            assert result.returncode != 0 and 'ROM1-ROM5' in result.stderr, result
            assert 'Original Lexicon 224 v4.4 ROM1-ROM5 verified' not in result.stdout
        for goal in ['all', 'flash']:
            result = run(['make', '-n', '-C', str(daisy), goal,
                          f'BUILD_DIR={cached}', f'PROFILE={cached / "programs-v44.bank224"}',
                          f'NATIVE_HALL_ROM_DIR={case}', f'LIBDAISY_DIR={work / "no-library"}'])
            assert result.returncode != 0 and 'ROM1-ROM5 are required' in result.stderr, result
        assert (cached / 'Hall224_Daisy.bin').read_bytes() == previous
    print('Daisy ROM gate: renamed files accepted; absent/empty/incomplete/corrupt/wrong-size rejected; '
          'cached builds and flash cannot bypass the check.')
