# Linux boat study checks

Status, 2026-10-24: `.github/workflows/linux-boat.yml` adds an Ubuntu 24.04 X11 build,
the routine CPU suite and an Xvfb mouse-capture startup/clean-exit smoke run. The workflow
runs on branch pushes or manual dispatch. Linux CI has compiled and run the CPU suite;
the full workflow and user acceptance remain pending.

2026-10-24, first run after the merge into `main`: GCC on glibc stopped compiling
`src/boat_profile.c` and `src/profiler.c` because strict C17 (`CMAKE_C_EXTENSIONS OFF`) hides
`clock_gettime`/`CLOCK_MONOTONIC`; macOS and Windows are unaffected. Both files now define
`_POSIX_C_SOURCE 200809L` first on non-Windows builds, as `src/boat_net_enet.c` already did.
No glibc toolchain was available locally (macOS, no container runtime), so the fix is verified
only by macOS Debug/Release builds and CTest; further Linux errors may surface on the next run.

The next run ([40085540407](https://github.com/doomdavve/proteus/actions/runs/40085540407), `4c9c7c8`)
compiled, then failed `boat_core_boundary` and `boat_net_boundary`: GCC's Debug build calls libm
`nearbyint` (used by `boat_clock.h`), which Clang inlines, and the symbol allowlist lacked it. It
is now allowed with the other libm rounding functions. The later checksum failure excerpt
does not include the boundary gate results.

Run [40087174088](https://github.com/doomdavve/proteus/actions/runs/40087174088/job/120852369782)
then failed `cube_render_checksum`: the CPU frame produced `a920a253d6c6aa0a` rather than
the existing `80d44611b75ac3fb` baseline. CMake now selects the reported Linux hash only
for Linux/GCC on x86-64; other configurations keep their existing baseline. Exact
floating-point rasterization is platform/toolchain dependent, but the specific numerical
source of this difference has not been isolated. This is a CI-derived baseline adjustment,
not a visually reviewed Linux frame or a renderer change. Linux rerun and screenshot review
remain pending; no local glibc toolchain or container runtime is available.

Local verification of the checksum adjustment, 2026-10-24, Apple Silicon/macOS: Debug
configure/build and all 105 active CTests passed (GPU tests enabled; fifteen retired field
cases disabled), in 86.11 s. The first sandboxed suite was interrupted after loopback
socket access was denied; the complete rerun used desktop/loopback access. Six CMake-script
cases checked checksum selection for Linux/GCC x86-64 and the unchanged fallback for
Linux/Clang, Linux ARM64, macOS and Windows. Local documentation links and `git diff --check`
passed. No manual playback was needed for this test-only change; Linux execution and visual
review of the new baseline remain unverified.

On Ubuntu, install `libasound2-dev libx11-dev libxrandr-dev libxi-dev libxcursor-dev
libxinerama-dev libgl1-mesa-dev xvfb xauth`, then:

```sh
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug -DPROTEUS_GPU_TESTS=OFF -DGLFW_BUILD_WAYLAND=OFF
cmake --build build-linux --parallel 2
ctest --test-dir build-linux -j 4 --output-on-failure
xvfb-run -a ./build-linux/boat_study --location basin --mouse-steering --frames 120
```

The [accepted control scheme](BOAT_HUD.md) uses raylib/GLFW for native capture, relative
movement and vertical scrolling. Shared controller checks run in `boat_handling`; the browser
event contract also runs when Node.js is available. The Xvfb run checks startup/exit only,
not real pointer movement, scrolling, audio or window-manager behavior. Wayland is unverified.

Completed task: add repeatable Linux CI/build commands and a scoped Linux CPU checksum
baseline. Next task: rerun the workflow and review its CPU screenshots, then test physical
input, help/focus/Esc release, renderer/audio and native multiplayer on Linux.
macOS/browser verification evidence belongs to [the HUD owner](BOAT_HUD.md).
