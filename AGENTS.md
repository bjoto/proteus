# AGENTS.md

## Project

Proteus is a small C17/raylib demo with low-resolution, integer-scaled visuals and
tracker music. Keep the code and project structure simple; this is a playground for
iterating on effects, timing, audio, and presentation.

## Layout

- `src/main.c`: command-line modes, application loop, virtual framebuffer, and presentation.
- `src/scenes.c`: single-scene timing and software-rendered waterfront entry point.
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
- Keep the main demo synchronized with Below the surface’s 120 BPM, speed-6, four-minute timeline
  when changing the arrangement or flock camera cues. The separate dance study uses the original
  125 BPM starter XM.
- Treat `assets/music/starter.xm` and the sample WAVs as generated outputs. Running
  `python3 tools/generate_audio.py` overwrites them; do so only when the generated audio is meant to
  change, and validate the resulting XM.
- Update the nearby documentation when controls, asset names, arrangement, or development commands
  change.
- Do not commit `build/`, caches, or machine-specific compile databases.

## Documentation maintenance

- Treat documentation as part of each implementation step. Before handing off, update the owning
  plan's current status, completed task, next task, verification evidence, and known limitations.
  Use [the documentation index](docs/README.md) to find the owner for each area.
- Update affected user guides and commands in the same change. Search for superseded claims across
  `README.md`, `docs/`, and relevant asset/workspace guides; do not fix only the newest log entry.
- Keep the index's status overview consistent when a milestone or next task changes. Each plan owns
  its detailed task status; link to it rather than copying detailed checklists into other documents.
- Distinguish implementation, automated verification, manual playback, and user acceptance. Record
  only checks actually performed, with date and relevant platform/options. Never turn an old test
  count into a claim about the current suite or infer user approval from a successful run.
- Keep dated history as history: label superseded proposals and baseline findings, and correct live
  summaries instead of appending contradictory updates. Editor autosaves are not planning sources.
- Check changed local links and command examples. Report unresolved documentation uncertainty and
  verification limits before handoff; documentation-only changes need no new audiovisual review.

## Before handing off

Run the build and CTest command above. Capture and inspect representative frames when visual effects
change. Add a manual demo check when motion, input, windowing, audio playback, or asset loading
changed. Use Xcode Instruments when performance or runtime behavior warrants deeper investigation.
Report anything you could not verify.
