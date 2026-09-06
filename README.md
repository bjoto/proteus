# Proteus

A small native C/raylib demo playground with deliberately low-resolution output
and tracker music.

## Generate the original sample pack and starter XM

```sh
python3 tools/generate_audio.py
```

The generated WAV files are source material for editing in FastTracker II Clone.
`starter.xm` is an intentionally simple, editable sketch: 125 BPM, eight patterns,
four-on-the-floor drums, bass, minor chords, and a small lead motif.

## Build and run

The first configure downloads the pinned raylib source. Later builds are offline.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build
./build/proteus
```

Controls:

- `1`: 320x180
- `2`: 320x200
- `3`: 320x240
- `F11`: toggle fullscreen
- `Esc`: quit

The image is always scaled by a whole number with nearest-neighbour filtering.
Unused screen space is letterboxed.

## Editing the music

Open `assets/music/starter.xm` in FastTracker II Clone. The instruments are:

1. Kick
2. Snare
3. Closed hi-hat
4. Bass
5. Synth

See `docs/MUSIC.md` for the pattern arrangement, channel layout, and a practical
first editing session.

The virtual framebuffer defaults to 320x200. Resolution is intentionally not an
architectural decision: try all three shapes while the demo runs.

## Editor setup

See the [Emacs development guide](docs/emacs.md) for `c-ts-mode`, Eglot,
clangd, formatting, compilation, navigation, and LLDB debugging.
