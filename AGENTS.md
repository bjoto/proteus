# AGENTS.md

## Project

Proteus is a small C17/raylib demo with low-resolution, integer-scaled visuals and
tracker music. Keep the code and project structure simple; this is a playground for
iterating on effects, timing, audio, and presentation.

## Layout

- `src/main.c`: command-line modes, application loop, virtual framebuffer, and presentation.
- `src/scenes.c`: scene timing and software-rendered visual effects.
- `src/profiler.c`: runtime profiling statistics, reports, and overlay.
- `src/audio_spectrum.c`: PCM capture, FFT analysis, and smoothed frequency-band levels.
- `src/xm_audio.c`: XM-specific playback helpers layered over raylib's bundled decoder.
- `assets/music/`: XM music played by the demo.
- `assets/samples/`: source WAV samples for tracker editing.
- `tools/generate_audio.py`: deterministic generator for the starter samples and XM.
- `tools/validate_xm.c`: XM parser/render smoke test used by CTest.
- `docs/`: focused workflow notes, including music and editor setup.

## Build and verify

The first configure fetches pinned raylib 6.0 sources and therefore needs network access.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Run the demo from the repository root so its relative asset paths resolve:

```sh
./build/proteus
```

For deterministic CPU visual verification, render and inspect representative frames without opening
a window. The `cube` scene defaults to GPU and therefore needs a graphics context; select
`--renderer cpu` for display-free captures. Without `--scene`, `--time` addresses the complete
timeline; with `--scene`, it is local to that scene. `--resolution` is optional and defaults to
320x200.

```sh
./build/proteus --renderer cpu --screenshot build/timeline.png --time 48
./build/proteus --renderer cpu --screenshot build/cube.png --scene cube --time 2.5 --resolution 320x240
```

On macOS, deeper CPU profiling can use Xcode's Instruments command-line tools when full Xcode is the
active developer directory. For example, record a Time Profiler trace against a long deterministic
scene run:

```sh
xcrun xctrace record --template 'Time Profiler' --output build/cube.trace \
  --launch -- ./build/proteus --renderer cpu --profile cube --frames 100000
```

## Conventions

- Format C with the checked-in `.clang-format`; use four spaces and keep lines near 100 columns.
- Keep the project warning-clean under the compiler flags in `CMakeLists.txt`.
- Preserve nearest-neighbour, integer-scaled rendering unless a change explicitly targets it.
- Keep animation timing synchronized with the 125 BPM, speed-6 XM timeline when changing scenes or
  arrangement.
- Treat `assets/music/starter.xm` and the sample WAVs as generated outputs. Running
  `python3 tools/generate_audio.py` overwrites them; do so only when the generated audio is meant to
  change, and validate the resulting XM.
- Update the nearby documentation when controls, asset names, arrangement, or development commands
  change.
- Do not commit `build/`, caches, or machine-specific compile databases.

## Before handing off

Run the build and CTest command above. Capture and inspect representative frames when visual effects
change. Add a manual demo check when motion, input, windowing, audio playback, or asset loading
changed. Use Xcode Instruments when performance or runtime behavior warrants deeper investigation.
Report anything you could not verify.
