#!/usr/bin/env python3
"""Require the owner's original 224 v4.4 chips, even for incremental builds."""
import argparse
import hashlib
import json
from pathlib import Path


def verify_roms(directory):
    # This string list is also included by the C++ plugin/bank importer.
    manifest = Path(__file__).resolve().parents[2] / 'import/rom_hashes.inc'
    hashes = json.loads('[' + manifest.read_text() + ']')
    if len(hashes) != 5 or len(set(hashes)) != 5:
        raise ValueError('Invalid original 224 ROM hash manifest')
    if not directory.is_dir():
        raise ValueError(f'ROM directory does not exist: {directory}')
    found = set()
    for path in directory.iterdir():
        if not path.is_file() or path.stat().st_size != 2048:
            continue
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest in hashes:
            found.add(hashes.index(digest))
    missing = [f'ROM{i + 1}' for i in range(5) if i not in found]
    if missing:
        raise ValueError('Missing or unrecognized original 224 v4.4 chips: ' + ', '.join(missing))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom_directory', type=Path)
    args = parser.parse_args()
    try:
        verify_roms(args.rom_directory)
    except (OSError, ValueError) as error:
        parser.exit(1, f'{error}\nSet NATIVE_HALL_ROM_DIR to your original Lexicon 224 v4.4 '
                       'ROM1-ROM5 folder. 224X/224XL and other versions are not supported.\n'
                       'A cached bank or previous firmware does not replace the ROMs.\n')
    print('Original Lexicon 224 v4.4 ROM1-ROM5 verified (SHA-256).')


if __name__ == '__main__':
    main()
