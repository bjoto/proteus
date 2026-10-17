# Proteus

A small native C/raylib demo playground with deliberately low-resolution output
and tracker music. The demo is now a single four-minute Eriksberg waterfront journey,
following five gulls to the 120 BPM “Below the surface” soundtrack.

The [Gothenburg boating plan](docs/BOATING_PLAN.md) records the proposed interactive direction:
a small boating playground, with free exploration and an optional arcade challenge.

## Generate the original sample pack and starter XM (archived sketch)

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

The sole scene, `cube`, uses the GPU renderer by default.
`--renderer cpu` selects the deterministic software reference; `--renderer gpu` is available for
explicit GPU comparison. GPU waterfront screenshots and profiles create a raylib/OpenGL context,
while CPU screenshots remain display-free.

Add `--gpu-aa` to enable supersampling for the GPU waterfront (off by default):

```sh
./build/proteus --scene cube --gpu-aa
./build/proteus --scene cube --time 2.5 --gpu-aa --screenshot build/cube-aa.png
```

The city renders at 4× width and height (1280x800 at the default 320x200), then averages
each 4x4 block into one virtual pixel. This shades 16 times as many main-pass pixels;
shadow and reflection map sizes stay fixed. The final image still uses nearest-neighbour,
integer-scaled presentation. The flag applies to playback, screenshots, and `--profile cube`,
including resolution changes; the CPU renderer is unaffected. GPU profiles report `gpu_aa` and
the internal render dimensions, with the downsampling pass included in submission timing.

Controls:

- `1`: 320x180
- `2`: 320x200
- `3`: 320x240
- `F2`: save the current virtual frame as `proteus-SCENE-MILLISECONDS.png`
- `F3`: toggle the debug profiler overlay
- `F11`: toggle fullscreen
- `Esc`: quit

The image is always scaled by a whole number with nearest-neighbour filtering.
Unused screen space is letterboxed.

## Boating sketch

For a downloadable Windows executable built on GitHub Actions, see
[Windows boat study builds](docs/WINDOWS_BUILD.md).

Explore the bundled Eriksberg waterfront with the same boat and controls:

```sh
./build/boat_study --location eriksberg
./build/boat_study --location eriksberg --gpu --gpu-aa
./build/boat_study --location basin
./build/boat_study --location eriksberg --screenshot build/eriksberg-boat.png --time 3
```

`basin` remains the default. Eriksberg spawns in the dock facing the crane and river; R returns
there. The map, buildings, Dockbron, crane, and dusk sky reuse the existing offline scene.
Shoreline collisions use an oriented bow, stern, and beam footprint; glancing contacts scrub speed,
turn the craft away, and rebound gently. The outer map boundary also blocks travel. CPU rendering
remains the default. For Eriksberg, `--gpu` enables
depth-tested player hull, steerable engine, wake and spray, with waterfront reflections and shadows.
The hull, engine and airborne spray cast shadows; surface foam receives them without casting solid
slab shadows. `--gpu-aa` implies `--gpu` and averages 4×4 samples per pixel, retaining the 320×200
integer-scaled presentation. GPU screenshots require a desktop graphics context; basin remains
CPU-only. Scenery is static, with no traffic or collision against overhead structures. Follow-camera
occlusion near buildings is not resolved. This is the local Eriksberg crop, not all of Gothenburg.

Map data © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright), ODbL 1.0.
See the bundled [source manifest](assets/geometry/eriksberg/manifest.json) for provenance.

Run `./build/boat_study` for the first handling prototype: a small craft in a simple basin,
viewed by a third-person camera following behind and above the boat. The basin is 240x160 world
units (four times wider and deeper than the original). The camera follows position immediately,
smooths heading changes, and looks ahead along the course. Hold left/right (or A/D) to turn the
wheel; release to hold its
position. One second moves it from centre to either limit; opposite input brings it back through
centre. The HUD shows a rotating wheel and its left/right percentage. Hold up/W to increase thrust
and down/S to reduce it, from -100% reverse through 0% neutral to 100% forward; releasing keys
holds the setting. Full-range adjustment takes four seconds. The HUD shows signed thrust and actual
engine response separately from speed. Passing through neutral is deliberate and smooth; R resets
to neutral thrust and centres the wheel. Esc closes the window.
The simulation pauses when the window loses focus. The view remains 320x200 with integer scaling.

The follow camera defaults to 20 units back and 14 units above the water. Hold `-` to pull back,
`+` (or `=`) to move closer, and PageUp/PageDown to raise/lower the camera. `C` restores the default
view; `R` resets the boat while keeping your view settings. Distance is limited to 8..48 and height
to 4..36. Settings last for the current run; reuse a preferred view with command-line options,
which also work for screenshots:

```sh
./build/boat_study --camera-distance 24 --camera-height 18
```

Steering angles the outboard at the stern. Its off-centre thrust pushes the stern sideways and
builds rotation; speed-dependent resistance at the bow and stern damps sliding and turning. The engine follows
the held wheel setting. Bring the wheel back to centre or countersteer to catch a turn. At idle,
turning the engine cannot start a turn from rest. Propeller wash shows the water being pushed in the
opposite direction to thrust. This is a simplified planar model with tunable forces and inertia.

