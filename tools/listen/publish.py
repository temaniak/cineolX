#!/usr/bin/env python3
"""Prepare and atomically publish a private Listen collection over the existing SSH key.

Generic input JSON uses local `file` / optional `preset` paths per variant.
The LexIR adapter imports the already validated local listening manifest.
No SSH password, access key, ROM or prepared bank belongs in an input manifest.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[2]
TARGET = json.loads((Path(__file__).with_name('target.json')).read_text())
ALLOWED = {'.wav', '.flac', '.mp3', '.m4a', '.ogg', '.opus', '.cineol224', '.pdf', '.png', '.jpg', '.csv', '.txt'}
MIME = {'.wav': 'audio/wav', '.flac': 'audio/flac', '.mp3': 'audio/mpeg', '.m4a': 'audio/mp4', '.ogg': 'audio/ogg', '.opus': 'audio/ogg'}


def run(args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def ssh(command, *args, capture=False):
    text = ' '.join(shlex.quote(str(v)) for v in [command, *args])
    return subprocess.check_output(['ssh', '-o', 'BatchMode=yes', TARGET['ssh'], text], text=True) if capture else run(['ssh', '-o', 'BatchMode=yes', TARGET['ssh'], text])


def sha(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''): result.update(chunk)
    return result.hexdigest()


def lexirs(folder, identifier, title):
    original = json.loads((folder / 'manifest.json').read_text())
    opt_off = bool(original.get('optimization_policy')) and all(
        not row['matched']['mode'] & 2 for row in original['cases'])
    cases = []
    special = {
        'Bright Hall V1': 'Название в архиве неоднозначно: по измерениям этот отклик ближе к заводской вариации V4.',
        'Small Room V3': 'В этом примере остаётся заметное расхождение. Сравни раннюю часть и окраску хвоста; это кандидат настройки, а не подтверждённое совпадение.',
        'Inverse Room V1': 'Обрати внимание на форму обратного хвоста: в архивном IR остаётся участок, который Cineol передаёт иначе.',
        'Inverse Room V2': 'Оцени нарастание и завершение обратного хвоста. Здесь сохранилось заметное расхождение огибающей.',
        'Rich Chamber V7': 'Длинный хвост не удалось полностью воспроизвести статическим откликом. Запись IR конечна, а алгоритм может звучать дольше.',
        'Rich Plate V7': 'Особенно важен конец хвоста: архивный IR не сохраняет почти бесконечное затухание аппаратного пресета.',
        'Dark Hall V5': 'Один из самых заметно различающихся примеров. Длинный хвост и его устойчивость требуют отдельной слуховой оценки.',
    }
    for row in original['cases']:
        name = row['name']; match = row['matched']; directory = folder / name
        identifier_case = re.sub(r'[^a-z0-9]+', '-', name.lower()).strip('-')
        notes = ['Сравни атаку и плотность отражений.', 'После 4,5 секунды остаётся только хвост.']
        if name in special: notes.append(special[name])
        if name.startswith('Dark Hall') and name not in special: notes.append('В отдельных частотных полосах длительность хвоста расходится заметнее, чем общий частотный баланс.')
        if 'optional_clean' in row: notes.append('C использует те же регуляторы с включённым Clean. Сравни тихий конец хвоста и шумовой остаток.')
        if opt_off: notes.append('Cineol пересоздан с выключенным Decay Opt по результатам прослушивания. Остальные регуляторы сохранены.')
        settings = [dict(label='LF Decay', value=row['display'][0][0]), dict(label='Mid Decay', value=row['display'][0][1]),
                    dict(label='Predelay', value=row['display'][0][5]), dict(label='Clean', value='Выкл.')]
        variants = []
        for variant_id, label, filename, preset_name in [('A', 'Lexicon IR', 'A-Lexicon-IR.flac', None),
                    ('B', 'Cineol', 'B-Cineol.flac', 'Matched.cineol224'),
                    ('C', 'Cineol Clean', 'C-Cineol-Clean.flac', 'Matched-Clean.cineol224')]:
            if not (directory / filename).is_file(): continue
            variant = dict(id=variant_id, label=label, file=str(directory / filename), settings=[dict(p) for p in settings])
            if variant_id != 'A':
                variant['settings'].extend([
                    dict(label='Decay Opt', value='Вкл.' if match['mode'] & 2 else 'Выкл.'),
                    dict(label='Mod', value='Вкл.' if match['mode'] & 1 else 'Выкл.'),
                    dict(label='Dynamic', value='Вкл.' if match['mode'] & 4 else 'Выкл.')])
            if variant_id == 'C': variant['settings'][-1]['value'] = 'Вкл.'
            if preset_name: variant['preset'] = str(directory / preset_name)
            variants.append(variant)
        cases.append(dict(id=identifier_case, title=name, group=name.rsplit(' V', 1)[0], order=match['program'] * 100 + int(name.rsplit(' V', 1)[1]), subtitle='Удары, аккорды и затухание',
                          duration=14, notes=notes, markers=[dict(time=.15, label='Удары'), dict(time=1.9, label='Аккорды'), dict(time=4.5, label='Только хвост')],
                          variants=variants, sections=[dict(title='О сравнении', paragraphs=[
                              'A — четырёхканальная true-stereo свёртка архивного отклика. B — работающий нативный алгоритм Cineol XL. Громкость выровнена одним коэффициентом на вариант.',
                              'Подбор выполнен по измерениям. Слуховая оценка совпадения остаётся открытой. Исходная прошивка, уровни записи и фаза модуляции аппарата неизвестны.',
                              f'Ближайшая заводская вариация по измерениям: V{match["identified_factory_variation"]}.'],
                              links=[dict(label='Об архиве Little Devil Studios', url='https://littledevilstudios.com.au/2015/03/lexicon-224xl-impulse-response-library/')])]))
    cases.sort(key=lambda case: case.pop('order'))
    return dict(schema=1, id=identifier, title=title,
                description='71 true-stereo сравнение · 14 алгоритмов XL · громкость выровнена' + (' · Decay Opt выключен' if opt_off else ''),
                createdAt=datetime.now(timezone.utc).isoformat(),
                defaultCase='concert-hall-v7' if opt_off else 'concert-hall-v1', cases=cases)


def prepare(source, source_base, output):
    import numpy as np
    if source.get('schema', 1) != 1: raise ValueError('Unsupported schema')
    identifier = source['id']
    if not re.fullmatch(r'[a-z0-9][a-z0-9_-]{0,99}', identifier): raise ValueError('Invalid collection id')
    if not source.get('title') or not source.get('cases'): raise ValueError('Title and cases are required')
    output.mkdir(parents=True, exist_ok=True); media = output / 'media'; media.mkdir(exist_ok=True)
    files = {}; seen_ids = set()
    def add_file(value):
        path = (source_base / value).resolve() if not Path(value).is_absolute() else Path(value).resolve()
        extension = path.suffix.lower()
        if extension not in ALLOWED or not path.is_file(): raise ValueError(f'Unsupported or missing media: {path}')
        digest = sha(path); name = digest[:24] + extension
        destination = media / name
        if not destination.exists() or sha(destination) != digest:
            # APFS clones are independent files without another full audio copy.
            if sys.platform == 'darwin':
                result = subprocess.run(['cp', '-c', str(path), str(destination)], stderr=subprocess.DEVNULL)
                if result.returncode: shutil.copyfile(path, destination)
            else: shutil.copyfile(path, destination)
        files[name] = dict(sha256=digest, bytes=path.stat().st_size)
        return path, f'/listen/media/{identifier}/{name}'
    for case in source['cases']:
        if not re.fullmatch(r'[a-z0-9][a-z0-9_-]{0,99}', case['id']) or case['id'] in seen_ids: raise ValueError('Invalid or duplicate case id')
        seen_ids.add(case['id']); ids = set(); maximum_peak = .001
        if not case.get('variants'): raise ValueError('Case needs audio variants')
        for variant in case['variants']:
            if not re.fullmatch(r'[A-Z0-9]{1,4}', variant['id']) or variant['id'] in ids: raise ValueError('Invalid variant id')
            ids.add(variant['id']); path, variant['url'] = add_file(variant.pop('file'))
            if path.suffix.lower() not in MIME: raise ValueError('Variant file must be audio')
            variant['mime'] = MIME[path.suffix.lower()]
            if 'preset' in variant:
                _, url = add_file(variant.pop('preset')); variant.setdefault('downloads', []).append(dict(label='Скачать пресет', url=url + '?download=1'))
            raw = subprocess.check_output(['ffmpeg', '-v', 'error', '-nostdin', '-i', str(path), '-ar', '12000', '-ac', '2', '-f', 'f32le', 'pipe:1'])
            x = np.frombuffer(raw, '<f4').reshape(-1, 2)
            if not len(x) or not np.isfinite(x).all(): raise ValueError(f'Invalid audio: {path}')
            variant['duration'] = len(x) / 12000
            # Actual decoded signal peaks; never a decorative/fake waveform.
            step = max(1, int(np.ceil(len(x) / 640)))
            padded = np.pad(np.max(np.abs(x), axis=1), (0, (-len(x)) % step))
            peaks = padded.reshape(-1, step).max(axis=1)
            variant['waveform'] = [round(float(v), 6) for v in peaks]
            maximum_peak = max(maximum_peak, float(peaks.max()))
        case['duration'] = max(v['duration'] for v in case['variants']); case['waveformScale'] = maximum_peak
    source['schema'] = 1; source.setdefault('createdAt', datetime.now(timezone.utc).isoformat()); source['files'] = files
    (output / 'manifest.json').write_text(json.dumps(source, ensure_ascii=False, separators=(',', ':')) + '\n')
    return source


def publish(collection, folder):
    identifier = collection['id']; remote = TARGET['root'] + '/data/collections/' + identifier
    ssh('mkdir', '-p', remote + '/media')
    run(['rsync', '-a', '--checksum', '-e', 'ssh -o BatchMode=yes', str(folder / 'media') + '/', f'{TARGET["ssh"]}:{remote}/media/'])
    staged = '.manifest-' + uuid.uuid4().hex + '.json'
    run(['rsync', '-a', '-e', 'ssh -o BatchMode=yes', str(folder / 'manifest.json'), f'{TARGET["ssh"]}:{remote}/{staged}'])
    activation = '''import pathlib,json,hashlib,sys,os,time
p=pathlib.Path(sys.argv[1]);staged=p/sys.argv[2];d=json.loads(staged.read_text())
assert d['id']==p.name and d['schema']==1
for name,info in d['files'].items():
 assert '/' not in name and not name.startswith('.')
 f=p/'media'/name
 assert f.stat().st_size==info['bytes'] and hashlib.sha256(f.read_bytes()).hexdigest()==info['sha256'],name
current=p/'manifest.json'
if current.exists():
 history=p/'history';history.mkdir(exist_ok=True);(history/(str(time.time_ns())+'.json')).write_bytes(current.read_bytes())
os.replace(staged,current)
print('Published',d['id'],len(d['cases']),'cases;',len(d['files']),'verified files')
'''
    ssh('python3', '-c', activation, remote, staged)
    ssh('python3', TARGET['root'] + '/bin/ensure_proxy.py', '--proxy', TARGET['proxyContainer'], '--app', TARGET['appContainer'], '--root', TARGET['root'])
    print(TARGET['url'] + '?collection=' + identifier)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    inputs = p.add_mutually_exclusive_group(required=True)
    inputs.add_argument('--manifest', type=Path, help='Generic manifest with local file paths')
    inputs.add_argument('--lexirs', type=Path, help='Existing lexir listening/ folder')
    p.add_argument('--id', default='lexicon-224xl-2026-11-04')
    p.add_argument('--title', default='Cineol × Lexicon 224XL')
    p.add_argument('--output', type=Path)
    p.add_argument('--deploy', action='store_true', help='Upload over SSH and atomically activate')
    args = p.parse_args()
    if args.lexirs: source, source_base = lexirs(args.lexirs.resolve(), args.id, args.title), Path('/')
    else: source, source_base = json.loads(args.manifest.read_text()), args.manifest.resolve().parent
    output = args.output or ROOT / 'build/listen/packages' / source['id']
    collection = prepare(source, source_base, output)
    print(f'Prepared {collection["id"]}: {len(collection["cases"])} cases, {len(collection["files"])} files')
    if args.deploy: publish(collection, output.resolve())
    else: print(output.resolve())


if __name__ == '__main__': main()
