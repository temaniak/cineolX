# CD Plate A Dynamic tail inspection

October 6, 2026. The listener clarified that both variants in the CD Plate A
Dynamic musical pair have a rapidly stair-like, digital-sounding ending. Dry
source listening was deferred. This bounded follow-up examines the tail rather
than treating the earlier during-playing report or the previous Hall residual
as an established explanation. No production DSP changes are made.

## Effective settings and comparisons

Reuse the unchanged independent renderer, v5 bank and four-second musical source
from [the enabled-mode checkpoint](XL_ENABLED_MUSICAL_VALIDATION.md). New captures
are eight seconds at 48 kHz, block 256, analog on, wet A/C, 0 dB input, 1,000 ms
warmup. Input is exactly zero after four seconds. Mod and Decay Opt remain off.

The CD Plate A profile has `shared_stop=true`: Dynamic selects STOP cells 6/7,
not 12/13. The original pair records 18/18 in 6/7 and the native fall quantizes
them to 16/16; normal cells 0/1 are 85/85. The earlier enabled-mode report wrongly
identified the unused 12/13 values of 5/5 as the effective STOP. That report is
corrected. Delay cell 45 is zero.

Two meaningful changes are compared with the existing Dynamic-on short-STOP
recording: turn only Dynamic off, or retain Dynamic and raise shared STOP cells
6/7 to 64/64 through physical overrides. Native/reference control audits agree.
No reference controller state, phase or WCS is injected into native.

Two diagnostic attempts are retained and excluded from the listening comparison:
requesting delay cell 45 at physical 64 still records zero and produces exact
original waveform prefixes; changing cells 12/13 to 128 changes those controls
but leaves the selected STOP positions and rapid post-input decay unchanged.
Neither is evidence about a changed effective STOP delay. Further physical delay
operation is deferred rather than opening a controller investigation here.

## Measured tail behavior

Levels below are stereo RMS over raw 100 ms windows beginning 0.5 seconds after
input stops. Floor-arrival times use 10 ms windows: the first window after the
last exceedance of the median 7–8 second level plus 3 dB. They are operational
diagnostics, not RT60 fits or audibility thresholds.

| Setting | Reference / native RMS at stop +0.5 s | Reference / native time from stop to floor +3 dB |
| --- | ---: | ---: |
| Dynamic on, STOP 18/18 | −72.994 / −73.721 dBFS | 0.76 / 0.76 s |
| Dynamic off, same controls | −49.666 / −49.628 dBFS | 1.68 / 1.69 s |
| Dynamic on, STOP 64/64 | −51.838 / −51.786 dBFS | 1.59 / 1.60 s |

The raw late residual remains around −91.5 dBFS in these cases. Dynamic with
short STOP brings the tail to it much sooner; lengthening effective STOP changes
that behavior in both independent versions. This supports a Dynamic/STOP cause
for the rapid ending. It does not prove that each heard stair or crunch is
quantization, or that the sound is normal on physical hardware.

A private native state probe reproduces the saved output exactly except for the
first two wrapper latency frames, and reports zero core saturations. It records
119 low/mid position changes during the four-second riff, alternating normal
85/85 with STOP 16/16 or 64/64. There are no further low/mid position changes
after input stops. A separate feedback-mid update occurs at capture 4.30842 s.
Do not describe the entire tail as repeated post-stop controller switches.

The graph uses integer state and the output path calls the existing ranging
12-bit DAC truncation. Quantization becoming exposed during rapid decay is
plausible. The present test does not isolate that mechanism or establish aliasing;
the visible RMS undulations alone are not proof of converter aliasing or damage.

## Listening, provenance and limits

Private root: `build/validation/xl-dynamic-tail-20261006/`. It retains commands,
control audits, unique raw captures, exact native probe logs, 10 ms envelopes,
`results.json`, `tail-envelopes.png` and a hash checkpoint. HEAD, clean Reflexion,
ROMs, bank and renderer identities are inherited from the enabled-mode checkpoint.
The existing five runtime source hashes and the frozen CPU source trees remain
unchanged. No plugin is installed and CPU is not repeated.

Two 11.3-second listening clips present the last 0.5 seconds of playing plus
three seconds of tail for short STOP / Dynamic off / longer STOP, separated by
0.4 seconds of silence. Reference and current native have separate clips. All
segments use the same scalar gain of 3.743855712430029 (about +11.47 dB), with
no EQ, alignment, denoising, tail fade or independent tail normalization. Their
ordering, bounds and hashes are in `listening/manifest.json`. The user subsequently
gave positive feedback and provisionally accepted this example's behavior;
the agent does not claim to have heard the clips.

Only confirmed original-prefix duplicate WAVs from the unsuccessful delay test
are deleted, with hashes and surviving-source paths retained. Identical new input
files share hard-linked storage. Unique recordings and pending listening remain.
Longer STOP changes the intended decay behavior; it is a setting alternative,
not a code correction or blanket fidelity acceptance. Any future smoothing must
be evaluated against the reference before changing the default algorithm.

## Hardware literature

A bounded October 6 web search found direct testimony about real 224XL artifacts:
[Sean Costello's VintageVerb modes article](https://valhalladsp.com/2023/02/10/valhallavintageverb-the-modes/)
describes listening to a 224XL and emulating converter, audio and modulation
quantization. His [2013 KVR technical explanation](https://www.kvraudio.com/forum/viewtopic.php?start=30&t=396339)
describes the XL's ranging 12-bit converters and noise from low-resolution
feedback processing. These are primary developer observations about the hardware,
not a report of the precise CD Plate A Dynamic short-tail symptom.

The [Lexicon 224X owner's manual, section 3.3.1](https://manualzz.com/doc/11604217/lexicon-224x-digital-reverb-effects-processor-user-manual)
documents gated results when stopped decay is shorter than running decay and
warns that short pauses in choppy material can be interpreted as phrase endings.
This is documentation for the related 224X, not confirmation of XL v8.21 behavior.
The search did not locate a reliable direct complaint identifying quantized
CD Plate A Dynamic short-tail endings on a physical 224XL. General hardware grit
and the intended Dynamic gating are supported; their exact combination in this
fixture remains an inference. The listener's provisional acceptance does not
establish physical-unit validation or a particular quantization mechanism.
No default smoothing is added and no known hardware fault is asserted.
