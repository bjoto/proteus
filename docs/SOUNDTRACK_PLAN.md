# Alternate soundtrack plan

> Runtime integration, 2026-10-11: the demo now uses the working
> `soundtrack/alternate/fasttracker/v01/below-the-surface.xm`, copied into the build
> assets by CMake. The sole `cube` scene follows five gulls across its four-minute
> 120 BPM timeline and holds the final shot. Earlier draft/acceptance notes below
> record the composition history; future tracker revisions remain possible.

Status: the user requested a FastTracker editing handoff on 2026-09-22. The full
song is now available as an editable [XM and tracker guide](../soundtrack/alternate/fasttracker/README.md),
with embedded instruments and separate note/chord voices. Continue musical edits
in the XM; the JSON/MIDI sources and original WAV mix remain reference versions.

Proof v03 is the current approved direction (commit `0706588`). Full sketch
v01 has been played through; musical feedback and milestone 3 acceptance remain
pending. Mix candidate v01 has rendered and its draft delivery package is prepared;
full mix playback completed successfully. Final listening acceptance remains open; the current working XM is now integrated
with the four-minute waterfront. Milestones 1 and 2 are complete. The approved dark synth
palette, restrained motif, dynamics, and grunge-influenced chord movement remain
the foundation.

This is the working plan for an original Proteus soundtrack. Update it as we listen,
choose directions, and complete milestones. The existing demo timeline does not
constrain the composition; the current waterfront follows the working XM.

## Musical brief

Create an approximately four-minute dark symphonic electronic adventure: powerful,
spacious, and dynamic enough to support distinctly different visual moods. Keep
the melody simple; develop tension and scale through texture, register, rhythm,
and silence. The user's response to proof v01 rules out brightness and cheerfulness
as the default direction. Duration and section timings are provisional.

The user's response to v02 adds Nirvana's "Something in the Way" as a mood
reference: brooding restraint can carry a soundtrack. Keep the electronic sound
and arrangement, and explore grunge-influenced chord movement through slow bass
changes, open fifths, repetition, and tension. The new harmony is an original
composition choice.

