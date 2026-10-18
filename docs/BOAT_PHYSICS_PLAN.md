# Boat physics: learnable handling

Status, 2026-10-18: A1–D2 implemented; Phase C player acceptance is recorded in the handoff
log. D1/D2 are opt-in basin tests; their handling/artistic play gate remains pending. Next
task: review neutral drift and crossing comfort, then tune if needed. E1/E2 remain optional
after Phase D acceptance. Calm-water handling and the accepted C3 wave baseline must be preserved.
The separate [04h harbor pass](HARBOR_BOATS.md) adds fixed berth collision envelopes and
kinematic moored-boat wave poses. [Travelling harbor vessels](HARBOR_TRAFFIC.md) add
player contacts using obstacle-relative velocity and visual foam wakes. Those harbor
passes remain kinematic and their foam adds no physical wake. The separate D1/D2 basin
source below supplies the new physical test.

Accepted direction: 2026-10-17. Parent design: [Boating playground](BOATING_PLAN.md).
Public controls: [README](../README.md#boating-sketch). [All plans](README.md).

## Experience to build toward

Leave Eriksberg carefully, open the throttle, feel the boat climb onto plane, ease off for a passing
wake, then come alongside a small island jetty. The handling should make that trip satisfying even
without missions or scoring. Island scenery is a separate project, not a physics prerequisite.

Feeling in control means understanding and anticipating the response, not eliminating inertia.
Prefer a believable, tunable game model over an engineering simulator.

- Calm water stays genuinely calm and remains available for practice and regression tests.
- Visible waves and their physical effects agree; avoid unexplained random impulses.
- Slowing down usually makes rough conditions easier to manage.
- Keep the camera steadier than the hull, with a stable horizon.
- Mistakes initially cost speed or another approach, not a capsize or reset.
- Use posture, wake, spray, and eventually sound to explain changes in handling.
- Add controls only when they offer useful choices. Automatic trim precedes manual trim.
- Do not change the main demo, soundtrack timing, or integer-scaled presentation.

## Priorities and estimates

Subjective 1–5 ratings: realism is added believability; fun is potential for satisfying control;
difficulty includes integration and tuning, not just initial code. These are not delivery estimates.

| Feature | Realism | Fun | Difficulty |
| --- | --- | --- | --- |
| Reverse, neutral, engine response | 4 | 5 | 2 |
| Planing transition | 5 | 5 | 3 |
| Buoyancy, pitch, roll | 5 | 4 | 3 |
| Waves affecting handling | 5 | 5 | 4 |
| Spatial currents | 4 | 4 | 3 |
| Wind and gusts | 4 | 3 | 3 |
| Crossable wakes | 4 | 5 | 4 |
| Engine trim and weight distribution | 5 | 4 | 4 |
| Hull-aware collisions and forgiving fenders | 4 | 5 | 3 |
| Propeller ventilation, severe instability, capsizing | 5 | 2 | 5 |
| Fully coupled fluid simulation | 5, if calibrated | 2 | 5+ |

Wind, propulsion failures, capsizing, and full fluid simulation are deferred, not scheduled tasks.

## Working contract for implementation agents

Implement only the first unchecked task unless the user names another. Read AGENTS.md, this plan,
and the relevant current source before editing. No renderer rewrite, new physics dependency, map
expansion, or unrelated cleanup. If a task proves too large, split it here rather than expanding scope.

Likely entry points (verify against the current tree):

- `src/boat.c`, `src/boat.h`: fixed-step simulation, CPU boat rendering, shared GPU player snapshot.
- `src/boat_map.c`: navigable water and shoreline boundaries.
- `src/boat_gpu.c`, `src/renderer_gpu.c`, `src/waterfront_scene.h`: GPU integration.
- `tools/boat_study.c`: input, HUD, deterministic preview and capture options.
- `tools/validate_boat.c`: handling regression tests; `CMakeLists.txt`: test registration.
- `README.md`: public controls and launch commands.

Keep simulation deterministic at the existing 120 Hz. Express tuning constants in named groups.
Keep simulation state separate from presentation; CPU/GPU use the same physical pose and water
parameters. Do not silently replace existing steering or coasting behaviour. Flat-water, neutral,
and reset cases must stay well-defined. New passive resistance should dissipate energy relative to
the water; moving water and powered thrust can supply energy, so test these separately.

For each task: add targeted deterministic tests, build, run CTest, and update affected documentation.
For visual/input changes, capture representative frames and run the interactive demo, including
Eriksberg GPU+AA where applicable. Report checks that could not run. Mark a task complete only after
its acceptance checks pass; record files changed, tests, tuning choices, and remaining limitations.
Do not commit unless asked. Stop for player feedback at the play gates before adding complexity.

Reusable prompt for a smaller coding model:

> Read AGENTS.md and docs/BOAT_PHYSICS_PLAN.md. Implement only task [ID]. Preserve existing unrelated
> changes. Follow its scope, acceptance checks, and the working contract. Do not start later tasks.
> Update the task status and handoff notes, and report verification and limitations. Do not commit.

## Phase A — Reverse and docking

- [x] **A1: Signed propulsion in the simulation.** Add a bounded reverse command and a small,
  deterministic engine-response state. Keep thrust applied at the stern; neutral removes powered
  thrust while existing coasting resistance remains. Do not add input/UI or docking geometry yet.
  Acceptance: forward and reverse impulses and steering torque have the expected signs; idle cannot
  turn a stationary boat; commands saturate; engine response and reset are deterministic; sustained
  reverse stays finite and bounded. Retain current forward-handling tests.
- [x] **A2: Controls and readable feedback.** Use Up/Down for held signed thrust, with neutral at
  zero and reverse below it. Show signed thrust and engine response separately from speed. Update
  the deterministic preview to demonstrate stopping and backing up. Acceptance: releasing input
  never changes direction; held Down passes smoothly through neutral before selecting reverse;
  reverse steering and wash match stern thrust; focus loss pauses; reset returns to neutral;
  CPU/GPU agree.
- [x] **A3: Hull-aware shoreline contact.** Replace or refine the circular clearance with a simple
  oriented hull footprint, reusing existing water boundaries. Use damped, forgiving contacts and
  contact torque; do not add damage or overhead-structure collisions. Acceptance: bow/stern clearance
  across several headings, glancing slides, bounded rebound, no energy gain against stationary
  walls, and no escape through shore or map edges at the tested maximum speed.

Play gate A: approach an existing quay, stop, reverse, and come alongside without fighting the
controls. Use the basin walls for repeatable practice. Tune before continuing.

## Phase B — Planing

- [x] **B1: Smooth handling transition.** Introduce a bounded planing state driven by forward
  water-relative speed, with smooth response and separate enter/exit behaviour if needed to avoid
  chatter. Blend deliberately tuned resistance/lift-response parameters; this is an approximation,
  not a claim of calibrated hydrodynamics. No waves or full vertical dynamics yet. Acceptance:
  no discontinuous acceleration, no rapid toggling near the transition, no reverse planing, neutral
  coasting does not create energy, and speed stays bounded. Preserve low-speed manoeuvrability.
- [x] **B2: Explain the transition visually.** Add bow rise followed by settling, and blend wake
  and spray with the planing state. Initially the vertical attitude may be presentation-only; keep
  that approximation explicit and isolated for C2 to replace. Apply the same pose to hull, cabin,
  engine, attachments, reflections, and shadows. Acceptance: CPU/GPU captures at displacement,
  transition, and cruise; no detached engine; slowing down returns smoothly to rest attitude;
  camera does not copy pitch/roll. Engine audio remains a separate optional task.

Play gate B: accelerating into open water feels rewarding and understandable, not like a hidden
gear shift. Confirm docking still works.

## Phase C — Buoyancy and gentle waves

- [x] **C1: Shared water query.** Add a small deterministic water API exposing height, surface
  orientation, and water velocity at a position and simulation time. Default to perfectly flat,
  still water. Add opt-in analytic waves and matching rendered surfaces for both renderers;
  do not apply it to the boat yet. Keep shared parameters/time explicit, not independent CPU/GPU
  tuning. Acceptance: known sample points, finite normals/velocities, flat-mode regression, and
  visual/physical crests aligned. Scope water rendering to the playable area if necessary and
  document approximations; do not redesign the entire waterfront renderer.
- [x] **C2: Floating hull.** Add vertical motion, pitch, roll, inertia, and several hull buoyancy
  samples with damping. Replace B2's presentation-only attitude with physical state; approximate
  planing lift must not be double-counted. Keep the existing horizontal/yaw model initially.
  Acceptance: stable rest, recovery from a small tilt/drop, no unforced growth of oscillations,
  finite long runs, reset, and correct engine/particle origins as the hull moves.
- [x] **C3: Playable gentle waves.** Couple local water-relative motion to resistance and tune
  encounter response by speed and heading. Use the gentle crossing-wave preset, not storms. Acceptance:
  repeatable head-on/beam/following encounters; reducing speed provides a manageable response;
  flat mode preserves calm handling; spray and wake remain attached to the water/hull correctly.
  Document simplified effects rather than adding destabilizing impulses to imitate realism.

Implemented C1 revision, accepted after visual feedback: three smaller crossing components,
subdued shading, and restrained GPU reflections replaced the single long swell and exaggerated
color bands. It was implemented together with C2 to assess the surface with a floating hull.

Play gate C: small waves are enjoyable to meet at different angles, and the camera remains calm.
Stronger waves and triangle-based buoyancy are optional upgrades only if this model proves limiting.

## Phase D — Currents and crossing wakes

- [x] **D1: Spatial current.** Add a smooth, opt-in current field through the water API. Evaluate
  hull/outboard resistance using local water-relative velocities, not ground speed; distinguish the
  two in debugging output. Start with an authored test channel, not claimed real Gothenburg currents.
  Acceptance: neutral drift, smooth field boundaries, uniform-current equivalence to moving through
  still water, deterministic routes, shoreline containment, and unchanged zero-current handling.
- [x] **D2: One crossable wake.** Add a deterministic moving wake source to the test area, with
  matching visible and physical waves and bounded lifetime/amplitude. A scripted source is enough;
  traffic AI and full fluid coupling are out of scope. Acceptance: repeatable crossing at several
  angles/speeds, clear approach cues, stable recovery, and no enduring energy growth after it passes.

Implementation: `--current` and `--crossing-wake` in basin free boating, independently or
combined with `--waves`. Zero initialization still means flat, still water. The current
API supports uniform flow or a compact polynomial channel with zero slope at its edges;
the authored preset peaks at (0.85, -0.25) units/s, centred on simulation X=0 with a 32 m
half-width. Axial drag/planing use centre water-relative speed; lateral hull stations and
the outboard sample their own local flow. Surface foam drifts with the flow, and spray
load, HUD water speed, and water audio use water-relative travel. Ground velocity remains
the navigation/contact velocity. Material vertical water velocity includes current travel
along the local slope as well as the explicit time derivative.

The scripted source moves from simulation (-24, -8) to (24, -8) at 4 units/s during the
first 12 seconds. It emits an outward-travelling compact wave packet: 0.38 m amplitude
bound, 6 m wavelength, 2.4 units/s spreading speed, 5 m packet half-width, and 12 s local
lifetime. Smooth sine-squared source/age windows and a cubic compact packet envelope
bound height and derivatives; the whole wake is exactly zero after 24 seconds. Analytic
height, normal, and velocity drive the existing six-support buoyancy/normal resistance.
The same packet is evaluated in CPU water geometry and GPU displacement/normal shading;
pale crests and the visible source provide approach cues. Source boxes are shared between
renderers; they are a non-contact scripted cue, with no navigation or fluid coupling.

```sh
./build/boat_study --current
./build/boat_study --crossing-wake --gpu-aa
./build/boat_study --current --crossing-wake --waves --gpu-aa --demo
./build/boat_study --renderer cpu --crossing-wake --time 9 --screenshot build/wake-crossing.png
```

Reset preserves the options and restarts time. The test demo waits six seconds, approaches
at 65% thrust, and returns to neutral after ten seconds; screenshot time uses that script.
The flags reject mapped locations and the course/replay/ghost modes, preserving existing
course comparison and saved replay formats. Real harbor currents and physical wakes from
the existing traveling fleet remain future work. No ambient-wave preset or main-demo
arrangement was changed.

Verification record: see the D1/D2 handoff entry below. Implementation checks do not mark
this play gate accepted; continuous manual handling/artistic review is next.

Play gate D: currents reward anticipation and wakes reward choosing speed/angle; neither feels like
an invisible punishment. Keep a calm-water option and do not make exposure mandatory for exploration.

## Phase E — Optional trim and loading

- [ ] **E1: Automatic trim and loading experiments.** Only after phases A–D feel good, try a few
  fixed mass/centre-of-mass presets and bounded automatic trim. Acceptance: stable rest and planing
  across presets, visible but recoverable handling differences, and no new mandatory controls.
- [ ] **E2: Optional manual trim.** Add only if E1 exposes a genuinely enjoyable tradeoff between
  acceleration, cruise, and wave comfort. Keep automatic mode as the default and show the setting.
  Acceptance: manual control is understandable without specialist knowledge; returning to automatic
  is smooth. If playtesting finds no useful choice, explicitly defer this task.

## Reference approaches

These motivate models, not exact coefficients or a requirement to adopt another engine:

- [Hydrodynamic Lift on Boats](https://arxiv.org/abs/1808.03313): planing and hydrodynamic support.
- [Habrador boat resistance implementation](https://www.habrador.com/tutorials/unity-boat-tutorial/5-resistance-forces/):
  an example of decomposed buoyancy/resistance forces and a possible later triangle-based approach.

## Handoff log

- 2026-10-18: D1/D2 implemented as the authored basin profiles above. Added
  `tools/validate_boat_environment.c`: smooth compact field boundaries, uniform-current
  equivalence through steering, neutral drift, basin containment, bounded wake amplitude,
  finite-difference spatial/time derivatives (including current travel along slopes),
  deterministic repeat crossings at 0/3/9 units/s and 0/45/90-degree headings, and stable
  recovery after the source passes. A production-shader readback probe checks GPU vertex
  height and fragment height/slopes against the water API at 165 points/times. CLI tests
  exercise separate/combined presets and reject courses/mapped locations.
  On macOS, Debug configure/build completed without compiler warnings (CMake retains
  raylib's existing macOS OpenGL deprecation notice). All 62 CTests passed, including
  16 desktop GPU tests. A `--current --crossing-wake --waves --gpu-aa --demo --frames 900`
  desktop playback smoke run exited successfully. This is scripted playback, not human
  handling acceptance. `git diff --check` and changed documentation links passed.
  Calm-profile regression: 160 CPU frames are byte-identical to the pre-D stern-wake
  baseline across cruise, turn, coast, and reverse. Native 320x200 CPU frames and 10 Hz
  GPU+AA passage sequences are under `build/phase-d/`; reviewed wake-only and combined
  current/ambient-wave frames before/during/after crossing. `wake-passage.mp4` and
  `combined-passage.mp4` show 24 s of motion; `review.c` and its executable preserve the
  local capture driver. The wake-only script ended at height 1.7e-8 m and negligible
  pitch/roll at 24 s; its peak hull angle, including acceleration/planing, was 0.228 rad.
  The combined script retains the expected ongoing ambient-wave motion after the wake. Manual continuous handling/artistic acceptance is pending;
  the source is a bounded non-contact test cue and real harbor flow remains unimplemented.


Dated records describe each step at the time it landed. In particular, B2's cosmetic
`visual_pitch` was replaced by C2's physical support model, and the older B/C play-gate
notes are superseded by the C3 acceptance below. Test counts refer to those revisions.

- 2026-10-17: C3 implementation adds local normal resistance at the six supports in
  `src/boat.[ch]` and `src/boat_water.[ch]`. It replaces wave-mode vertical damping with a
  dissipative 3D normal force using point velocity (translation plus pitch/roll/yaw), local
  analytic normal, and water surface velocity. Generalized torques apply the same force to
  angular motion; there are no random impacts or additional spring/lift terms. Exit damping
  is capped by available support and dry stations exert no resistance. Total damping remains
  13 with at most 35% gain by 12 units/s. Flat mode retains the accepted C2 calculation.
  A smoothed bow-entry load (1.2 units/s scale, 8/s response) modulates spray and wake width;
  foam now aligns to the sampled surface normal through one shared CPU/GPU pose helper.
  Spray inherits the exact local attachment velocity. The wave preset, rendering, camera, controls,
  engine steering, and original horizontal drag/skeg terms are unchanged. Orbital flow/current,
  hull-pressure simulation, ventilation, and capsize behavior remain out of scope.
  `tools/validate_boat.c` tests negative relative work and dry/exit limits separately from energy
  transfer by moving water; controlled 2/12-unit/s head-on, beam and following encounters;
  repeatability, lower slow-speed angular motion/vertical acceleration/spray load; particle
  surface alignment; shoreline containment; bounded neutral drift; and a recorded C2 calm-water
  forward/turn/reverse trace. Existing long-run and buoyancy recovery tests remain enabled.
  Debug configure/build and all 27 CTests passed, including GPU review and wave capture.
  Inspected close CPU basin/Eriksberg and Eriksberg GPU+AA captures at transition/cruise
  (`build/boat-c3-*.png`); both CPU and GPU+AA windowed demo smoke runs passed.
  Controlled 2-to-12-unit/s encounters measured angular-rate RMS rising from 0.155 to 0.303
  rad/s head-on, 0.145 to 0.261 beam-on, and 0.061 to 0.134 following. Vertical acceleration
  and bow-entry load also decrease at the lower tested speed for all three headings.
  These are tuning checks, not calibrated sea-state measurements. Play gate C accepted after the user reported that C3 feels and looks great.
  Preserve this baseline when starting Phase D.

- 2026-10-17: Revised C1 and completed C2 after the user approved improving the surface and
  adding buoyancy together. `boat_water.[ch]` now superposes three components: amplitude/wavelength/
  angular-speed 0.13/16/1.96, 0.06/9/2.62, 0.025/5.5/3.35, travelling along (0.6,-0.8),
  (-0.8,-0.6), (0.96,0.28). CPU shading is subdued; shared two-unit tessellation resolves the
  dominant waves (tested centroid height error <0.035). GPU uploads water topology once and uses
  the explicit snapshot parameters/time in `waterfront_water.vs/fs`; analytic fragment normals
  drive restrained reflection distortion, without the old independent shader ripples in wave mode.
  Planar reflection source and water shadows are restored. Flat/timeline shader behavior remains.
  `boat.[ch]` replaces `visual_pitch` with heave/pitch/roll and velocities. Six unilateral damped
  supports use gravity 9.81, total stiffness 64, damping 13, pitch/roll inertia 2/0.45 at 120 Hz.
  Dry stations provide no buoyancy/damping. The previous 8-degree transition/3-degree cruise trim
  now redistributes support, with at most 0.05 units of planing rise; no second attitude is added.
  The horizontal/yaw model remains unchanged. Attachments, reflections/shadows and spray follow
  the rigid hull pose; foam follows local wave height and spray expires at the water surface.
  Tests cover exact calm rest, dry drop/submerged and tilt recovery, bounded passive energy,
  slope response signs, rigid attachments through 16 headings, a level camera, repeatable
  encounters at 0/5/10 speed and four headings, a five-minute powered/reversing wave run, reset,
  and foam surface attachment. C3 still owns horizontal encounter resistance and comfort tuning.
  Debug configure/build and all 27 CTests passed, including a new GPU wave capture and the
  existing GPU review/material checks. Inspected close CPU/GPU+AA captures at rest, transition,
  and cruise (`build/boat-c2-*.png`, `build/boat-wave-gpu.png`). CPU basin and Eriksberg GPU+AA
  windowed demo smoke runs passed. Subjective comfort and enjoyment still need player feedback;
  CPU water has no scene reflections, GPU reflection projection remains planar, and mesh heights
  between vertices approximate the analytic surface.

- 2026-10-17: C1 complete following the user's request to start Phase C. Added
  `src/boat_water.[ch]`: explicit analytic height, unit normal, and vertical surface velocity
  at simulation X/Z and time; zero settings return exact flat/still water. The opt-in `--waves`
  preset is amplitude 0.22, wavelength 48, period 4 seconds, travelling along (0.6, -0.8).
  CPU and GPU consume the same tessellated surface and exaggerated slope colors. Eriksberg
  reuses imported water triangles, preserving shoreline coverage; the basin uses its rectangle.
  Longest-edge subdivision targets four-unit edges; tested centroid height error stays below
  0.012 units. Both paths reflect simulation X once. GPU keeps a dynamic vertex/color buffer;
  wave mode deliberately omits the old independent ripple/reflection/shadow water material.
  Flat mode retains the previous rendering and handling. Wave query velocity is surface motion,
  not an orbital-flow/current model; no wave forces or hull/foam following are applied until
  C2/C3, so intersections remain visible. Reset restarts time and preserves the CLI wave option.
  Updated `boat.[ch]`, `boat_gpu.[ch]`, the CPU boating scenery entry point, study CLI,
  CMake, README, boating notes, and the validator. Tests cover known crests/travel, normal and
  time derivatives, finite long sampling, mesh/query alignment at both locations, reset, and
  exact horizontal/attitude equivalence with waves selected. Debug build and all 26 CTests
  passed, including GPU review; inspected basin/Eriksberg CPU and Eriksberg GPU+AA captures
  (`build/boat-c1-*.png`) and completed short CPU/GPU+AA windowed demo smoke runs.
  C2 is next; player assessment of buoyancy and wave encounters waits for C2/C3.

- 2026-10-17: A1 complete. Added bounded reverse propulsion and signed, rate-limited
  `engine_response` in `src/boat.[ch]`; thrust and wash use the response at the stern.
  Before A2, keyboard throttle remained forward-only. `validate_boat` covers forward/reverse signs,
  saturation, deterministic response/reset, and a sustained reverse run. A2 adds player controls
  and feedback.
- 2026-10-17: A2 complete. Replaced the proposed selector with held signed thrust in `src/boat.[ch]`:
  Up/W moves toward forward and Down/S moves through neutral into reverse. The HUD shows signed
  thrust and engine response, while the deterministic preview demonstrates the transition.
  `validate_boat` covers the signed range, reversal, saturation, and reset; CPU/GPU continue to
  consume the same `Boat` pose through `boat_scene_frame`. Focus loss retains the existing
  fixed-tick pause.
- 2026-10-17: A3 complete. Replaced the centre-point clearance with six oriented bow, stern, and
  beam samples. `boat_map` returns the nearest shoreline normal; contact applies a damped impulse
  and the associated torque, then preserves the last safe pose. The same footprint contains basin
  walls. `validate_boat` covers a glancing rebound and all eight cardinal/intercardinal headings;
  Eriksberg runs retain shoreline and map-crop containment. Contacts remain a deliberately simple
  footprint, without quay geometry above the waterline or damage.
- 2026-10-17: B1 complete. Added a bounded, rate-limited `planing` blend in `src/boat.[ch]`, driven
  only by forward speed. Separate 6.5/5.0 unit-per-second enter/exit thresholds prevent chatter;
  the blend reduces the existing longitudinal resistance to 62% at full plane. This remains a
  horizontal handling approximation: no visual attitude or vertical lift has been added. The
  deterministic boat validator covers smooth entry, neutral energy decay, planing decay, and the
  reverse exclusion; README and boating notes describe the limitation.
- 2026-10-17: B2 complete. `src/boat.[ch]` adds isolated `visual_pitch` at 120 Hz: an
  8-degree transition hump plus a 3-degree cruise target, smoothed with a 5/s exponential response.
  No physical lift or horizontal forces were added. A shared pose helper rotates cabin/outboard
  offsets with the hull and composes local engine steering; CPU rendering and the GPU snapshot
  use it, so GPU reflection/shadow passes inherit the same pose. The camera stays independent.
  Planing blends wake spread/size/lifetime and adds symmetric spray; slip-driven turning spray
  remains. Foam stays on the water while spray originates at a hull attachment.
  `tools/boat_study.c` now holds straight cruise before turning at 3s and lowering thrust at 4s;
  README and boating notes document the visual approximation and revised preview.
  `tools/validate_boat.c` covers transition rise/cruise settling, smooth return to rest/reset,
  attachment alignment through 16 headings (including Euler singularities), planing spray,
  camera stability, and independence from horizontal handling. Debug configure/build and all
  24 CTests passed, including GPU review. Inspected CPU and Eriksberg GPU+AA captures at
  0.5/1.8/2.9s (`build/boat-b2-cpu-*.png` and `build/boat-b2-gpu-*.png`); CPU and Eriksberg GPU+AA
  windowed demo smoke runs also passed. Attitude remains cosmetic until C2; no engine audio.
  Play gate B still needs player feedback on acceleration and docking before starting C1.
- 2026-10-17: Plan saved. Control recommendations and coefficients remain proposals until their
  respective implementation step. Each play gate needs user feedback before starting the next phase.
