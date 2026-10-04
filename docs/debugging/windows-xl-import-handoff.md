# Windows XL ROM import crash: handoff

Prepared on 2026-10-04 for continuing this investigation on a Windows machine.
Read this document and `AGENTS.md` before making changes. The user communicates
in Russian; repository source and documentation must remain in English.

## Objective and user authorization

Find and fix the Windows host crash during the first import of original
Lexicon 224XL v8.21 ROMs into Cineol-X 224. The published **0.9.6 Fixed** build
still crashes Ableton on the user's Windows machine. Earlier users reported
similar crashes in Studio One on Windows 11. Original 224 v4.4 import reportedly
works for those users, while XL presets remain unavailable after a failed import.

The user previously authorized publishing the corrected build as the same
version, with a Fixed suffix. Do not publish another speculative fix. Establish
the failure location and validate the actual supported-ROM import on Windows.
The user has now offered to continue the investigation locally on Windows.

An earlier request concerned slow **first import**, rather than loading an
already cached bank. That work was stopped when the crash was reported. No
performance improvement has been implemented. Diagnose the crash first.

## Repository and release identity

- Primary repository: https://github.com/temaniak/cineolX
- Current product version: **0.9.6**, not 9.6.0.
- Release: https://github.com/temaniak/cineolX/releases/tag/v0.9.6
- Published fixed source commit: `e42f6c9d1c3a5058537790cf06817315cc2ca16d`.
- The original `v0.9.6` tag was retained at its original commit. The replaced
  release archives identify the fixed source commit in `BUILD-INFO.txt`.
  Checking out the old tag does **not** retrieve the fixed source.
- Diagnostic handoff branch: `debug/windows-xl-runtime`. This branch adds
  diagnostics and this document; it has not replaced any release assets.
- Previous fixed build CI: https://github.com/temaniak/cineolX/actions/runs/37217997162
  Both Windows x64 and macOS universal jobs passed, but did not execute a full
  import with real, supported ROMs.
- JUCE: 8.0.14. Reflexion pin:
  `f68ea1d069fef4a5663201693bfdfa1c579ffd69`. See `dependencies.json` for all pins.

Keep plugin identifiers and parameter IDs stable. Keep the audio callback free
of allocations, file/network I/O, locks and waits. Never commit or upload ROMs,
prepared banks, captures or firmware images. Dependency checkouts must stay
clean; apply dependency changes to `patches/` and export them into ignored
`build/` using `script/prepare_dependency.py`. Do not use MCU builds as evidence
of desktop audio CPU margin.

The repository's historical AGENTS support statement only names original 224;
the current source and the user's explicit requested investigation also cover
XL v8.21. Use the firmware identities already recognized by the current source.

## Observed crash evidence

The user reproduced the crash using the **Fixed** Windows build, **224XL v8.21**
and **Ableton Live 12 Intro**. They described the visible text as "prepared".
Do not assume preparation completed: the plugin normally displays
`Preparing <stage>...`, and the exact stage was not recorded.

The supplied Application log contains two crashes on 2026-10-04, at 20:09:29
and 20:11:15, with the same signature:

```text
Application:       Ableton Live 12 Intro.exe
Faulting module:   ucrtbase.dll
Module version:    10.0.19041.3636
Fault offset:      0x000000000007286e
Exception code:    0xc0000409
WER event:         BEX64
WER parameter P9:  0000000000000007
```

`0xc0000409` with subcode 7 is consistent with
`FAST_FAIL_FATAL_APP_EXIT`, including an `abort()` or `terminate()` path. It does
not itself prove a stack buffer overflow, a broken CRT installation, or which
component initiated termination. A call stack is needed to locate the caller.
Microsoft explanation:
https://devblogs.microsoft.com/oldnewthing/20230731-00/?p=108505

No Ableton crash ZIP, minidump or debugger stack has been examined yet. After the
next launch following a crash, Ableton puts its report in
`%APPDATA%\Ableton\Live Reports`. Inspect such material locally; do not add it
to the public repository. Official report guidance:
https://help.ableton.com/hc/en-us/articles/209071629-Where-to-find-Crash-Reports

## What the published Fixed build already changed

1. `native-hall/plugin/Editor.cpp`: separate native file and directory choosers,
   explicit ROM overlay opening, Close button, visible busy/error status even
   when 224 is already available, and closing the overlay when import succeeds.
   JUCE's Windows native chooser with files and directories both enabled was
   effectively selecting folders only.
