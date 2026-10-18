# Alternate soundtrack workspace

The brief, palette provenance, render commands, listening guide, and revision
decisions live in [SOUNDTRACK_PLAN.md](../../docs/SOUNDTRACK_PLAN.md).

**Runtime:** Proteus now bundles the working XM below at build time and plays it
with the four-minute gull-follow waterfront. Rebuild after editing the selected XM.
A new filename must also be selected in CMakeLists.txt's `proteus_assets` copy command;
see the tracker guide's [runtime instructions](fasttracker/README.md#runtime-playback).

**Current editing handoff:** open
[fasttracker/v01/below-the-surface.xm](fasttracker/v01/below-the-surface.xm) in
FastTracker II Clone. The [tracker guide](fasttracker/README.md) maps the channels,
instruments, and section patterns, and explains the sample-based transfer. Continue
the song in a new XM revision; tracker edits do not update the JSON sources below.

- `palette-v01.json`: editable notes, instruments, dynamics, and mix settings.
- `palette-v02.json`: a dark intro/build audition with original drone and pulse
  patches, supported by the GM choir and drums.
- `proof-v01.json`: the 78-second proof of direction, with two themes, section
  markers, notes, instrument definitions, and mix settings.
- `proof-v02.json`: the accepted direction for the 78-second sketch: dark synths, a
  three-note signal, low strings/choir, and alternating pressure/withdrawal.
- `proof-v03.json`: current approved direction, committed as `0706588`; slow
  power-chord movement with v02's instrumentation, rhythm, dynamics, and arrangement.
- `full-v01.json`: four-minute full composition sketch expanded from approved
  proof v03. It retains the six tracks/patches and mix, and adds section and
  harmony timestamp guides. Full playback completed; musical acceptance is pending.
- `mix-v01.json`: post-processing candidate based on full-v01, preserving its
  notes, timing, patches, track volumes, master gain, and reverb. Its render and
  full playback are complete; musical listening acceptance remains pending.
- `compose_proof.py`: optional recipe for the original proof score, using the
  earlier palette definitions. It refuses to overwrite an existing score; edit
  a new JSON revision directly for subsequent listening iterations.
- `compose_full.py`: deterministic recipe that expands proof v03 into the first
  full sketch. It refuses to overwrite a score; use `--output` with a new source
  filename for another generated draft.
- `render.py`: validates the project, exports MIDI, builds/runs the native renderer,
  and records the render manifest. Python standard library only.
- `render.swift`: macOS offline sampler, original oscillator synths, and shared
  hall. Sampled voices use the installed DLS or SF2 bank. Xcode command-line tools
  are required.
- `export_fasttracker.py`: creates source-sample auditions and transfers the score
  into an editable XM with embedded multisamples, key-offs, and note dynamics.
- `check_fasttracker.py`: independently verifies the original generated XM against
  its source score, cue map, and sample WAVs.

Run from the repository root:

```sh
python3 soundtrack/alternate/render.py
afplay build/soundtrack/palette-v01/palette.wav
```

Outputs go under `build/`; choose a new `--output` directory for each render.
Keep a new source revision beside v01 when changing the palette.

Render the dark synth audition:

```sh
python3 soundtrack/alternate/render.py soundtrack/alternate/palette-v02.json \
  --output build/soundtrack/palette-v02
afplay build/soundtrack/palette-v02/palette.wav
```

Render the current proof of direction:

```sh
python3 soundtrack/alternate/render.py soundtrack/alternate/proof-v03.json \
  --output build/soundtrack/proof-v03 --name proof
afplay build/soundtrack/proof-v03/proof.wav
```

Render the four-minute full sketch:

```sh
python3 soundtrack/alternate/render.py soundtrack/alternate/full-v01.json \
  --output build/soundtrack/full-v01 --name soundtrack
afplay build/soundtrack/full-v01/soundtrack.wav
```

The render writes `soundtrack.wav`, `soundtrack.mid`, a project snapshot, and
`manifest.json`. `section-levels.json`, when present, is an additional local QA
report; `render.py` does not generate it. The WAV is 240 seconds: musical material
ends at 3:52, followed by its eight-second release.
The score has 464 beats at 120 BPM, so seconds = beats / 2. Use the source's
`sections` array for listening boundaries and its `harmony` array for chord-change
timestamps. Those arrays are useful listening/editing guides; explicit notes
determine the audio. MIDI exports the section labels as markers. Output directories
must be empty or absent; the existing full-v01 render directory is populated, so
repeat renders need a new output path, such as
`build/soundtrack/full-v01-rerender`.

Render the current mix candidate (choose a fresh output directory if rerendering):

```sh
python3 soundtrack/alternate/render.py soundtrack/alternate/mix-v01.json \
  --output build/soundtrack/mix-v01 --name soundtrack
afplay build/soundtrack/mix-v01/soundtrack.wav
```

