# Waterfront GPU migration plan

Status: planned, not implemented. Written 2026-09-12.

## Decision and scope

Use the existing C17/raylib 6.0 application with custom shaders. Render the city into
a 320x240 or 320x200 GPU color/depth target and present it with nearest-neighbour,
integer scaling. Keep 320x180 working as an existing compatibility option and keep
the current 320x200 default until the user requests another default.

The first milestone is the existing city fly-over on the GPU with working deterministic
time selection, PNG capture, and comparable composition. Additional detail, lighting,
shadows, and water follow in separate steps. The intended appearance is an ambitious
late-90s demo: crisp silhouettes, restrained colors, small textures, expressive lights.
The exact balance of textured and flat-shaded surfaces remains an artistic choice;
start by preserving the present flat-shaded appearance.

This plan supersedes the CPU-only requirement in WATERFRONT_PLAN.md for the new GPU
path. It does not describe current runtime behavior. Keep the existing software path
as a reference during migration. Avoid promising permanent feature parity between the
two renderers. Other scenes can be ignored during city development; do not delete them.

No engine replacement, Metal migration, ray tracing, compute shaders, deferred renderer,
general scene graph, new map downloads, music regeneration, or camera-route redesign is
part of this work. Do not implement optional effects while completing the baseline.

## Recommended agent and execution method

Use **GPT-5.6 Terra, medium reasoning**, as the default implementation agent. The steps
below are intentionally small enough to complete in separate sessions. Use **GPT-5.6
Luna, medium reasoning**, for baseline collection and straightforward documentation.
Use **Terra high** for projection, integration, shadows, and reflection work. If a
reduced rendering failure remains unexplained after two focused attempts, ask **GPT-5.6
Sol high** or **GPT-6 Astra high** to diagnose that failure rather than rewrite the renderer.

