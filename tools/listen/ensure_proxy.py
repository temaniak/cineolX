#!/usr/bin/env python3
"""Idempotent, validated /listen route in the currently mounted Caddyfile.

Runs on the VPS as the existing deployment user. Does not restart Caddy.
Backups preserve the exact previous bytes; live bind-mounted inode is retained.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess


def run(args, **kwargs): return subprocess.run(args, check=True, **kwargs)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--proxy', default='nutrition-journal-production-caddy-1')
    p.add_argument('--app', default='nk-listen')
    p.add_argument('--root', type=Path, default=Path('/home/tunnel/nk-listen'))
    args = p.parse_args()
    inspect = json.loads(subprocess.check_output(['docker', 'inspect', args.proxy]))[0]
    source = Path(next(m['Source'] for m in inspect['Mounts'] if m['Destination'] == '/etc/caddy/Caddyfile'))
    before = source.read_bytes(); text = before.decode()
    marker = '# NK Listen route (managed by tools/listen/ensure_proxy.py)'
    if marker in text:
        print('Listen HTTPS route already present'); return
    header = '{$APP_DOMAIN} {\n'
    if text.count(header) != 1: raise RuntimeError('Unexpected Caddyfile; refusing to guess its application block')
    block = f'''  {marker}
  @nk_listen path /listen /listen/*
  handle @nk_listen {{
    reverse_proxy {args.app}:3048
  }}

'''
    candidate = text.replace(header, header + block, 1).encode()
    backup_root = args.root / 'backups'; backup_root.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup = backup_root / f'Caddyfile-{stamp}.before'; backup.write_bytes(before)
    staged = backup_root / f'Caddyfile-{stamp}.candidate'; staged.write_bytes(candidate)
    inside = f'/tmp/nk-listen-{stamp}.caddy'
    run(['docker', 'cp', str(staged), f'{args.proxy}:{inside}'], stdout=subprocess.DEVNULL)
    run(['docker', 'exec', args.proxy, 'caddy', 'validate', '--config', inside, '--adapter', 'caddyfile'])
    if hashlib.sha256(source.read_bytes()).digest() != hashlib.sha256(before).digest():
        raise RuntimeError('Caddyfile changed during validation; no live edit made')
    def write_live(value):
        with source.open('wb') as file:
            file.write(value); file.flush(); os.fsync(file.fileno())
    write_live(candidate)
    try:
        run(['docker', 'exec', args.proxy, 'caddy', 'reload', '--config', '/etc/caddy/Caddyfile', '--adapter', 'caddyfile'])
    except Exception:
        write_live(before)
        run(['docker', 'exec', args.proxy, 'caddy', 'reload', '--config', '/etc/caddy/Caddyfile', '--adapter', 'caddyfile'])
        raise
    (backup_root / f'route-{stamp}.json').write_text(json.dumps(dict(source=str(source), before_sha256=hashlib.sha256(before).hexdigest(),
                  after_sha256=hashlib.sha256(candidate).hexdigest(), backup=str(backup)), indent=2) + '\n')
    print('Listen HTTPS route validated and reloaded; previous sites retained')


if __name__ == '__main__': main()
