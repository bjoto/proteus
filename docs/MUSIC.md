# Working on the music

The demo now plays the working [Below the surface XM](../soundtrack/alternate/fasttracker/v01/below-the-surface.xm)
at 120 BPM, speed 6. See its [tracker guide](../soundtrack/alternate/fasttracker/README.md).
Rebuild after editing it to refresh the bundled runtime copy. The four-minute waterfront
and flock camera hold at 4:00; the XM has a silent end hold. If the arrangement length or
section cues change, update `WATERFRONT_DEMO_DURATION` and the flock route cue times in
`src/waterfront_scene.c` / `src/waterfront_scene.h` alongside the audio.

The playback helper reconstructs the XM decoder from its source before starting or
seeking. Raylib's duration scan leaves a pending jump to the silent ending which its
partial decoder reset does not clear; reusing that state would play only the first
sound. The XM validation covers that loader sequence and backward seeking as well as
full-song decoding. Call the helper only while the audio stream is stopped.

## Archived starter sketch

The notes below describe the original starter XM, retained for the separate dance study.

For the new composition's brief, milestones, and listening criteria, see the
[alternate soundtrack plan](SOUNDTRACK_PLAN.md).

`starter.xm` is a sketch, not precious source material. It is meant to give us
something audible and synchronized on day one.

## Arrangement

At 125 BPM and speed 6, four rows make one beat and 16 rows make one 4/4 bar.
Every pattern is four bars and lasts 7.68 seconds.

| Pattern | Role | Layers |
|---|---|---|
| 00 | Intro | Kick and sparse hat |
| 01 | Rhythm enters | Drums and bass |
| 02 | Harmony enters | Drums, bass, minor chord stabs |
| 03 | First peak | Full arrangement and lead |
| 04 | Breakdown | Drums and bass; no chords or lead |
| 05 | Rebuild | Full arrangement and lead |
| 06 | Lift | New root, full arrangement and lead |
| 07 | Ending/loop | Full arrangement and snare fill |

## Channels

| Channel | Purpose |
|---|---|
| 1 | Kick |
| 2 | Snare |
| 3 | Closed hi-hat |
| 4 | Bass |
| 5–7 | Three-note chord |
| 8 | Lead |

## First useful edits in FastTracker II Clone

1. Save the generated module under a new name before experimenting.
2. Solo channels to hear what each layer contributes.
3. Change the bass notes in channel 4, then move the chord notes by the same
   number of semitones.
4. Rewrite channel 8. A short, memorable motif is more valuable than many notes.
5. Replace instruments one at a time by loading WAV samples from
   `assets/samples` into their existing instrument slots.

The generator is deterministic. Running it again overwrites `starter.xm` and the
five generated WAV files, so keep the evolving composition under another name,
for example `proteus-theme.xm`, and update the filename in `src/main.c`.

## Making new samples without a sample library

Run `python3 tools/generate_audio.py` from the repository root. It uses only the
Python standard library. Check sample encoding with
`python3 -m unittest discover -s tools -p 'test_generate_audio.py'`, then run
`ctest --test-dir build --output-on-failure` to check playback through the XM decoder.

The drums in `make_drums()` are synthesized at 44.1 kHz and stored as 16-bit mono
PCM in both the WAVs and XM. Each has its own fixed noise seed:

- Kick: an exponential pitch sweep from 155 Hz to 49 Hz, a short second harmonic,
  soft saturation, and a filtered noise beater transient. Sweep time controls the
  punch; body decay controls the weight and length.
- Snare: two decaying drum-body modes plus bandpass-filtered wire noise and a short
  snap. Body/noise balance controls the tone; wire decay controls the rattle.
- Closed hi-hat: six inharmonic sine partials mixed with filtered noise, with a
  fast attack and two short decays. Filter frequency controls brightness and the
  slower decay controls how tightly the hat closes.

`finish_sample()` removes subsonic DC, fades the sample boundaries, and sets a
separate peak level for each instrument with headroom. XM tuning preserves the
drums' intended playback rate at C-4; its finite tuning resolution introduces a
small rounding error.

The bass in `make_bass()` also uses 44.1 kHz/16-bit mono PCM. A finite mix of 24
saw/pulse-like harmonics gives it a bright pluck, with a brightness cutoff falling
from 1800 Hz towards 260 Hz over a 65 ms decay. A separate fundamental decays more
slowly to retain low-end weight. The 2.5 ms attack and 45 ms tail fade soften the
sample boundaries. Change the harmonic weights for the tone, cutoff and its decay
for the pluck, or body decay for a longer bass. These times describe the source
sample; lower tracker notes stretch it during playback.

The lead/chord instrument in `make_synth()` is a 44.1 kHz/16-bit pluck with three
saw-like voices: a strong centre voice and two quieter voices detuned by ±5 cents.
Twenty harmonics per voice keep the spectrum controlled when the melody plays
high notes. Brightness falls from 2600 Hz towards 650 Hz with an 85 ms decay;
the amplitude decays over 160 ms. A 4 ms attack and 60 ms tail fade smooth the
boundaries of the 440 ms source sample. Detuning and voice gains control thickness;
the cutoff and harmonic weights control how sharply chords and lead cut through.
Higher tracker notes shorten the source envelope during playback.

Both melodic fundamentals compensate for the old samples' approximate XM tuning
so the existing bass line, chords, and lead keep their audible pitch. All five
instruments now use 44.1 kHz/16-bit PCM. Instrument slots, patterns, volumes, and
125 BPM/speed-6 timing are unchanged.

Audition `assets/samples/kick.wav`, `snare.wav`, and `hat.wav` individually, then
listen to `./build/proteus --time 7.68` for the drums with bass and the later full
arrangement. To compare revisions, keep a copy of the previous XM before
regenerating, and solo channels 1–3 in FastTracker II Clone.

For the bass, audition `assets/samples/bass.wav`, then solo channel 4 in the
tracker and listen with the drums at 7.68 seconds and the chords at 15.36 seconds.

For the synth, audition `assets/samples/synth.wav`, then listen to
`./build/proteus --time 15.36` for chords and `./build/proteus --time 23.04` for
the full arrangement with melody. Solo channels 5–7 for the chord voicing or
channel 8 for the lead in FastTracker II Clone.
