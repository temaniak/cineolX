# Sequential slider layout

November 2, 2026. Active controls on each page now fill the six variable columns
from left to right, preserving their relative firmware order. Empty/unavailable
slots follow them. Original-224 Diffusion moves from the fifth to the first
column on page 2. XL uses one visual-to-firmware-slot map for faders, captions,
values, help, ranges, factory resets and host attachments. Global controls keep
their dedicated columns. Compact mode uses the same ordered bindings.

The UI map does not renumber DAW parameters or alter prepared-bank records.
Page rebinding closes a physical fader's outstanding host gesture before
attaching another parameter. Inactive columns clear stale host-binding metadata.

Validation passes: all 28 programs and 127 pages in both full and compact modes
(254 page layouts), 34 moved XL binding observations, contiguous active columns,
aligned labels/values/faders, no host parameter events from navigation, balanced
rebind gestures, 641 endpoints, all-page help and bidirectional value dragging,
state restoration, original hidden controls and full dual-bank audio regression.
The core/desktop source hashes match the preceding accepted timing block;
navigation audio comparisons remain sample-identical. No DSP/CPU campaign is
needed for this presentation-only change.

Universal arm64/x86_64 AU, VST3 and Standalone are built and pass strict deep
signature verification. No installation, release or host-listening claim is made.
Private build/check logs, baseline UI/test source, hashes and before/after real
editor screenshots are retained in `build/validation/slider-order-20261102/`.
Reviewed generated material consists of the active baseline, comparison images
and validation logs; none is obsolete or removed in this block.