2. `native-hall/plugin/RomBank.cpp`: recognized chip counts, unsupported known
   firmware rejection and clearer errors. Original 224 requires five v4.4 chips;
   XL requires eleven v8.21 chips. XL v8.1A is rejected. Content hashes, rather
   than filenames, identify chips; see the pinned firmware catalog.
3. `native-hall/import/xl_display.hpp`: moved the display formatter's 64 KiB
   scratch copy out of the coroutine frame into caller-owned heap storage.
   `xl_import.cpp` allocates that storage before starting the operator tasks.
   This removed one potential frame-size abort, but did not establish the
   Windows crash's actual cause.
4. `native-hall/tests/rom_import_check.cpp`: synthetic invalid ROM, truncated
   file, broken ZIP, unknown/duplicate ZIP entries, Unicode filename, folder,
   missing file, retry and cached-224 UI regressions. The existing `--empty`
   mode runs them on public CI without private firmware.

Mac validation of that published fixed source included a full real XL import,
all 22 programs, byte-identical comparison to an earlier reference bank,
adding XL alongside cached 224 without changing 224, cached XL restart, and
display checks. Audio allocation/deallocation checks passed. These results do
not prove supported-ROM Windows import is safe.

## New diagnostics on the handoff branch

The three edited source files are:

- `native-hall/import/xl_import.hpp`
- `native-hall/import/xl_import.cpp`
- `native-hall/tests/rom_import_check.cpp`

`check_preparation_runtime()` constructs the actual Engine, Machine, bank and
display scratch objects, then creates the real `prepare_programs`,
`prepare_displays`, `selectProgram`, `readPages`, `moveSlider`, `setToggle` and
`gotoPage` tasks. It tests cancellation at the first progress callback, before
any firmware execution, and verifies all coroutine frames are released.

The console regression executable runs this probe in a JUCE background thread
under `--runtime-check`, and before the existing `--empty` checks. Stage messages
are flushed so an abort still leaves its last stage and stderr message visible.
The test exercises the compiler-specific frame layout and normal JUCE worker
thread stack without requiring ROMs. Clang can elide some suspended task
allocations, so the probe accepts elision and measures the actual cancellation
execution as well.

The new source was built as macOS universal Release. Running the probe locally
on arm64 passed and reported:

```text
XL runtime: largest frame=16168, peak frames=1; pass
```

The complete `--empty` regression also passed locally with an isolated cache,
including the new runtime probe and the earlier invalid-input/UI tests:
`--empty: pass; audio new=0, delete=0`.

The upstream pool payload limit is **16368 bytes** (16384-byte block minus
16-byte header): this leaves only **200 bytes** on that compiler/architecture.
This is a strong reason to check the MSVC frame layout, not proof that MSVC
exceeds the limit. The new Windows probe has not been run yet, and no new CI
run was dispatched for this branch before the handoff.

The new diagnostics do not change production import behavior or sound, and
have not been published as a new fixed release.

## Important abort paths and files

- `deps/reflexion/juce-plugin/source/operator/task.hpp`:
  `fatal()` prints `lexplug operator: ...` to stderr and calls `std::abort()`.
  `FramePool` has 16 blocks of 16384 bytes and aborts on oversized frames,
  exhaustion or destruction with live frames. Creating a coroutine outside
  `PoolScope` aborts. `PromiseBase::unhandled_exception()` also aborts when a C++
  exception escapes a coroutine.
- `deps/reflexion/juce-plugin/source/operator/machine.hpp`: fixed queues
  (64 actions, 8 waiters, 4 roots), fatal invariant checks, `run_task()` and the
  pool. `Machine` has sizable inline silent/render buffers and is currently a
  local variable during preparation. Its pool storage is allocated on the heap.
- `deps/reflexion/juce-plugin/source/operator/larc_operator.hpp`:
  `recordBase()` aborts if a firmware parameter record cannot be found.
- `patches/reflexion-scheduler.patch`: allocation-free event captures and a
  pre-reserved 64-event queue. Overflow currently calls `std::abort()`.
- `native-hall/import/xl_import.cpp`: actual preparation root and child tasks,
  firmware observers and native profile validation. Some observers can throw.
