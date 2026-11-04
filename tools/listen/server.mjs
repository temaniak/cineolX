import http from 'node:http';
import { createReadStream } from 'node:fs';
import { readFile, readdir, stat, realpath } from 'node:fs/promises';
import { dirname, join, resolve, extname, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { randomBytes, createHmac, timingSafeEqual } from 'node:crypto';

const HERE = dirname(fileURLToPath(import.meta.url));
const MIME = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8', '.flac': 'audio/flac', '.wav': 'audio/wav', '.mp3': 'audio/mpeg',
  '.m4a': 'audio/mp4', '.ogg': 'audio/ogg', '.opus': 'audio/ogg', '.png': 'image/png', '.jpg': 'image/jpeg',
  '.svg': 'image/svg+xml', '.cineol224': 'application/octet-stream', '.pdf': 'application/pdf' };
const same = (a, b) => Buffer.byteLength(a) === Buffer.byteLength(b) && timingSafeEqual(Buffer.from(a), Buffer.from(b));
const safeId = value => /^[a-z0-9][a-z0-9_-]{0,99}$/.test(value);

export function makeServer(options = {}) {
  const base = (options.base ?? process.env.BASE_PATH ?? '/listen').replace(/\/$/, '');
  const dataRoot = resolve(options.dataRoot ?? process.env.DATA_ROOT ?? join(HERE, '../../build/listen/data'));
  const publicRoot = resolve(options.publicRoot ?? join(HERE, 'public'));
  const key = options.key ?? process.env.ACCESS_KEY;
  const secure = options.secure ?? process.env.SECURE_COOKIES === 'true';
  const publicOrigin = options.publicOrigin ?? process.env.PUBLIC_ORIGIN;
  if (!key || key.length < 24) throw new Error('ACCESS_KEY must contain at least 24 characters');
  const cookieName = 'nk_listen_session';
  const sign = value => createHmac('sha256', key).update(value).digest('base64url');
  const cookie = value => `${cookieName}=${value}; Path=${base || '/'}; HttpOnly; SameSite=Lax; Max-Age=${value ? 2592000 : 0}${secure ? '; Secure' : ''}`;
  const authenticated = req => {
    const value = (req.headers.cookie ?? '').split(';').map(s => s.trim()).find(s => s.startsWith(`${cookieName}=`))?.slice(cookieName.length + 1) ?? '';
    const parts = value.split('.');
    if (parts.length !== 3) return false;
    const expiry = Number(parts[0]);
    return Number.isSafeInteger(expiry) && expiry > Date.now() / 1000 && expiry < Date.now() / 1000 + 2678400 &&
      same(parts[2], sign(`${parts[0]}.${parts[1]}`));
  };
  const attempts = new Map();
  const json = (res, status, body, extra = {}) => {
    const data = Buffer.from(JSON.stringify(body));
    res.writeHead(status, { 'Content-Type': MIME['.json'], 'Cache-Control': 'no-store', 'Content-Length': data.length, ...extra });
    res.end(data);
  };
  const readJson = async path => JSON.parse(await readFile(path, 'utf8'));
  async function file(req, res, root, relative, isPrivate = false) {
    const path = await realpath(join(root, relative)).catch(() => null);
    const canonicalRoot = await realpath(root).catch(() => root);
    if (!path || !path.startsWith(canonicalRoot + sep)) return json(res, 404, { error: 'Файл не найден' });
    const info = await stat(path);
    if (!info.isFile()) return json(res, 404, { error: 'Файл не найден' });
    const etag = `"${info.size.toString(16)}-${Math.trunc(info.mtimeMs).toString(16)}"`;
    const headers = { 'Content-Type': MIME[extname(path).toLowerCase()] ?? 'application/octet-stream', 'Accept-Ranges': 'bytes',
      'Cache-Control': isPrivate ? 'private, max-age=86400' : 'no-cache', ETag: etag };
    if (req.headers['if-none-match'] === etag && !req.headers.range) { res.writeHead(304, headers); return res.end(); }
    let start = 0, end = info.size - 1, status = 200;
    if (req.headers.range && (!req.headers['if-range'] || req.headers['if-range'] === etag)) {
      const range = /^bytes=(\d*)-(\d*)$/.exec(req.headers.range);
      if (!range || (!range[1] && !range[2])) { res.writeHead(416, { ...headers, 'Content-Range': `bytes */${info.size}` }); return res.end(); }
      if (!range[1]) start = Math.max(0, info.size - Number(range[2]));
      else { start = Number(range[1]); if (range[2]) end = Math.min(end, Number(range[2])); }
      if (!Number.isSafeInteger(start) || !Number.isSafeInteger(end) || start < 0 || start > end || start >= info.size) {
        res.writeHead(416, { ...headers, 'Content-Range': `bytes */${info.size}` }); return res.end();
      }
      status = 206; headers['Content-Range'] = `bytes ${start}-${end}/${info.size}`;
    }
    headers['Content-Length'] = Math.max(0, end - start + 1);
    if (req.url?.includes('download=1')) headers['Content-Disposition'] = `attachment; filename="${relative.split('/').at(-1)}"`;
    res.writeHead(status, headers);
    if (req.method === 'HEAD' || !info.size) return res.end();
    const stream = createReadStream(path, { start, end });
    stream.on('error', () => res.destroy());
    res.on('close', () => stream.destroy());
    stream.pipe(res);
  }
  return http.createServer(async (req, res) => {
    res.setHeader('X-Content-Type-Options', 'nosniff');
    res.setHeader('Referrer-Policy', 'no-referrer');
    res.setHeader('X-Robots-Tag', 'noindex, nofollow, noarchive');
    res.setHeader('Content-Security-Policy', "default-src 'self'; script-src 'self'; style-src 'self'; media-src 'self' blob:; img-src 'self' data:; connect-src 'self'; object-src 'none'; frame-ancestors 'none'; base-uri 'self'; form-action 'self'");
    try {
      const url = new URL(req.url, 'http://localhost');
      const pathname = decodeURIComponent(url.pathname);
      if (pathname === `${base}/health`) return json(res, 200, { ok: true });
      if (pathname === base) { res.writeHead(308, { Location: `${base}/` }); return res.end(); }
      if (!pathname.startsWith(`${base}/`)) return json(res, 404, { error: 'Не найдено' });
      const route = pathname.slice(base.length);
      if (['/api/unlock', '/api/logout'].includes(route) && req.method === 'POST') {
        const expectedOrigin = publicOrigin ?? `http://${req.headers.host}`;
        if (req.headers.origin && req.headers.origin !== expectedOrigin) return json(res, 403, { error: 'Недопустимый источник' });
        if (route === '/api/logout') return json(res, 200, { ok: true }, { 'Set-Cookie': cookie('') });
        const ip = req.headers['x-forwarded-for']?.split(',')[0]?.trim() ?? req.socket.remoteAddress;
        const attempt = attempts.get(ip);
        if (attempt && attempt.until > Date.now() && attempt.count >= 15) return json(res, 429, { error: 'Повтори через минуту' });
        let input = '';
        for await (const chunk of req) { input += chunk; if (input.length > 2048) return json(res, 413, { error: 'Слишком большой запрос' }); }
        let supplied;
        try { supplied = JSON.parse(input).key; } catch { return json(res, 400, { error: 'Неверный запрос' }); }
        if (typeof supplied !== 'string' || !same(supplied, key)) {
          if (attempts.size > 1000) attempts.clear();
          attempts.set(ip, { count: attempt && attempt.until > Date.now() ? attempt.count + 1 : 1, until: Date.now() + 60000 });
          return json(res, 401, { error: 'Ключ не подошёл' });
        }
        attempts.delete(ip);
        const token = `${Math.floor(Date.now() / 1000) + 2592000}.${randomBytes(18).toString('base64url')}`;
        return json(res, 200, { ok: true }, { 'Set-Cookie': cookie(`${token}.${sign(token)}`) });
      }
      if (!['GET', 'HEAD'].includes(req.method)) return json(res, 405, { error: 'Метод не поддерживается' }, { Allow: 'GET, HEAD' });
      if (route === '/api/session') return json(res, 200, { authenticated: authenticated(req) });
      if (route.startsWith('/api/') || route.startsWith('/media/')) {
        if (!authenticated(req)) return json(res, 401, { error: 'Открой персональную ссылку доступа' });
        if (route === '/api/collections') {
          const folders = await readdir(join(dataRoot, 'collections')).catch(() => []);
          const collections = [];
          for (const id of folders.filter(safeId)) {
            const manifest = await readJson(join(dataRoot, 'collections', id, 'manifest.json')).catch(() => null);
            if (manifest && manifest.schema === 1 && manifest.id === id) collections.push({ id, title: manifest.title, description: manifest.description,
              createdAt: manifest.createdAt, caseCount: manifest.cases.length });
          }
          collections.sort((a, b) => b.createdAt.localeCompare(a.createdAt));
          return json(res, 200, { collections });
        }
        const collection = /^\/api\/collections\/([a-z0-9_-]+)$/.exec(route);
        if (collection && safeId(collection[1])) {
          const manifest = await readJson(join(dataRoot, 'collections', collection[1], 'manifest.json')).catch(() => null);
          return manifest ? json(res, 200, manifest) : json(res, 404, { error: 'Подборка не найдена' });
        }
        const media = /^\/media\/([a-z0-9_-]+)\/([a-f0-9]{24}\.[a-z0-9]+)$/.exec(route);
        if (media && safeId(media[1])) return await file(req, res, join(dataRoot, 'collections', media[1], 'media'), media[2], true);
        return json(res, 404, { error: 'Не найдено' });
      }
      const assets = { '/': 'index.html', '/app.js': 'app.js', '/style.css': 'style.css' };
      if (assets[route]) return await file(req, res, publicRoot, assets[route]);
      return json(res, 404, { error: 'Не найдено' });
    } catch (error) {
      if (!res.headersSent) json(res, error instanceof URIError ? 400 : 500, { error: 'Не удалось обработать запрос' });
      else res.destroy();
      console.error('request_failed', error.code ?? error.name);
    }
  });
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const port = Number(process.env.PORT ?? 3048);
  const host = process.env.HOST ?? '127.0.0.1';
  const server = makeServer();
  server.listen(port, host, () => console.log(`Listen ready at http://${host}:${port}${process.env.BASE_PATH ?? '/listen'}/`));
  for (const signal of ['SIGINT', 'SIGTERM']) process.on(signal, () => server.close(() => process.exit(0)));
}
