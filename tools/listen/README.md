# Listen

A reusable private listening library on the user's VPS. Audio comparisons,
comments and settings are data; the player is deployed once and reused.

Production: `https://45-128-204-208.sslip.io/listen/`
SSH: the existing `nukkiez-vps` alias, using its SSH key. Server state lives in
`/home/tunnel/nk-listen`. No server password is needed for routine publication.
The production access key is generated on the server in `server.env` (mode 0600).
Do not copy that file into the checkout, logs or a collection manifest.

## Publish audio

When the user says “upload these to streaming” / “закинь на стриминг”, reuse this
tool rather than writing a new page. Publish only the assets selected by the
current request. Return the collection URL. The user's private entry link is
separate and opens the same library from another device.

For the prepared LexIR campaign:

The current LexIR source has Decay Opt disabled in every Cineol render and preset
following the user's November 4 listening diagnosis. Its export policy must not
re-enable Opt based only on envelope-fit scores. The old diagnostic collection
and superseded audio were removed at the user's explicit request; routine
publication still retains existing media unless deletion is separately requested.

```sh
python3 tools/listen/publish.py \
  --lexirs build/validation/lexirs-20261104/listening \
  --id lexicon-224xl-2026-11-04 \
  --title 'Cineol × Lexicon 224XL' --deploy
```

For any new comparison, create a manifest beside the source audio:

```json
{
  "schema": 1,
  "id": "my-comparison-2026-11-04",
  "title": "My comparison",
  "description": "What is being compared",
  "defaultCase": "example-1",
  "cases": [{
    "id": "example-1",
    "title": "Example 1",
    "subtitle": "Percussion and stopped chords",
    "notes": ["Compare the attack.", "Listen after the input stops."],
    "markers": [{"time": 4.5, "label": "Input stops"}],
    "settings": [{"label": "Decay", "value": "2.0 s"}],
    "variants": [
      {"id": "A", "label": "Reference", "file": "reference.flac"},
      {"id": "B", "label": "Candidate", "file": "candidate.flac", "preset": "candidate.cineol224"}
    ],
    "sections": [{"title": "More context", "paragraphs": ["Additional comments."]}]
  }]
}
```

```sh
python3 /Users/evgeniystrukov/reflexion-224/tools/listen/publish.py \
  --manifest /absolute/path/to/collection.json --deploy
```

Omit `--deploy` to prepare a local package for review. File paths are relative
to the input JSON, or absolute. Requires Python + NumPy, FFmpeg, SSH and rsync.
Supported audio: WAV, FLAC, MP3, M4A, Ogg and Opus. Use already level-matched
audio for fair A/B comparisons: publication does not normalize, EQ, resample
or recompress the listening files. The lower-rate decode is **only** for a
640-point waveform; original audio bytes and SHA-256 hashes are retained.

Per-variant `settings` and downloadable presets can differ. Per-case notes,
markers, properties, download links and expandable text/link sections change
the presentation without replacing the player. For a genuinely new kind of
panel, extend the section renderer in `public/app.js` and retain the schema.

## Publication behavior

- Content-addressed media, checked on the VPS before activation.
- SSH upload uses the existing key; audio is served over HTTPS with byte ranges.
- Manifest replacement is atomic. A normal upload never restarts the player.
- Old manifests and already uploaded media are retained. There is no `--delete`.
- The HTTPS route is checked against the currently mounted Caddyfile on each
  publication, so a later infrastructure deployment can be repaired by rerunning
  the publisher. Unexpected proxy structure is refused rather than guessed.
- No ROMs, banks or raw IR archives are accepted as audio/preset uploads.
- No public index: collection metadata, audio and presets all require a signed
  HttpOnly session. The private entry key is in a URL fragment, exchanged over
  HTTPS and immediately removed from the address bar. It is not a server password.
- Session lasts 30 days. The private entry link can reopen access on a new device.

## Update the reusable app

```sh
node --test tools/listen/server.test.mjs
python3 tools/listen/deploy.py
```

The app runs in the separate `nk-listen` official Node 24 Alpine container on
the existing Caddy edge network, with no public host port. Its app/data mounts
are read-only. Previous app release/container and the previous Caddyfile are
retained for rollback. The candidate Caddyfile is validated before a graceful
reload; existing sites keep their routes. The actual Node-container tests run
before app deployment succeeds.

`target.json` holds non-secret deployment coordinates. `ensure_proxy.py` owns
only its marked `/listen` handler in the existing APP_DOMAIN block. It discovers
the current bind mount through Docker instead of assuming a release directory.

## Local preview

Prepare a collection without `--deploy`, then place it (or a symlink to it)
under `build/listen/data/collections/<id>`. Run with a **test-only** access key:

```sh
ACCESS_KEY=local-listen-preview-test-key-2026 \
  DATA_ROOT="$PWD/build/listen/data" node tools/listen/server.mjs
```

Open `http://127.0.0.1:3048/listen/#key=local-listen-preview-test-key-2026`.
Production uses its independently generated key and Secure cookies.

Design reference and selected layout: `design/concept.png`, `design/SPEC.md`.
Implementation is buildless HTML/CSS/modules with no frontend dependencies.
Real browser playback/seek, keyboard controls, two/three-variant cases, mobile
layout and protected production access are required checks before handoff.

Technical references: [MDN byte ranges](https://developer.mozilla.org/en-US/docs/Web/HTTP/Guides/Range_requests),
[media currentTime](https://developer.mozilla.org/en-US/docs/Web/API/HTMLMediaElement/currentTime),
[Caddy reload](https://caddyserver.com/docs/command-line#caddy-reload).
