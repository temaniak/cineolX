# Reusable private audio streaming — November 4, 2026

The user's requested workflow is deployed at
`https://45-128-204-208.sslip.io/listen/` on their existing VPS
`45.128.204.208`. Access uses a separate personal entry link. That secret is
not included in this report or the repository.

## Delivered behavior

- One reusable player with data-driven collections, examples and variants.
- First collection: 71 Cineol / Lexicon 224XL examples, 156 lossless audio
  variants and 85 preset references. Content addressing deduplicates these
  into 240 verified stored files.
- A/B/C preserves the selected time; inactive variants are muted. These are
  streamed HTML audio elements, not a claim of sample-accurate Web Audio switching.
- Real decoded waveform, seeking, loop, restart, volume/mute and keyboard controls.
- Per-example comments, timestamp cues, per-variant settings, downloadable
  audio/presets and expandable contextual sections.
- Searchable library and responsive phone layout.
- Reusable publisher for arbitrary JSON collections; no new HTML page per upload.
  An additional local fixture exercised custom text/settings and four variants.

Sources and operation: [tools/listen/README.md](../tools/listen/README.md).
The repository's AGENTS.md and non-secret workflow memory point future
“закинь на стриминг” requests to this publisher.

## Server and validation

The separate `nk-listen` container uses official Node 24 Alpine. Its source/data
mounts are read-only, and it exposes no public host port. Existing Caddy routes
`/listen` through the private Docker edge network. Caddy configuration was
backed up and validated before a graceful reload. Prior app releases and stopped
containers remain available for rollback. Normal audio uploads do not restart
the app: media is verified first, then the manifest is replaced atomically.

The existing key-based `nukkiez-vps` SSH alias handles subsequent publication.
The temporary server password supplied by the user was not saved in source,
configuration or memory. The independent listening access key was generated on
the server in its 0600 `server.env` file.

Verified results:

| Check | Result |
|---|---|
| Node server tests, including production Node container | Passed |
| Anonymous metadata and media requests | HTTP 401 |
| Entry-link exchange | Signed HttpOnly / Secure / SameSite session; fragment removed |
| HTTPS from the Mac's in-app browser | Loaded and played actual audio |
| Authenticated range request | HTTP 206, correct 64-byte FLAC prefix and Content-Range |
| Preset download | XML parsed; 74 parameters |
| Upload integrity | All 240 file sizes and SHA-256 hashes verified before activation |
| Existing main and money application health endpoints | Both HTTP 200 |
| Live browser playback | Three 14-second players ready, no media errors |
| Seek and A/B/C | Shared position retained; selected variant alone audible |
| Input-silent loop/end behavior | Loop restarted after the end; pause/restart worked |
| Keyboard | Native browser space and A/B shortcuts worked |
| Search / examples | Small Room V3 loaded its distinct notes and two-variant controls |
| Generic uploader | Different collection text, settings and four variants, without changing player code |
| Mobile viewport | 390 × 844; no horizontal overflow, sparse readable timeline, collapsible library |

The initial localhost test needed permission to bind a loopback port; it passed
under that permission. A locator-based keypress on the non-focusable `body`
was unsupported by the browser binding; native browser key input passed.
Neither issue required weakening the application or using a browser fallback.

Private logs, upload manifests, hashes and checks are retained in
`build/listen/`. The original comparison sources remain in their validation
directory. Local package copies use independent APFS clones to avoid another
full audio copy; `clone-review.json` records identical retained sources.
The disposable generic test package and superseded screenshot were removed;
their small input manifest/log and original source audio were retained.

## Visual fidelity review

Selected concept: [concept.png](../tools/listen/design/concept.png), generated
with the built-in Image Gen tool. Brief and design tokens are in
[SPEC.md](../tools/listen/design/SPEC.md).

The implementation was compared directly with the concept using `view_image`.
The final browser captures were also inspected with `view_image`:

- `build/listen/preview/listen-desktop.png`
- `build/listen/preview/listen-mobile.png`

Method: native Codex in-app browser, with the concept's 1536 × 1024 viewport
and a 390 × 844 phone viewport, using the browser's screenshot API. Full-page
captures include dynamic content below the viewport and exclude the browser's
15-pixel scrollbar. Temporary viewport overrides were reset. The published tab
is retained as the user-facing deliverable; the local development server is stopped.

| Compared feature | Concept / implementation evidence and resolution |
|---|---|
| Palette and framing | Charcoal canvas, slightly lighter flat sidebar, periwinkle selection and sage Clean text retained; no decorative card grid |
| Header and hierarchy | Listen wordmark, two navigation actions, lock, collection breadcrumb and large selected-example title retained |
| Sidebar | 344-pixel desktop rail, selector/search and flat list rows; imported examples reordered so the selected Concert Hall appears at the top |
| Variant controls | Equal-width A/B/C choices, strong selected outline and consistent labels; four-plus variants wrap into a reusable grid |
| Waveform and transport | Same waveform/timeline/transport organization; waveform uses real decoded source peaks, with functioning seek/playhead |
| Notes and settings | Open separated sections, four-column desktop / two-column phone properties, download links and consistent text hierarchy |
| Phone continuation | Collapsed library, full-width player and touch-sized choices; overcrowded timeline labels were repaired with responsive, correctly positioned ticks |

Visible-copy review: core navigation and control copy match the selected
concept. Intentional data-driven differences are the actual collection count,
complete list of examples, measured values, exact cue times, an additional
Clean comment, and expandable context. Previous/next buttons support the real
71-example library. The real waveform and initial time differ from the
concept's illustrative waveform/3-second position. Longer content increases
the full-page height instead of clipping it.

The final implementation was faithfully checked against the selected design
for these elements. No material visual or functional mismatch remains in the
tested desktop and mobile views. Physical iPhone/Safari testing and listening
acceptance of the reverb comparisons are not claimed by this web-delivery work.