- `native-hall/plugin/RomBank.cpp`: background worker and an outer try/catch.
  That catch can report ordinary exceptions, but cannot intercept `abort()` or
  the coroutine promise's own abort-on-unhandled-exception policy.

Other candidates include an exception inside a coroutine, a scheduler/operator
invariant, or a Windows stack/temporary-object issue. No candidate is confirmed.
Do not merely increase a pool size or replace system DLLs without evidence.

## Suggested Windows continuation

1. Verify this branch and a clean baseline. Initialize only the pinned desktop
   dependency: `git submodule update --init deps/reflexion`. Keep local private
   ROMs outside tracked source; use an isolated cache for all console tests.
2. Use Visual Studio 2022 C++ build tools/Windows SDK, CMake, Python and Git.
   The existing PowerShell build helper configures x64 and sets up Git's
   `patch.exe` path for dependency export:

   ```powershell
   ./script/build_plugin.ps1 -Jobs 4
   ```

   It now runs the new runtime probe through `--empty`. If it aborts, preserve
   its final stage and the `lexplug operator: ...` stderr message. Build failure
   due to a failing check is useful evidence; inspect the already-built exe.

3. Run the probe independently if needed:

   ```powershell
   $cineolCheck = Join-Path $PWD 'build/windows/cineol_rom_import_check_artefacts/Release/cineol_rom_import_check.exe'
   & $cineolCheck --runtime-check
   ```

4. If the probe passes, run the actual importer locally with the user's known
   eleven v8.21 ROMs. The existing `--cancel SOURCE` mode exercises a real
   background import without requiring a reference bank. Use a fresh isolated
   cache; the default cache must not be disturbed:

   ```powershell
   $env:CINEOL224_CACHE_DIR = Join-Path $PWD ('build/xl-crash-cache-' + [guid]::NewGuid())
   & $cineolCheck --cancel 'C:\PATH\TO\PRIVATE\224XL-v8.21'
   ```

   The cancellation command waits for import progress; it may already execute
   firmware before cancellation. Record the actual last stage. It is not a full
   successful-import test.

5. For a full isolated import/reference test, enable desktop tools in the
   existing build directory and build `cineol_xl_extract`:

   ```powershell
   cmake -S . -B build/windows -DNATIVE_HALL_BUILD_TOOLS=ON
   cmake --build build/windows --config Release --parallel 4 --target cineol_xl_extract
   & './build/windows/native-hall/Release/cineol_xl_extract.exe' 'C:\PATH\TO\PRIVATE\224XL-v8.21' './build/xl-reference.bankxl'
   & $cineolCheck --import-xl 'C:\PATH\TO\PRIVATE\224XL-v8.21' (Join-Path $PWD 'build/xl-reference.bankxl')
   & $cineolCheck --cached-xl
   ```

   Use an empty isolated cache before `--import-xl`. The extractor itself may
   expose the failure earlier, with console diagnostics. A reference generated
   by this same build validates import/publication consistency; for unchanged
   sound, also compare against the user's previous trusted bank if available.
   Inspect `--add-xl` in the test source for preserving an existing 224 cache.

6. If an abort remains unexplained, inspect the Ableton report/dump or launch
   the console test under a native debugger. Break on `ucrtbase!abort` and
   `ucrtbase!terminate`, then collect the caller stack. WinDbg commands include
   `bu ucrtbase!abort`, `bu ucrtbase!terminate`, `g`, `k`, and `!analyze -v`.
   Retain optimized Release behavior when adding debug symbols: changing
   optimization can also change coroutine frame size and hide the failure.
7. Apply the smallest evidenced fix. Add a regression for the actual failure,
   rerun supported-ROM Windows import, cached restart and adding XL to 224, and
   verify audio remains allocation-free. Keep private data local. Run
   `python script/audit_source.py` on staged source before pushing.
8. Report the concrete cause and the checks that actually ran. Only then prepare
   updated 0.9.6 Fixed assets. The earlier OAuth token could push ordinary source
   and dispatch workflows, but lacked `workflow` scope for workflow-file edits.
   The existing workflow already invokes `--empty`, so no workflow edit was
   needed to introduce the new diagnostic coverage.

No agent session or private attachment automatically transfers through Git.
This document transfers the investigation's known state; the Windows session
should read it, inspect the current source and continue from the runtime probe.
