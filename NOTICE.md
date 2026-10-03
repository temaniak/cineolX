# Source provenance

Cineol-X 224 was separated from local native-plugin work in the Reflexion
workspace. The new repository has its own history and does not carry that
workspace's private carrier-board code or commit history.

ChatGPT was actively used during the creation of this project, assisting with
code development, debugging, testing and documentation.

- Reflexion: https://github.com/joelanders/reflexion-224, pinned in
  `dependencies.json`. Runtime import and validation use its 8080 host and row
  machine; the plugin uses its host-rate bridge. `patches/reflexion-scheduler.patch`
  preserves the local bounded scheduler change in build-only exports.
- JUCE: https://github.com/juce-framework/JUCE, version 8.0.14.
- libDaisy: https://github.com/electro-smith/libDaisy, pinned in
  `dependencies.json`. The QSPI bootstrap/linker layout uses libDaisy startup
  conventions; `patches/libdaisy-qspi-init.patch` initializes the HAL QSPI handle
  before deinitialization in a bootloader-launched RAM application.
- DSP analog/filter/rate modeling and program topology were developed from
  the original local native implementation and the Reflexion reference.
- The panel and fader textures were generated with ImageGen for the approved
  Cineol-X 224 interface; interactive elements are rendered by JUCE.

Dependency license notices remain in their upstream repositories, including the
two 8080 component notices in Reflexion. The pinned Reflexion tree has no
project-wide license file. A project-wide license for the new Cineol-owned
files has not been selected. This notice does not grant rights to dependency
code, third-party firmware or extracted data.
