# Working on the starter XM

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

The functions in `tools/generate_audio.py` are deliberately small. Useful
parameters to change are:

- exponential decay: larger numbers make a shorter, tighter sound;
- oscillator frequency: changes pitch;
- mixtures of sine and saw waves: changes timbre;
- the fixed random seed: creates a different but repeatable noise texture.

This gives the production its own sound rather than making it dependent on an
unknown sample pack. We can add richer instruments, loops, or recorded samples
once the musical direction is clearer.
