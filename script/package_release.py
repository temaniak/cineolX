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


def numeric_version():
    return re.search(r'project\(CineolX224 VERSION ([0-9]+\.[0-9]+\.[0-9]+)',
                     (ROOT / 'CMakeLists.txt').read_text()).group(1)


def version():
    public = re.search(r'set\(CINEOL_RELEASE_VERSION "([^"\n]+)"\)',
                       (ROOT / 'CMakeLists.txt').read_text())
    if public and not re.fullmatch(r'[0-9]+\.[0-9]+(?:\.[0-9]+|[A-Z]+)', public.group(1)):
        raise ValueError('Invalid public release version')
    return public.group(1) if public else numeric_version()


def is_prerelease():
    return bool(re.fullmatch(r'[0-9]+\.[0-9]+[A-Z]+', version()))


def verify_bundle(bundle, platform):
    if platform == 'macos':
        info = plistlib.loads((bundle / 'Contents/Info.plist').read_bytes())
        if info['CFBundleShortVersionString'] != numeric_version():
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
        shutil.copy2(ROOT / 'docs/releases' / ('v' + version() + '.md'), staging / 'RELEASE-NOTES.md')
        metadata = dict(version=version(), numeric_version=numeric_version(), prerelease=is_prerelease(), platform=label,
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
                    'FIRST LAUNCH - IMPORT YOUR ROMS\n'
                    'Original 224 programs: Lexicon 224 v4.4 ROM1-ROM5, five 2048-byte files.\n'
                    'XL preview programs: Lexicon 224 XL v8.21, the complete eleven-chip\n'
                    'set of 2048- and 4096-byte files. Import both sets for all 28 programs.\n'
                    'Click Choose ROMs... for ZIPs or all ROM files together, or Choose\n'
                    'folder... for a directory. Wait for preparation; only imported engines\n'
                    'are enabled. Import errors stay visible when adding the other engine.\n'
                    'To add the other engine later, click the Model / 224 / 224 XL indicator.\n'
                    '224X v8.1, 224 XL v8.1A and incomplete or modified sets are rejected.\n'
                    'ROMs and prepared banks are not included. Your files stay local;\n'
                    'later launches use the cache. See README.md for details.\n')
        install += ('\nUPGRADING TO ' + version() + '\n'
                    'XL sound corrections require the version-5 prepared cache. Re-import your\n'
                    'complete original 224XL v8.21 ROM set once after updating. The previous\n'
                    'XL caches, including the 0.9.6 version-4 cache, are retained. Original-224\n'
                    'caches and sound presets remain compatible. No re-import is needed if\n'
                    'the corrected version-5 cache has already been prepared locally.\n'
                    'The editor opens at 984 x 744; compact display mode is 984 x 252.\n'
                    'The gear contains Display brightness, Option illumination and Compact\n'
                    'display mode. Drag screen values up/down; hold Shift for fine adjustment.\n'
                    'Appearance is saved in the DAW session. A host may restore window size.\n'
                    'See RELEASE-NOTES.md for changes, validation and known limitations.\n')
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
            if any(Path(file).suffix.lower() in {'.rom', '.bank224', '.bankxl', '.hall224', '.wcs', '.bin', '.elf', '.hex'} for file in zipped.namelist()):
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
    parser.add_argument('--prerelease', action='store_true')
    args = parser.parse_args()
    if args.prerelease:
        print('true' if is_prerelease() else 'false')
        return
    if args.version:
        print(version())
        return
    if args.check_tag:
        expected = 'v' + version()
        if os.environ.get('RELEASE_TAG') != expected:
            parser.error(f'Release tag must match public version: {expected}')
        print(f'Release tag matches {expected}')
        return
    if not args.platform or not args.build_dir:
        parser.error('--platform and --build-dir are required')
    package(args.platform, args.build_dir.resolve(), args.output_dir.resolve())


if __name__ == '__main__':
    main()
