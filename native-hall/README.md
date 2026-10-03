# Native 224 engine

The portable C++17 engine implements four network topologies and six original
224 v4.4 programs. The JUCE plugin and generic Daisy adapter share the engine
and the data-preparation importer. See the [main README](../README.md) for
supported ROMs, build commands, controls, cache handling and known DSP limits.

`ProgramBank` occupies 123,460 bytes, plus its 20-byte file header. It must stay
alive while the engine uses it. `Engine48` occupies less than 64 KiB and uses
fixed buffers. Embedded state should be placed statically, not on the callback
stack. Keep bank preparation and file operations outside audio processing.
