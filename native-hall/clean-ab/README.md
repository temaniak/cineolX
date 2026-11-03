# Cineol Clean AB — fixed-point listening prototype

Separate AU, VST3 and Standalone with the seven approved original-224/XL
programs. It does not replace Cineol-X 224 or use the 48 kHz Float Hall.

- **A / Original**: current native engine, original converter quantization.
- **B / Clean**: the same row graphs and native clocks, with 12 additional
  fractional bits in integer signal memory/arithmetic and finer ADC/DAC
  conversion. Analog converter filters, coefficients, addresses, modulation
  rules and controller scheduling are retained. There is no output gate,
  noise reduction, widening EQ or change to native sample rates.
- Both branches process continuously. A/B uses a 10 ms crossfade and retains
  both tails. Changing the algorithm resets both branches.
- Shared controls and Input affect both branches. B Level Trim affects only B;
  the B wet RMS meter includes this trim. Output affects the final mix equally.
- Original controls use the common plugin defaults. XL faders start at the
  model's factory values; Previous/Next Page exposes its actual control pages.
  Reset Controls restores original default faders / XL factory faders and
  disables Mod, Optimization and Dynamic Decay. It keeps level trims.
- Dirt and Space are fixed at zero. The original converter profiles and native
  clocks are the comparison baseline. This prototype does not test those knobs.

The application reads existing local imports from Cineol-X 224. Import the
original 224 v4.4 and/or XL v8.21 in the main plugin first, then reopen Clean AB.
An unavailable model is muted with a visible instruction. This prototype never
writes the imported bank cache, and no ROMs or banks are distributed with it.

Start at 100% wet and 48 kHz. Compare during playing, after input stops, and
near the end of a tail. Match perceived active-signal loudness using B Level
Trim. The proposed cleanup intentionally changes quiet-tail behavior; listening
acceptance of preserved character remains open.

## Build

```sh
./script/build_clean_ab.sh
./script/build_clean_ab.sh --run
```

Outputs are under `build/clean-ab/native-hall/clean-ab/CineolCleanAB_artefacts/Release/`.
Use the AU/VST3 folders on macOS or the Standalone app. The wrapper uses the
existing host-rate bridge at other host sample rates. Windows is not validated;
the convenience script currently targets macOS formats.

The option `CINEOL_BUILD_CLEAN_AB` defaults to OFF. `generate_precision.py`
creates namespace-isolated copies of local core/desktop sources in the build
folder, with checked critical substitutions and a source hash manifest. No production or
external dependency checkout is edited. The selected fixed-point audio words
have 28 bits including sign (16 original plus 12 fractional); 64-bit containers
keep arithmetic and explicit signed wrapping well-defined. Controller
observations are reduced back to original control words, so controller decisions
near a level boundary can still differ when converter precision changes.

`cineol_clean_zero_check` compiles the export with zero additional bits and
original converters. Its seven-program, four-recipe check must match A exactly.
`cineol_clean_check` checks finite output, callback storage and the actual
precision variant. `cineol_clean_plugin_check` checks host rates, mono/stereo,
dry latency, A/B retention, state and UI construction. Both bank files are
required for these private audio checks; see the validation report for commands.

Validation, measurements and limitations:
[Clean A/B prototype report](../../docs/CLEAN_AB_PROTOTYPE_2026-11-02.md).
