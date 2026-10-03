# Validation of the separated repository

The following checks were run from this repository on macOS/Apple silicon.

- AUv2, VST3 and Standalone 0.5.1 built with no source ROM directory available.
  The empty-cache check passed with dry audio and zero audio-thread allocation
  or deallocation.
- Desktop `--check` passed: six program networks, automation/state, host rates,
  import cancellation, successful import and a separate cached restart.
- Six independent network checks each covered 12,000 full-scale stereo frames
  and 47 parameter sets with exact outputs and saturation diagnostics.
- The ROM bank SHA-256 is
  `15d14dcebbbafad35269e8159ddfedfb24ca9fdd203531b3d85cf094ce6e72ed`,
  matching the existing native implementation's bank byte for byte.
- Generic Daisy Seed and Patch SM QSPI images built and passed ITCM, AXI SRAM,
  DTCM, DMA and reset-vector placement checks. ARU edge arithmetic remained
  inlined in the hot network.
- Logical control checks covered normalized, 0..3.3 V, 0..5 V and -5..+5 V
  ranges, inversion, all ten targets, both buttons, the two-button chord,
  distinct program RGB colors and stereo processing under program changes.
- Simulated hardware tests covered ten ADC channels, voltage scale/offset,
  independent button/RGB polarity, exactly two input and three output GPIOs,
  and no runtime waits. No button LED outputs are exposed.
- Build/flash rejection checks covered absent, empty, incomplete, modified and
  wrong-size ROM sets, including cached products and renamed valid chips.
- The source audit checked staged content for private hardware details,
  embedded ROM bytes, generated products, workstation paths, non-English
  documentation and accidentally included review translations.

Dependency checkouts were unchanged; compatibility patches were applied to
ignored build exports. These results establish software/build checks, not
physical board wiring or realtime CPU margin. No module was flashed as part of
this repository preparation. Public pin defaults are unassigned. Non-macOS
desktop builds remain unverified.
