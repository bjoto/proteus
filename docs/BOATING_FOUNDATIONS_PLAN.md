# Boating foundations implementation plan

Prepared 2026-10-19 against `f60ede8`, following the
[architecture review](ARCHITECTURE_REVIEW.md). This document owns this technical sequence;
[BOATING_PLAN.md](BOATING_PLAN.md) continues to own gameplay direction.

Status: P1, P2a, P3a, C1 and C2 implemented; C3's automated gate and replacement baseline are
implemented, with its manual mode playback pending. Full P2/P3 remain pending. Next for the
[multiplayer prerequisite](MULTIPLAYER_PLAN.md): manual C3 mode playback, then the basin W1/W3 slice (immutable definition, explicit water/world/session).
At preparation, the user requested a plan only. Implementation began with P1 on 2026-10-19;
execute later requested tasks using the protocol below. No additional approval gates are created
by this plan. Verification of the original planning work is recorded at the end.

## Outcome and scope

Implement, in order:

1. Performance instrumentation and repeatable boating workloads.
2. Integer world time with precise water/traffic evaluation and explicit replay compatibility.
3. Explicit ownership of location data, map caches, water state, and render resources.
4. Separate simulation, session orchestration, and presentation with enforceable dependencies.

Keep C17/raylib, 120 Hz physics, 320x200 integer-scaled presentation, current handling constants,
current controls/help/pause behavior, default wakes, basin course rules, and the four-minute
music-driven main demo. Keep the existing executable names and package paths.

This sequence does **not** implement collision acceleration, new collision response, spatial
render chunks, streaming, new boats/courses, AI, multiplayer, an ECS, threads, render interpolation,
hot reload, or a new asset format. Those require separate work after measurement. Do not mix
handling tuning or asset regeneration into a structural task.

## Execution protocol for an implementation agent

- Read this document, `AGENTS.md`, the task's named files, and its prerequisite handoff. Use
  `rg` to find all callers of changed APIs. Do not reread every historical design log.
- Work on one task at a time. Each task ends in a compiling, testable state. If asked for a
  whole phase, continue through its tasks without repeatedly asking permission.
- Record the starting commit and dirty files. Preserve unrelated edits. Do not commit or push
  unless requested. Treat numbered tasks as reviewable changes, not automatic git commits.
- Keep structural edits mechanically equivalent: preserve float expression order, loop order,
  branch order, random/emission sequences, and update timing. Phase C is the explicit exception
  for clock semantics; record its differences and establish the new baseline before W1.
- Temporary compatibility wrappers are allowed only where a task explicitly names them. Record
  each wrapper and its removal task; do not add hidden singleton state to avoid caller changes.
- A failing check is not permission to loosen its tolerance. Reduce the failure to a concrete
  case, compare the last passing baseline, and fix it. If the required behavior is ambiguous,
  report the precise decision and continue only independent work. Graphics unavailability is
  a verification limitation; do not substitute a CPU pass for a GPU result.
- At handoff update the ledger below: implementation status, exact checks/platform/options,
  artifact paths, observed differences, limitations, and next task. Distinguish automated
  checks, manual playback, and user acceptance. Keep the index summary current at phase changes.
- Run the common verification recipe at each task handoff. Add the task-specific checks; avoid
  repeating expensive visual checks for purely mechanical build/report changes.

## Task ledger

All rows start **pending**. Replace the evidence column with actual dated results as work lands.

