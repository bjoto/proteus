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

Start at a global timeline position, or select a scene and use a scene-local time:

```sh
./build/proteus --time 30
./build/proteus --scene cube --time 2.5
```

The music seeks with the visuals so their timing remains synchronized. These options have the same
meaning in interactive and screenshot modes.

Controls:

- `1`: 320x180
- `2`: 320x200
- `3`: 320x240
- `F3`: toggle the debug profiler overlay
- `F11`: toggle fullscreen
- `Esc`: quit

The image is always scaled by a whole number with nearest-neighbour filtering.
Unused screen space is letterboxed.

## Capture a screenshot

Render a PNG directly from the CPU-side framebuffer without opening a window:

```sh
./build/proteus --screenshot build/proteus.png --time 48
```

By default, `--time` addresses the complete visual timeline. Add `--scene` to select a scene and
interpret the time relative to its start:

```sh
./build/proteus --screenshot build/cube.png --scene cube --time 2.5
```

Screenshot mode also accepts `--resolution` and defaults to 320x200.

The profiler keeps a rolling 240-frame history (about four seconds at 60 FPS) and
graphs total frame wall time, CPU scene generation plus texture upload, and
presentation time (including any vertical-sync wait). It also reports rolling
min/average/95th-percentile/max timing, the active scene, display dimensions,
framebuffer traffic, and audio position. Samples and the graph update every frame,
while the numeric readout refreshes four times per second so it remains readable. The profiler also
shows smoothed sub-bass, bass, low-mid, mid, presence, and high-frequency levels, plus overall level
and onset strength from the live XM output.

## Repeatable scene profiling

Run one scene against a deterministic 60 FPS timeline and print a machine-readable
key/value report:

```sh
./build/proteus --profile fire
```

The default run is headless and measures 600 frames after 60 warm-up frames. Use
`--frames`, `--warmup`, and `--resolution` to change the workload. For example:

```sh
./build/proteus --profile plasma --frames 1200 --warmup 120 --resolution 320x240
```

Add `--visible` to create a graphics context and include texture-upload and presentation
timings. Without it, those report fields are zero and the run isolates CPU scene generation.
The report includes its build type and a final-framebuffer checksum so automated comparisons
can reject mismatched builds or unintended visual changes. Run `./build/proteus --help` for
the complete interface.

For function-level CPU sampling on macOS, the built-in `sample` command can attach to a
longer headless run. Start the sampler in one terminal, then the workload in another:

```sh
sample -wait proteus 10 1 -file build/fire.sample.txt
./build/proteus --profile fire --frames 100000
```

With full Xcode selected as the active developer directory, Instruments can instead record
a Time Profiler trace directly:

```sh
xcrun xctrace record --template 'Time Profiler' --output build/fire.trace \
  --launch -- ./build/proteus --profile fire --frames 100000
```

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

The visuals follow the eight-pattern music loop. Two patterns each are given to
the starfield, fire, and plasma. The final two introduce a rotating, shaded cube,
transformed, projected, and rasterized entirely in the CPU-side virtual framebuffer.

## Editor setup

See the [Emacs development guide](docs/emacs.md) for `c-ts-mode`, Eglot,
clangd, formatting, compilation, navigation, and LLDB debugging.
