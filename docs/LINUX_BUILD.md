# Linux boat study checks

Status, 2026-10-24: `.github/workflows/linux-boat.yml` adds an Ubuntu 24.04 X11 build,
the routine CPU suite and an Xvfb mouse-capture startup/clean-exit smoke run. The workflow
runs on branch pushes or manual dispatch; it has not run for this uncommitted worktree.
No Linux execution or user acceptance is claimed.

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

Completed task: add repeatable Linux CI/build commands. Next task: run the workflow after the
branch is pushed, then test physical input, help/focus/Esc release, renderer/audio and native
multiplayer on Linux. macOS/browser verification evidence belongs to [the HUD owner](BOAT_HUD.md).
