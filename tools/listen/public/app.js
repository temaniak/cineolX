const $ = id => document.getElementById(id);
const base = new URL('./', location.href).pathname.replace(/\/$/, '');
const store = { get(key, fallback) { try { return localStorage.getItem(`listen:${key}`) ?? fallback; } catch { return fallback; } },
  set(key, value) { try { localStorage.setItem(`listen:${key}`, String(value)); } catch {} } };
const state = { collection: null, item: null, players: new Map(), active: 'B', playing: false, position: 0, generation: 0,
  duration: 14, volume: Math.min(1, Math.max(0, Number(store.get('volume', '.8')) || .8)), muted: false };
const node = (tag, cls, text) => { const value = document.createElement(tag); if (cls) value.className = cls; if (text != null) value.textContent = text; return value; };
const clock = n => `${Math.floor(Math.max(0, n) / 60)}:${String(Math.floor(Math.max(0, n) % 60)).padStart(2, '0')}`;
const cueClock = n => { const fraction = (n - Math.floor(n)).toFixed(2).slice(1).replace(/0+$/, ''); return clock(n) + (fraction === '.' ? '' : fraction); };
const downloadIcon = '<svg viewBox="0 0 24 24"><path d="M12 3v12m-5-5 5 5 5-5M4 16v5h16v-5"/></svg>';
function safeUrl(value) { const url = new URL(value, location.href); if (url.origin === location.origin || url.protocol === 'https:') return url.href; throw new Error('Недопустимая ссылка'); }
function mediaUrl(value) { const url = new URL(value, location.href); if (url.origin !== location.origin || !url.pathname.startsWith(`${base}/media/`)) throw new Error('Недопустимый адрес аудио'); return url.href; }
async function api(path, body) {
  const response = await fetch(`${base}/api${path}`, { credentials: 'same-origin', ...(body == null ? {} : { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }) });
  const data = await response.json();
  if (!response.ok) { const error = new Error(data.error ?? 'Не удалось загрузить'); error.status = response.status; throw error; }
  return data;
}
function error(message) { $('error').textContent = message; $('error').hidden = !message; }
function status(text = 'Переключение сохраняет позицию') { $('player-status').textContent = text; }
function showLogin() { pause(); $('workspace').hidden = true; $('login').hidden = false; $('logout').hidden = true; }
async function unlock(key) { await api('/unlock', { key }); $('access-key').value = ''; $('login-error').textContent = ''; await loadLibrary(); }
$('unlock-form').addEventListener('submit', async event => { event.preventDefault(); try { await unlock($('access-key').value.trim()); } catch (e) { $('login-error').textContent = e.message; } });
$('logout').addEventListener('click', async () => { await api('/logout', {}); clearPlayers(); showLogin(); });
$('help-toggle').addEventListener('click', () => $('help').showModal());
$('help-close').addEventListener('click', () => $('help').close());
$('help').addEventListener('click', event => { if (event.target === $('help')) $('help').close(); });
$('library-toggle').addEventListener('click', () => $('workspace').classList.toggle('library-open'));
$('collection').addEventListener('change', () => loadCollection($('collection').value).catch(e => error(e.message)));
$('search').addEventListener('input', renderList);

