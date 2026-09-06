# AGENTS.md

## Project

Proteus is a small C17/raylib demo with low-resolution, integer-scaled visuals and
tracker music. Keep the code and project structure simple; this is a playground for
iterating on effects, timing, audio, and presentation.

## Layout

- `src/main.c`: application loop, virtual framebuffer, scene timing, and visual effects.
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

Run the build and CTest command above. Add a manual demo check when runtime visuals, input, windowing,
audio playback, or asset loading changed. Report anything you could not verify.
