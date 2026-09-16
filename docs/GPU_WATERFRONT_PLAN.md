# Waterfront GPU migration plan

Status: steps 01–11 implemented; GPU visual verification remains pending. Written 2026-09-12.

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

## Migration invariants

Every step must preserve these boundaries. They are decisions, rather than suggestions, so
an implementation can be reviewed without inferring them from the later milestones.

| Concern | Required boundary |
| --- | --- |
| Time | A caller supplies scene-local time. Scene evaluation derives every animated value from it; GPU code receives evaluated transforms and uniforms only. |
| Context | The application or fixture creates and destroys the raylib window/context. GPU renderer code may create and release resources only while that context is alive. |
| Rendering | The CPU backend writes `Color` pixels. The GPU city backend writes one virtual-resolution render texture. Neither backend reads the other's output during a normal frame. |
| Presentation and export | Both display and PNG export use the completed virtual image for their selected backend. Export is the only normal GPU-to-CPU readback. |
| Ownership | Shared scene data owns CPU arrays; GPU resources own their converted/uploaded copies. A GPU resource must never retain a pointer to temporary or borrowed scene storage. |
| Compatibility | `cube`, its timeline mapping, all supported virtual resolutions, and the CPU path's deterministic tests continue to work throughout the migration. |

Do not introduce a runtime fallback from `--renderer gpu` to CPU after a context, shader, or
framebuffer error. Report the failed GPU request and return nonzero. A fallback would make
diagnostics and GPU test results ambiguous. The normal default may remain CPU until step 12.

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
step, routine implementation choices do not require repeated permission. Once its required
checks have passed, create one focused commit for that step before handing it off. Stage only
the step's intended source, asset, test, and documentation changes; never commit `build/`,
caches, generated reference captures, or unrelated user work. If a required check cannot run,
keep useful work, record the limitation, and leave the step uncommitted until the limitation is
resolved or explicitly accepted.

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

The first GPU fixture should be a deliberately narrow executable or diagnostic mode with a
documented interface: accept `--screenshot FILE`, `--resolution WIDTHxHEIGHT`, and a
noninteractive validation switch such as `--check`. `--check` must return nonzero for a
context failure, shader compile/link failure, incomplete framebuffer, failed readback/export,
wrong image dimensions, or failed depth/orientation probe. It must print the OpenGL version
and renderer strings, selected target size, and enough shader log text to identify the failing
stage. This fixture is a prerequisite proof, not an alternate city renderer or a new public
application mode.

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
   Record image dimensions and whether each capture was visually inspected. Do not commit
   these generated PNGs.

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
   Use `LoadImageFromTexture` and export the resulting image. Free readback memory. The
   fixture owns image-orientation normalization, so city integration does not have to guess
   whether a readback needs flipping.
4. Verify actual OpenGL version, shader logs, depth occlusion, PNG dimensions, orientation,
   hidden-window behavior, and context teardown. Also test target resize repeatedly. The
   asymmetric marker must make all four rotations/reflections distinguishable; a symmetric
   triangle cannot prove this check.

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

