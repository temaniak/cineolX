#!/usr/bin/env python3
"""Verify and package only public desktop bundles, never private ROM fixtures."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def version():
    return re.search(r'project\(CineolX224 VERSION ([0-9]+\.[0-9]+\.[0-9]+)',
                     (ROOT / 'CMakeLists.txt').read_text()).group(1)


def verify_bundle(bundle, platform):
    if platform == 'macos':
        info = plistlib.loads((bundle / 'Contents/Info.plist').read_bytes())
        if info['CFBundleShortVersionString'] != version():
            raise RuntimeError(f'{bundle.name}: stale bundle version')
        executable = bundle / 'Contents/MacOS' / info['CFBundleExecutable']
        architectures = subprocess.check_output(['lipo', '-archs', str(executable)], text=True).split()
        if set(architectures) != {'arm64', 'x86_64'}:
            raise RuntimeError(f'{bundle.name}: expected universal binary, got {architectures}')
        subprocess.run(['codesign', '--verify', '--deep', '--strict', str(bundle)], check=True)
    else:
        executable = bundle / 'Contents/x86_64-win/Cineol-X 224.vst3' if bundle.is_dir() else bundle
        data = executable.read_bytes()
        offset = struct.unpack_from('<I', data, 0x3c)[0]
        if data[:2] != b'MZ' or data[offset:offset + 4] != b'PE\0\0' or struct.unpack_from('<H', data, offset + 4)[0] != 0x8664:
            raise RuntimeError(f'{bundle.name}: expected Windows x64 PE')


def package(platform, build_dir, output_dir):
    if (platform == 'macos') != (sys.platform == 'darwin'):
        raise RuntimeError('Package on the build platform so signatures and permissions can be verified')
    label = 'macOS-universal' if platform == 'macos' else 'Windows-x64'
    name = f'Cineol-X-224-v{version()}-{label}'
    products = [('VST3', 'Cineol-X 224.vst3')]
    products += [('AU', 'Cineol-X 224.component'), ('Standalone', 'Cineol-X 224.app')] if platform == 'macos' else [('Standalone', 'Cineol-X 224.exe')]
    artefacts = build_dir / 'NativeHall224_artefacts/Release'
    output_dir.mkdir(parents=True, exist_ok=True)
    archive = output_dir / (name + '.zip')
    if archive.exists():
        archive.unlink()
    with tempfile.TemporaryDirectory(prefix='cineol-package-', dir=ROOT / 'build') as temporary:
        staging = Path(temporary) / name
        staging.mkdir()
        for folder, filename in products:
            source = artefacts / folder / filename
            if not source.exists():
                raise RuntimeError(f'Missing build product: {source}')
            verify_bundle(source, platform)
            destination = staging / folder / filename
            destination.parent.mkdir(exist_ok=True)
            if source.is_dir():
                shutil.copytree(source, destination, symlinks=True)
            else:
                shutil.copy2(source, destination)
            verify_bundle(destination, platform)
        for document in ('README.md', 'NOTICE.md'):
            shutil.copy2(ROOT / document, staging / document)
        metadata = dict(version=version(), platform=label,
                        commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                        dependencies=json.loads((ROOT / 'dependencies.json').read_text()), juce='8.0.14')
        (staging / 'BUILD-INFO.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
        install = f'Cineol-X 224 v{version()} - {label}\n\n'
        if platform == 'macos':
            install += ('Copy AU/Cineol-X 224.component to ~/Library/Audio/Plug-Ins/Components/\n'
                        'Copy VST3/Cineol-X 224.vst3 to ~/Library/Audio/Plug-Ins/VST3/\n'
                        'Copy Standalone/Cineol-X 224.app to Applications.\n'
                        'Requires macOS 11 or later. Bundles are ad-hoc signed, not notarized.\n'
                        'If macOS blocks a downloaded bundle you trust, remove quarantine from\n'
                        'the installed bundle using: xattr -dr com.apple.quarantine <bundle path>\n')
        else:
            install += ('Copy the entire VST3/Cineol-X 224.vst3 folder to\n'
                        'C:\\Program Files\\Common Files\\VST3\\\n'
                        'Standalone/Cineol-X 224.exe is the optional standalone application.\n')
        install += ('\nRestart or rescan your DAW after installing.\n'
                    'Open the gear in the top-right corner and enable Low latency for\n'
                    'a direct dry signal with zero reported plugin latency. Reverb filter\n'
                    'delay and pre-delay remain. This setting defaults to off and is saved\n'
                    'per instance. A transport restart may be needed after switching.\n\n'
                    'Supply your own original Lexicon 224 v4.4 ROM1-ROM5 on first use.\n'
                    'ROMs and prepared banks are not included. See README.md for details.\n')
        (staging / 'INSTALL.txt').write_text(install, encoding='utf-8')
        if platform == 'macos':
            subprocess.run(['ditto', '-c', '-k', '--keepParent', str(staging), str(archive)], check=True)
            extracted = Path(temporary) / 'extracted'
            subprocess.run(['ditto', '-x', '-k', str(archive), str(extracted)], check=True)
            for folder, filename in products:
                verify_bundle(extracted / name / folder / filename, platform)
        else:
            with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as zipped:
                for file in sorted(staging.rglob('*')):
                    if file.is_file():
                        zipped.write(file, file.relative_to(staging.parent))
        with zipfile.ZipFile(archive) as zipped:
            if zipped.testzip() is not None:
                raise RuntimeError('ZIP integrity check failed')
            if any(Path(file).suffix.lower() in {'.rom', '.bank224', '.hall224', '.wcs', '.bin', '.elf', '.hex'} for file in zipped.namelist()):
                raise RuntimeError('Private/firmware file in release archive')
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix('.zip.sha256').write_text(f'{digest}  {archive.name}\n', encoding='utf-8')
    print(f'Verified release archive: {archive.name} ({archive.stat().st_size} bytes)')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=('macos', 'windows'))
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'build/dist')
    parser.add_argument('--check-tag', action='store_true')
    parser.add_argument('--version', action='store_true')
    args = parser.parse_args()
    if args.version:
        print(version())
        return
    if args.check_tag:
        expected = 'v' + version()
        if os.environ.get('RELEASE_TAG') != expected:
            parser.error(f'Release tag must match CMake version: {expected}')
        print(f'Release tag matches {expected}')
        return
    if not args.platform or not args.build_dir:
        parser.error('--platform and --build-dir are required')
    package(args.platform, args.build_dir.resolve(), args.output_dir.resolve())


if __name__ == '__main__':
    main()