Mix-v01 adds parallel midband tanh saturation before the shared hall while retaining
the dry low end: drone drive 20 dB, wet 0.30, band 160–3000 Hz; pulse drive 24 dB,
wet 0.40, band 180–3800 Hz. The existing notes, timing, patches, track volumes,
master gain, and reverb are preserved. Its 240-second render peaks at −4.39 dBFS
with −22.14 dBFS RMS and zero clipped samples. The opening sample is zero, and the
final second is silent. The quiet centre measures −31.61 dBFS RMS; first peak
−21.08 dBFS; full return −18.23 dBFS; widening −17.34 dBFS. All 844 notes, timings,
patches, and track settings were preserved; the MIDI matches full-v01 byte for byte.
At the 2026-09-22 mix checkpoint, the build passed and CTest passed 14/16; two GPU
checks could not create a GLFW context because no monitor was available. Those context
failures are historical; see the [current verification baseline](../../docs/README.md#verification-baseline).
Full mix playback completed with exit 0; listening
acceptance and headphone/speaker review remain pending. The `mix-v01` output folder
is populated, so rerenders need a new output path.

The draft delivery has been generated and its hashes checked. Recreate it with a
fresh output directory if needed (the current output folder is populated):

```sh
python3 soundtrack/alternate/prepare_delivery.py \
  --render build/soundtrack/mix-v01 \
  --output build/soundtrack/delivery-mix-v01
```

The package contains `soundtrack.wav`, `soundtrack.mid`, `project.json`,
`render-manifest.json`, `cue-sheet.json`, and `delivery-manifest.json`. Its 11 cues
cover the complete 240-second audio (11,520,000 frames); verified manifest hashes
bind the cue sheet to the audio. This is a draft delivery pending listening
acceptance. Runtime integration uses the editable XM below, with a four-minute waterfront timeline.

`--name` selects the WAV/MIDI filename stem (default `palette`). The proof's
`sections` array stores beat positions and a listening guide; the exporter uses
the beats for MIDI markers. For this fixed 120 BPM score, seconds = beats / 2.
Update the informational `seconds` values if changing the tempo. Detailed
timestamped listening notes are in the soundtrack plan.
Earlier sources/renders remain available for comparison; `compose_proof.py`
reconstructs proof v01 only, and `compose_full.py` creates the first full sketch
from proof v03. Edit a new JSON revision to develop the current sketch further.
The informational `harmony` arrays record intended chord movement; explicit
notes determine the sound. Full-v01 is a draft awaiting listening acceptance.

Each note is `[start_beat, duration_beats, MIDI_pitch, velocity]`, with beat zero
at the start and MIDI 60 = C4. Velocity is 1–127. Programs and channels are
zero-based; channel 9 selects percussion. `volume` is MIDI CC7, `pan` is CC10
(0 left, 64 centre, 127 right). Each track has its own channel. Chords are notes
at the same beat; overlapping repetitions of the same pitch are rejected to
avoid ambiguous note-offs. `beats` is the musical length before `tail_seconds`.

Change notes, velocity, or `program` in JSON and render again; no Swift rebuild
is needed. The current format supports a fixed tempo and note velocity, not
tempo maps or continuous controller/pitch-bend automation. Extend it only when
the composition needs those controls. MIDI exports preserve notes, patches,
tempo, volume, and pan; use the JSON settings to restore master gain and the
shared medium-hall effect in a DAW. Selecting the same bank is necessary to
reproduce the sampled timbres.

For an original synth voice, replace `program` with a `synth` object, as in v02:

| Setting | Meaning |
|---|---|
| `kind` | `drone` for a held envelope, `pulse` for a decaying pluck |
| `gain` | Patch output gain, before track volume and master gain |
| `detune_cents` | Equal/opposite detune for left/right harmonic layers |
| `cutoff_start`, `cutoff_end` | Base filter frequency in Hz, swept exponentially across the musical duration |
| `motion_hz`, `motion_depth` | Rate and fractional depth of the filter movement |
| `drive` | Saturation amount; the fundamental layer stays clean |
| `sub_mix` | Balance of the stable fundamental against the detuned harmonic layers |
| `attack`, `release` | Envelope times in seconds |

The pulse adds a fixed 75 ms filter decay and 160 ms amplitude decay. Notes are
generated from finite harmonic oscillators at twice the output sample rate, then
filtered and downsampled; there are no additional instrument downloads. Velocity
affects amplitude, and each note releases after its editable duration. Leave room
for releases and reverb in `tail_seconds`. The fixed patch sweeps are supported;
arbitrary automation and pitch bends are still outside the current project format.

Custom tracks export notes and dynamics to MIDI, with their names labelled as
custom synthesis and no GM program change. MIDI alone cannot reproduce these
patches. Retain the JSON and renderer, or use rendered audio/recreate the settings
in the destination DAW.
