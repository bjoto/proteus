# Marine boat instruments

Status, 2026-10-24: implemented in the isolated `boat-marine-hud` branch at
`/private/tmp/proteus-boat-hud`, based on `38d76c6`. Ready for visual review and
later integration with the multiplayer work. No user acceptance is claimed.
The [boating plan](BOATING_PLAN.md) owns the gameplay direction.

Completed task: a small bottom-centred marine instrument case with a dark enamel face,
pale markings, muted metal lip, flush screws and amber/sea-green needles. Three readings:

- **Thrust:** held command, signed percent, reverse/forward bar and fixed neutral mark.
  This is `Boat.throttle`, not delayed engine response or estimated propulsive force.
- **Speed:** planar ground-speed magnitude in knots (`hypot(velocity) * 1.94384449`).
  Reverse and lateral motion still show positive speed. The dial stops at 60 kn;
  the number continues above 60. This is neither signed forward speed nor speed through water.
- **Steering:** spring-centred `Boat.wheel`, with port/starboard percent and a needle.
  This shows the control setting rather than heading, turn rate or engine deflection.

The 240x76 panel stays small at 960x600; it draws at 2x only when both width >=1000
and height >=720. Windows shorter than 360 pixels use a 240x40 numeric strip to keep
the boat visible. All placement/font sizes use integer scales. The bottom 28 pixels
remain available to existing connection/wake status. Help covers the HUD; replay and
network play read the same displayed `view->boat` as the scene.

## Integration handoff

`src/boat_hud.[ch]` is a presentation-only module, linked in `proteus_render`.
It allocates no resources and retains no state. It only needs raylib and three floats;
no protocol, prediction, core physics or multiplayer files change.

The integration in `tools/boat_study.c` adds the header and calls `boat_hud_draw`
after scene presentation and before course/status/help overlays. If the main agent has
changed that section, preserve its view selection and pass the displayed local player's
velocity magnitude, throttle and wheel. Do not use remote-boat or raw keyboard state.
`CMakeLists.txt` adds the module to `proteus_render` and a native-only `boat_hud_review`
executable. README/index/plan edits describe the new presentation.

Next task: user review of instrument readability and placement, then integrate this branch
with the multiplayer work. Inspect a connected session after integration, especially own
spawn/reset and any new bottom-of-window UI. Integration must preserve the main agent's
uncommitted work. This branch does not include those changes.

## Review commands

From the worktree root, build normally, then run the actual boating view:

```sh
./build/boat_study --location river --gpu-aa
./build/boat_study --location river --gpu-aa --demo --frames 180
```

Existing `boat_study --screenshot` captures remain scene-only and do not include window
HUD overlays. The dedicated review tool composites deterministic telemetry onto a scene
capture using a render texture (a hidden macOS window can yield a black screen capture):

```sh
./build/boat_study --renderer cpu --location river --time 3 --screenshot build/hud-river-background.png
./build/boat_hud_review --background build/hud-river-background.png --case cruise --screenshot build/hud-cruise.png
./build/boat_hud_review --background build/hud-river-background.png --case neutral --compact --screenshot build/hud-neutral-compact.png
./build/boat_hud_review --background build/hud-river-background.png --case reverse --screenshot build/hud-reverse.png
./build/boat_hud_review --background build/hud-river-background.png --case full --large --screenshot build/hud-full-large.png
```

The review tool requires a graphics context, even with a CPU background. Cases are neutral,
cruise, reverse/port lock and full thrust/starboard lock (including off-scale dial speed).
Default size is 960x600, compact is 320x200, large is 1280x800. Omit `--screenshot`
and add `--frames 600` for a visible static review. These fixtures use illustrative telemetry,
not telemetry sampled from the background scene.

## Verification and limitations

2026-10-24, macOS / Apple M1 Max: Debug configure/build completed with
`PROTEUS_GPU_TESTS=ON`; project code compiled without warnings. Initial raylib build
reported its existing `stb_vorbis.c` pointer-comparison warning. Dependencies were read
from the main checkout's existing pinned source caches; all build output stayed here.
The full CTest suite passed 102 active tests including 21 active GPU cases; 15 retired wake-field
cases stayed disabled. The final full run after the compact-layout refinement also passed all 102 active
cases (21 active GPU); see `/private/tmp/proteus-hud-ctest-final.log`.

Inspected all four fixture captures at the sizes above, including the refined compact
strip. A visible scripted GPU+AA river run (`--demo --frames 180`) completed successfully.
This is agent-run windowed verification, not manual keyboard play or user acceptance.
No new input, motion or audio behavior was introduced. Windows, Linux, browser compilation,
interactive replay and connected multiplayer HUD checks remain unverified. At the minimum
window size, pre-existing course/replay overlays may still occupy substantial screen space;
the HUD does not redesign those overlays. Captures and smoke logs remain in the worktree's
ignored `build/` and `/private/tmp/proteus-hud-*.log`.
