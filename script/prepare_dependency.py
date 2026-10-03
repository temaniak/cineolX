#!/usr/bin/env python3
"""Export pinned Git sources and apply compatibility patches only in build/."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile


root = Path(__file__).resolve().parents[1]


def git(source, *arguments):
    return subprocess.check_output(['git', '-C', str(source), *arguments], stderr=subprocess.PIPE)


def export(source, output, commit):
    actual = git(source, 'rev-parse', 'HEAD').decode().strip()
    if actual != commit:
        raise RuntimeError(f'{source}: expected {commit}, found {actual}; initialize the pinned submodule')
    # Export only library build inputs, omitting CMSIS examples, docs and test
    # suites. Submodule source remains a pinned reference in the repository.
    selections = {
        'libDaisy': {'src','core','Drivers','Middlewares'},
        'CMSIS_5': {'CMSIS/Core/Include'},
        'CMSIS-DSP': {'Include','PrivateInclude','Source'},
        'STM32H7xx': {'Include','Source'},
        'STM32H7xx_HAL_Driver': {'Inc','Src'},
    }
    paths=[]
    if source.name in selections:
        for record in git(source, 'ls-tree', '-z', commit).split(b'\0'):
            if not record: continue
            metadata, name = record.split(b'\t', 1)
            name=name.decode()
            if metadata.split()[1]!=b'tree' and not metadata.startswith(b'160000 '): paths.append(name)
        paths.extend(sorted(selections[source.name]))
    archive = git(source, 'archive', '--format=tar', commit, *paths)
    with tarfile.open(fileobj=io.BytesIO(archive)) as contents:
        for member in contents.getmembers():
            if member.name.startswith('/') or '..' in Path(member.name).parts:
                raise RuntimeError('Invalid dependency archive path')
        contents.extractall(output)
    for record in git(source, 'ls-tree', '-rz', commit).split(b'\0'):
        if not record.startswith(b'160000 '):
            continue
        metadata, path = record.split(b'\t', 1)
        child_commit = metadata.split()[2].decode()
        child = path.decode()
        if source.name=='libDaisy' and not child.startswith(('Drivers/','Middlewares/')): continue
        export(source / child, output / child, child_commit)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dependency', choices=['reflexion', 'libDaisy'])
    parser.add_argument('--source', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    pin = json.loads((root / 'dependencies.json').read_text())[args.dependency]['commit']
    source = (args.source or root / 'deps' / args.dependency).resolve()
    output = args.output.resolve()
    # Restrict cleanup/replacement to the project's ignored build directory.
    if root / 'build' not in output.parents:
        parser.error('Dependency exports must be inside this project\'s build/ directory')
    patch = root / 'patches' / ('reflexion-scheduler.patch' if args.dependency == 'reflexion'
                              else 'libdaisy-qspi-init.patch')
    stamp = 'export-v2\n' + pin + '\n' + hashlib.sha256(patch.read_bytes()).hexdigest() + '\n'
    try:
        if git(source, 'rev-parse', 'HEAD').decode().strip() != pin:
            raise RuntimeError('Dependency checkout does not match dependencies.json')
        if (output / '.cineol-export').exists() and (output / '.cineol-export').read_text() == stamp:
            print(f'{args.dependency}: pinned export ready')
            return
        staging = output.with_name(output.name + '.preparing')
        if staging.exists():
            shutil.rmtree(staging)
        staging.mkdir(parents=True)
        export(source, staging, pin)
        subprocess.run(['patch', '-p1', '--batch', '-i', str(patch)], cwd=staging, check=True)
        (staging / '.cineol-export').write_text(stamp)
        if output.exists():
            shutil.rmtree(output)
        staging.rename(output)
        print(f'{args.dependency}: exported {pin}; source checkout unchanged')
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        detail = error.stderr.decode(errors='replace') if isinstance(error, subprocess.CalledProcessError) and error.stderr else str(error)
        parser.exit(1, f'{detail}\nRun ./script/setup_dependencies.sh'
                       + (' --daisy' if args.dependency == 'libDaisy' else '') + '\n')


if __name__ == '__main__':
    main()
