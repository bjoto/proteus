# Alternate soundtrack workspace

The brief, palette provenance, render commands, listening guide, and revision
decisions live in [SOUNDTRACK_PLAN.md](../../docs/SOUNDTRACK_PLAN.md).

- `palette-v01.json`: editable notes, instruments, dynamics, and mix settings.
- `palette-v02.json`: a dark intro/build audition with original drone and pulse
  patches, supported by the GM choir and drums.
- `proof-v01.json`: the 78-second proof of direction, with two themes, section
  markers, notes, instrument definitions, and mix settings.
- `proof-v02.json`: the accepted direction for the 78-second sketch: dark synths, a
  three-note signal, low strings/choir, and alternating pressure/withdrawal.
- `proof-v03.json`: current harmonic audition: slow power-chord movement with
  v02's instrumentation, rhythm, dynamics, and arrangement preserved.
- `compose_proof.py`: optional recipe for the original proof score, using the
  earlier palette definitions. It refuses to overwrite an existing score; edit
  a new JSON revision directly for subsequent listening iterations.
- `render.py`: validates the project, exports MIDI, builds/runs the native renderer,
  and records the render manifest. Python standard library only.
- `render.swift`: macOS offline sampler, original oscillator synths, and shared
  hall. Sampled voices use the installed DLS or SF2 bank. Xcode command-line tools
  are required.

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

`--name` selects the WAV/MIDI filename stem (default `palette`). The proof's
`sections` array stores beat positions and a listening guide; the exporter uses
the beats for MIDI markers. For this fixed 120 BPM score, seconds = beats / 2.
Update the informational `seconds` values if changing the tempo. Detailed
timestamped listening notes are in the soundtrack plan.
Earlier sources/renders remain available for comparison; the optional
`compose_proof.py` recipe reconstructs v01 only. Edit a new JSON revision to
develop the current sketch further. V03's informational `harmony` array records
the intended chord movement; its explicit notes determine the sound.

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