Implementation defaults: the GPU route uses bundled 32x32 facade, roof, and quay textures as
subtle albedo modulation over the imported colors. They repeat at 0.04 texture units per world
unit, use point filtering and repeat wrapping, and use world-aligned UVs only on static city
architecture. The existing flat material remains in use for unclassified/background geometry and
is retained as the comparison baseline. `tools/generate_gpu_textures.py` regenerates these
standard-library PNG assets deterministically. This is an exploratory textured style, not an
approved art direction.

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
./build/proteus --screenshot build/gpu-reference/cpu-200-t0.png --scene cube --time 0 --resolution 320x200
./build/proteus --screenshot build/gpu-reference/cpu-200-t2.5.png --scene cube --time 2.5 --resolution 320x200
./build/proteus --screenshot build/gpu-reference/cpu-200-t5.png --scene cube --time 5 --resolution 320x200
./build/proteus --screenshot build/gpu-reference/cpu-200-t10.png --scene cube --time 10 --resolution 320x200
./build/proteus --screenshot build/gpu-reference/cpu-200-t15.png --scene cube --time 15 --resolution 320x200
./build/proteus --screenshot build/gpu-reference/cpu-240-t5.png --scene cube --time 5 --resolution 320x240
./build/proteus --screenshot build/gpu-reference/cpu-180-t5.png --scene cube --time 5 --resolution 320x180
./build/proteus --scene cube --time 0 --resolution 320x240
```

The last command is a manual interactive check, not a finite automated test. Baseline captures
must be collected before changing defaults, and explicitly use `--renderer cpu` after step 05.
The seven captures above are the minimum baseline set: all five route checkpoints at the default
resolution, plus a representative image at each compatibility resolution. Add rather than
replace captures when a later step needs a more revealing comparison.

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

Documentation creation alone does not complete step 01. Step 01 has a recorded CPU baseline;
its remaining manual motion review is called out below rather than being treated as passed.

| Step | Status | Evidence / deviations |
| --- | --- | --- |
| 01 CPU baseline | Implemented, verification pending | Build and 12/12 CTest passed; seven captures inspected. Full interactive route-motion review remains pending. See record below. |
| 02 GPU target/capture fixture | Complete | Custom-shader render-target fixture, readback, orientation/depth checks, resizing, and visible comparison passed on the development Mac. See record below. |
| 03 Shared scene evaluation | Complete | Scene-owned static geometry and explicit scene-time evaluation now feed the unchanged CPU submission order. See record below. |
| 04 Static GPU city | Implemented, verification pending | Static city diagnostic path added; see record below. |
| 05 City life/application integration | Implemented, verification pending | GPU city life and application backend routing are in place; desktop GPU checks remain pending. See record below. |
| 06 GPU regression/profiling | Implemented, verification pending | Opt-in `PROTEUS_GPU_TESTS` adds a context-required capture/review CTest and target. Desktop context verification remains pending. |
| 07 Surface detail | Implemented, verification pending | Bundled point-filtered facade, roof, and quay detail textures are on the GPU route; desktop visual review remains pending. |
| 08 Local lighting | Implemented, verification pending | Eight bounded, hysteresis-selected lamp/headlight lights now shade the GPU route; desktop context/capture review remains pending. |
| 09 Directional shadows | Implemented, verification pending | One texel-snapped directional depth map, caster pass, hard direct-light attenuation, and fixture diagnostics are in place; desktop review remains pending. See record below. |
| 10 Atmosphere | Implemented, verification pending | Distance haze is applied after direct, local, and emissive lighting, with identical-time on/off switches; desktop visual review remains pending. See record below. |
| 11 Water reflection | Implemented, verification pending | The semantic water mesh samples a separate 160x120 reflected scene with deterministic distortion; desktop review remains pending. See record below. |
| 12 Default/documentation | Pending | |

### Step 01 record — 2026-09-12

Baseline commit: `9055b5d3a790e782ef0074a4bdaa5c333d88d659` (`Add plan for GPU rendering the
waterfront`). The tree was already dirty before the baseline: `docs/3D.md` and
`docs/WATERFRONT_PLAN.md` contained existing documentation edits. This plan was also edited to
record the baseline; no source, asset, or runtime behavior changed.

Configured Debug with `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`, built with
`cmake --build build -j`, and ran `ctest --test-dir build --output-on-failure`. All 12 tests
passed in 0.74 seconds: storyboard and renderer validation, XM/audio checks, screenshot/CLI
checks, and all four deterministic scene checksums.

Inspected captures, all under `build/gpu-reference/` and intentionally untracked:

- `cpu-200-t0.png`, `cpu-200-t2.5.png`, `cpu-200-t5.png`, `cpu-200-t10.png`, and
  `cpu-200-t15.png` are 320x200.
- `cpu-240-t5.png` is 320x240; `cpu-180-t5.png` is 320x180.

The early route keeps the road, cars, lit windows, and lamp patches legible. The crane enters
the 2.5-second frame, crosses the center around 5–10 seconds, and frames the basin at 15
seconds; the water, quay, and distant window grid remain recognizable. The 180- and 240-pixel
captures retain the 5-second framing. A visible launch at scene time zero created the expected
point-filtered 320x240 texture, initialized Core Audio and `assets/music/starter.xm`, and
reported Apple M1 Max OpenGL 4.1 / GLSL 4.10. The command runner did not retain the window for
the complete 15.36-second route, but the demo continued running until the user closed it. Full
motion, resize, and audible playback review have not been accepted. Perform that review before
marking step 01 complete.

### Step 02 record — 2026-09-12

Changed `CMakeLists.txt`, `tools/gpu_fixture.c`, and `assets/shaders/gpu_fixture.fs`. The new
`gpu_fixture` executable creates a hidden raylib context by default, has no audio initialization,
and supports `--check`, `--resolution`, `--screenshot`, `--visible`, `--frames`, and
`--invalid-shader`. It renders intersecting red/blue cubes to a point-filtered color/depth target,
uses a custom GLSL 330 fragment shader, replaces the target at a half-size intermediate before
the requested size, verifies four asymmetric orientation markers plus the blue depth result, and
normalizes the capture exactly once. The target is presented with a negative-height source
rectangle. A custom-shader load is rejected if raylib supplies its default program instead;
`--invalid-shader` deliberately returns nonzero and preserves the compiler diagnostic.

Configured Debug, built with `cmake --build build -j`, and ran
`ctest --test-dir build --output-on-failure`: all 12 existing tests passed in 0.74 seconds.
Hidden `--check --screenshot` runs passed at 320x180, 320x200, and 320x240. The exported,
untracked artifacts are `build/gpu-fixture-180.png`, `build/gpu-fixture-200.png`, and
`build/gpu-fixture-240.png`; their dimensions match their selected targets. Inspected all three:
the red/green/blue/yellow corner markers establish image orientation, and the blue foreground
cube correctly occludes the intersecting red cube. A 60-frame visible 320x200 run exported
`build/gpu-fixture-visible.png`, which is byte-identical to the hidden 320x200 capture.

Checks ran on Apple M1 Max, OpenGL `4.1 Metal - 90.5`, GLSL `4.10`. The hidden-context fixture
created and destroyed cleanly on this Mac. `./build/gpu_fixture --invalid-shader` returned exit
code 1 and printed the expected GLSL compiler syntax error. No GPU CTest is added yet: step 06
will make context-dependent checks opt-in.

### Step 03 record — 2026-09-12

Changed `src/waterfront_scene.[ch]`, `src/waterfront.[ch]`, and `CMakeLists.txt`.
`waterfront_scene` owns the generated-city mesh descriptor and authored street routes, and evaluates
the camera, bridge transform/color, and boat position exclusively from scene-local time. The CPU
adapter retains its original draw/submission order, including its deterministic window occupancy,
coplanar light patches, crane, traffic, and wake behavior. The storyboard target now links the
same shared scene module, so proxy-clearance validation and both routes continue to use the
scene-owned city geometry.

Built with `cmake --build build -j` and ran `ctest --test-dir build --output-on-failure`: all 12
tests passed in 1.47 seconds, including unchanged exact CPU scene checksums and storyboard seeking.
Captured and inspected `build/step03-cube-t5.png` (320x200) and
`build/step03-cube-t5-240.png` (320x240). Each is byte-identical to its corresponding step-01
reference capture (`cpu-200-t5.png` and `cpu-240-t5.png`); the inspected frame retains the road,
traffic, lit windows, crane silhouette, and basin framing. No interactive behavior changed or was
re-reviewed. The next implementation step is 04, static GPU city port in the diagnostic harness.

### Step 04 record — 2026-09-12

Changed `CMakeLists.txt`, `tools/gpu_fixture.c`, `src/renderer_gpu.[ch]`, and added
`assets/shaders/waterfront_city.fs`. The existing `gpu_fixture` diagnostic now accepts `--city`
and `--time SECONDS`. This path retains the fixture's virtual-resolution target, capture, and
point-filtered presentation, but builds/uploads the city, deterministic emissive window quads,
crane, and bridge mesh only once. It evaluates the shared scene camera and bridge transform for
each requested scene-local time. The city uses the same seven sky bands in virtual-pixel space,
the existing explicit clip values through the shared camera, authored facade colors with baked
flat ambient/directional shading, and unlit window colors. Traffic, boat/wake, lamps, light
patches, shadows, reflections, textures, and postprocessing remain intentionally absent.

`cmake --build build -j` completed without warnings in project code and
`ctest --test-dir build --output-on-failure` passed all 12 CPU tests in 0.80 seconds. GPU visual
verification is pending: attempts to run both `./build/gpu_fixture --check` and the new
`./build/gpu_fixture --city --check --resolution 320x200 --time 5` stalled during hidden raylib
window creation in this command environment, before the fixture printed its OpenGL context
details. macOS reported a `com.apple.hiservices-xpcservice` connection failure. This is an
environment/session failure rather than a city-render result; rerun the city captures at 0, 2.5,
5, 10, and 15 seconds at 320x200 and 320x240 on the development desktop, inspect them against
the step-01 references, and check a visible presentation before accepting step 04.

### Step 05 record — 2026-09-12

Changed `CMakeLists.txt`, `src/main.c`, `src/renderer_gpu.[ch]`, and
`src/waterfront_scene.[ch]`; this step also retains the uncommitted step-04 files
(`tools/gpu_fixture.c` and `assets/shaders/waterfront_city.fs`). `--renderer cpu|gpu` now
selects the backend and defaults to CPU. GPU is used for `cube`; the other scenes retain their
CPU framebuffer-upload path. The GPU scene owns reusable cube/plane meshes and draws the shared,
explicit scene-time traffic, lamp posts/lenses, opaque stepped pools, boat, and wake without
rebuilding static city resources or reading back during live frames.

GPU screenshots create a hidden context with no audio device, render the same virtual target used
by live frames, normalize the render-texture orientation once for PNG export, and release all GPU
resources before closing the context. GPU profiling similarly uses that frame path and labels its
measurements as CPU submission, explicitly reporting no CPU framebuffer upload. Live rendering
uses the existing integer destination calculation, negative-height GPU source rectangle, and
replacement-before-release resize handling. Music initialization and seek behavior are unchanged.

`cmake --build build -j` completed warning-clean for project sources and
`ctest --test-dir build --output-on-failure` passed all 12 CPU tests in 1.54 seconds. The CPU
`cube` profile still produced its exact checksum `fd8832e64f03e590`.

GPU capture verification remains blocked in this command environment: `./build/proteus --renderer
gpu --scene cube --time 0 --screenshot build/step05-gpu-t0.png --resolution 320x200` stalled while
creating the hidden macOS raylib window and emitted the existing
`com.apple.hiservices-xpcservice` connection failure before context details/capture output. This
matches the step-04 environment limitation, so no GPU visual acceptance is claimed. On the
development desktop, capture and inspect times 0, 2.5, 5, 10, and 15 at 320x200 and 320x240;
compare direct seeks with sequential evaluation; then manually check the full route, 1/2/3
resolution changes, window resize, fullscreen, quit, and audio playback/seek. The next step is
06 only after that review.

### Step 06 record — 2026-09-12

Added the default-off `PROTEUS_GPU_TESTS` CMake option, labelled `gpu` CTest case, and the
`gpu_review` build target. With the option enabled, both execute `tools/gpu_review.py`, which first
runs the GPU fixture and then captures the CPU and GPU `cube` paths at local times 0, 1.7, 2.7, 5,
7.5, 10, and 15 seconds. It leaves PNG sources, a CPU | GPU | amplified-diff contact sheet, and a
JSON manifest containing CMake build details plus the fixture's OpenGL vendor/renderer/version
report under `build/gpu-review/`. The review checks dimensions, exposed sky-band ordering, city
coverage, and broad lower-frame lit moving-object regions with tolerances. It does not compare
cross-driver image hashes. `gpu_fixture --check` now also probes explicit front/back winding and
the normal vertex attribute, in addition to its existing target, orientation, depth, and shader
failure checks.

With the option off, `cmake --build build -j` and the ordinary display-free
`ctest --test-dir build --output-on-failure` passed all 12 tests in 0.81 seconds. The opt-in
configuration exposed exactly one `gpu_review` CTest (`ctest --test-dir build -N -L gpu`). Attempting
the review target in this session again stalled while creating the hidden context, before the
fixture could emit a manifest or OpenGL details, so GPU acceptance remains pending. Run
`cmake --build build --target gpu_review` on the desktop session and inspect
`build/gpu-review/contact-sheet.png`; a context/shader/capture failure intentionally fails the
target and CTest instead of skipping it.

### Step 08 record — 2026-09-12

Added `assets/shaders/waterfront_city.vs` and extended `waterfront_city.fs`,
`src/renderer_gpu.[ch]`, `src/main.c`, and `tools/gpu_fixture.c`. The GPU city now passes world
position and correctly transformed normals to its shader. It combines a cool ambient fill and a
warm directional fill with at most eight selected unshadowed local lights. Lamps are warm points
(radius 15, intensity 0.78); vehicle headlights are warm, downward-tilted spots (radius 18,
intensity 1.25, inner/outer cone cosines 0.90/0.72). Candidate lights are derived only from the
shared scene time. Three slots are reserved for two warm boom floods and a cool trolley work light,
three for street lamps, and two for headlights. The crane floods are broad downward spots (radii
31–34, intensities 1.3–1.45); the trolley is a narrower cool spot (radius 26, intensity 1.35).
Street lamps use radius 18 and intensity 1.0 to establish warm quay pools. Candidates are ranked
against the authored camera target rather than the eye position, and a selected light receives a 65%
squared-distance-score discount on the following frame, so a replacement must be roughly 40%
closer to the shot focus before it takes a slot.

Lighting is accumulated before a six-level material ramp. The cool ambient fill is 0.07/0.09/0.18
with a blue directional fill at 0.45 intensity, leaving room for local lights to change the surface
ramp. This preserves albedo hue and prevents nearby lamps from adding white directly to materials.
Windows, lamp lenses, headlights, and the boat light are separately emissive. The crane also has
chunky orange and blue emissive fixtures, matching its three local lights. The
GPU-only opaque lamp/headlight pools have been removed;
the CPU reference still draws them. These local lights are intentionally unshadowed, so authored
radii limit—but cannot eliminate—light leaking through walls.

For identical-time comparison captures, `--gpu-lights on|off` on `proteus --renderer gpu` and
`--lights on|off` on `gpu_fixture --city` toggle local lights only; ambient/directional fill and
emissive appearance remain stable. For example:

```sh
./build/proteus --renderer gpu --gpu-lights on --screenshot build/step08-on.png \
  --scene cube --time 5 --resolution 320x200
