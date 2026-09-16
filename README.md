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

The `cube` waterfront uses the GPU renderer by default. Other scenes remain CPU-rendered.
`--renderer cpu` selects the deterministic software reference; `--renderer gpu` is available for
explicit GPU comparison. GPU waterfront screenshots and profiles create a raylib/OpenGL context,
while CPU screenshots remain display-free.

Controls:

- `1`: 320x180
- `2`: 320x200
- `3`: 320x240
- `Left` / `Right`: jump to the previous / next scene and seek the music with it
- `F2`: save the current virtual frame as `proteus-SCENE-MILLISECONDS.png`
- `F3`: toggle the debug profiler overlay
- `F11`: toggle fullscreen
- `Esc`: quit

The image is always scaled by a whole number with nearest-neighbour filtering.
Unused screen space is letterboxed.

## Capture a screenshot

Render a PNG without opening a window from the CPU-side framebuffer:

```sh
./build/proteus --renderer cpu --screenshot build/proteus.png --time 48
```

By default, `--time` addresses the complete visual timeline. Add `--scene` to select a scene and
interpret the time relative to its start:

```sh
./build/proteus --screenshot build/cube.png --scene cube --time 2.5
```

Screenshot mode also accepts `--resolution` and defaults to 320x200.

The default GPU `cube` screenshot needs a desktop graphics context. Use `--renderer cpu` for a
display-free waterfront reference capture:

```sh
./build/proteus --renderer cpu --screenshot build/cube-cpu.png --scene cube --time 2.5
./build/proteus --renderer gpu --screenshot build/cube-gpu.png --scene cube --time 2.5
```

## GPU visual review

GPU checks are deliberately opt-in because they need a desktop graphics context. Configure them on
a machine that can create a raylib/OpenGL window, then run either the labelled CTest case or the
review target:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DPROTEUS_GPU_TESTS=ON
cmake --build build -j
ctest --test-dir build -L gpu --output-on-failure
# or: cmake --build build --target gpu_review
```

The review command captures the `cube` scene at local times 0, 1.7, 2.7, 5, 7.5, 10, and 15 seconds
with both renderers. It writes `build/gpu-review/contact-sheet.png` (CPU | GPU | amplified visual
diff), all source captures, and `manifest.json` with the build and OpenGL renderer details. It checks
dimensions, city coverage, sky-band presence/order, and broad lit moving-object regions with
tolerances; it never compares CPU/GPU image hashes. The GPU fixture in the same command checks
render target dimensions, orientation, depth, winding, normal submission, and shader rejection.

If no graphics context is available in an automated session, the GPU test fails and records the
context error in the manifest. Ordinary CTest does not include this test and remains display-free.

The GPU waterfront enables its directional shadow map by default. Use `--gpu-shadows off` for a
same-time comparison. The focused fixture can compare its supported shadow resolutions and export
the light-frustum depth map for review:

```sh
./build/gpu_fixture --city --time 5 --shadow-resolution 512 --screenshot build/shadow-512.png
./build/gpu_fixture --city --time 5 --shadow-resolution 1024 --screenshot build/shadow-1024.png
./build/gpu_fixture --city --time 5 --shadow-map --screenshot build/shadow-depth.png
```

Distance atmosphere is also enabled by default. Compare its cool dusk haze at the same scene time
with `--gpu-atmosphere off` in the demo or `--atmosphere off` in the fixture.

The GPU water plane uses a 160x120 reflected scene pass by default. Compare it with
`--gpu-reflections off` in the demo or `--reflections off` in the fixture:

```sh
./build/gpu_fixture --city --time 10 --reflections on --screenshot build/reflection-on.png
./build/gpu_fixture --city --time 10 --reflections off --screenshot build/reflection-off.png
```

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

The default CPU run is headless and measures 600 frames after 60 warm-up frames. Use
`--frames`, `--warmup`, and `--resolution` to change the workload. For example:

```sh
./build/proteus --profile plasma --frames 1200 --warmup 120 --resolution 320x240
```

Add `--visible` to create a graphics context and include texture-upload and presentation
timings. Without it, those report fields are zero and the run isolates CPU scene generation.
The report includes its build type and a final-framebuffer checksum so automated comparisons
can reject mismatched builds or unintended visual changes. Run `./build/proteus --help` for
the complete interface.

`--profile cube` uses the GPU waterfront by default and reports CPU submission timing for the
shadow, reflection, and city passes; it does not report GPU timestamp-query timing. Select
`--renderer cpu --profile cube` for the original software checksum/profile route.

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
the starfield, fire, and plasma. The final two show a 15.36-second Eriksberg waterfront
fly-by: follow evening traffic along Monsungatan, pass lit apartments, and look back
across the dock, with the crane in the background. The `cube`
scene name is retained for screenshots and profiling. It uses bundled geographic geometry and an
authored crane; the GPU route is the default for this scene and the CPU route remains the reference.
See [the 3D renderer notes](docs/3D.md).
The proposed next visual pass is described in [the living city plan](docs/LIVING_CITY_PLAN.md).

Map data © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright),
[ODbL 1.0](https://opendatacommons.org/licenses/odbl/1-0/). The retained snapshot and
height assumptions are recorded in `assets/geometry/eriksberg/manifest.json`.

## Editor setup

See the [Emacs development guide](docs/emacs.md) for `c-ts-mode`, Eglot,
clangd, formatting, compilation, navigation, and LLDB debugging.