These are project recommendations, not measured performance guarantees. Official
[OpenAI model guidance](https://learn.chatgpt.com/docs/models) describes Terra as an
everyday all-rounder, Luna as suitable for clear repeatable tasks, and Sol/Astra as
options for more difficult work. Availability depends on the client and account.
Model names and guidance were checked on 2026-09-12; use the available picker equivalents
if they change. No automatic delegation or model configuration is required by this plan.

Run one numbered step at a time unless explicitly asked for a range. Read its prerequisites,
finish its checks, update the progress table, and stop at its boundary. Within an assigned
step, routine implementation choices do not require repeated permission. Keep useful work
if a check cannot run, but record the limitation and do not call that check passed.

Copy this prompt into a fresh implementation session, replacing NN:

```text
Read AGENTS.md and docs/GPU_WATERFRONT_PLAN.md. Implement step NN only.
Check the progress log and prerequisite evidence first. Preserve existing user changes.
Follow the stated scope, renderer contracts, and acceptance checks. Make routine choices
autonomously. Do not start later steps or optional effects. Build and run CTest, and do
the visual/manual checks required for this step. Update the progress log with changes,
commands, artifact paths, results, and remaining limitations. Report the outcome and stop.
```

## Starting code and constraints

| Location | Current responsibility and migration implication |
| --- | --- |
| `src/main.c` | CPU pixels uploaded every live frame; separate screenshot/profile loops. GPU paths must share rendering and resource lifetime. |
| `src/scenes.c` | Timeline dispatch; `cube` delegates to `waterfront_draw`. Keep scene identity and timing. |
| `src/waterfront.c` | Camera, static geometry submission, windows, traffic, lamps, boat, crane. Some temporary meshes/materials are stack-backed. |
| `src/renderer3d.[ch]` | Software rasterizer, camera and mesh types. Keep reference renderer functional. |
| `assets/geometry/eriksberg/geometry.h` | Bundled vertices and per-triangle colors. Do not regenerate geographic inputs for the port. |
| `tools/waterfront_storyboard.c` | Short/long camera routes and CPU validation. Preserve existing checks. |
| `src/profiler.[ch]` | CPU render/upload statistics and checksums; these labels will need backend awareness. |
| `CMakeLists.txt` | raylib pin, executables, exact CPU checksums, screenshot tests. GPU checks need separate requirements. |

Keep the `cube` command-line name. Preserve the 15.36-second short route, 30.72-second
storyboard route, 125 BPM speed-6 music relationship, and scene-local versus timeline
`--time` semantics. Animation must be a function of explicit time and stable seeds;
never depend on previous draw calls, accumulated frame deltas, or wall time in shaders.

## Rendering contract

Use desktop OpenGL 3.3-compatible GLSL (`#version 330`) with the pinned raylib backend;
verify the actual context/shader support in step 02. Mac support is the first target.
Do not silently raise the graphics API requirement. raylib's desktop OpenGL route has
a long-term platform risk because [Apple deprecated OpenGL](https://developer.apple.com/documentation/metal/migrating-opengl-code-to-metal).
This project deliberately accepts that tradeoff for the experiment.

Suggested small additions, introduced only when needed:

- `src/waterfront_scene.[ch]`: renderer-independent static scene data and evaluated objects.
- `src/renderer_gpu.[ch]`: GPU resources, mesh conversion, passes, resize, and readback.
- `assets/shaders/`: explicit vertex/fragment shader files copied by the existing asset rule.
- A small GPU validation executable or equivalent diagnostic mode for focused fixtures.

Names can change to fit the code. Do not build a generic backend plugin system. The scene
owns authored data; the GPU renderer owns GPU resources; the application owns the context.
Destroy GPU resources before closing the context. On resize, validate replacement targets
before releasing working ones. Check shader compilation/linking and framebuffer validity;
report a clear error instead of accepting raylib's fallback shader as success.

Keep CPU math/types where useful. Do not memcpy `Mesh3D` into raylib `Mesh`: the current
mesh lacks a vertex-count field and stores color per triangle. Conversion must know vertex
bounds, duplicate vertices at face/color boundaries as necessary, and provide flat normals.
Respect raylib mesh index limits; split large meshes or use valid non-indexed buffers.
Keep CPU and GPU ownership separate to avoid unloading borrowed generated arrays.

Use Y-up coordinates and existing transforms. Convert vertical FOV radians to the units
raylib expects; match near/far planes explicitly rather than assuming defaults. Verify
view handedness, matrix order, triangle winding, and inverse-transpose normal transforms
for nonuniform scale. Never fix a flipped scene by scattering sign changes through assets.

Static city/window geometry is built and uploaded once. Moving objects reuse meshes and
change transforms. Per-frame uniforms/objects are bounded; no per-triangle draw-call loop
for the whole city and no GPU upload of all static geometry every frame. A tiny typed list
of meshes, transforms, material IDs, and lights is sufficient. Temporary stack pointers
must never escape scene-building calls. Camera-based CPU window rejection must not remove
geometry needed by later shadow or reflection passes.

Baseline pass order:

```text
explicit scene time -> camera and moving objects on CPU
                    -> sky + opaque city into virtual-resolution GPU target
                    -> nearest-neighbour integer-scaled window presentation
                    -> PNG readback on request, from that same virtual target
```

Later: shadow pass before the city; reflected view before water; optional virtual-resolution
postprocess after composition. Use separate source/destination textures for postprocessing.
Never sample a texture while rendering into that same attachment. No routine GPU-to-CPU
readback during interactive playback. Render-texture vertical orientation must be corrected
exactly once for each presentation/export path and checked using asymmetric geometry.

## Steps

### 01 — Record the software baseline

Agent: Luna medium. Prerequisites: none. Changes: reference notes/artifacts only.

1. Read the existing renderer and waterfront documentation, inspect git status, configure,
   build, and run CTest. Record failures before making changes.
2. Capture scene-local times 0, 2.5, 5, 10, and 15 seconds at 320x200 and 320x240.
   Capture one 320x180 frame. Store artifacts under `build/gpu-reference/` (not in git).
3. Inspect the images and play the short route. Record recognizable comparisons: road
   alignment, windows, car/boat positions, crane silhouette, sky bands, and light patches.
4. Record commit ID plus dirty-tree status, build type, commands, and artifact paths here.

Acceptance: baseline CTest results and inspected frames are recorded; no runtime behavior
changes. If the baseline has failures, identify them rather than regenerating expected hashes.

### 02 — Prove the low-resolution GPU target and capture

Agent: Terra medium. Prerequisite: 01. Scope: isolated diagnostic, not city integration.

1. Add a small GPU fixture using raylib, one custom shader, color/depth render target,
   two intersecting colored objects, and an asymmetric marker.
2. Render at both main resolutions. Present with point sampling and integer scaling;
   disable multisample antialiasing for the virtual target. Avoid high-resolution rendering
   followed by downsampling. Keep colors simple; defer gamma/palette changes.
3. Support fixture PNG export through a hidden context with no audio initialization.
   Use `LoadImageFromTexture` and export the resulting image. Free readback memory.
4. Verify actual OpenGL version, shader logs, depth occlusion, PNG dimensions, orientation,
   hidden-window behavior, and context teardown. Also test target resize repeatedly.

Acceptance: visible and captured fixture agree; intersecting geometry is depth-correct;
shader failure produces a useful nonzero failure. Hidden capture is demonstrated on the
Mac, or its concrete platform failure is reported before city work proceeds. Do not call
a hidden window display-independent/headless rendering.

### 03 — Extract shared scene evaluation

Agent: Terra high. Prerequisite: 02. Scope: CPU-preserving refactor.

1. Separate renderer-independent camera/object evaluation and static scene construction
   from software drawing. Keep existing camera math and animation formulas intact.
2. Represent static geometry with explicit counts, colors/material IDs, and stable ownership.
   Reuse car/boat meshes with evaluated transforms; preserve deterministic window occupancy.
3. Adapt the CPU renderer to consume the shared data in its original submission order.
   Preserve ties between exactly coplanar surfaces and the original light patches.
4. Retain proxy-clearance validation and storyboard long-route behavior.

Acceptance: all original CPU checksum and seeking tests still pass, and reference captures
match. Do not update hashes merely to accommodate this refactor. If exact parity requires
a small CPU adapter, prefer that over a large shared rendering abstraction.

### 04 — Port the static city and camera

Agent: Terra high. Prerequisite: 03. Scope: GPU scene in the diagnostic harness.

1. Convert/upload static city geometry, bridge, crane, windows, and sky once. Keep the
   authored colors and flat directional/ambient shading initially.
2. Use the shared camera and explicit clip planes. Reproduce the existing sky bands in
   virtual pixel coordinates. Keep emissive windows unlit rather than treating them as lights.
3. Batch static geometry sensibly. Retain distinct material behavior; avoid introducing
   texture atlases, instancing, or a material editor before they are needed.
4. Capture the baseline times and compare geometry placement, framing, depth, and colors.

Acceptance: static city composition matches at both main resolutions; near-plane crossings
and nonuniformly scaled fixtures work; all CPU tests remain unchanged. Differences caused
by triangle fill rules are acceptable when explained. Missing surfaces or reversed lighting
are bugs. No shadows, reflections, new facade textures, or postprocessing yet.

### 05 — Port city life and integrate the application

Agent: Terra high. Prerequisite: 04. Scope: complete baseline GPU fly-over.

1. Draw traffic, boat/wake, lamps, and stepped light patches from shared scene time. Use
   reusable meshes; preserve opaque patch depth behavior for this baseline.
2. Add explicit `--renderer cpu|gpu` selection, defaulting to CPU during migration. `gpu`
   selects the city backend; other scenes continue through their CPU texture-upload path.
3. Route live drawing and screenshots through the same GPU frame function. GPU screenshots
   initialize a hidden context without audio; CPU screenshots remain context-free.
4. Support GPU profiling invocation with honest CPU-submission labels initially. Preserve
   CLI validation and requested-time semantics across live, screenshot, and profile modes.
5. Reuse the application's integer destination calculation. Handle GPU target resize and
   correct source-rectangle orientation. Keep audio playback/seek behavior intact.

Acceptance: all city objects are present and deterministic; captures at 0/2.5/5/10/15 seconds
agree with live views; direct seek agrees with sequential evaluation at the same time.
Run the full route, resize and resolution keys, fullscreen, quit, and audio checks. No
per-frame readback or static mesh rebuild. Old scenes need only remain functional.

This is the first complete GPU milestone. Review images and motion before adding effects.
The user can assign step 06 next; this checkpoint is not permission to run the entire plan.

### 06 — Establish GPU regression checks and profiling

Agent: Terra medium. Prerequisite: 05.

1. Keep CPU hashes exact and explicitly run those tests with the CPU backend. Add GPU
   fixture tests for depth, winding, normals, orientation, and dimensions, plus city captures.
2. Label context-dependent tests `gpu`. Make them explicitly opt-in through a documented
   CMake option such as `PROTEUS_GPU_TESTS`; ordinary CPU CTest remains usable without a
   display. When GPU tests are enabled, context/shader failures must fail, not silently skip.
3. Use semantic pixel regions/tolerances for diagnostic fixtures. Compare city references
   visually or with documented image tolerances; do not require cross-driver exact hashes.
   A GPU checksum may be logged as diagnostic data, never compared with the CPU checksum.
4. Label submission, present/wait, and readback costs honestly. Remove claims of a full
   framebuffer upload on GPU city frames. Record draw calls and triangle counts if cheap.
5. Profile Release at the fixed route/resolution. Target steady 60 fps on the development
   Mac, with hardware/context/build details recorded. Vsync-limited wall time is not GPU time.
   Use delayed GPU timer queries only if supported and needed; report unavailable timings
   instead of substituting CPU time. Use Instruments for unresolved CPU costs.

Acceptance: ordinary CTest and enabled GPU CTest pass in suitable environments; the report
separates submission from execution and screenshots; direct seeks require no warmup frames.

### 07 — Add restrained surface detail

Agent: Terra medium. Prerequisite: 06.

1. Add a small deterministic set of facade/roof/quay materials (e.g. 32x32 or 64x64 textures).
   Keep existing footprints and skyline. Bundle assets; no runtime network dependencies.
2. Use perspective-correct UVs. World-aligned mapping is acceptable for static architecture;
   moving/rotating objects use model-space coordinates or authored UVs.
3. Preserve nearest sampling. Test nearest mip-level sampling or reduced texture frequency
   if distant details shimmer; mipmap selection must not blur final presentation pixels.
4. Add a few silhouette details visible on the route before increasing indiscriminate density.
   Keep flat-color materials available for comparison and background geometry.

Acceptance: detail reads at both resolutions and improves the moving shot. Inspect texture
scale, seams, distant shimmer, and roof normals. Record artistic defaults; do not claim the
textured style was already approved in the original discussion.

### 08 — Add local lighting

Agent: Terra medium/high. Prerequisite: 07.

1. Add a bounded forward-light array: start with up to eight selected point/spot lights,
   plus ambient and directional fill. Stable selection/hysteresis should avoid obvious
   light popping; do not loop over every street lamp for every fragment.
2. Give lights position, color, radius, intensity, and spot cone where appropriate. Keep
   emissive appearance independent of light emission. Accumulate illumination then apply
   the material's chosen stepped ramp; preserve hue and avoid immediate white clipping.
3. Replace the old opaque light patches on the GPU path where real illumination now covers
   the same role. Keep the reference CPU path's patches. Avoid double-counting brightness.
4. Keep most lights unshadowed. Unshadowed local lights can leak through walls; use authored
   radii/placement and acknowledge that limitation rather than promising physical occlusion.

Acceptance: headlights move correctly and illuminate facing surfaces; lamps affect nearby
ground/walls; distant light selection is stable through the route. Capture lights on/off at
identical times. Keep palette stepping and dithering conservative and independently tunable.

### 09 — Add one directional shadow map

Agent: Terra high; Sol high for persistent matrix/depth bugs. Prerequisite: 08.

1. Follow the pinned raylib shadow-map example as a reference. Start with one 1024x1024
   depth map and an orthographic light view covering a useful local portion of the route.
   Check framebuffer setup; raylib's ordinary render-target depth attachment should not
   be assumed to be a sampleable depth texture.
2. Submit static and moving shadow casters, excluding decorative light patches. Stabilize
   light projection by snapping its movement to shadow texels. Include off-camera casters
   that can shadow visible receivers. Handle outside-map samples as unshadowed.
3. Use conservative normal/slope-aware bias; begin with hard shadows, optionally a small
   comparison filter if needed. Shadows attenuate direct light, not emissive or ambient fill.
4. Provide diagnostic depth-map/light-frustum views. Compare 512 and 1024 maps before raising
   resolution. No cascades or shadows for every light in this step.

Acceptance: buildings and vehicles cast plausible shadows; no pervasive acne, detached
shadows, or obvious crawling during the complete camera motion. Document remaining coverage
limits, bias values, draw cost, and the selected quality settings.

### 10 — Add distance atmosphere

Agent: Terra medium. Prerequisite: 09.

1. Add depth/distance fog in the city shader, using a cool dusk color and explicit distances.
   Maintain foreground contrast and separation of the background crane and buildings.
2. Tune any color quantization after lighting/fog deliberately. Optional ordered dithering
   must be anchored to virtual pixels and deterministic; do not add temporal noise/history.
3. Keep glow off by default. If tested, use a small emissive-only virtual-resolution pass
   with separate source/destination targets and a strict intensity limit.

Acceptance: compare fog on/off captures and moving views; foreground stays readable and the
sky horizon is coherent. If glow does not improve the shot, leave it out and record that.

### 11 — Add a planar water reflection

Agent: Terra high. Prerequisite: 10. Optional extension, separately assignable.

1. Identify the actual basin water plane and its triangles/material. Split water from opaque
   land geometry so it is drawn once in a dedicated pass; do not overlay a coplanar duplicate.
2. Reflect the camera across that plane; render the city excluding water into a 160x120
   (or 160x100) reflection target. Clip geometry below the water plane and account for
   reflected winding/culling. Keep reflection visibility separate from main-camera culling.
3. Sample the reflection in the water material with bounded time-derived distortion, dark
   tint, and simple view-angle weighting. Keep the boat/wake depth ordering correct.
4. Evaluate whether the reflection adds enough to justify another scene draw. Do not add
   screen-space reflections, refraction simulation, or reflection recursion.

Acceptance: shoreline stays attached, below-water geometry does not leak into reflection,
motion is stable, screenshots seek directly, and the frame budget is recorded. Keep a
reflection-off comparison; fall back to stylized water if the effect harms readability.

### 12 — Final review and make the GPU city the default

Agent: Terra medium; Luna for documentation after behavior is verified. Prerequisite: 10
and any separately assigned optional step. Complete the artistic review before this step.

1. Compare the final route with step 01 at both main resolutions. Check composition,
   readability, motion, lighting, screenshots, runtime resources, and measured performance.
2. Make GPU the default city renderer while retaining `--renderer cpu` for the reference.
   Keep the `cube` name, other CPU scenes, and original timeline. Update tests to select
   CPU explicitly where context-free operation or CPU hashes are required.
3. Document capture context requirements, backend selection, shader assets, GPU-test opt-in,
   profiling interpretation, quality defaults, and known limits in README.md and docs/3D.md.
   Update AGENTS.md's screenshot guidance to distinguish CPU and GPU capture accurately.
4. Remove temporary diagnostic clutter only after retaining useful fixtures. Keep reference
   artifact paths and a concise completion record; do not commit build outputs.

Acceptance: full build/CTest plus enabled GPU tests pass; manual window/audio checks pass;
screenshots work through the documented CLI; no unjustified current-behavior claims remain.
Unverified platforms and any deferred effects are listed explicitly.

## Validation commands and handoff evidence

Existing commands (step 01 and every implementation step):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
mkdir -p build/gpu-reference
./build/proteus --screenshot build/gpu-reference/cpu-240-t5.png --scene cube --time 5 --resolution 320x240
./build/proteus --scene cube --time 0 --resolution 320x240
```

The last command is a manual interactive check, not a finite automated test. Baseline captures
must be collected before changing defaults, and explicitly use `--renderer cpu` after step 05.

Planned commands, usable only after their implementing steps:

```sh
./build/proteus --renderer gpu --screenshot build/gpu-t5.png --scene cube --time 5 --resolution 320x240
./build/proteus --renderer gpu --scene cube --time 0 --resolution 320x240
./build/proteus --renderer gpu --profile cube --frames 923 --warmup 60
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DPROTEUS_GPU_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

GPU frame profiling uses deterministic frame times as the existing profile mode does; the
warmup must not change the time selected for any measured frame. Context-dependent commands
may need the environment's normal permission flow. Never bypass a rejected action.

For each completed step, record: changed files; build and CTest outcome; context/hardware
for GPU checks; inspected image paths and times; manual checks; performance where relevant;
known deviations and the exact next step. Use “implemented, verification pending” if a
required check cannot run. Do not mark visual acceptance based only on compilation.

## Progress log

All implementation steps are pending. Documentation creation does not complete step 01.

| Step | Status | Evidence / deviations |
| --- | --- | --- |
| 01 CPU baseline | Pending | |
| 02 GPU target/capture fixture | Pending | |
| 03 Shared scene evaluation | Pending | |
| 04 Static GPU city | Pending | |
| 05 City life/application integration | Pending | |
| 06 GPU regression/profiling | Pending | |
| 07 Surface detail | Pending | |
| 08 Local lighting | Pending | |
| 09 Directional shadows | Pending | |
| 10 Atmosphere | Pending | |
| 11 Water reflection | Optional, pending | |
| 12 Default/documentation | Pending | |

## Technical references

- [raylib 6.0 API cheatsheet](https://www.raylib.com/cheatsheet/raylib_cheatsheet_v6.0.pdf):
  render targets, shader APIs, texture filtering, hidden windows, and GPU image readback.
- [raylib shadow-map example](https://www.raylib.com/examples/shaders/loader.html?name=shaders_shadowmap_rendering):
  directional shadow rendering reference. Prefer its matching source in the pinned checkout
  under `build/_deps/raylib-src/examples/shaders/` when implementing.
- [Current renderer notes](3D.md) and [previous waterfront plan](WATERFRONT_PLAN.md):
  camera, generated geometry, CPU materials, and existing validation.