While coasting, water flowing across the angled outboard creates a passive steering force.
It fades as the boat slows; no flow means no coasting steering. Forward drag includes a quadratic
term, while hull grip grows with forward speed and local sideways slip. Try building speed,
lowering the throttle to 0%, and holding a modest wheel angle through the coast. These are tunable
approximations for calm water, without planing or waves.

The basin walls bounce the craft back gently; the two orange markers are visual turning references
and have no collision yet. A spreading wake traces your path and fades; hard turns throw spray
according to sideways slip. Reset clears both effects. This sketch is silent, with no scoring or
geographic scenery.
The [boating plan](docs/BOATING_PLAN.md) tracks the next steps toward Gothenburg and the archipelago.

For display-free captures of a scripted acceleration and right turn, or a windowed smoke check:

```sh
./build/boat_study --screenshot build/boat-start.png
./build/boat_study --screenshot build/boat-turn.png --time 3
./build/boat_study --frames 180
./build/boat_study --demo --frames 720
```

`--demo` previews the same scripted run as screenshots: raise thrust for two seconds, turn after
one second, then lower it through neutral into reverse. R replays it.
Screenshot `--time` selects a point in that run (0..60 seconds). The water effects use the same
fixed simulation ticks as handling.

## Dance study

For a procedural house-inspired shuffle, run `./build/dance_study`. Edit
[`assets/dance/house.dance`](assets/dance/house.dance) to arrange named phrases, then press `L`
to reload. The default pairs fast footwork with slower torso and arm phrasing. Compare the
relaxed version with `--choreography assets/dance/house-groove.dance`, or use `--classic`
for the earlier cross-and-reach study. See the
[dance study notes](docs/DANCE_STUDY.md) for the language, controls and screenshots.

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
dimensions, city coverage, exposed dusk sky, and broad lit moving-object regions with
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

The sky fades from indigo through violet to a peach sunset horizon, with dark cloud streaks
and a small pale moon. CPU and GPU share a fixed world panorama, so the sky follows camera
rotation naturally; GPU water reflections use the same sky through the mirrored camera.

Distance atmosphere is also enabled by default. Compare its muted rose dusk haze at the same scene time
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
./build/proteus --renderer cpu --profile cube
```

The default CPU run is headless and measures 600 frames after 60 warm-up frames. Use
`--frames`, `--warmup`, and `--resolution` to change the workload. For example:

```sh
./build/proteus --renderer cpu --profile cube --frames 1200 --warmup 120 --resolution 320x240
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
sample -wait proteus 10 1 -file build/cube.sample.txt
./build/proteus --renderer cpu --profile cube --frames 100000
```

With full Xcode selected as the active developer directory, Instruments can instead record
a Time Profiler trace directly:

```sh
xcrun xctrace record --template 'Time Profiler' --output build/cube.trace \
  --launch -- ./build/proteus --renderer cpu --profile cube --frames 100000
```

## Editing the music

The runtime uses [below-the-surface.xm](soundtrack/alternate/fasttracker/v01/below-the-surface.xm).
CMake copies that working XM into the build's `assets/music/` directory on every build;
rebuild after saving tracker edits. Playback uses the bundled asset path, so it also works
when launched outside the repository root. See the [tracker guide](soundtrack/alternate/fasttracker/README.md)
for its instruments, channels, and section cues. The starter XM remains an archived sketch
and is still used by the separate dance study.

The waterfront runs from 0:00 through the four-minute ending at 120 BPM, speed 6.
Both `--time` and `--scene cube --time` now address the same timeline; times beyond
240 seconds hold the final shot. The song plays once and retains its silent ending.
There is no scene switching. Close and relaunch to replay.

Five low-poly gulls glide in a loose formation with staggered wingbeats and gentle banking.
The camera trails the flock with a stable horizon, following a broad circuit over the street,
river, and dock. Turns follow the soundtrack's section cues. Bird, camera, and actor positions
are deterministic functions of time, so direct seeks match continuous playback. The flock is
shared by CPU and GPU rendering, including GPU reflections and shadows. The original short/long
camera routes remain available in the standalone `waterfront_storyboard` diagnostic.

The virtual framebuffer defaults to 320x200 with integer scaling; try all three shapes while
it runs. The `cube` name is retained for screenshots and profiling. The city uses bundled
geographic geometry and an authored crane. See [the 3D renderer notes](docs/3D.md) and
[the living city plan](docs/LIVING_CITY_PLAN.md).

Map data © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright),
[ODbL 1.0](https://opendatacommons.org/licenses/odbl/1-0/). The retained snapshot and
height assumptions are recorded in `assets/geometry/eriksberg/manifest.json`.

## Editor setup

See the [Emacs development guide](docs/emacs.md) for `c-ts-mode`, Eglot,
clangd, formatting, compilation, navigation, and LLDB debugging.