./build/proteus --renderer gpu --gpu-lights off --screenshot build/step08-off.png \
  --scene cube --time 5 --resolution 320x200
```

`cmake --build build -j` completed warning-clean and the 12 display-free CTests passed in 0.80
seconds (`ctest --test-dir build -LE gpu --output-on-failure`; this build has the opt-in GPU test
enabled). GPU visual verification remains pending: the on-light city fixture was stopped after 20
seconds during hidden raylib window creation, before it printed context details or compiled the
new shader. macOS again reported a `com.apple.hiservices-xpcservice` connection failure. On a
desktop session, inspect on/off captures at 0, 2.5, 5, 10, and 15 seconds at 320x200 and 320x240,
then check headlight motion, nearby lamp influence, selection transitions, and the visible route.

### Step 09 record — 2026-09-16

Added `assets/shaders/waterfront_shadow.[vs|fs]`, extended
`assets/shaders/waterfront_city.fs`, and updated `src/renderer_gpu.[ch]`, `src/main.c`, and
`tools/gpu_fixture.c`. The GPU route now creates an explicitly depth-textured, depth-only
framebuffer and rejects initialization if its framebuffer is incomplete. It renders the city,
bridge, crane, vehicle bodies, boat, and lamp poles from one directional orthographic light before
the main city pass; windows, lenses, crane fixtures, the boat lamp, and wake remain out of the
caster pass. The default 1024x1024 map covers a 500-world-unit square around the camera target.
Its center is snapped in the light's right/up basis to 0.488-world-unit texels, so camera motion
does not continuously shift depth samples. A 512 option is available only for comparison.

The city shader removes only its directional fill in shadow. Ambient, selected local lights, and
emissive materials stay independent. The conservative slope-aware
comparison bias is `max(0.0012 * (1 - NdotL), 0.00018)` in normalized depth space; samples outside
the light frustum are deliberately unshadowed. `--gpu-shadows on|off` enables identical-time app
captures, while `gpu_fixture --city --shadows on|off` does the same in the focused harness.
`gpu_fixture --city --shadow-map` renders the light-frustum depth-map diagnostic; use
`--shadow-resolution 512` or `1024` to compare coverage quality before changing the default.
The directional key is a low, cool `(-0.45, 0.55, -0.35)` fill at 0.72 intensity: this gives
longer, palette-readable shadows than the prior high, low-intensity fill. The GPU route now keeps
full 8-bit material-lighting colors and a smooth virtual-resolution sky gradient, while retaining
the nearest-neighbour low-resolution presentation.

All lamp-post lanterns and each car's two front lenses are continuously emissive; they do not
depend on the bounded forward-light selection. The 32-slot forward-light list now carries every
authored lamp post throughout the route, alongside crane lights and nearby moving headlights, so
lamp pools remain on as the camera approaches. Visible sources remain on throughout the route.

`cmake --build build -j` completed warning-clean. The 12 display-free CTests completed successfully;
the enabled `gpu_review` test then reached its desktop-context stage but did not complete in this
session. A separately bounded 20-second depth-map fixture launch stalled during hidden GLFW window
creation and emitted the prior `com.apple.hiservices-xpcservice` connection failure. No GPU visual
acceptance is claimed. A later visible desktop launch exposed that the first implementation's nested
depth pass returned raylib to the window framebuffer, leaving the virtual target with sky only; the
renderer now explicitly restores the caller's framebuffer and viewport before its visible city pass.
On a desktop session, capture shadows on/off at route times 0, 2.5, 5, 10, and 15 at 320x200 and
320x240; inspect both 512 and 1024 maps plus the depth-map diagnostic; and check full-route motion
for acne, detached silhouettes, coverage loss, or crawling. Record the selected quality, GPU cost,
and any unavoidable coverage/bias limits before accepting this step.

### Step 10 record — 2026-09-16

Added distance-based atmosphere to `assets/shaders/waterfront_city.fs` and scene uniforms in
`src/renderer_gpu.[ch]`. The shader blends its final shaded (or emissive) material color toward the
smooth GPU sky horizon color `(50, 59, 88)` only after lighting, so shadows, local lights, and
source emissives retain their normal near-field contrast. The haze begins at 145 world units and
rises smoothly to a 76% blend at 620 units. It is deterministic, with no temporal noise or history
buffer.

`--gpu-atmosphere on|off` applies to GPU screenshot, profile, and interactive routes; the focused
fixture exposes the same switch as `--atmosphere on|off`. Both default to on. Glow was not added:
the current city already uses emissive source geometry, and this step does not justify an extra
virtual-resolution pass without desktop capture evidence that it improves the shot.

`cmake --build build -j` completed warning-clean. Desktop GPU capture remains pending because the
hidden GLFW context stalls in this session. On a desktop session, compare atmosphere on/off at
scene-local 0, 2.5, 5, 10, and 15 seconds at 320x200 and 320x240; verify that nearby road, lamps,
and vehicles stay crisp while the distant crane/buildings recede coherently toward the sky. Adjust
only the documented distance/opacity constants if those captures show an avoidable foreground wash
or horizon seam.

### Step 11 record — 2026-09-16

The generated geometry already marks every water triangle with its authored `(45, 79, 104)` color
at y=0, so `src/renderer_gpu.c` now separates that semantic mesh from land, walls, facades, roofs,
and quay during its one-time upload. It renders the non-water city, bridge, vehicle/boat bodies,
lamp poles, windows, and crane from the camera mirrored about y=0 into a point-filtered 160x120
color/depth target. The source has no below-water geometry; excluding the water mesh and wake from
that pass prevents feedback and below-surface leakage. Reflected winding is handled by disabling
backface culling only for the reflected submission.

`assets/shaders/waterfront_water.fs` maps water world positions through the exact reflected
view-projection matrix, applies two bounded deterministic sine ripples (at most 0.5% of texture
width/height), and blends the tinted reflection at 10–42% based on the view angle. Water remains a
dark blue independent of the reflection. The main pass draws water before the bridge, boat, and
wake, retaining their depth order. `--gpu-reflections on|off` and `gpu_fixture --reflections on|off`
provide identical-time comparisons; both default to on.

`cmake --build build -j` completed warning-clean. Desktop GPU capture is still required: compare
reflection on/off at scene-local 0, 2.5, 5, 10, and 15 seconds at 320x200 and 320x240; confirm the
shoreline remains attached, distant crane/window forms are legible but subdued, boat/wake stay in
front of the surface, and motion has no seams or unstable sampling. Measure the reflected-pass
cost during the profile route before accepting this optional step.

## Technical references

- [raylib 6.0 API cheatsheet](https://www.raylib.com/cheatsheet/raylib_cheatsheet_v6.0.pdf):
  render targets, shader APIs, texture filtering, hidden windows, and GPU image readback.
- [raylib shadow-map example](https://www.raylib.com/examples/shaders/loader.html?name=shaders_shadowmap_rendering):
  directional shadow rendering reference. Prefer its matching source in the pinned checkout
  under `build/_deps/raylib-src/examples/shaders/` when implementing.
- [Current renderer notes](3D.md) and [previous waterfront plan](WATERFRONT_PLAN.md):
  camera, generated geometry, CPU materials, and existing validation.