Use Skaven as a reference for theatrical character, memorable melodies, expressive
sample sounds, rhythmic detail, and playfulness alongside grandeur. Write original
themes. A useful reference is [War in the Middle Earth – Redux](https://soundcloud.com/skaven252/war-in-the-middle-earth-redux),
which Skaven describes as epic synth-rock, sequenced in FL Studio from an earlier
tracker composition.

Core ingredients:

- One short, recognisable main motif, developed into fragile, threatening, and
  forceful forms. Proof v02 reduces it to three pitches.
- A contrasting quiet treatment with room around the notes. A separate lyrical
  theme is optional if it serves the atmosphere; avoid busy competing melodies.
- Punchy electronic drums and mobile synth bass, with strings, brass, choir, bells,
  and an expressive synth lead.
- Tracker character through articulation, pitch slides, fills, and quick flourishes.
- Two major peaks, with the largest instrumental range and full choir saved for
  the final one. Quiet passages must hold attention on their own.
- Exaggeration through harmony, register, rhythm, and anticipation as well as volume.
  A peak can remain dark; a bright or major-key resolution is not required.

## Provisional musical arc

| Approximate time | Movement | Musical role | Mood |
|---|---|---|---|
| 0:00–0:24 | The signal | Distant drone, open fifths, and sparse three-note motif | Mystery and scale |
| 0:24–0:56 | Awakening | Slow chord changes; pulse gradually takes shape | Discovery and motion |
| 0:56–1:28 | First weight | First full pressure cycle, restrained in register and choir | Confidence with unease |
| 1:28–1:56 | Fracture | Interrupted pulse, chromatic roots, altered motif | Threat and instability |
| 1:56–2:28 | Suspended world | Spacious centre with two slow signal statements | Wonder and vulnerability |
| 2:28–2:55 | The gathering | Familiar chords return; density and weight rise | Purpose and anticipation |
| 2:55–2:56 | Held breath | Parts release before the final entrance | Anticipation |
| 2:56–3:36 | Full return / widening | Largest cycle; upper signal register and four-voice choir enter at 3:12 | Dark grandeur |
| 3:36–3:52 | Afterglow | Final impact, then the opening signal retreats | Release and lingering tension |
| 3:52–4:00 | Release | Instrument and hall decay | Closure |

These are the authored full-v01 section boundaries at 120 BPM. The audio file is
4:00 including its eight-second release; composed material ends at 3:52. Treat the
guide as provisional until the full arrangement has been heard and accepted.

Sections should have clear musical boundaries and meaningful accents for eventual
visual choreography. Phrase lengths, tempo, key, and metre remain composition choices;
the existing 125 BPM XM grid is not a requirement for this alternate track.

## Working method

Work in small, audible iterations. Each milestone produces a listening render and
editable source, followed by a short note recording what worked and what to change.
Use timestamped feedback where possible, such as "0:34: the lead needs more lift."
Update this document with decisions instead of maintaining separate competing plans.

The assistant can prepare arrangement data, MIDI, cue sheets, scripts, and exports
where the available tools permit. Listening feedback determines whether the music
achieves the intended effect. Tool setup and successful rendering alone do not
establish musical quality.

The current authoring handoff is FastTracker II Clone, requested by the user after
the full mix. The initial JSON/MIDI and macOS render workflow remains available to
reproduce that reference. Continue the composition in versioned XM files; preserve
the embedded samples and instrument envelopes with the arrangement. The rendered
mix remains the timbral reference because the XM uses sampled synth snapshots and
has no live shared hall.

## Milestones

### 1. Establish a usable sound palette

- [x] Check available DAW, MIDI/rendering tools, and instrument libraries.
- [x] Choose a small initial palette: lead, harmony, bass, drums, and one atmospheric voice.
- [x] Render a short phrase to confirm the path from editable notes to audible output.
- [x] Record the tools, instrument sources, and exact render procedure here.

Done when: we have a reproducible audio render and enough control to change notes,
instruments, and dynamics without rebuilding the toolchain.

#### Established setup (2026-09-20)

Local inspection found FastTracker II Clone, Python 3.14.2, FFmpeg, `afplay`, and
Xcode/Swift 6.3.3 on Apple Silicon macOS 26.6.2. REAPER, GarageBand, FluidSynth,
TiMidity, and third-party instrument plug-ins were not found in the application,
PATH, and user/system audio-library locations checked. Apple's built-in General
MIDI DLS bank is available.

Use the checked-in [alternate workspace](../soundtrack/alternate/) for this first
palette. Author notes and settings in `palette-v01.json`; `render.py` exports a
type-1 MIDI file and invokes a small Swift AVAudioEngine offline renderer. This
establishes a working path without installing a DAW. A piano-roll/DAW workflow
remains open for the composition milestone; the MIDI export provides a handoff.

| Role | Initial patch | Program (zero-based) | MIDI channel (display numbering) |
|---|---|---|---|
| Lead | Lead 2 / sawtooth | 81 | 1 |
| Harmony | String Ensemble 1 | 48 | 2 |
| Bass | Synth Bass 1 | 38 | 3 |
| Drums | Standard kit: kick 36, snare 38, closed hat 42 | 0 | 10 |
| Atmosphere | Choir Aahs | 52 | 4 |
| Dark drone (v02) | Original detuned oscillator patch | Custom synthesis | 5 |
| Growling pulse (v02) | Original saturated oscillator patch | Custom synthesis | 6 |

Instrument source: the macOS-supplied
`/System/Library/Components/CoreAudio.component/Contents/Resources/gs_instruments.dls`.
Its SHA-256 on this machine is
`739d277474bddeb120372625b70c83c653faaee44de26c3191848cc6c64bfb74`.
The bank remains an external system dependency and is not copied into the repo.
The initial five voices use this stock GM bank. The v02 drone and pulse are
original oscillator patches implemented in `render.swift`, with their settings in
`palette-v02.json`; they need no external samples. Expressive articulations,
stronger electronic drums, brass, and bells remain candidates for later
listening-led changes.

The audition uses 120 BPM, 4/4, and E minor as test settings, not commitments for
the full soundtrack. Notes, durations, velocity, program, channel volume, and pan
are explicit in the JSON. The shared effect is Apple's medium-hall reverb at 14%
wet, with master gain 0.6, 48 kHz stereo 16-bit WAV output, and a four-second tail.
Bank reverb/chorus controller sends are zero. These settings live with the notes;
MIDI alone does not preserve the shared effect or master gain.

From the repository root:

```sh
python3 soundtrack/alternate/render.py
afplay build/soundtrack/palette-v01/palette.wav
```

The first command compiles `render.swift` into `build/soundtrack/toolchain/` when
needed, then writes `palette.wav`, `palette.mid`, `project.json`, and
`manifest.json` in `build/soundtrack/palette-v01/`. The manifest records tool/OS
versions, source/bank/audio hashes, duration, levels, and clipping count. Rendering
uses [Apple's offline audio API](https://developer.apple.com/documentation/avfaudio/performing-offline-audio-processing)
and does not require a running DAW or sound output device. Sandboxed command
runners may need approval to access system Audio Units; `afplay` needs audio-device
access separately.

Output directories must be empty or absent, so an existing audition is preserved.
For a new palette revision, copy the project to a new name such as
`palette-v03.json`, change its notes/settings, then:

```sh
python3 soundtrack/alternate/render.py soundtrack/alternate/palette-v03.json \
  --output build/soundtrack/palette-v03
afplay build/soundtrack/palette-v03/palette.wav
```

For a repeatability check, pass the unchanged v01 project with another output
directory. Two independent v01 renders produced byte-identical WAVs on this
machine. Other macOS/instrument-bank versions may sound different; compare the
recorded hashes before assuming byte reproducibility. `--bank /path/to/bank.sf2`
can audition another locally supplied bank using the same program mapping.

### 2. Make the 60–90-second proof of direction

- [x] Write a simple main motif and a contrasting quiet treatment.
- [x] Arrange quiet passages, heavier returns, and a final surge.
- [x] Develop the same motif in the quiet and heavy passages.
- [x] Export a playable stereo sketch with a brief timestamped listening guide.

Listen for:

- Can we remember the main motif after playback?
- Does the quiet passage feel like music worth hearing by itself?
- Does the transition create anticipation and make the arrival satisfying?
- Is the climax excessive in the enjoyable way we want?
- Does the electronic/orchestral balance have character?

Done when: listening feedback supports the theme, contrast, and palette. Revise this
short sketch before spending time orchestrating the full song if those are weak.

### 3. Sketch the complete composition

- Expand the accepted material into the full arc using a modest instrument count.
- Develop the threatening variation and the first peak.
- Compose transitions and an intentional ending.
- Establish the tempo map, phrase lengths, section names, and major accents.

Current draft: [full-v01.json](../soundtrack/alternate/full-v01.json), generated
from approved proof v03 by `compose_full.py`; its four-minute render and cue-level
measurements are in `build/soundtrack/full-v01/`. The score contains 844 notes on
the same six tracks/patches and mix as v03, with the full arc and revised ending
velocities. This is an authored sketch, not an accepted composition: listening
feedback is still required before marking milestone 3 complete. Initial full
playback and revised-ending playback both completed with exit 0; playback
completion does not record musical acceptance.

Done when: the entire piece works as a continuous listening experience, every section
has a distinct mood, and the finale feels larger than the first peak.

### 4. Orchestrate and mix

- Develop articulations, fills, countermelodies, spatial effects, and sound design.
- Add dirt and grit to the accepted dark drone and pulse through post-processing.
  Compare against palette v02 and preserve the mood, low-end weight, and movement
  that worked in the audition; choose the processing in the context of the mix.
- Preserve melodic clarity and the difference between intimate and massive passages.
- Check the complete render for clipping, accidental clicks, and obscured melody.
- Audition on headphones and speakers where available; record any listening limitations.

Current mix candidate: [mix-v01.json](../soundtrack/alternate/mix-v01.json) keeps
full-v01's notes, timing, patches, track volumes, master gain, and reverb. It adds
parallel midband tanh saturation before the shared hall, with dry low end retained:
deep drone drive 20 dB / wet mix 0.30 over 160–3000 Hz, and growling pulse drive
24 dB / wet mix 0.40 over 180–3800 Hz. The render is complete and technical checks
passed; full-length playback completed with exit 0. No perceptual, headphone, or speaker assessment
has been recorded yet.

Done when: a complete stereo render holds up without visuals and has the intended
dynamic range. Keep the editable project and instrument settings with the revision.

### 5. Prepare demo integration

- Choose the playback format based on the accepted music and runtime requirements.
  A stereo audio file plus explicit cues is the default proposal; XM is optional.
- Export section boundaries, tempo changes, and important accents with stable names
  and times measured from the start of the final audio render.
- Keep aligned stems for drums, bass, harmony, lead, and atmosphere if useful for
  iteration. Runtime stem mixing is only needed if we later want adaptive playback.
- Design visual scenes around the musical moods and transitions.
- Implement playback and synchronization, then verify start, seek, scene jumps,
  spectrum analysis, and ending behavior against the soundtrack.
- Update controls/workflow documentation and run the project build, CTest, and a
  manual audiovisual demo check when integration changes code or playback.

The draft package from mix-v01 is prepared with `prepare_delivery.py`. It contains
the WAV, MIDI, project, render manifest, cue sheet, and delivery manifest. The cue
sheet has 11 sections spanning the full 240 seconds; verified manifest hashes bind
the cues to the audio. Musical acceptance is pending. Runtime playback and visual
timing integration are also pending.

Done when: the demo follows the accepted composition and musical cues stay aligned
through playback and seeking.

## Artifact handling

Use a separate alternate-soundtrack workspace in the repository when composition
begins. Keep source, instrument provenance/settings, and render instructions together.
Use revisioned listening renders so comparisons remain possible. Put disposable
renders in `build/` and add chosen delivery assets under `assets/music/` later.
Record any external libraries needed to reproduce the sound.

The starter XM and its generator remain available for comparison. This exploration
does not require regenerating or replacing them.

## Progress and decisions

- [x] Agree the musical direction and initial arc.
- [x] Create this working plan.
- [x] Establish tools and palette.
- [x] Produce the short proof of direction.
- [x] Assess and refine the proof from listening feedback (v03 approved).
- [x] Author and render the complete four-minute composition sketch (full-v01).
- [x] Prepare the grit mix candidate and validate its draft delivery package (mix-v01).
- [ ] Record listening feedback and approval for the full composition and mix.
- [x] Integrate the current working XM with the demo and its four-minute visual timeline.

Next action: continue editing the full song in FastTracker, using mix-v01 as the
sound reference, and record timestamped feedback before
treating the candidate delivery as accepted. The full sketch retains the six
voices, simple signal, and E5–C5–F5–E5 harmonic backbone, with a chromatic fracture,
quiet centre, restrained first weight, and larger final return.

Open choices: DAW/piano-roll authoring, final instruments, composition tempo/key,
final duration, and delivery format. Resolve them when their milestone needs them.

For each listening revision, append: render location, changes, listening feedback,
decision, and next action.

### Palette v01 — 2026-09-20

- Render: `build/soundtrack/palette-v01/palette.wav` (22 seconds); MIDI, source
  snapshot, and manifest are alongside it. Editable source:
  [palette-v01.json](../soundtrack/alternate/palette-v01.json).
- Changes: initial five-voice audition, then a four-bar ensemble exercise.
- Listening guide: 0:00 lead; 0:02 strings; 0:04 bass; 0:06 drums; 0:08 choir;
  0:10–0:18 ensemble; 0:18–0:22 release/reverb tail. Effect tails overlap the solo
  boundaries. Listen for lead character, string/choir separation, bass definition,
  and whether the kit needs a more electronic sound.
- Technical checks: each solo window contains signal; stereo output peaks at
  −11.43 dBFS with zero clipped samples; the last second is silent. Independent
  renders matched byte for byte. A temporary version with the lead changed to a
  square patch, lowered an octave, and reduced in velocity rendered successfully
  with different PCM. Apple's native MIDI importer read all five tracks and 145
  notes from the export. `afplay` completed through the Mac's audio output.
- Project verification: Debug configure/build and all 16 CTest cases passed,
  including the existing GPU checks (which needed execution outside the sandbox).
- Listening feedback: pending. Playback and signal checks do not establish musical
  quality; the assistant has not perceptually assessed the audio. The ensemble
  exercise is not yet the main motif or the proof of direction.
- Decision: the tools and control needed for milestone 1 are established. Keep
  this palette provisional until listening feedback; proceed to milestone 2
  after that review.

### Palette v02 — dark synths — 2026-09-20

- Request: add dedicated dark, powerful sounds for the mood-building intro and
  threatening middle section. This is a palette extension; motif development is
  still the next milestone.
- Render: `build/soundtrack/palette-v02/palette.wav` (30 seconds), with MIDI,
  project snapshot, and manifest alongside it. Editable source:
  [palette-v02.json](../soundtrack/alternate/palette-v02.json).
- New voice: **deep drone**. A centred E1 fundamental supports stereo layers
  detuned by ±11 cents, with a 1.6-second attack and 2.2-second release. Its filter
  base opens from 100 to 950 Hz across the musical duration, with slow 0.13 Hz
  motion and mild saturation. The fundamental stays clean under the moving
  harmonics. Higher velocity increases level without normalising individual notes.
- New voice: **growling pulse**. Detuned layers at ±7 cents, stronger saturation,
  a 6 ms attack, decaying amplitude/filter envelopes, and an 85 ms release. A
  rising 180–1400 Hz base cutoff and 0.31 Hz motion change its brightness through
  the phrase. Syncopated eighth/sixteenth-note figures and velocity accents provide
  the rhythmic tension. These are synthesised sounds, not renamed GM patches.
- Sound settings: finite harmonic oscillators, synthesis at twice the output
  sample rate, filtering before/after saturation, and a stable fundamental layer.
  Notes, detune, cutoff endpoints, motion, drive, fundamental mix, gain, attack,
  and release are editable in JSON. The shared hall is 19% wet; master gain is
  0.85; delivery is 48 kHz stereo 16-bit PCM with a six-second tail. The existing
  GM choir and kit provide context.
- Listening guide: 0:00 drone alone; 0:02 distant choir enters; 0:08 pulse enters
  as the opening drone releases; 0:12 combined build with percussion; 0:16 a
  semitone rise increases tension; 0:20 return to E; 0:24–0:30 release/tail. Listen
  for low-end weight, movement in the sustained sound, and whether the pulse has
  enough grit without masking the choir.
- Technical checks: two independent renders produced identical WAVs; peak
  −3.41 dBFS, RMS −17.15 dBFS, zero clipped samples, zero opening samples, and a
  silent final second. The opening fundamental was checked against E1. `afplay`
  completed successfully. Debug configure/build and all 16 CTest cases passed.
- Listening feedback: the user confirms that both voices work and particularly
  likes the mood they set. They would like the sounds a bit more "dirty" and
  "gritty", with that refinement handled in post-processing.
- Decision: accept both voices for mood and character and use them in the proof
  of direction. Keep v02 as the comparison reference; plan additional dirt/grit
  for orchestration and mixing, preserving the qualities that were liked. This
  feedback accepts the dark voices, not yet the full composition or broader
  initial palette.

Render v02 from the repository root (choose a fresh output directory for a repeat):

```sh
python3 soundtrack/alternate/render.py soundtrack/alternate/palette-v02.json \
  --output build/soundtrack/palette-v02
afplay build/soundtrack/palette-v02/palette.wav
```

The MIDI export carries the custom synth tracks' notes and dynamics, with track
names identifying them as custom synthesis. It deliberately assigns no GM
program to those tracks: importing MIDI alone will not reproduce their sounds.
The JSON plus renderer is the source for these patches; use the rendered audio
or recreate the patch settings when moving the arrangement into a DAW.

### Proof v01 — The signal — 2026-09-20

- Render: `build/soundtrack/proof-v01/proof.wav`, **1:18** including a six-second
  release tail; 36 bars at 120 BPM in 4/4. `proof.mid`, `project.json`, and
  `manifest.json` are alongside it. The editable score is
  [proof-v01.json](../soundtrack/alternate/proof-v01.json); the original arrangement
  recipe is [compose_proof.py](../soundtrack/alternate/compose_proof.py).
- Main motif: E–B–D–E, a rising fifth followed by the minor seventh and octave,
  then an answer through G–F♯–E–D–B. Short pickups and longer arrival notes give it
  a distinctive rhythm. Bells introduce the interval shape; flute states the
  complete fragile version; the saw lead returns with the same phrase in the
  finale. The final statement raises G and D to G♯ and D♯ for the E-major lift.
- Contrasting phrase: a lower, flowing E–G–B–A / G–E line, followed by a stepwise
  answer and a leading-tone cadence. It appears alone in the quiet section, then
  as the flute countermelody beneath the heroic theme. It is intended to feel
  lyrical rather than simply doubling the lead.
- Arrangement: the accepted drone establishes the opening; the pulse drives the
  rebuild and becomes a quieter moving layer under the finale. A semitone threat,
  rising activity/register, snare roll, and half-second break prepare the arrival.
  The E-major lift adds the widest choir voicing. A decisive E-major chord leads
  to the opening bell gesture returning with its seventh raised and resolved.
- Instruments: 11 tracks. The accepted drone and pulse patch settings are intact,
  with arrangement-specific volume/velocity. The original GM lead, strings, bass,
  choir, and kit return. Four additional Apple-bank patches supply solo flute
  (program 73, channel 7), celesta (8, channel 8), brass section (61, channel 9),
  and pizzicato strings (45, channel 11). Programs are zero-based, displayed
  channels one-based. These additional voices remain provisional.
- Mix: 17% shared medium-hall reverb, master gain 0.65, 48 kHz stereo 16-bit PCM.
  This is a composition sketch with velocity and arrangement dynamics; the extra
  dirt/grit requested for the dark voices has not been added yet.

| Time | Passage | Listen for |
|---|---|---|
| 0:00–0:08 | The signal | Accepted drone and distant choir; bells plant the main gesture. |
| 0:08–0:16 | Fragile theme | The full main motif on a quiet solo flute. Does it stand on its own? |
| 0:16–0:24 | Lyrical answer | A contrasting phrase, with space around the melody. |
| 0:24–0:39.5 | The gathering | Pulse returns; a chromatic detour, arpeggios, brass fragments, and percussion build tension. |
| 0:39.5–0:40 | Hold your breath | Parts release early, leaving a brief gap and their tails. |
| 0:40–0:48 | First arrival | Full lead statement with the lyrical flute beneath it. Is the arrival satisfying? |
| 0:48–0:56 | Answer and expansion | The theme repeats with a different harmonic answer and a stronger cadence. |
| 0:56–1:04 | Harmonic lift | Parallel E major, higher/wider choir, and the largest combined sound. Is it excessive enough? |
| 1:04–1:12 | Afterglow | Decisive chord, then a resolved echo of the opening bells. |
| 1:12–1:18 | Release | Instrument/hall decay to silence. |

Reproduce from the repository root, using a fresh output directory:

```sh
python3 soundtrack/alternate/render.py soundtrack/alternate/proof-v01.json \
  --output build/soundtrack/proof-v01 --name proof
afplay build/soundtrack/proof-v01/proof.wav
```

`--name` chooses the WAV/MIDI filename stem; existing palette commands retain
their original names. Section boundaries are included in the JSON and exported
as MIDI markers. As with v02, MIDI contains the custom synth notes but needs the
JSON/renderer or recreated patches to reproduce those timbres.

To inspect or revise the composition, edit a new JSON revision directly. The
optional recipe can reconstruct the original score with
`python3 soundtrack/alternate/compose_proof.py --output build/soundtrack/proof-rebuilt.json`;
it refuses to overwrite an existing file. Its output was checked against the
checked-in v01 score. It reads the two earlier palette sources to retain their
instrument definitions.

- Verification: independent renders produced identical WAVs; peak −4.66 dBFS,
  whole-file RMS −23.56 dBFS, zero clipped samples, zero opening samples, and a
  silent final second. The lyrical passage measures −33.00 dBFS RMS and the final
  lift −19.02 dBFS RMS (about 14 dB contrast); these are signal measurements, not
  a perceptual assessment. Section measurements are in `section-levels.json`
  beside the render. Apple's MIDI importer read all 989 notes across 11 instrument
  tracks plus the marker track. Full-length `afplay` playback completed successfully.
  Debug configure/build and all 16 CTest cases passed.
- Listening feedback: the user found this version too bright and cheerful and
  requested more darkness, a dynamic setting, and a much simpler melody.
- Decision: revise the composition around the accepted dark palette, fewer notes,
  and contrasting pressure/space. Preserve v01 as the comparison; its major-key
  lift and bright melodic instrumentation are not the accepted direction.

### Proof v02 — Below the surface — 2026-09-20

- Render: `build/soundtrack/proof-v02/proof.wav`, **1:18**, with `proof.mid`, source
  snapshot, and manifest alongside it. Editable source:
  [proof-v02.json](../soundtrack/alternate/proof-v02.json). V01's score and audio
  are preserved unchanged for comparison.
- Response to feedback: the accepted drone and pulse now carry the arrangement.
  The flute, celesta, pizzicato arpeggios, brass fanfare, and bright saw melody are
  absent. The E-major lift and major resolution are replaced by an E pedal,
  semitone/tritone tension, and a retreat into the low register.
- Simpler melody: **E–F–B**, three pitches with long gaps between statements. The
  signal slows in the suspended passage and rises one octave only near the last
  peak. There is no simultaneous countermelody. Across the whole arrangement,
  the score falls from 989 to 267 notes, and from 11 to six instrument tracks;
  the signal itself has 23 attacks.
- Dynamics and setting: two cycles of pressure and withdrawal replace the long
  rising bridge/finale. The pulse changes density, percussion leaves space, and
  low strings/choir enter selectively. A low impact at 0:50 is followed by a
  hollow gap before the final return. The contrast comes from changing texture,
  register, rests, and velocity as well as level.
- Instruments: original drone and pulse patches unchanged, low GM strings
  (program 48), low GM choir (52), and sparse kit percussion (0, channel 10):
  kick, low toms, restrained snare, occasional soft closed hat, no crash cymbals.
  The sixth voice is a new filtered **three-note signal**, derived from the drone
  oscillator with ±4-cent detune, 260–720 Hz cutoff sweep, 180 ms attack, 650 ms
  release, and less fundamental mix. Its complete settings are in the JSON.
- Mix: 21% shared medium-hall reverb, master gain 0.72, 48 kHz stereo 16-bit PCM,
  with the same 120 BPM/36-bar duration and six-second tail. Additional grit remains
  deferred to post-processing; this revision addresses composition and palette.

| Time | Passage | Listen for |
|---|---|---|
| 0:00–0:12 | Below the surface | Low drone and distant three-note signal, with space around each entrance. |
| 0:12–0:24 | Pulse wakes | Uneven pulse and low percussion gather weight over the pedal. |
| 0:24–0:36 | Suspended | Rhythm disappears; the signal slows, leaving choir and low strings. |
| 0:36–0:50 | Pressure | Heavier return, denser pulse, and semitone/tritone tension. |
| 0:50–0:58 | Rupture | One low impact, followed by a hollow withdrawal. |
| 0:58–1:10 | The return | Largest surge; the same simple signal remains central and the harmony stays dark. |
| 1:10–1:12 | Back into shadow | Retreat to the pedal, without a major-key resolution. |
| 1:12–1:18 | Release | Natural decay to silence. |

Render from the repository root, with a fresh output directory:

```sh
python3 soundtrack/alternate/render.py soundtrack/alternate/proof-v02.json \
  --output build/soundtrack/proof-v02 --name proof
afplay build/soundtrack/proof-v02/proof.wav
```

Edit a new JSON revision for subsequent changes. No additional instrument library
or renderer change is required; the score includes all custom patch settings and
exports its section markers to MIDI.

- Verification: peak −4.87 dBFS, whole-file RMS −19.81 dBFS, zero clipped samples,
  zero opening samples, and a silent final second. The suspended passage measures
  −29.40 dBFS RMS and the rupture −31.75 dBFS, compared with −17.39 dBFS for the
  last return (roughly 12 and 14 dB contrast). Section measurements are beside the
  render. Repeat renders matched byte for byte. Native MIDI import recovered all
  267 notes across six instrument tracks plus the marker track. The three pitch
  classes and unchanged v01 artifacts were checked. Full-length audio playback
  completed successfully. Debug build and all 16 CTest cases passed.
- Listening feedback (2026-09-21): the user likes this direction and associates
  its moodiness with Nirvana's "Something in the Way" as a soundtrack reference.
  Keep the synths and arrangement, and try some grunge-style chord progression.
- Decision: preserve v02 as the accepted reference for mood, instrumentation,
  and arrangement. Make a harmonic revision for comparison before expanding.

### Proof v03 — slow chord movement — 2026-09-21

- Render: `build/soundtrack/proof-v03/proof.wav`, **1:18**. `proof.mid`, the source
  snapshot, and manifest are alongside it. Editable source:
  [proof-v03.json](../soundtrack/alternate/proof-v03.json). V02 remains unchanged.
- Harmonic idea: **E5 → C5 → F5 → E5**. Here "5" means a root-and-fifth power
  chord, not an octave designation. The falling bass into C gives weight; the
  F-to-E semitone return supplies tension. Strings use open root/fifth/octave
  voicings; choir voices mainly use fifths and octaves, with an upper tension
  note retained at the last peak. Held voices can overlap a bass change.
- Changes: the first pulse drops from E to C at 0:20, and the quiet passage sits
  over C. The heavier passage follows E at 0:36, C at 0:40, F at 0:44, and E at
  0:48. The final return repeats E at 0:58, C at 1:02, F at 1:06, and E at 1:10.
  The low rupture impact and its following gap keep their original pitches.
- Melody: each three-note phrase keeps its rhythm and interval contour. Selected
  phrases move with the harmony: C–D♭–G in the suspended passage and F–G♭–C over
  the F-root pressure. The high phrase beginning at 1:04 stays C-based while the
  harmony moves to F, leaving a suspended colour rather than adding more notes.
- Preserved: all six track patches, every note's start/duration/velocity, all 267
  notes, the entire percussion track, section boundaries, tempo, and mix settings.
  The musical changes are pitches only. No renderer or instrument-library changes
  were needed, and no additional dirt/grit processing was applied.

Render from the repository root with a fresh output directory:

```sh
python3 soundtrack/alternate/render.py soundtrack/alternate/proof-v03.json \
  --output build/soundtrack/proof-v03 --name proof
afplay build/soundtrack/proof-v03/proof.wav
```

The score includes a `harmony` array documenting the intended chord changes and
their times. It is a listening/editing guide; the explicit notes determine the
audio. Section markers continue to export to MIDI.

- Verification: score comparison confirmed the preserved timing, dynamics,
  patches, percussion, and mix, plus unchanged v02 source/audio. Native MIDI
  import read all 267 notes across six instrument tracks plus the marker track.
  The WAV peaks at −5.13 dBFS, with whole-file RMS −20.61 dBFS and no clipped
  samples. The opening starts at zero and the final second is silent. Both
  withdrawals retain more than 8 dB of RMS contrast against the final return;
  section measurements are alongside the render. Full-length audio playback
  completed successfully. Debug configure/build and all 16 CTest cases passed.
- Listening feedback (2026-09-21): the user approved this part and requested a
  commit, then continued work on the music.
- Approval commit: `0706588`.
- Decision: v03 is the accepted proof of direction and the baseline for milestone
  3. Preserve this revision while expanding into the full composition. Its approval
  covers the foundation, not the later full arrangement or final mix.

### Full sketch v01 — 2026-09-22

- Source: [full-v01.json](../soundtrack/alternate/full-v01.json), authored from
  approved proof v03 by [compose_full.py](../soundtrack/alternate/compose_full.py).
  The render, MIDI, source snapshot, manifest, and section measurements are in
  `build/soundtrack/full-v01/`. The source recipe refuses to overwrite an existing
  score; preserve this revision and write future drafts to a new filename.
- Arrangement: 0:00 signal; 0:24 awakening; 0:56 first weight; 1:28 fracture;
  1:56 suspended world; 2:28 gathering; 2:55 held breath; 2:56 full return;
  3:12 widening; 3:36 afterglow; 3:52 release. Musical events end at 3:52 and
  the eight-second release makes the WAV exactly 4:00.
- Changes: expanded the approved three-note signal and E5–C5–F5–E5 power-chord
  backbone into the complete arc, including a chromatic fracture, quiet centre,
  two pressure peaks, upper-register/choir widening, and intentional retreat.
  The six track patches and mix settings carry over from proof v03; final-return
  note velocities were updated in this revision.
- Original render command from the repository root:

  ```sh
  python3 soundtrack/alternate/render.py soundtrack/alternate/full-v01.json \
    --output build/soundtrack/full-v01 --name soundtrack
  afplay build/soundtrack/full-v01/soundtrack.wav
  ```

  The existing output directory is populated; repeat renders need a fresh output
  path, for example `build/soundtrack/full-v01-rerender`.

  Section markers are MIDI markers. For this fixed 120 BPM score, one beat is
  0.5 seconds; the source `sections` and `harmony` arrays include seconds for
  timestamp lookup. The `section-levels.json` report is additional local QA, not
  output generated by `render.py`.
- Technical QA: 240.0-second stereo render, peak −4.42 dBFS, whole-file RMS
  −22.03 dBFS, zero clipped samples. Section measurements show the suspended
  world at −31.61 dBFS RMS and full return at −17.51 dBFS RMS. Native MIDI import
  read all 844 notes across six instrument tracks plus the marker track. Updated
  final render was checked clean. All 16 CTest cases passed.
- Listening feedback: initial full playback and revised-ending playback both
  completed with exit 0. Listening assessment and musical acceptance remain pending.
- Decision: milestone 3 is authored but not accepted. Keep milestone 4 mixing and
  milestone 5 integration open; neither musical acceptance nor final mix/integration
  is complete.

### Mix candidate v01 and draft delivery — 2026-09-22

- Source: [mix-v01.json](../soundtrack/alternate/mix-v01.json), based on full-v01.
  All 844 notes, timings, patches, track settings, master gain, and reverb are
  preserved. The MIDI export is byte-identical to full-v01. A rerender of approved
  proof v03 with the updated renderer also matched its baseline WAV byte for byte.
- Processing: parallel midband tanh saturation is applied before the shared hall,
  retaining the dry low end. Deep drone: 20 dB drive, 0.30 wet, 160–3000 Hz band.
  Growling pulse: 24 dB drive, 0.40 wet, 180–3800 Hz band.
- Render command, from repository root (use a new output directory for rerenders):

  ```sh
  python3 soundtrack/alternate/render.py soundtrack/alternate/mix-v01.json \
    --output build/soundtrack/mix-v01 --name soundtrack
  afplay build/soundtrack/mix-v01/soundtrack.wav
  ```

- Draft delivery command (also choose a new output directory when rerunning):

  ```sh
  python3 soundtrack/alternate/prepare_delivery.py \
    --render build/soundtrack/mix-v01 \
    --output build/soundtrack/delivery-mix-v01
  ```

  The package contains `soundtrack.wav`, `soundtrack.mid`, `project.json`,
  `render-manifest.json`, `cue-sheet.json`, and `delivery-manifest.json`. The 11
  cues cover all 240 seconds / 11,520,000 audio frames; delivery hashes were
  verified and bind cue data to the audio.
- Technical checks: 240 seconds, peak −4.39 dBFS, RMS −22.14 dBFS, zero clipped
  samples, zero opening sample, and silent final second. Quiet centre RMS is
  −31.61 dBFS; first peak RMS −21.08 dBFS; full return (176–192 s) −18.23 dBFS;
  widening (192–216 s) −17.34 dBFS.
- Playback check: full mix `afplay` completed with exit 0. No
  perceptual assessment or headphone/speaker review is recorded.
- Project checks: current build passed. CTest passed 14 of 16; the two GPU checks
  could not create a GLFW graphics context because no monitor was available, even
  outside the sandbox. An earlier full 16/16 CTest run remains recorded with the
  full-v01 verification above.
- Decision: milestone 4 has a rendered, technically checked mix candidate awaiting
  listening acceptance. A draft delivery package is prepared; milestone 5 runtime
  playback and visual timing integration remain pending.

### FastTracker editing handoff — 2026-09-22

- User decision: continue editing the full song in FastTracker II Clone. The
  editable file is `soundtrack/alternate/fasttracker/v01/below-the-surface.xm`;
  the adjacent [guide](../soundtrack/alternate/fasttracker/README.md) maps its
  channels, instrument slots, and section patterns.
- Transfer: all 844 note attacks, ten instruments with 55 embedded samples,
  22 channels, and 33 patterns. The eleven section boundaries remain exact at
  120 BPM / speed 6. Note attacks and melodic key-offs round to the nearest tick
  (at most 10.42 ms); drums use complete one-shots. A silent end pattern follows
  the four-minute arrangement.
- Sound: the synth grit is sampled, filters use a fixed middle-of-song setting,
  stereo samples are folded to mono and panned, and the shared hall is omitted.
  This is an editable adaptation; keep mix-v01 as the sound reference. FastTracker
  edits become the working composition and do not update the old JSON/MIDI files.
- Verification: independent XM inspection confirmed the notes, key-offs, cue
  positions, and sample data. The bundled decoder rendered the full song with
  no clipping and a silent ending hold. The XM opened in FastTracker and was
  visually checked; manual playback of the rendered final minute completed.
  Debug configure/build and all 16 CTest cases passed.
