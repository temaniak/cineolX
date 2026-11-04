import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, mkdir, writeFile, rm, symlink } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { makeServer } from './server.mjs';

test('private media, signed session, byte-range seeking and manifest replacement', async () => {
  const root = await mkdtemp(join(tmpdir(), 'listen-server-'));
  const collection = join(root, 'collections', 'example'); const media = join(collection, 'media');
  await mkdir(media, { recursive: true });
  const name = '0123456789abcdef01234567.wav'; const bytes = Buffer.from('0123456789abcdef');
  await writeFile(join(media, name), bytes);
  await writeFile(join(root, 'secret.txt'), 'not public');
  const alias = '1123456789abcdef01234567.wav'; await symlink(join(root, 'secret.txt'), join(media, alias));
  const manifest = { schema: 1, id: 'example', title: 'Example', createdAt: '2026-11-04', cases: [{ id: 'one', variants: [] }] };
  await writeFile(join(collection, 'manifest.json'), JSON.stringify(manifest));
  const key = 'test-only-key-with-at-least-24-characters';
  const server = makeServer({ dataRoot: root, key, secure: true, publicOrigin: 'https://listen.example' });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const base = `http://127.0.0.1:${server.address().port}/listen`;
  const unlock = supplied => fetch(base + '/api/unlock', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ key: supplied }) });
  try {
    assert.equal((await fetch(base + '/api/collections')).status, 401);
    assert.equal((await fetch(`${base}/media/example/${name}`, { headers: { Range: 'bytes=0-3' } })).status, 401);
    assert.equal((await unlock('wrong')).status, 401);
    const crossOrigin = await fetch(base + '/api/unlock', { method: 'POST', headers: { Origin: 'https://other.example' }, body: JSON.stringify({ key }) });
    assert.equal(crossOrigin.status, 403);
    const login = await unlock(key); assert.equal(login.status, 200);
    const setCookie = login.headers.get('set-cookie'); assert.match(setCookie, /HttpOnly/); assert.match(setCookie, /Secure/); assert.match(setCookie, /Path=\/listen/);
    const cookie = setCookie.split(';')[0]; const headers = { Cookie: cookie };
    const list = await fetch(base + '/api/collections', { headers }); assert.equal((await list.json()).collections[0].title, 'Example');
    const full = await fetch(`${base}/media/example/${name}`, { headers }); assert.deepEqual(Buffer.from(await full.arrayBuffer()), bytes); assert.equal(full.headers.get('accept-ranges'), 'bytes'); assert.match(full.headers.get('cache-control'), /^private/);
    const partial = await fetch(`${base}/media/example/${name}`, { headers: { ...headers, Range: 'bytes=2-5' } });
    assert.equal(partial.status, 206); assert.equal(partial.headers.get('content-range'), 'bytes 2-5/16'); assert.equal(await partial.text(), '2345');
    const suffix = await fetch(`${base}/media/example/${name}`, { headers: { ...headers, Range: 'bytes=-3' } }); assert.equal(await suffix.text(), 'def');
    const invalid = await fetch(`${base}/media/example/${name}`, { headers: { ...headers, Range: 'bytes=500-' } }); assert.equal(invalid.status, 416); assert.equal(invalid.headers.get('content-range'), 'bytes */16');
    const head = await fetch(`${base}/media/example/${name}`, { method: 'HEAD', headers }); assert.equal(head.headers.get('content-length'), '16'); assert.equal((await head.arrayBuffer()).byteLength, 0);
    assert.equal((await fetch(`${base}/media/example/${alias}`, { headers })).status, 404);
    assert.equal((await fetch(base + '/media/example/%2e%2e%2fsecret.txt', { headers })).status, 404);
    assert.equal((await fetch(base + '/api/collections', { headers: { Cookie: cookie + 'tamper' } })).status, 401);
    manifest.title = 'Updated without restart'; await writeFile(join(collection, 'manifest.json'), JSON.stringify(manifest));
    assert.equal((await (await fetch(base + '/api/collections/example', { headers })).json()).title, manifest.title);
    const asset = await fetch(base + '/'); assert.equal(asset.status, 200); assert.match(asset.headers.get('content-security-policy'), /frame-ancestors 'none'/);
  } finally { await new Promise(resolve => server.close(resolve)); await rm(root, { recursive: true }); }
});
