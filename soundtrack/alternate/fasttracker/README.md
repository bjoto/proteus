# Edit Below the surface in FastTracker II

Open [v01/below-the-surface.xm](v01/below-the-surface.xm). This is the editable
four-minute arrangement transferred from `mix-v01.json`: 844 individual notes,
22 channels, 33 patterns, ten instruments, and 55 embedded mono samples. It needs
no external instrument bank for playback or editing in FastTracker.

```sh
open -a /Applications/ft2-clone-macos.app \
  "$PWD/soundtrack/alternate/fasttracker/v01/below-the-surface.xm"
```

Save your next revision under a new XM filename. The XM becomes the working song;
edits in FastTracker do not update the earlier JSON/MIDI sources. Keep this first
transfer and the original WAV mix as references. The exporter refuses to overwrite
a populated directory, including one containing your tracker edits.

## Runtime playback

Proteus now plays this working XM. `cmake --build build -j` copies it into the build
asset directory without modifying the source. The sole `cube` scene follows a small
flock for four minutes, then holds its last shot while the song ends in silence.
Rebuild after tracker edits; update the demo duration/camera cues if the arrangement changes.

## Finding the parts

Tempo is **120 BPM, speed 6**. Four rows make a beat, 16 rows a bar, and a normal
64-row pattern lasts eight seconds. Some patterns are shorter to put every section
boundary at the beginning of a pattern. The table uses the hexadecimal order and
pattern numbers displayed by FastTracker; every section starts at row `00`.

| Time | Section | Order / pattern |
|---|---|---|
| 0:00 | The signal | `00` |
| 0:24 | Awakening | `03` |
| 0:56 | First weight | `07` |
| 1:28 | Fracture | `0B` |
| 1:56 | Suspended world | `0F` |
| 2:28 | The gathering | `13` |
| 2:55 | Held breath | `17` |
| 2:56 | Full return | `18` |
| 3:12 | Widening | `1A` |
| 3:36 | Afterglow | `1D` |
| 3:52 | Release | `1F` |
| 4:00 | Silent end hold | `20` |

The last pattern jumps to itself with `B20`, leaving silence after the ending.
Stop playback normally, or replace that ending behavior when rearranging the song.

| Channels (decimal) | Part | Instrument slot (hex) |
|---|---|---|
| 1–2 | Deep drone | `01` |
| 3–4 | Growling pulse | `02` |
| 5–8 | Low choir | `03` |
| 9–16 | Percussion and overlapping drum tails | `04`–`08` |
| 17–18 | Three-note signal | `09` |
| 19–21 | Low strings | `0A` |
| 22 | Ending control | — |

Drum instruments are kick `04`, snare `05`, floor tom `06`, closed hat `07`, and
low tom `08`. Their original trigger notes are C-2, D-2, F-2, F#2, and G-2.
Chord voices and overlapping releases occupy separate channels so you can change
individual notes. Solo the listed channel group when working on a part.

Volume columns carry the note dynamics. `EDx` places an attack within a row;
`Kxx` and key-off notes release held sounds. When moving notes, move their key-offs
as well. Instrument volume envelopes control release length; sample loops sustain
the drone, pulse, choir, signal, and strings. The sample keyboard maps cover notes
between the recorded pitches too, using the nearest recorded sample.

## Sound of the transfer

This is an editable tracker adaptation of the mix. The synths were sampled at a
middle-of-song filter setting; their grit is baked into the samples. The evolving
filters, continuous stereo detuning image, and shared Apple hall are not recreated
as live effects. Samples are folded to mono and panned by instrument. Note
velocities and releases are approximated with tracker volumes/envelopes; drums
play as complete one-shots. The XM is drier and quieter than the original mix.

All source pitches and note attacks are retained. Note starts and melodic key-offs
are placed on the nearest tick, with at most 10.42 ms of timing rounding. Sample
tuning follows FastTracker's finetune steps (about 2.83 cents sharp at this sample
rate). These are transfer limitations, not changes to the written harmony.

The [original mix](../../../build/soundtrack/mix-v01/soundtrack.wav) remains the
sound reference. Individual source WAVs are in `v01/samples/`; loop points,
instrument provenance, source/render hashes, note mapping, and section positions
are recorded in `v01/transfer.json`. The XM embeds the samples and loops.

## Recreating the transfer

Run from the repository root. Rendering the source instruments needs macOS, the
same Apple bank as the original mix, and the existing Swift renderer. This follows
the [XM format reference](https://github.com/milkytracker/MilkyTracker/blob/master/resources/reference/xm-form.txt)
and uses ordinary XM 1.04 instruments, envelopes, delays, and key-offs.

Choose fresh paths for both stages; these example paths differ from the existing
bank and editable handoff:

```sh
python3 soundtrack/alternate/export_fasttracker.py prepare \
  --output build/soundtrack/ft2-bank-rerender
for project_file in build/soundtrack/ft2-bank-rerender/[0-9]*.json; do
  python3 soundtrack/alternate/render.py "$project_file" \
    --output "${project_file%.json}" --name samples
done
python3 soundtrack/alternate/export_fasttracker.py export \
  --bank build/soundtrack/ft2-bank-rerender \
  --output build/soundtrack/ft2-transfer-rerender
python3 soundtrack/alternate/check_fasttracker.py build/soundtrack/ft2-transfer-rerender
```

The checker verifies the generated transfer, including exact note counts/pitches,
key-offs, timing, cue positions, sample delta encoding, loops, and tuning. Its
original-file hash check will intentionally fail after you edit an XM in FastTracker.

## Verification of v01

- Independent XM inspection recovered all 844 attacks and 602 melodic key-offs,
  all 55 sample WAVs, and all eleven section boundaries.
- The project's bundled XM decoder rendered the entire song: peak −8.98 dBFS,
  RMS −27.34 dBFS, no clipped or nonfinite samples. Playback remains silent in the
  ending hold beyond 4:00. The local decoder render is
  `build/soundtrack/fasttracker-v01.wav`.
- FastTracker II Clone opened the file; its title, 120 BPM / speed 6, pattern
  list, and first drone note were visually confirmed.
- Manual playback of the XM-rendered final 64 seconds completed successfully.
  This confirms playback; the sampled adaptation still needs your musical review.
- Debug configure/build and all 16 CTest cases passed.
