# Marine boat instruments

Status, 2026-10-24: implemented in isolated branch `boat-marine-hud` at
`/private/tmp/proteus-boat-hud`, based on `38d76c6`. The user prefers the revised
shared-pixel, vertical-thrust, visual-only direction and requested bottom anchoring
and slight transparency. Those final adjustments are implemented; their visual review
and later multiplayer integration remain pending. The [boating plan](BOATING_PLAN.md)
owns gameplay direction.

Completed task: a bottom-centred marine case with pale markings, muted metal trim,
flush screws and amber/sea-green needles. All three readings are visual ranges, with
no numeric readouts or units:

- **Thrust:** a vertical telegraph. Forward is above neutral; reverse is below.
  The handle shows `Boat.throttle`, not delayed engine response. Thrust still holds when
  released. Spring-to-neutral is an undecided proposal; no input or physics behavior changed.
- **Speed:** a needle shows planar ground-speed magnitude, including lateral/reverse motion.
  Its internal display range remains 0..60 knots, saturating above that range, without
  displaying units or numbers. This is not signed forward speed or speed through water.
- **Steering:** the spring-centred wheel setting, with port/starboard endpoints and a needle.
  This shows `Boat.wheel`, not heading, turn rate or engine deflection.

The 166x44 panel has one layout at every window size and sits flush against the scene's
bottom edge (including its shallow shadow). The case is slightly translucent; markings
and needles remain opaque. A three-by-five bitmap font, rasterized circles and integer
line drawing keep every stroke on the scene's pixel grid.

The scene and HUD are composited into one 320x200 render texture **before** presentation,
then scaled together with nearest filtering and the same integer-scaled destination.
There is no independent HUD scale or window-resolution text. Blending onto the opaque
scene also avoids double-multiplying translucent colors. Letterboxing follows the scene.
Existing course/help/profiler/status overlays retain their previous screen-resolution
rendering; this change owns the three-instrument HUD. Wake/connection status moves above
the case so it does not cover the instruments. Help covers the HUD.

## Integration handoff

`src/boat_hud.[ch]` is presentation-only and linked in `proteus_render`. The draw function
allocates no resources and retains no state. It needs raylib and three telemetry floats.
No protocol, prediction, core physics or multiplayer files change.

`tools/boat_study.c` owns a 320x200 `frame_target`, including allocation validation,
nearest filtering and cleanup. Each frame, it draws the scene texture into this target
with the existing CPU/GPU orientation normalization, then calls `boat_hud_draw` with the
displayed local player's velocity magnitude, throttle and wheel. It presents the combined
texture once to the existing integer-scaled scene destination. Preserve these orientation
and lifetime details when resolving integration conflicts. Do not use remote-boat or raw
keyboard state. Scene-only CLI screenshots retain their existing behavior.

`CMakeLists.txt` adds the module to `proteus_render` and a native-only `boat_hud_review`
executable. README/index/plan edits describe the current presentation.

Next task: review bottom placement/transparency, then integrate with multiplayer.
Inspect a connected session after integration, especially own spawn/reset and new UI.
Preserve the main agent's uncommitted work; this branch does not include those changes.
The user has not decided whether held thrust should spring to neutral.

## Review commands

From the worktree root, build normally, then run the boating view:

```sh
./build/boat_study --location river --gpu-aa
./build/boat_study --location river --gpu-aa --demo --frames 180
```

`boat_study --screenshot` remains scene-only. The review tool composites deterministic
telemetry onto a background at 320x200, then nearest-scales the combined frame:

```sh
./build/boat_study --renderer cpu --location river --time 3 --screenshot build/hud-river-background.png
./build/boat_hud_review --background build/hud-river-background.png --case cruise --screenshot build/hud-cruise.png
./build/boat_hud_review --background build/hud-river-background.png --case neutral --compact --screenshot build/hud-neutral-compact.png
./build/boat_hud_review --background build/hud-river-background.png --case reverse --screenshot build/hud-reverse.png
./build/boat_hud_review --background build/hud-river-background.png --case full --large --screenshot build/hud-full-large.png
```

The review tool requires a graphics context. Default output is 960x600 (3x), compact
320x200 (1x), large 1280x800 (4x); all use the same layout and pixel grid. Omit
`--screenshot` and add `--frames 600` for a visible static review. Fixture telemetry
is illustrative, not sampled from the background scene.

## Verification and limitations

Historical first pass, 2026-10-24, macOS / Apple M1 Max: Debug build and 102 active
CTests (21 active GPU) passed; 15 retired wake-field cases stayed disabled. Four
captures were inspected and a scripted GPU river run completed. The user rejected
that pass's mixed pixel sizes and requested the visual-only revision above.

Current pass, 2026-10-24, macOS / Apple M1 Max: Debug build completed without project
warnings and all 102 active CTests passed (21 active GPU, 15 retired cases disabled),
including the final bottom/transparency revision. Full output:
`/private/tmp/proteus-hud-ctest-bottom.log`. Inspected neutral, cruise, reverse and
full-range captures at 1x/3x/4x. A pixel-by-pixel check using the existing PNG reader
verified exact uniform blocks throughout every capture, including the translucent case.
Scripted GPU+AA river (`--demo --frames 180`) and CPU basin (`--demo --frames 60`)
windowed runs completed successfully. Formatting, diff whitespace and local file links
passed. This is agent-run verification; no manual keyboard play is claimed.
Dependencies are read from the main checkout's pinned source caches; all output stays
in this worktree. Initial raylib compilation had its existing `stb_vorbis.c` warning.

Windows, Linux, browser compilation, manual keyboard play, interactive replay and connected
multiplayer HUD checks remain unverified. Pre-existing course/replay overlays can occupy
substantial screen space at the minimum window size; they are not redesigned here.
Artifacts are in ignored `build/` and `/private/tmp/proteus-hud-*.log`.