| Task | Deliverable | Depends on | Status / evidence |
| --- | --- | --- | --- |
| P1 | Frame/tick metrics and correct timing boundaries | — | Implemented 2026-10-19; original verification limits and later 70-test result recorded below. P2a is implemented; P3a was next at this checkpoint. |
| P2 | Repeatable benchmark/report tool and renderer counters | P1 | Pending |
| P2a | Existing arcade-loop simulation benchmark; subset of P2 | P1 | Implemented 2026-10-23; shared-driver parity, benchmark contracts and 85 active CTests pass; [evidence](BOATING_PERFORMANCE.md) |
| P3 | Baseline report and workload fixtures | P2 | Pending |
| P3a | Current arcade-loop baseline; subset of P3 | P2a | Implemented 2026-10-23; [three-run trace, timings and captures](BOATING_PERFORMANCE.md#p3a-current-arcade-baseline--2026-10-23); full P3 matrix pending |
| C1 | Integer tick authority and replay revision | P3 | Implemented 2026-10-23 after P3a slice; [clock handoff below](#c1-handoff--2026-10-23) |
| C2 | Precise CPU/GPU time consumers | C1 | Implemented 2026-10-23; [C2 handoff below](#c2-handoff--2026-10-23); 85 active CTests pass |
| C3 | Long-session/compatibility gate and new baseline | C2 | Automated gate and baseline implemented 2026-10-23; manual wave/traffic/pause/course/ghost/replay playback pending; [C3 handoff below](#c3-handoff--2026-10-23) |
| W1 | Explicit immutable location definitions | C3 | Pending |
| W2 | Caller-owned collision and water-mesh caches | W1 | Pending |
| W3 | World/session owners and water migration | W2 | Pending |
| W4 | Reload/isolation/resource-lifetime gate | W3 | Pending |
| S1 | Extract pose, camera, scenery, and drawing | W4 | Pending |
| S2 | Separate cosmetic state from boat simulation | S1 | Pending |
| S3 | Session-owned course/replay orchestration and finish I/O | S2 | Pending |
| S4 | Shared build targets and dependency checks | S3 | Pending |
| S5 | Final comparison, playback, and handoff | S4 | Pending |

## Multiplayer prerequisite slices — 2026-10-23

The user requested multiplayer planning only. [MULTIPLAYER_PLAN.md](MULTIPLAYER_PLAN.md)
owns that proposal, protocol and fleet milestones; no implementation is authorized by this
status entry. This document continues to own core extractions. The original complete P/C/W/S
sequence remains useful, but is not an all-or-nothing prerequisite for a basin network slice.

For that track, perform P2a/P3a first, then the C1–C3 clock/phase/compatibility contract.
Next take the **basin subset** of W1/W3 (immutable definition, explicit water/world/session),
S1/S2 (shared pure pose math and cosmetic/view separation), S3 (course/file work outside the
physical tick) and S4 (core-only dependency gate). These may precede mapped W2/W4 because
the basin has no map collision cache. Two independent basin worlds and presentation-off
physics must work before fleet/network code. Keep remaining full-task acceptance pending.

Mapped W1/W2/W3 and W4 lifecycle gates are required before river/Eriksberg multiplayer.
Full renderer counters, all P3 workloads, course save-queue polish and broad view consolidation
may follow the basin proof. Do not postpone the physical checkpoint boundary, integer time,
wave phase precision, or headless dependency checks. Record partial scope/evidence under these
existing IDs, with any temporary offline wrappers and their removal task; do not duplicate
physics or add a default-world singleton to get a server compiling.

The single-player update order below remains the structural reference. Multiplayer M2 owns
the later explicit multi-boat schedule and contact changes, with separate behavior evidence.
The arcade `BoatWakeObjects` world already owns shared emitter state; W3 must preserve that
ownership rather than moving it back into individual boats. Legacy D3 bookkeeping can remain
in its offline adapter during migration. `BoatFlag` also belongs in presentation after S2.
The default P3 wake fixture is now arcade objects (128 total slots, 32 per emitter); any
64-entry D3 workload must be explicitly selected and labeled as legacy comparison.

### P2a — first multiplayer prerequisite task

Add a **simulation-only** `tools/boat_benchmark.c` target and a small tools-only shared driver
helper (proposed `tools/boat_study_drive.[ch]`). Read `demo_step`, `timed_boat_step`,
`object_study_reset`/`object_track` in `tools/boat_study.c`, `boat_profile_time`, and
`tools/wake_objects_review.c` before editing. Share the actual default one-boat arcade
input/step path; preserve advance → boat step → stern track, float clock arithmetic,
expressions and emission sequence. Keep the rival and optional wake modes compiling with
their current behavior. Do not introduce the future fleet scheduler here.

1. Support only `--scenario wake-loop --mode simulation --renderer none`, plus bounded
   `--warmup`, `--frames`, `--output`. Reject other modes/unknown options clearly. Advance two
   existing 120 Hz ticks per nominal frame without a window/audio device. Warmup and measured
   frames are one continuous scenario; do not reset the driver or world between them.
2. Share the current driver; do not tune it to fill the pool. It stops commanding powered
   turns after its existing tick 1440. Label the report as that finite scripted wake-return
   fixture, not a sustained full-pool benchmark. Report actual births/peak objects/landings.
3. Use `boat_profile_time(false)`; preallocate bounded timing samples. Write JSON metadata
   and p50/p95/p99/max plus per-frame CSV **after** measured ticks. Report unavailable render/
   GPU timings as null, include scenario revision/build/platform/options and actual tick counts.
   Validate count/size arithmetic and output errors. No timed file I/O or personal-best writes.
4. Record a hash/trace of named physical fields and wake/emitter values (including motion
   timers, velocities and counters), never raw structs/pointers. Verify repeated identical
   runs agree on this build. Do not claim cross-platform bit-identical simulation.
5. Add a short CTest schema/error/deterministic-state check, including a baseline comparison
   of the old app driver versus the shared helper for the same command sequence. Capture
   before changing the helper, then compare the migrated app's CPU output at matching ticks.
   No new pixel baseline or relaxed physics tolerance is justified by this extraction.
6. Run configure/build/CTest; inspect the before/after CPU captures and one scripted app run
   if the shared app path changed. Create `docs/BOATING_PERFORMANCE.md` with the report contract
   and actual evidence, link it in the index, mark P2a implemented only, and set P3a next.

Proposed smoke command, available only after this task:

```sh
./build/boat_benchmark --scenario wake-loop --mode simulation --renderer none --warmup 120 --frames 1800 --output build/bench/arcade-baseline
```

Deliverable: compiling existing programs plus one bounded headless benchmark, reproducible
state evidence and honest report. Non-goals: renderer counters, every P3 fixture, ENet/sockets,
integer time, physical/collision changes or new UI. P3a subsequently captures the baseline
report/traces and representative CPU/GPU views before C1; full P2/P3 remain pending.

## Design decisions fixed for this sequence

These are implementation constraints, not open design questions for each task.

**Ownership at completion:**

| Type / owner | Owns | Does not own |
| --- | --- | --- |
| `BoatSession` | Clock, world, player, cosmetic state, course, run/replay state | Window/GPU/audio device |
| `BoatWorld` | Location definition reference, collision cache, shared water settings/history | Clock, camera, draw resources |
| `Boat` | Physical state, controls/engine response, physical wake emitter bookkeeping | Location flag, environment, course, camera, cosmetic particles |
| `BoatPresentation` | Camera-follow state and existing bounded cosmetic particles/emission state | Forces, contacts, race progression |
| `BoatWaterMeshCache` | CPU topology/displacement buffers and patch cache for one bound world | Water field authority |
| `BoatGpu` / CPU view owner | Render resources and their water-mesh cache, bound to one world | Simulation ownership |
| `WaterfrontSceneFrame` | Owned evaluated frame values consumed by render passes | Pointers to mutable simulation state |

Use `BoatLocation` with `BASIN`, `ERIKSBERG`, and `RIVER`; eliminate the ambiguous combination
of `Boat.eriksberg` and a separate global river boolean. Immutable compiled geometry and
location-independent model recipes may remain shared. Stateful caches need explicit owners.
No new runtime map-selection UI is required: lifecycle tests exercise switching.

Use a small `BoatInput { float steering_rate, throttle_rate; }` with today's signed rate semantics.
Do not reinterpret it as absolute wheel/throttle position. There is one playable boat in scope.

**Live update order at completion:** consume input; advance the integer tick; evaluate traffic/
water at that tick; update horizontal motion and contacts; buoyancy; emit physical wake history;
update camera/cosmetic particles using the resulting pose/field; update course crossings and
recording; then return to the outer loop. Preserve today's internal ordering where operations
interleave: extract the actual statement sequence before moving functions. Camera smoothing is
still fixed-tick. Physical wakes remain simulation state; foam and spray particles are cosmetic.
Rendering, replay viewing, and ghost evaluation must never advance or mutate the live world.

Do not implement multi-vessel stepping now. A future fleet must read a consistent field and
define emission visibility between ticks; introducing `BoatWorld` alone does not establish that.

## Phase P — instrumentation before optimization

### P1 — frame/tick records and timing boundaries

Read/edit: `src/profiler.[ch]`, `tools/boat_study.c`; add `src/boat_profile.[ch]` for boating-specific
records/reporting so the main demo's existing profiler output remains compatible.

1. Add a bounded metrics record. Store completed-frame wall time, simulation-loop total, physics
   tick total/max/count, replay/course work, audio update, snapshot/render CPU time, present/wait,
   and active-frame clamped-away time. Timer nesting must be documented: parent totals include
   children and must not be summed with them. Reset frame counters once, before tick iteration.
2. Time the whole simulation section and the actual `boat_step` call separately. The current
   scene timer begins after both simulation and audio; retain its meaning or rename it explicitly.
3. Count `max(raw_active_delta - 0.1, 0)` as discarded active time. Pause/help/focus loss is a
   separate state, not dropped-performance time. Do not alter accumulator/clamp behavior here.
4. Use monotonic elapsed time. Reuse `GetTime()` with a live raylib window; for display-free
   profiling provide a small platform timer using `clock_gettime(CLOCK_MONOTONIC)` on macOS/Linux
   and `QueryPerformanceCounter` on Windows. Do not use `timespec_get(TIME_UTC)` for new elapsed
   measurements. Keep wall-clock timestamps separate from durations.
5. Add boating rows to F3 without obscuring game controls. Keep collection allocation-free in the
   hot loop; formatting/sorting occurs at report/overlay refresh boundaries. Handle zero samples.

Acceptance: a report/record test covers zero samples, nested totals, ticks/frame, pause vs dropped
time, and percentile calculations using known arrays. An instrumentation-on/off deterministic
drive has identical physical state and CPU output. This is meaningful behavior protection, not
a test that merely duplicates timer calls.

### P2 — benchmark tool, counts, and report contract

Read/edit: `tools/boat_water_review.c`, `tools/boat_study.c`, `src/boat_gpu.[ch]`,
`src/renderer_gpu.[ch]`, `src/boat_replay.c`, CMake. Add `tools/boat_benchmark.c`.

1. Share the existing scripted controls/routes in a small tools-only fixture helper. Preserve
   their behavior. Both app-style and benchmark driving must ultimately use the production tick
   entry point; do not fork physics or invent an easier benchmark-only simulation.
2. Implement the **proposed** CLI below. `--mode simulation` advances two fixed ticks per nominal
   60 Hz frame and needs no window, renderer, or audio device. `--mode render` runs the same ticks
   and draws using the selected backend; GPU mode needs a context. CPU render mode stays display-free.
   Simulation mode requires `--renderer none`; render mode accepts `cpu|gpu`. GPU AA is rejected
   outside GPU render mode. Reject unknown/contradictory options with a nonzero exit.
3. Count application mesh submissions and submitted triangles per main/reflection/shadow pass,
   plus AA downsample separately. Call them submissions, not driver draw calls. Wrap the production
   submission sites so counts describe executed branches. Reuse patch rebuild/capacity counters;
   add bytes uploaded and active wake/particle counts. Track owned-buffer capacity/high-water
   estimates; do not label these process RSS or complete driver VRAM.
4. Write JSON summary plus per-frame CSV under `--output`. Include schema version, scenario/seed,
   source commit when known and dirty/unknown indication, build type, compiler/platform, renderer,
   resolution/AA, features, warmup and measured counts, timer source, and mean/p50/p95/p99/max.
   Percentile convention: sorted index `floor((n-1)*p/100)`. No NaN/Infinity in JSON.
5. Preallocate measured-sample storage; do not include CSV formatting/disk writes or screenshot
   readbacks in timed frames. Record initialization separately. Track warmup maxima separately
   so cold/rebuild spikes are not silently discarded. Simulation mode has no present/GPU values:
   use unavailable/null, not misleading zero-cost measurements.
6. GPU execution queries are **deferred in this sequence**. Report `gpu_execution_ms: null` and
   label GPU measurements CPU submission/present time. Do not use `glFinish` to manufacture a
   timing number. The report must not claim achieved GPU headroom from submission alone.

Acceptance: short headless benchmark smoke/schema check in CTest; repeated identical drives
produce identical state hashes and workload counts (timings may differ). AA toggling does not
change physics. Failed output writes return failure. Run GPU counter checks where available.

### P3 — capture the baseline and lock fixtures

Add `docs/BOATING_PERFORMANCE.md` as the owner of measured performance evidence, linked here
and from the index. Keep raw CSV/images/logs in ignored build output.

Required scenario IDs and provenance:

| ID | Fixture | Required coverage |
| --- | --- | --- |
| `wake-loop` | Existing free-boating demo loop | Default player wake, full history, return through wake |
| `eriksberg-route` | Existing water-review route | Dock departure, dense harbor, shoreline approach |
| `river-route` | Existing river-review route | Bridge passage and patch movement |
| `shore-contact` | Existing validator's deterministic glancing/beam contacts | Contact work/spikes |
| `course-finish` | New fixed gate-following input fixture using the water-review controller pattern | Ghost, recording growth, best/export save |

Use production simulation at every tick. Short scenarios may reset between attempts, but label
resets; explicitly label finite arcade-return and sustained full-pool workloads separately.
The legacy D3 workload fills its 64-slot history; the current arcade fixture reports its
actual occupancy, with a separate sustained-source fixture for the 128-slot fleet bound. Assert scenario
milestones (arrival, contact, full history, finish); a non-finishing drive is a failed fixture.
The finish scenario must write only into its benchmark output directory, never personal bests.
The existing replay validator's `route()` prescribes poses: it is useful for recorder tests,
but is **not** an input-driven physics fixture. For `course-finish`, steer toward the next gate
using the existing route controller pattern, aim beyond its crossing plane, and advance the
target only when production course state advances. Freeze the successful input fixture/settings;
never teleport through gates to make a full-game benchmark pass. Generate the opponent in an
untimed setup run, then measure a new attempt with that ghost. Record fixture revision changes.

Run simulation for all scenarios; GPU AA off/on for the first three and course finish on the
available development machine. CPU render is a reference, not a requirement to meet 60 fps.
Default warmup is 120 nominal frames; measured length is 7,200 frames (120 seconds). If a route
needs longer to reach its milestone, explicitly select and record that length. Record machine
model, OS, power mode, and background-load limitations; this is a development baseline, not a
minimum-supported-hardware certification. Initial product target is 60 fps with 120 Hz physics.

Record physical-state traces and representative CPU/GPU captures before phase C. State hashes
must serialize named fields, not padding/pointers. Use the same-machine baseline for structural
equivalence; retain existing tolerance/invariant tests across compilers. Do not use whole-struct
`memcmp` as a cross-platform determinism claim.

Acceptance: table of actual measurements, trace/capture paths and reproduction commands. No
optimization is bundled with the baseline. Unsupported GPU/hardware checks are explicit gaps.

Proposed commands, available only after P2 (not current repository commands):

```sh
./build-release/boat_benchmark --scenario wake-loop --mode simulation --renderer none --warmup 120 --frames 7200 --output build/bench/baseline-sim
./build-release/boat_benchmark --scenario river-route --mode render --renderer gpu --gpu-aa --warmup 120 --frames 14400 --output build/bench/baseline-river-aa
```

## Phase C — clock correction, the only intended simulation change

### C1 — authoritative ticks, precise seconds, compatibility

Read/edit: `src/boat.[ch]`, `src/boat_course.[ch]`, `src/boat_replay.[ch]`,
`tools/boat_study.c`, fixture/validator callers. Add a small `src/boat_clock.h`.

1. Define `BOAT_TICK_HZ = 120`, retain float `BOAT_STEP` for the existing force integrator, and
   define exact double tick duration `1.0 / BOAT_TICK_HZ` for the accumulator/course timestamps.
   Clock authority is `uint64_t tick`; seconds are `(double)tick / BOAT_TICK_HZ`, never accumulated.
   Advance exactly once per completed live tick. Reset starts at zero; pause advances nothing.
2. Transitional ownership: put the clock in `Boat` until W3. A double `time` may remain as a
   derived live cache to keep this migration small, but all live writes go through clock helpers.
   Screenshot fixtures use an explicit time/tick setup helper; audit direct `.time` assignments.
   W3 moves authority to the session; S3 removes the transitional cache from physical state.
3. Replay sampling is continuous-time pose viewing, not simulation. Until S3, a sampled `Boat`
   may contain its recorded double view time and must never be passed to live stepping. Do not
   quantize replay interpolation or seek to a tick. Keep the main demo's music clock independent.
4. Change replay sample `world_time` to double. Bump `REPLAY_VERSION` from 1 to 2 and
   `BOAT_PHYSICS_REVISION` from the current 5 to 6 once for the entire C phase (use the next
   unused revision if another change consumes it). Serialize the double with `%.17g`, parse
   with `%lf`, and use exact double tick duration in version-2 headers/interval validation.
   Audit interpolation, finite/range checks, and fixture parsers too: do not narrow `world_time`
   through the existing array of float pose values during validation.
5. Retain the current strict revision rejection policy. Preserve old personal-best files under
   their old names; do not migrate, delete, or silently compare them. New automatic best paths
   use the new physics revision. Explicit incompatible imports fail clearly. This sequence does
   not add a legacy replay viewer. Update README/replay guidance in the same change.

Acceptance: 0/1/28,800/72,000/432,000 ticks yield tick-derived seconds, with no accumulated drift;
reset and pause work; exact-double course tick-boundary/fractional crossings pass. Version-2
round-trip and malformed-file checks pass; old fixture rejection leaves destination and disk
files intact. C1 alone is not completion of the long-session fix: C2 must remove float consumers.

### C2 — water, traffic, and shader time contracts

Read/edit: `src/boat_water.[ch]`, `src/waterfront_scene.[ch]`, `src/waterfront_harbor.[ch]`,
`src/waterfront_traffic.[ch]`, `src/renderer_gpu.c`, both `waterfront_water` shaders, and
`tools/validate_boat_environment.c`'s production-shader probe.

1. Carry double evaluation time through water/traffic/scene APIs and the scene frame; wake birth
   times become double. Compute wake age by subtracting doubles before narrowing bounded age to
   float. Keep short particle age/lifetime values and other bounded integration timers as floats.
   Audit every upload of scene time: a double pointer must never be passed as `SHADER_UNIFORM_FLOAT`.
   Upload explicit float phase/payload values, including any retained short main-demo time path.
2. For each ambient wave compute `remainder((double)angular_speed * seconds, 2*pi)` in double
   before converting to float. Use the same packed phase for CPU evaluation and vertex/fragment
   shaders. Do not compute `angular_speed * (float)seconds` anywhere on the physical-water path.
   Retain spatial wave parameters, amplitudes, derivatives, directions, and accepted speeds.
3. Pack player wake **age at frame time**, rather than absolute birth time, in the existing
   per-segment shader payload. Shader local age is `packed_age - longitudinal_offset * inverse_speed`;
   document the renamed payload slot and update both shader stages and probes together.
4. Keep scripted crossing wake time absolute and nonperiodic: it stops after its original support
   expires. Gate expired sources before float conversion; do not wrap them into recurring events.
5. Audit cosmetic shader ripple frequencies too. Give each periodic component its own double-
   reduced phase uniform. Never wrap one global time by an arbitrary period when components
   have different frequencies. Preserve the main demo's original short-time path if necessary
   for its exact baseline; any compatibility branch must be explicit and tested.
6. Traffic already reduces route phases internally in double but receives float time. Preserve
   double into that reduction. Audit car/gull/environment-strip periodic arithmetic similarly;
   reduce each period before float conversion, including negative samples used for wake trails.

Acceptance: CPU derivatives and production shader height/slope probes pass at normal times,
one hour, 24 hours, and immediately around individual phase wraps. Recent wakes at large time
match equivalent short-age wakes with ambient waves disabled; expired wakes never reappear.
Route periodicity and contact/render positions agree. Test water continuity through wraps;
the chosen angular representation may wrap, the resulting surface must not jump.

### C3 — clock gate and replacement baseline

Extend `validate_boat_environment`, traffic, replay, and clock checks; add `validate_boat_clock`
if a separate short validator keeps responsibilities clearer. Cheap direct-time tests cover 24
hours without simulating every preceding tick. Also run one actual simulated hour of a bounded
basin scenario in Release, checking finite state, wake capacity, and clock consistency; render
only representative samples so this does not become a full audiovisual soak requirement.

Compare against P3: calm-water handling should retain the force integrator's behavior; explicit
time-driven motion can differ because drift is corrected. Explain differences instead of updating
expected hashes blindly. Inspect CPU/GPU frames and manually play wave, traffic, pause/reset,
course/ghost/replay modes. Record any unavailable manual check. Run P3 workloads again and adopt
this as the baseline for W/S, with compatibility revision recorded. Do not tune coefficients to
make time-driven trajectories match the old incorrect clock.

Gate: no remaining accumulated float live world clock, no unbounded float input on physical
water/traffic phase paths, documented replay behavior, all applicable automated checks pass.

## Phase W — explicit world ownership

### W1 — immutable location definitions instead of global selection

Read/edit: `src/waterfront_scene.[ch]`, `src/boat_map.c`, `src/boat_water.c`,
`src/waterfront.c`, `src/renderer_gpu.[ch]`, all `waterfront_scene_select_river` callers.
Add `src/boat_location.[ch]` with enum and immutable `BoatWorldDefinition` records.

Definitions contain location ID, borrowed compiled map mesh (none for basin), bridge-triangle
metadata, basin bounds, spawn and existing location-specific defaults. Separate world defaults
from camera presentation settings in the record or a companion lookup. Store no active location
globally. Pass the definition to city-mesh/bridge queries, GPU initialization, CPU drawing and map
queries. The main demo passes Eriksberg explicitly. Preserve current geometry/material inference;
replacing importer metadata is outside this sequence.

If data types are coupled to renderer declarations, extract unchanged `Triangle3D`/`Mesh3D`
definitions into a small `geometry3d.h` included by `renderer3d.h`; do not redesign mesh storage.

Acceptance: remove the selector and `river_enabled`; compile every executable. In one process,
query Eriksberg and river alternately and verify their distinct mesh/bridge data. Existing demo
checks, mapped captures, and bridge/navigation validators retain the C3 baseline.

### W2 — per-owner map and water-mesh caches

Read/edit: `src/boat_map.[ch]`, `src/boat_water.[ch]`, `src/boat_gpu.[ch]`, CPU drawing and
review tools. Add `src/boat_water_mesh.[ch]` for tessellation/cache code moved from water physics.

1. Replace `shore`, `shore_count`, `water_count` globals with `BoatMap`. Init takes a definition;
   query takes a `const BoatMap *`; destroy is safe on zero/partially initialized state. Keep
   exhaustive collision algorithms unchanged. Surface initialization failure must not look like
   a normal shoreline collision: fail world loading before simulation begins.
2. Move `Surface`, subdivision/clipping, `surfaces`, `patches`, and patch centers into
   `BoatWaterMeshCache`. Bind each cache to one definition. CPU and GPU views own their caches;
   no basin/mapped global array remains. Water query/force code allocates no render mesh.
3. Preserve borrowed-view lifetime explicitly: returned mesh views last until their owner's
   next mutation/destruction. Do not copy owner structs with interior pointers. All destroy
   functions release only their own resources, including partial-failure cleanup.

Acceptance: two initialized maps/caches can be queried interleaved; destroying/rebuilding one
does not invalidate the other. Run repeated init/destroy under ASan/UBSan where supported. Use
the existing map/patch outputs as reference; do not fix performance by changing tessellation.

### W3 — introduce the world and session owners

Add `src/boat_world.[ch]`, `src/boat_session.[ch]`; update `src/boat.[ch]`, app/tools/tests.

1. `BoatWorld` owns its immutable definition reference, initialized collision cache, and the
   existing `BoatWater` settings/history. Read-only physics APIs accept `const` world/water
   pointers; update paths receive mutable pointers only where required. Do not heap-allocate
   individual particles or wake entries.
2. Introduce a minimal `BoatSession` containing world, boat, and clock. Move the C1 clock out
   of `Boat`; session advances once and supplies explicit `now` to stepping. Remove duplicate
   live clock authority. Course/cosmetic fields can remain on `Boat` until S2/S3.
3. Pass environment/contact/time explicitly into boat physics. Keep emitter bookkeeping on
   the boat and publish its emitted segment into `world.water` at the same point as today.
   Remove `Boat.eriksberg`/owned water; location behavior comes from the definition. All tools
   and validators use explicit sessions/worlds; no default-world compatibility singleton.
4. Reset the player/session using the existing CLI settings, clear physical and cosmetic wakes,
   and restart the clock. Reset does not destroy/recreate unchanged map assets. Preserve the
   existing race-session best/opponent behavior when the later S3 owner migration lands.
5. A ghost/replay samples a view at recorded time without mutating the live field/history. Bind
   render resources to the world's definition explicitly. Main demo callers use their own world
   resources and music-derived time; they do not participate in boat session ticking.

Acceptance: two independent worlds with different location/water settings stay independent under
interleaved stepping. Copies used by tests must explicitly clone state or reinitialize ownership;
never shallow-copy owners. Single-player named-field traces/captures match C3. Search all callers
and remove old no-context APIs rather than leaving hidden alternate behavior.

### W4 — lifecycle gate

Add `tools/validate_boat_world.c` with CPU tests registered in CTest. In one process test
basin → Eriksberg → river → Eriksberg → basin; compare a freshly initialized world to each
revisited world, including shoreline queries, spawn, water settings, and patch topology. Test
invalid definition/partial-init cleanup. Keep two worlds alive for the isolation tests.

Add a GPU-context variant that destroys the old view while its context/world are alive, then
creates the new world/view and captures equivalent frames. Destroy in order: view/GPU resources,
CPU presentation caches, session/world resources, then window/context. Fixed immutable shared
model geometry can remain process-lifetime; identify that explicitly in ownership documentation.

Gate: no active-map global, no mutable global map/water topology cache, no borrowed resource
surviving its owner, and location A→B→A matches fresh A. Record lifecycle/manual/ASan results;
do not claim an interactive location-switching feature was added.

## Phase S — separate simulation and presentation

### S1 — extract pure pose and rendering responsibilities

Read/edit: `src/boat.[ch]`, `src/boat_gpu.[ch]`, `src/boat_course.[ch]`, CPU waterfront drawing.
Add `src/boat_pose.[ch]`, `src/boat_view.[ch]`, `src/boat_course_view.[ch]`.

Move shared hull rotation/attachment math into `boat_pose` with no draw calls; physics buoyancy
and presentation must use this one implementation. Move camera construction, scene assembly,
particle appearance/pose helpers, basin/environment scenery, and CPU draw functions to
`boat_view`. Move course box visitors/HUD/drawing to `boat_course_view`; keep gate crossing math
in `boat_course`. Temporary header forwarding is allowed until S4, without circular includes.

Build one `WaterfrontSceneFrame` per displayed frame in the view layer. CPU and GPU boating
entry points consume that frame plus explicit bound world resources, rather than rebuilding
actors independently. Preserve CPU reference details intentionally different from GPU shading.

Acceptance: captures and named-field traces match W4; rendering a frame twice does not alter
session state, wake emission, replay position, or random sequences. All passes use the same
evaluated actor poses. Pose helpers remain covered through existing heading/attachment tests.

### S2 — move camera and cosmetic state out of physics

Add `src/boat_presentation.[ch]`. Move particle arrays, cosmetic emission counters/timers, and
camera-follow state into `BoatPresentation`. Keep physical wake emitter state and wave-entry
load used by physics/audio on `Boat`. Move `water_step` cosmetic behavior and camera update
mechanically; keep their fixed-tick schedule, expressions, and deterministic variation sequence.

`boat_session_step` calls physics, then presentation at the preserved points in the update
sequence. Read-only pose/motion outputs provide the information particles need; do not recalculate
forces in presentation. Permit presentation to be disabled for simulation-only benchmarks/tests.
Existing camera-follow values in replay remain presentation values, not physical state.

Acceptance: with identical input/world, presentation enabled/disabled produces identical
physical traces and physical wake histories. Enabled particles/camera and CPU captures match
W4. Render at 30/60/144 Hz with the same tick-indexed inputs and verify equal simulation state;
do not claim different wall-clock keyboard sampling is automatically identical.

### S3 — session rules, replay views, and finish persistence

Read/edit: `src/boat_session.[ch]`, `src/boat_course.[ch]`, `src/boat_replay.[ch]`,
`src/boat_audio.[ch]`, `tools/boat_study.c` and benchmark fixtures.

1. Move course state and run/replay orchestration into `BoatSession`; physics returns previous/
   current pose for swept gate checks and consumes only `BoatInput`, environment, and explicit
   tick time. Keep existing course gate order, split interpolation, replay transport and controls.
2. Introduce a small `BoatViewState` for live/replay/ghost pose plus needed presentation/audio
   values. Replay sampling returns this rather than manufacturing a partially valid live `Boat`.
   Move shared view/replay value types to a data-only header if required to avoid include cycles.
   Remove C1's transitional time cache. Keep replay interpolation continuous and render-only.
3. Separate completed-run bookkeeping from persistence. Queue a bounded pending finish/save
   request, flush after the catch-up tick loop, and flush pending work before reset/shutdown.
   Define request ownership so a reset cannot invalidate the recording being saved. Retain
   atomic replacement, export behavior, notices, and in-memory best on write failure. Keep this
   synchronous outside the tick loop; background I/O and replay compression remain out of scope.
4. Keep device audio in the app adapter, reading the selected live/replay view. Input polling,
   window focus/help, music updates, and presentation stay in `boat_study`; it delegates session
   stepping and view assembly. Preserve existing replay audio behavior, including its known
   approximations, rather than silently adding unrecorded state.

Acceptance: course/ghost/replay/save validators pass; no filesystem calls occur inside a physical
tick; reset/finish/pause/replay combinations preserve best files and live-world isolation. Tests
write only temporary outputs. Manually check focus/help, controls, audio, course finish, replay
seek and reset. No file format bump is needed for an ownership-only change after C1.

### S4 — enforce boundaries in the build

Edit CMake after the source dependencies are real, not before. Use a few static libraries:

- Core: clock, boat/pose/input, world/location/map, water queries, course crossing math.
- Presentation: cosmetic simulation, boat/course views, scene actor evaluation, CPU renderer
  and water mesh generation. Existing harbor/traffic modules may contain both core queries and
  visual code: extract contact/route evaluation into data-only/core files if otherwise required
  to pull drawing into core. Share definitions rather than duplicate formulas.
- GPU: GPU renderer/view code linked by graphics executables.
- Session/adapters: session orchestration/replay persistence/audio/profile as appropriate;
  keep executable-specific resource paths and build labels on their existing consumers.

Avoid circular libraries and a generic service interface. Link shared code once per executable;
remove repeated source-list inclusion. Keep raylib math/types available in core, but no core
header includes `boat_view.h`, `renderer_gpu.h`, `waterfront_scene.h`, or drawing declarations.
Enforce forbidden includes/calls with a small explicit source-boundary check in CTest. It must
cover keyboard/window/audio/file I/O and drawing APIs in core, not ban harmless vector math.

Split existing mixed validators by responsibility without deleting assertions: fast physics/
water/course checks link core; scene/particle/camera checks link presentation; GPU probes remain
conditional. A display-free core validator must run without creating graphics/audio resources.
Add CPU/render/GPU test labels. Broader Windows CI and sanitizers can use these targets; no
release/deployment workflow redesign is included.

Acceptance: clean Debug/Release builds, all applicable tests, packaging/relocation smoke retained.
Touching only a GPU implementation rebuilds no core object. Core tests do not compile drawing
implementations. Transitional wrappers/headers listed in earlier handoffs are removed.

### S5 — final gate

Rerun the P3 matrix against C3 on the same machine/configuration, recording median/p95/p99/max,
workload counts, rebuild/save spikes, and owned capacity estimates. Explain significant changes;
do not attribute wins to GPU execution without GPU timing. Run each comparison at least three
times if claiming a performance change. Investigate a repeatable >5% CPU median/p95 regression
before calling the structural migration complete; this is a local investigation trigger, not a
shared-runner CI failure threshold.

Inspect representative CPU/GPU frames and perform the manual checks listed above. Update
`docs/3D.md`, relevant water/traffic/replay guidance, README commands, and `AGENTS.md` layout to
the implemented boundaries. Keep historical review evidence dated; link its recommendations to
this completed plan rather than rewriting history.

Completion means: one live tick authority; explicit independent worlds and cache lifetimes;
rendering cannot mutate physics; cosmetic work can be disabled without altering physics; course/
save/device behavior sits outside core; baseline comparison and limitations are recorded. This
does not establish readiness for multiplayer, unlimited maps, or arbitrary numbers of boats.

## Verification recipes

Existing commands, run from the repository root at implementation handoffs:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
git diff --check
```

Keep the existing GPU option if configured; record its value. For explicit graphics verification
configure with `-DPROTEUS_GPU_TESTS=ON`. Without a desktop context use a separate CPU-only build
with the option OFF and leave GPU evidence pending. Do not change the shared build to hide failures.

Separate Release measurements, reusing the already fetched pinned raylib source when available:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_RAYLIB="$PWD/build/_deps/raylib-src" -DPROTEUS_GPU_TESTS=OFF
cmake --build build-release -j
ctest --test-dir build-release --output-on-failure
```

Do not run two full suites concurrently in the same build/output directory. For new fixtures,
use separate directories per scenario/test. Release measurements run without sanitizers.

At visual/time/lifecycle changes capture basin wake-loop, Eriksberg and river/bridge CPU/GPU
frames at documented times, plus course/ghost/replay; inspect rather than merely create them.
Use existing CLI/review tools and the later P2 fixtures. A manual demo smoke is required when
motion/input/audio/loading changes; automated screenshots do not establish human feel acceptance.

## Handoff template

```text
Task ID / status:
Starting revision and unrelated edits:
Changed ownership/APIs and migrated callers:
Behavior changes (none expected outside phase C):
Commands/platform/options and actual results:
Trace/report/capture locations and comparison:
Manual playback / user acceptance (separate):
Temporary wrappers and their removal task:
Known limits / unresolved decisions:
Next task:
```

Suggested implementation prompt:

> Implement task P1 from docs/BOATING_FOUNDATIONS_PLAN.md. Follow its fixed decisions and
> execution protocol. Preserve unrelated changes and current behavior. Complete the task's
> validation and documentation handoff, then stop before P2. Do not tune physics or optimize
> unrelated code. Report any unavailable checks precisely.

For a phase, replace P1 with the phase name and explicitly request its full task range. Do not
ask the implementation agent to execute the entire migration in one unbounded change.

## Planning verification

2026-10-19: source/caller audit performed against `f60ede8`; no runtime changes made for this
plan. Debug configure/build succeeded on macOS with the existing `PROTEUS_GPU_TESTS=ON`
configuration; CMake retained raylib's OpenGL deprecation notice. The full CTest attempt passed
the first 50 CPU checks, then stalled for more than four minutes in `ferry_gpu_screenshot`.
Only that test process and its parent CTest run were terminated. This is not a passing GPU run;
the cause was not diagnosed. The remaining boating XM check passed separately in 1.44 seconds:
all 51 CPU tests passed across those runs, while this attempt provides no completed GPU result.
Changed local file links and whitespace checks passed. No manual audiovisual review was needed
for this documentation-only change. Phase P measurements, clock fixes, ownership changes,
separation, and their acceptance remain pending; the earlier review's GPU results remain dated
historical evidence, not a claim about this test attempt.

## P1 handoff — 2026-10-19

- **Task ID / status:** P1 implemented; P2 remains pending.
- **Starting revision and unrelated edits:** `f60ede8`. At start, `docs/BOATING_PLAN.md` and
  `docs/README.md` were modified, while `docs/ARCHITECTURE_REVIEW.md` and this plan were
  untracked. The boating-plan edit and architecture review were preserved; only the index's
  foundations row and this plan's live status were updated for P1. No commit or push was made.
- **Changed ownership/APIs and migrated callers:** `boat_study` now writes bounded 240-frame
  `BoatFrameMetrics` records through `boat_profile.[ch]`. The existing main-demo `Profiler`
  remains compatible. `profile_time()` and display-free boating timing now use monotonic platform
  clocks; live boating uses raylib `GetTime()`. `validate_boat_profile` exercises records and a
  timed drive. No compatibility wrappers were added.
- **Behavior changes:** No intended physics, clamp, pause, or replay behavior change. The boating
  F3 overlay adds simulation, physics tick total/max/count, course/replay, audio, render/upload,
  present/wait, and discarded-active-time rows. A completed frame is now recorded at its end.
  Boating statistics are sorted only at the existing four-times-per-second overlay refresh.
  Simulation includes physics and in-loop course/replay work; course/replay also includes input
  and ghost evaluation outside simulation. Frame wall includes all sections. Parent and child
  totals overlap and must not be summed. The old scene row still means draw plus CPU upload.
- **Automated verification:** macOS, Debug, `PROTEUS_GPU_TESTS=ON`:
  `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`, `cmake --build build -j`,
  `ctest --test-dir build --output-on-failure -LE gpu -j 4` passed 52/52.
  `boat_profile_records` passed zero-sample, percentile, tick, pause/discard, nested-record,
  named physical-state, and byte-identical CPU output checks with instrumentation on/off.
  `git diff --check` passed. A full `ctest --test-dir build --output-on-failure -j 4 --timeout 60`
  passed its first 52 CPU tests; `gpu_review` timed out at 60 seconds, after which the run was
  stopped before the remaining GPU tests. This is not a completed GPU suite result.
- **Artifacts and comparison:** CPU test captures are in `build/`, including
  `boat-default-player-wake.png`, `boat-eriksberg.png`, and `boat-river.png`. The deterministic
  profile validator compared on/off physical state and CPU pixels in memory. No P3 performance
  baseline or GPU comparison exists yet.
- **Manual playback / user acceptance:** A windowed `./build/boat_study --renderer cpu
  --profiler --frames 3` attempt did not reach frame completion in this environment and was
  stopped. The visual F3 overlay and interactive playback were not inspected; no user acceptance
  is inferred. The change does not alter motion, input, audio, or asset loading.
- **Known limits / next task:** Graphics-context access remains unavailable in this run, matching
  the earlier planning attempt's GPU limitation. P2 is next: benchmark/report tool and renderer
  counters. It must keep GPU results explicitly pending until a working desktop context is
  available.

## Combined web/timing verification — 2026-10-19

Before committing the pending P1 work with browser support, the combined native Debug build
and all 70 CTests passed on Apple Silicon macOS, including 18 GPU cases with desktop access
and a 120-second per-test timeout. Evidence: `build/web-tags-ctest.log`; the
[web build record](WEB_BUILD.md#verification-evidence) owns browser checks and the completed
native scripted playback smoke. This resolves the earlier run's graphics-access limitation;
it does not establish a P3 performance baseline or human acceptance of the F3 overlay.
P1 is implemented; P2 remains next. Integer world-time migration (C1/C2) is still pending.

## Windows CI follow-up — 2026-10-20

After commit `84ca2bf` was pushed, Windows run
[39309664424](https://github.com/doomdavve/proteus/actions/runs/39309664424/job/118255664525)
failed compiling `src/profiler.c`: the new `_WIN32` `windows.h` include exposed Win32
`Rectangle`, `CloseWindow`, `ShowCursor`, and `DrawText` declarations that conflict with
raylib. Applied the repository's existing `WIN32_LEAN_AND_MEAN`/`NOGDI`/`NOUSER` header
defines in both profiling translation units. This changes no timing logic. The local host
has no MSVC/Windows SDK; the fix awaits the next Windows Actions run before claiming Windows
support passes. P2 remains next.

## P2a extraction progress — 2026-10-23

Historical intermediate checkpoint; superseded by the completion record below.

Starting revision `2994e0d`; README and twelve existing documentation edits plus the new
multiplayer plan were present before implementation and are preserved. The driver extraction
is implemented; P2a remains in progress until its benchmark and full verification land.
The [performance record](BOATING_PERFORMANCE.md#shared-driver-extraction--2026-10-23)
records the frozen-driver checks, identical inspected CPU captures and scripted desktop run.
Next: benchmark schema/error/deterministic-state checks and common verification. No handling,
clock, wake-emission or asset changes are intended; no user acceptance is inferred.

## P2a completion — 2026-10-23

P2a is implemented; P3a was next at this checkpoint. Full P2 renderer counters and full P3 fixtures remain pending.
The tools-only shared driver preserves the current default arcade input/step path and optional
rival/wake paths. `boat_benchmark` adds bounded simulation-only JSON/CSV reporting and named
physical/wake state hashing; it does not change physics, float time, emission or controls.

[The performance record](BOATING_PERFORMANCE.md#p2a-completion-evidence--2026-10-23) owns the
report contract, exact commands/platform/options, captures, scripted app check, repeated hashes
and limitations. Debug configure/build and all 85 active CTests pass with desktop access
(20 active GPU cases, 113.79 s; fifteen retired field tests disabled). Final focused benchmark
and driver tests pass after metadata/output-error changes. CPU before/after captures match at
1080/2400 ticks and were inspected; the frozen driver matches all 3840 tick hashes.
No cross-platform determinism, controlled P3a baseline or user acceptance is inferred.

The extraction was committed as `b06ecd2`; benchmark/report completion follows separately.
Pre-existing documentation/planning edits are preserved and excluded from these commits.

## P3a trace preparation — 2026-10-23

At this intermediate checkpoint, P3a was in progress. The benchmark records bounded per-frame physical/wake diagnostic traces
outside its timed interval, including warmup. The [performance record](BOATING_PERFORMANCE.md#p3a-trace-preparation--2026-10-23)
owns the trace contract and three-run milestone/repeat runner. Focused Debug driver and report
checks pass; the common full suite is being completed before controlled Release measurements.
No physics, time arithmetic, controls, emission or rendering changed. Next: controlled repeated
measurements, inspected CPU/GPU captures and the final baseline evidence. Full P2/P3 stay pending.

## P3a completion — 2026-10-23

P3a is implemented for the current finite arcade wake-loop simulation fixture. The
[performance record](BOATING_PERFORMANCE.md#p3a-current-arcade-baseline--2026-10-23)
contains the three matching Release traces, 120-second measured-frame timing table, CPU/GPU
capture paths and inspection, exact reproduction commands, machine/build/power metadata,
Debug 85-case full-suite pass, scripted app run and verification limits. The fixture
capsizes after its one landing and all wakes expire early; its 120-second p50 is therefore
weighted toward a settled state. This is a known workload limitation, not a full-pool cost
claim. P3a changes diagnostic trace output only; no handling, clock or renderer behavior
changed. Full P2 renderer counters and the other P3 workload fixtures remain pending.

C1 is next on the multiplayer prerequisite track: integer tick authority and explicit replay
compatibility. The current 14,640-tick float time differs from nominal 122 s by about 7.7 ms.
C1/C2/C3 must record their intended clock/phase differences against this baseline before
world/session extraction. No cross-platform bit identity or user acceptance is inferred.

## C1 handoff — 2026-10-23

- **Status / next:** C1 implemented on the multiplayer prerequisite track after P3a.
  C2 precise water/traffic/shader time is next; C1 alone does not complete long-session
  phase/wake precision. Full P2/P3 remain pending. Starting revision was `5c7ef7a`; the
  existing README and documentation edits and untracked multiplayer plan were preserved.
- **Clock / compatibility:** `Boat.tick` is the temporary live authority until W3, with
  `Boat.time` a derived double cache; S3 removes the cache from physical state.
  `boat_set_tick` sets exact fixture/live ticks. The unchanged float `BOAT_STEP` still drives
  forces; app accumulation and course timestamps use exact `1.0/120`. Replay samples are
  continuous-time views guarded against live stepping. Replay format 2 stores double
  `world_time` with `%.17g`; physics revision 6 uses new personal-best filenames. Version-1
  and revision-5 imports fail without replacing in-memory runs or old files. No legacy reader
  or default-world singleton was added.
- **Automated verification:** Apple M1 Max/macOS 26.6.2, Debug,
  `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`, `cmake --build build -j`, and
  `ctest --test-dir build -j 4 --output-on-failure` pass 85 active cases (20 GPU) in
  112.36 s with desktop access; fifteen retired field cases stay disabled.
  `boat_replay` covers ticks 0/1/28,800/72,000/432,000, reset, exact fractional gate
  crossings, version-2 double round trips, continuous replay viewing and version-1
  rejection without loss of the destination or disk file. Focused replay/benchmark checks
  and the CPU suite also pass; `git diff --check` passes.
- **Baseline difference:** The [C1 comparison](BOATING_PERFORMANCE.md#c1-clock-comparison--2026-10-23)
  retains every recorded physical/wake CSV field at all 7320 rows except derived time/hash;
  14,640 ticks end at exactly 122 s instead of 121.992271 s. Release CPU/GPU captures at
  9/11/20 s match P3a byte for byte and were inspected. Scripted 180-frame desktop demo
  and course runs completed. No human handling/focus/audio acceptance is claimed.
- **Limits at C1:** Water/traffic/scene/shader APIs still narrowed time to float; the
  [C2 handoff](#c2-handoff--2026-10-23) records that migration. C3 owns the long-session gate
  and replacement baseline. Windows/Linux/browser execution and manual pause/focus/replay
  playback were not checked.

## C2 handoff — 2026-10-23

- **Status / next:** C2 implemented on the multiplayer prerequisite track, starting from
  `6500a65` with the existing README/documentation edits and untracked multiplayer plan
  preserved. C3 long-session gate and replacement baseline is next; full P2/P3 remain pending.
- **Time contract:** Water, harbor, traffic, scene-actor, boating-gull and flag APIs take
  double world seconds; `WaterfrontSceneFrame.time` and D3 `BoatWakeSegment.emitted` are
  double. `boat_clock.h` adds `boat_phase`/`boat_cycle`, a nearest-cycle FMA reduction in
  double before narrowing. Wake ages subtract doubles and are gated before narrowing; short
  particle/integration timers remain float. Bounded main-demo timeline entry points
  (`waterfront_demo_evaluate`, `waterfront_scene_evaluate`) keep their float timeline time.
- **Water/shader payloads:** `boat_water_wave_uniforms` packs each ambient component's
  `remainder(angular_speed * seconds, 2*pi)` into `waveParameters[i].w`; CPU queries and both
  shader stages subtract that same float phase. The shared `time` uniform is removed: no
  double is uploaded as a float. `playerWakeParameters[i].y` is now age at frame time (was
  absolute birth), and shader local age is `packed_age - u * inverse_speed`. The scripted
  crossing uses `crossingWakeTime`, which is -1 outside its finite 0–24 s support, so it
  never recurs. Cosmetic ripples use per-component `detailRipplePhase` (1.5, 2.1 rad/s) and
  `reflectionRipplePhase` (0.80, 0.55, 0.65, 0.35 rad/s). The fragment shader's ambient loop
  is `ambientWaves()`, shared by production and the parity probe. Traffic keeps its double
  route reduction with double input; `waterfront_traffic_period` now returns the exact double
  period. Car lanes, gull cues/wingbeats, flag wind and the current cue reduce each period
  before float trigonometry. No compatibility wrapper or main-demo branch was needed.
- **Performance:** `boat_water.c` now queries through internal const pointers instead of
  copying the 2.7 KB `BoatWater` per call; public signatures are unchanged. A Release CPU
  `boat_study --location eriksberg --waves --player-wake --time 60` screenshot run took
  1.29 s versus 1.48 s for `6500a65` (default basin 0.33 s versus 0.35 s), same machine.
- **Automated verification:** Apple M1 Max/macOS 26.6.2, Debug, the common configure/build
  and `ctest --test-dir build -j 4 --output-on-failure` pass 85 active cases (20 GPU) in
  110.46 s; fifteen retired field cases stay disabled. `boat_environment` now checks ambient
  water against an independent double reference (<1e-5 m) at 0/10/3600/86400/30-day epochs,
  FD derivatives there, 60 individual phase-wrap samples, large-epoch D3 wakes against equal
  short-age wakes, and expired D3/crossing wakes at later times. A control shows the former
  float-time phase misses the reference by 1.53 mm at 24 h. `boat_environment_gpu` reads the
  production fragment and vertex stages through an RGBA32F target: 1155 samples at 0 h, 1 h,
  24 h and ±1e-4 s around each component's wrap, max error 3.44e-4 (wake tolerance 5e-4,
  ambient-only 1.5e-4). `harbor_traffic` adds 1 h/24 h/30-day route periodicity, smooth motion,
  render/contact agreement and gull continuity; `waterfront_actors` adds smooth car lanes at
  the same epochs. `wake_objects_gpu` was updated to the one-argument `playerWake`.
- **Baseline difference:** The [C2 comparison](BOATING_PERFORMANCE.md#c2-time-consumer-comparison--2026-10-23)
  has three Release arcade traces identical to C1 at every row (final hash
  `24d582aae45971b4`). Release CPU captures at 9/11/20 s and GPU 9/20 s match C1 byte for
  byte; GPU 11 s differs in two pixels by one level. Of 32 Debug CPU/GPU reference frames,
  all CPU and main-demo GPU frames match; six boating GPU frames differ in at most four pixels.
  Traffic renders at 24 h and 30 days were inspected. Scripted 180-frame Release demo, Eriksberg
  wave/player-wake demo and course runs exited normally.
- **Manual playback:** On 2026-10-23 the user played the default basin build from
  `cmake -S . -B build/review -DCMAKE_BUILD_TYPE=RelWithDebInfo`,
  `cmake --build build/review --target boat_study -j` and `./build/review/boat_study`, and
  reported that it works great. Other locations and options were not reported as played.
- **Limits:** GPU tolerances were measured on one Apple GPU; Windows/Linux/browser were not run.
  No simulated hour was run (C3). The benchmark trace CSV and profiler overlay still print
  `Boat.time` as float (diagnostic display only). The frozen P2a oracle
  `tools/boat_drive_baseline.h` restores only the derived `time` cache after its crossing
  boat's recovery reset, whereas production restores the tick; C3 should decide whether the
  oracle needs a C1-era revision. Specific pause/focus/replay and long-session playback
  were not separately reported.

## C3 handoff — 2026-10-23

- **Status / next:** C3's automated long-session gate and replacement baseline are implemented
  (code `ce3b53d`, after C2 `0252d0a` and the multiplayer documentation commit `dc5a5f0`).
  The specified manual playback of wave, traffic, pause/reset, course, ghost and replay modes
  has not been performed; the user's 2026-10-23 default-basin play predates C3 and covers only
  that mode. Next: that manual playback, then the basin W1/W3 slice from the
  [multiplayer prerequisite slices](#multiplayer-prerequisite-slices--2026-10-23). Full P2/P3
  and mapped W/S tasks remain pending.
- **Gate audit:** Live world time is `Boat.tick`; no float accumulator acts as a world clock.
  Remaining `+= BOAT_STEP` fields are bounded emission/contact timers that reset; the course
  timer accumulates exact double tick durations within one run. Physical water and traffic
  phases take double input and reduce before narrowing (C2). Arcade wake objects already use
  64-bit relative tick ages. Compatibility remains replay format 2 and physics revision 6
  for the whole C phase; C2/C3 changed no handling coefficients.
- **Replay behavior:** Recorded `world_time` is absolute session time, not course time, so a
  course may start long after launch. Loading previously rejected samples above 100,000 s
  (about 27.8 hours), which would have made such a personal best unloadable. Version-2 loading
  now accepts finite, non-negative world time up to `BOAT_REPLAY_MAX_WORLD_SECONDS` (100 years).
  No format, version or revision change; earlier valid files remain valid.
- **New validator:** `validate_boat_clock` (CTest `boat_clock`, 10.6 s Debug) checks exact
  tick seconds through 2^40 ticks; 200,000 random phase/cycle reductions over 30 days, which
  equal libm `remainder` after narrowing; the P3a input sequence started at tick 0, one day and
  30 days, with bit-identical physical hashes for all 14,640 ticks (17 births, one landing);
  the legacy D3 trail over 2400 ticks with zero divergence; 30 s powered wave sessions with
  D3 trails at one day and 30 days in basin, Eriksberg and river (finite, contained, riding
  waves); and an exact save/load round trip of a course recorded 30 days into a session.
  `boat_clock_soak_smoke` runs `--soak 60 ... --waves` in the routine suite.
- **Simulated hour:** Release `validate_boat_clock --soak 3600 build/bench/c3 [--waves]` ran
  432,000 ticks each with a steered player loop, a crossing boat and arcade wakes, checking
  every tick: finite state, basin containment, exact tick-derived time, matching wake-world
  tick, `emitted == expired + live` and capacity. Calm: 11,401 births, peak 63/128, 263
  landings, 86 pose recoveries, hash `43dfc74ddac0055d`, 93.9 s CPU. Waves: 11,587 births,
  peak 64/128, 256 landings, 86 recoveries, hash `56e31ce5c3d7a7c4`, 97.6 s CPU. Frames at
  1, 30 and 60 minutes were inspected. Full numbers are in the
  [C3 baseline](BOATING_PERFORMANCE.md#c3-replacement-baseline--2026-10-23).
- **Baseline comparison:** P3a's three Release runs at `ce3b53d` match C1 and C2 at every trace
  row; final time is exactly 122 s (P3a accumulated 121.992271 s), hash `24d582aae45971b4`.
  This calm fixture has no time-driven water, so only derived time differs from P3a, as the
  clock correction intends. CPU views match P3a byte for byte; GPU 9/11 s and GPU AA 9/11 s
  differ by at most two pixels at one level, from C2's shader restructure. This is the
  adopted W/S baseline.
- **Automated verification:** Apple M1 Max/macOS 26.6.2, Debug common recipe: 87 active CTests
  pass (20 GPU) in 119.04 s; fifteen retired field cases stay disabled.
- **Observation, not changed:** At full throttle into overlapping 1.8 m arcade crests the
  soak's player reached a 14.4 m ballistic peak far from the crossing boat. This is
  existing arcade handling, unrelated to the clock; gameplay tuning, if wanted, belongs to
  the boating plan, not C3.
- **Limits:** One machine; Windows/Linux/browser not run. Soak frames are CPU only. The frozen
  P2a oracle `tools/boat_drive_baseline.h` still restores only the derived `time` after a
  crossing-boat recovery; production restores the tick. It remains an untouched oracle; the
  calm-independence check uses the production driver. The benchmark CSV/profiler display
  print `Boat.time` as float (diagnostic only).
