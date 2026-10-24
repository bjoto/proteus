# Linux boat study checks

Status, 2026-10-24: `.github/workflows/linux-boat.yml` adds an Ubuntu 24.04 X11 build,
the routine CPU suite and an Xvfb mouse-capture startup/clean-exit smoke run. The workflow
runs on branch pushes or manual dispatch. No Linux execution or user acceptance is claimed.

2026-10-24, first run after the merge into `main`: GCC on glibc stopped compiling
`src/boat_profile.c` and `src/profiler.c` because strict C17 (`CMAKE_C_EXTENSIONS OFF`) hides
`clock_gettime`/`CLOCK_MONOTONIC`; macOS and Windows are unaffected. Both files now define
`_POSIX_C_SOURCE 200809L` first on non-Windows builds, as `src/boat_net_enet.c` already did.
No glibc toolchain was available locally (macOS, no container runtime), so the fix is verified
only by macOS Debug/Release builds and CTest; further Linux errors may surface on the next run.

The next run ([40085540407](https://github.com/doomdavve/proteus/actions/runs/40085540407), `4c9c7c8`)
compiled, then failed `boat_core_boundary` and `boat_net_boundary`: GCC's Debug build calls libm
`nearbyint` (used by `boat_clock.h`), which Clang inlines, and the symbol allowlist lacked it. It
is now allowed with the other libm rounding functions; not yet rerun on Linux.

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

Completed task: add repeatable Linux CI/build commands. Next task: get the workflow green, then test physical input, help/focus/Esc release, renderer/audio and native
multiplayer on Linux. macOS/browser verification evidence belongs to [the HUD owner](BOAT_HUD.md).