async function loadLibrary() {
  const { collections } = await api('/collections');
  $('login').hidden = true; $('logout').hidden = false; $('workspace').hidden = false;
  $('collection').replaceChildren(...collections.map(item => { const option = node('option', '', item.title); option.value = item.id; return option; }));
  if (!collections.length) { $('empty').hidden = false; $('comparison').hidden = true; return; }
  const requested = new URLSearchParams(location.search).get('collection') ?? store.get('collection', '');
  const selected = collections.some(c => c.id === requested) ? requested : collections[0].id;
  await loadCollection(selected);
}
async function loadCollection(id) {
  pause(); error('');
  const data = await api(`/collections/${encodeURIComponent(id)}`);
  if (data.schema !== 1 || !Array.isArray(data.cases)) throw new Error('Формат подборки не поддерживается');
  state.collection = data; $('collection').value = id; store.set('collection', id); $('search').value = '';
  $('collection-count').textContent = `${data.cases.length} ${data.cases.length === 1 ? 'пример' : 'примеров'}`;
  const requested = new URLSearchParams(location.search).get('case');
  const selected = data.cases.find(c => c.id === requested) ?? data.cases.find(c => c.id === data.defaultCase) ?? data.cases[0];
  $('empty').hidden = !!selected; $('comparison').hidden = !selected;
  if (selected) selectCase(selected.id);
}
function renderList() {
  if (!state.collection) return;
  const query = $('search').value.trim().toLocaleLowerCase();
  const cases = state.collection.cases.filter(c => [c.title, c.group, ...(c.tags ?? [])].join(' ').toLocaleLowerCase().includes(query));
  $('case-list').replaceChildren(...cases.map(item => {
    const button = node('button', 'case-row', item.title); button.dataset.caseId = item.id; button.setAttribute('aria-current', String(state.item?.id === item.id));
    button.addEventListener('click', () => { selectCase(item.id); if (matchMedia('(max-width:850px)').matches) $('workspace').classList.remove('library-open'); }); return button;
  }));
  $('no-results').hidden = cases.length > 0;
}
function clearPlayers() {
  pause();
  for (const player of state.players.values()) { player.pause(); player.removeAttribute('src'); player.load(); }
  state.players.clear(); $('media').replaceChildren(); state.generation += 1;
}
function selectCase(id) {
  const item = state.collection.cases.find(c => c.id === id); if (!item) return;
  clearPlayers(); state.item = item; state.position = 0; state.duration = item.duration ?? item.variants[0]?.duration ?? 0;
  const wanted = store.get('variant', 'B'); state.active = item.variants.some(v => v.id === wanted) ? wanted : item.variants.find(v => v.id === 'B')?.id ?? item.variants[0].id;
  $('breadcrumb').textContent = state.collection.title; $('case-title').textContent = item.title; $('case-subtitle').textContent = item.subtitle ?? state.collection.subtitle ?? '';
  document.title = `${item.title} — Listen`; $('seek').max = state.duration; $('waveform').setAttribute('aria-valuemax', String(state.duration));
  const generation = state.generation;
  for (const variant of item.variants) {
    const audio = new Audio(); audio.preload = 'auto'; audio.src = mediaUrl(variant.url); audio.volume = state.volume; audio.muted = state.muted || variant.id !== state.active;
    audio.dataset.variant = variant.id; $('media').append(audio);
    audio.addEventListener('loadedmetadata', () => { if (generation !== state.generation) return; if (Number.isFinite(audio.duration) && variant.id === state.active) { state.duration = audio.duration; $('seek').max = audio.duration; renderTimeline(); } audio.currentTime = Math.min(state.position, Number.isFinite(audio.duration) ? audio.duration : state.position); });
    audio.addEventListener('waiting', () => { if (generation === state.generation && variant.id === state.active && state.playing) status('Буферизация…'); });
    audio.addEventListener('playing', () => { if (generation === state.generation && variant.id === state.active) status(); });
    audio.addEventListener('error', () => { if (generation === state.generation && variant.id === state.active) { pause(); status('Не удалось загрузить аудио. Обнови страницу или открой ссылку доступа заново.'); } });
    audio.addEventListener('ended', () => { if (generation !== state.generation || variant.id !== state.active) return; if ($('loop').checked) { seek(0); play(); } else { pause(); state.position = state.duration; renderProgress(); } });
    state.players.set(variant.id, audio);
  }
  $('variants').replaceChildren(...item.variants.map(variant => {
    const button = node('button', 'variant-button'); button.dataset.id = variant.id;
    button.append(node('strong', '', variant.id), node('span', 'variant-name', variant.label));
    button.setAttribute('aria-pressed', String(variant.id === state.active));
    button.addEventListener('click', () => selectVariant(variant.id)); return button;
  }));
  $('variants').classList.toggle('many', item.variants.length > 3);
  renderNotes(); renderVariantDetails(); renderList(); renderTimeline(); renderProgress(); status();
  const index = state.collection.cases.indexOf(item); $('previous').disabled = index === 0; $('next').disabled = index === state.collection.cases.length - 1;
  history.replaceState(null, '', `${location.pathname}?${new URLSearchParams({ collection: state.collection.id, case: item.id })}`);
  if ('mediaSession' in navigator && window.MediaMetadata) navigator.mediaSession.metadata = new MediaMetadata({ title: item.title, artist: state.collection.title, album: 'Listen' });
}
function renderNotes() {
  const paragraphs = state.item.notes ?? state.collection.notes ?? [];
  $('notes').replaceChildren(...paragraphs.map((text, index) => node('p', index > 1 ? 'note-detail' : '', text)));
  $('markers').replaceChildren(...(state.item.markers ?? state.collection.markers ?? []).map(marker => {
    const button = node('button', 'marker', `${cueClock(marker.time)} · ${marker.label}`); button.addEventListener('click', () => seek(marker.time)); return button;
  }));
  const sections = state.item.sections ?? [];
  $('extra-sections').replaceChildren(...sections.map(section => {
    const wrapper = node('details'); wrapper.append(node('summary', '', section.title));
    for (const text of section.paragraphs ?? []) wrapper.append(node('p', '', text));
    for (const link of section.links ?? []) { const a = node('a', '', link.label); a.href = safeUrl(link.url); if (new URL(a.href).origin !== location.origin) a.rel = 'noreferrer noopener'; wrapper.append(a); }
    return wrapper;
  }));
}
function renderVariantDetails() {
  const variant = state.item.variants.find(v => v.id === state.active);
  const properties = variant.settings ?? state.item.settings ?? [];
  $('settings').replaceChildren(...properties.map(property => { const wrapper = node('div', 'setting'); wrapper.append(node('dt', '', property.label), node('dd', '', property.value)); return wrapper; }));
  const links = [...(variant.downloads ?? []), { label: 'Скачать аудио', url: `${variant.url}?download=1` }, ...(state.item.downloads ?? [])];
  $('downloads').replaceChildren(...links.map(link => { const a = node('a', '', link.label); a.href = safeUrl(link.url); const icon = node('span'); icon.innerHTML = downloadIcon; a.prepend(icon); return a; }));
  for (const button of $('variants').children) button.setAttribute('aria-pressed', String(button.dataset.id === state.active));
  drawWaveform();
}
function selected() { return state.players.get(state.active); }
function updateVolumes() { for (const [id, audio] of state.players) { audio.volume = state.volume; audio.muted = state.muted || id !== state.active; } }
function selectVariant(id) {
  if (!state.players.has(id) || id === state.active) return;
  const position = selected()?.currentTime ?? state.position;
  state.active = id; state.position = position;
  const next = selected(); if (next.readyState >= 1 && Math.abs(next.currentTime - position) > .002) next.currentTime = position;
  updateVolumes(); store.set('variant', id); renderVariantDetails();
  if (state.playing) next.play().then(() => status()).catch(() => { pause(); status('Нажми воспроизведение'); });
  renderProgress();
}
async function play() {
  if (!state.item) return;
  if (state.position >= state.duration - .01) seek(0);
  const generation = state.generation; state.playing = true; status('Загрузка аудио…'); renderPlay();
  const outcomes = await Promise.allSettled([...state.players].map(async ([id, audio]) => { if (audio.readyState >= 1 && Math.abs(audio.currentTime - state.position) > .04) audio.currentTime = state.position; await audio.play(); return id; }));
  if (generation !== state.generation) return;
  if (selected().paused) { pause(); const activeIndex = [...state.players.keys()].indexOf(state.active); const reason = outcomes[activeIndex]?.reason; status(reason?.name === 'NotAllowedError' ? 'Нажми воспроизведение' : 'Аудио пока недоступно. Попробуй ещё раз.'); }
  else { status(); if ('mediaSession' in navigator) navigator.mediaSession.playbackState = 'playing'; }
}
function pause() { state.playing = false; for (const audio of state.players.values()) audio.pause(); renderPlay(); if ('mediaSession' in navigator) navigator.mediaSession.playbackState = 'paused'; }
function renderPlay() { $('play').setAttribute('aria-label', state.playing ? 'Пауза' : 'Воспроизвести'); $('play-icon').innerHTML = state.playing ? '<path d="M6 4h4v16H6zm8 0h4v16h-4z"/>' : '<path d="M8 4v16l13-8z"/>'; }
function seek(time) { state.position = Math.max(0, Math.min(Number(time), state.duration)); for (const audio of state.players.values()) if (audio.readyState >= 1) audio.currentTime = Math.min(state.position, Number.isFinite(audio.duration) ? audio.duration : state.position); renderProgress(); }
$('play').addEventListener('click', () => state.playing ? pause() : play());
$('restart').addEventListener('click', () => seek(0));
$('seek').addEventListener('input', event => seek(event.target.value));
$('volume').value = String(state.volume);
$('volume').addEventListener('input', event => { state.volume = Number(event.target.value); state.muted = false; store.set('volume', state.volume); updateVolumes(); });
$('mute').addEventListener('click', () => { state.muted = !state.muted; updateVolumes(); $('mute').setAttribute('aria-label', state.muted ? 'Включить звук' : 'Выключить звук'); $('mute').setAttribute('aria-pressed', String(state.muted)); });
$('loop').checked = store.get('loop', 'false') === 'true'; $('loop').addEventListener('change', event => store.set('loop', event.target.checked));
for (const [button, step] of [['previous', -1], ['next', 1]]) $(button).addEventListener('click', () => { const index = state.collection.cases.indexOf(state.item); const next = state.collection.cases[index + step]; if (next) selectCase(next.id); });
function renderTimeline() {
  const width = $('waveform').clientWidth; const budget = Math.max(2, Math.floor(width / 70));
  const step = [.1, .2, .5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600].find(n => n >= state.duration / budget) ?? Math.ceil(state.duration / budget);
  const times = Array.from({ length: Math.floor(state.duration / step) + 1 }, (_, i) => i * step);
  if (Math.abs(times.at(-1) - state.duration) > .001) { if (times.length > 1 && (state.duration - times.at(-1)) / state.duration * width < 48) times.pop(); times.push(state.duration); }
  $('timeline').replaceChildren(...times.map(time => { const tick = node('span', '', cueClock(time)); tick.style.left = `${time / Math.max(state.duration, .001) * 100}%`; return tick; }));
  $('duration').textContent = clock(state.duration);
}
function renderProgress() { $('elapsed').textContent = clock(state.position); $('seek').value = String(state.position); $('waveform').setAttribute('aria-valuenow', state.position.toFixed(2)); $('waveform').setAttribute('aria-valuetext', `${clock(state.position)} из ${clock(state.duration)}`); drawWaveform(); }
function drawWaveform() {
  const canvas = $('waveform'); const width = canvas.clientWidth; const height = canvas.clientHeight; if (!width || !height || !state.item) return;
  const scale = devicePixelRatio || 1; if (canvas.width !== Math.round(width * scale) || canvas.height !== Math.round(height * scale)) { canvas.width = Math.round(width * scale); canvas.height = Math.round(height * scale); }
  const ctx = canvas.getContext('2d'); ctx.setTransform(scale, 0, 0, scale, 0, 0); ctx.clearRect(0, 0, width, height);
  const variant = state.item.variants.find(v => v.id === state.active); const peaks = variant.waveform ?? [];
  const playhead = width * state.position / Math.max(state.duration, .001); const peakScale = state.item.waveformScale ?? Math.max(.001, ...peaks);
  ctx.strokeStyle = '#303947'; ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(0, height / 2); ctx.lineTo(width, height / 2); ctx.stroke();
  if (peaks.length) {
    ctx.beginPath(); for (let i = 0; i < peaks.length; i++) { const x = i / Math.max(1, peaks.length - 1) * width; const y = height / 2 - Math.max(.4, peaks[i] / peakScale * height * .45); if (!i) ctx.moveTo(x, y); else ctx.lineTo(x, y); }
    for (let i = peaks.length - 1; i >= 0; i--) ctx.lineTo(i / Math.max(1, peaks.length - 1) * width, height / 2 + Math.max(.4, peaks[i] / peakScale * height * .45));
    ctx.closePath(); ctx.fillStyle = state.active === 'C' ? '#91bc94bb' : '#859bedd9'; ctx.fill();
  }
  ctx.strokeStyle = '#f4f5f8'; ctx.lineWidth = 1.5; ctx.beginPath(); ctx.moveTo(playhead, 0); ctx.lineTo(playhead, height); ctx.stroke();
  ctx.beginPath(); ctx.arc(Math.min(width - 4, Math.max(4, playhead)), 4, 4, 0, Math.PI * 2); ctx.fillStyle = '#fff'; ctx.fill();
}
new ResizeObserver(() => { drawWaveform(); if (state.item) renderTimeline(); }).observe($('waveform'));
let dragging = false;
function waveformSeek(event) { const rect = $('waveform').getBoundingClientRect(); seek((event.clientX - rect.left) / rect.width * state.duration); }
$('waveform').addEventListener('pointerdown', event => { dragging = true; $('waveform').setPointerCapture(event.pointerId); waveformSeek(event); });
$('waveform').addEventListener('pointermove', event => { if (dragging) waveformSeek(event); });
$('waveform').addEventListener('pointerup', () => { dragging = false; });
$('waveform').addEventListener('pointercancel', () => { dragging = false; });
$('waveform').addEventListener('keydown', event => { if (['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) { event.preventDefault(); seek(event.key === 'Home' ? 0 : event.key === 'End' ? state.duration : state.position + (event.key === 'ArrowLeft' ? -.5 : .5)); } });
document.addEventListener('keydown', event => { if (event.ctrlKey || event.metaKey || event.altKey || /INPUT|TEXTAREA|SELECT/.test(document.activeElement?.tagName) || $('help').open || $('workspace').hidden) return;
  if (event.code === 'Space') { event.preventDefault(); state.playing ? pause() : play(); }
  else if (['a', 'b', 'c'].includes(event.key.toLowerCase())) selectVariant(event.key.toUpperCase());
});
let lastSync = 0;
function tick(now) {
  if (state.playing && selected()) {
    state.position = selected().currentTime;
    if (now - lastSync > 1000) { for (const [id, audio] of state.players) if (id !== state.active && audio.readyState >= 2 && !audio.paused && Math.abs(audio.currentTime - state.position) > .06) audio.currentTime = state.position; lastSync = now; }
    renderProgress();
  }
  requestAnimationFrame(tick);
}
requestAnimationFrame(tick);
if ('mediaSession' in navigator) { navigator.mediaSession.setActionHandler('play', play); navigator.mediaSession.setActionHandler('pause', pause); navigator.mediaSession.setActionHandler('seekto', info => seek(info.seekTime)); }
async function init() {
  const key = new URLSearchParams(location.hash.slice(1)).get('key');
  if (key) { history.replaceState(null, '', location.pathname + location.search); try { await unlock(key); return; } catch (e) { showLogin(); $('login-error').textContent = e.message; return; } }
  try { const session = await api('/session'); if (session.authenticated) await loadLibrary(); else showLogin(); }
  catch (e) { showLogin(); $('login-error').textContent = e.message; }
}
init();
