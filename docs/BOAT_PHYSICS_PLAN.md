# Boat physics: learnable handling

Accepted direction: 2026-10-17. Tasks are completed incrementally and recorded below.
Parent design and current controls: [Boating playground](BOATING_PLAN.md).

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
- [ ] **B2: Explain the transition visually.** Add bow rise followed by settling, and blend wake
  and spray with the planing state. Initially the vertical attitude may be presentation-only; keep
  that approximation explicit and isolated for C2 to replace. Apply the same pose to hull, cabin,
  engine, attachments, reflections, and shadows. Acceptance: CPU/GPU captures at displacement,
  transition, and cruise; no detached engine; slowing down returns smoothly to rest attitude;
  camera does not copy pitch/roll. Engine audio remains a separate optional task.

Play gate B: accelerating into open water feels rewarding and understandable, not like a hidden
gear shift. Confirm docking still works.

## Phase C — Buoyancy and gentle waves

- [ ] **C1: Shared water query.** Add a small deterministic water API exposing height, surface
  orientation, and water velocity at a position and simulation time. Default to perfectly flat,
  still water. Add a single opt-in analytic wave and matching rendered surface for both renderers;
  do not apply it to the boat yet. Keep shared parameters/time explicit, not independent CPU/GPU
  tuning. Acceptance: known sample points, finite normals/velocities, flat-mode regression, and
  visual/physical crests aligned. Scope water rendering to the playable area if necessary and
  document approximations; do not redesign the entire waterfront renderer.
- [ ] **C2: Floating hull.** Add vertical motion, pitch, roll, inertia, and several hull buoyancy
  samples with damping. Replace B2's presentation-only attitude with physical state; approximate
  planing lift must not be double-counted. Keep the existing horizontal/yaw model initially.
  Acceptance: stable rest, recovery from a small tilt/drop, no unforced growth of oscillations,
  finite long runs, reset, and correct engine/particle origins as the hull moves.
- [ ] **C3: Playable gentle waves.** Couple local water-relative motion to resistance and tune
  encounter response by speed and heading. Start with one gentle wave train, not storms. Acceptance:
  repeatable head-on/beam/following encounters; reducing speed provides a manageable response;
  flat mode preserves calm handling; spray and wake remain attached to the water/hull correctly.
  Document simplified effects rather than adding destabilizing impulses to imitate realism.

Play gate C: small waves are enjoyable to meet at different angles, and the camera remains calm.
Stronger waves and triangle-based buoyancy are optional upgrades only if this model proves limiting.

## Phase D — Currents and crossing wakes

- [ ] **D1: Spatial current.** Add a smooth, opt-in current field through the water API. Evaluate
  hull/outboard resistance using local water-relative velocities, not ground speed; distinguish the
  two in debugging output. Start with an authored test channel, not claimed real Gothenburg currents.
  Acceptance: neutral drift, smooth field boundaries, uniform-current equivalence to moving through
  still water, deterministic routes, shoreline containment, and unchanged zero-current handling.
- [ ] **D2: One crossable wake.** Add a deterministic moving wake source to the test area, with
  matching visible and physical waves and bounded lifetime/amplitude. A scripted source is enough;
  traffic AI and full fluid coupling are out of scope. Acceptance: repeatable crossing at several
  angles/speeds, clear approach cues, stable recovery, and no enduring energy growth after it passes.

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
- 2026-10-17: Plan saved. Control recommendations and coefficients remain proposals until their
  respective implementation step. Each play gate needs user feedback before starting the next phase.
