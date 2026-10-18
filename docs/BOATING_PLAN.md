# Gothenburg boating playground

Status: updated 2026-10-18. `boat_study` implements free boating in basin, Eriksberg,
and river locations, reverse/docking, planing, buoyancy, optional gentle waves, and a timed
S course with recordings, persistent personal bests, ghosts, and replay controls. The study
is silent. The score-loop arcade mode and archipelago remain proposals.

This document owns gameplay direction. Detailed next tasks belong to the
[physics plan](BOAT_PHYSICS_PLAN.md), [living-city plan](LIVING_CITY_PLAN.md), and
[river expansion record](eriksberg/bridge/README.md). See the [plan index](README.md)
for status across tracks. Revise the design as we discover what is enjoyable to play.

## Current prototype

`--location river` extends the geographic area to Älvsborgsbron, using the same dock spawn
and boat physics. A shared CPU/GPU blockout includes the deck, open towers, main cables,
and hangers. The four tower-leg footprints obstruct navigation while the deck stays above
the navigable water. Its default follow view is 32 m back / 6 m high; C restores that view.
The original Eriksberg crop and main demo remain the defaults for their existing modes.
See the [bridge expansion record](eriksberg/bridge/README.md) for assumptions and route checks.

`--location eriksberg` selects the bundled geographic waterfront; `--location basin` selects the
physics test area (the default). The Eriksberg spawn is in the dock basin facing the crane and river.
Buildings, shore, bridge, crane and sky reuse the bundled offline scene. The held controls, follow
camera, wake, and spray work in either location; R returns to the selected location's spawn.
Water-triangle boundary edges constrain an oriented bow, stern, and beam footprint at the shoreline
and map crop. Glancing impacts scrub speed, turn the craft away, and rebound gently. Eriksberg
supports `--gpu --gpu-aa`: the player hull, engine and airborne
spray participate in depth, reflection and shadow passes; surface foam is depth-tested and receives
shadows without casting solid shadows. Optional AA uses 4×4 supersampling before integer scaling.
GPU is the default for all locations, including the basin course. `--renderer cpu` selects the
software renderer for window-free captures; GPU captures need a desktop graphics context.
`--gpu-aa` only enables supersampling and does not select a renderer or location.
Buildings and quays remain static, with no overhead structure collisions or camera
occlusion handling near buildings. It does not include the
rest of Gothenburg or the archipelago. Source attribution is retained in the HUD and README.

The [generated boat and berth foundation](HARBOR_BOATS.md) adds six leisure boats at a
west-bank pontoon and a working east-bank berth in Eriksberg and river locations. Moored
hulls, pontoon fingers, and gangway have forgiving static contacts. The visual generator
covers skiff through speedboat and passenger-ferry recipes; it does not change the player craft or implement
speed tiers, unlocks, prices, or progression saves. Those remain future design work.

Three travelling leisure craft and a compact ferry now follow authored dock/river circuits
in both geographic locations. [Harbor traffic](HARBOR_TRAFFIC.md) documents continuous
routes, visual wakes, lights, and moving hull contacts. Traffic follows fixed schedules;
it does not yield or navigate dynamically. Its wakes do not affect handling. The basin
course and replay baseline are unchanged.

The boat-level [water review](WATER_REVIEW.md) records a repeatable dock/river/quay circuit
and the completed 04f surface and 04g selected waterline passes. Those visual changes
preserve handling; the separately implemented river expansion adds playable geography.

Run `./build/boat_study`. Left/right or A/D adjust the wheel, clamped from full
left to full right. Centre to either limit takes one second. Releasing keys lets an overdamped
spring return the wheel smoothly to centre without overshooting; opposite input brings it back
through centre. Return strength is set by `WHEEL_RETURN_RATE` in `src/boat.c` (initially 3/s,
about 1.5 seconds from full lock to within 1% of centre). A rotating wheel and left/right percentage show
the setting. Up/W raises signed thrust and down/S lowers it; releasing the keys holds the setting.
The thrust range is -100% (reverse) through 0% (idle) to 100% (forward), with four seconds of
held input covering the full range. A signed percentage and centred bar show the setting
independently of speed, and the HUD shows actual signed engine response separately. Lowering
thrust passes smoothly through idle and continues into reverse only while Down/S remains held.
R resets to neutral thrust, centres the wheel, and clears effects; Esc exits.
The silent handling sketch has a third-person follow camera, stern-applied engine thrust, rotational
inertia, water resistance at two hull stations, and forgiving basin-wall collisions. Steering
angles the outboard; its lateral thrust pushes the stern and creates torque. The outboard follows
the wheel as it springs back when keys are released. Returning the wheel to centre lets rotation settle
through water resistance. Countersteering can catch
a turn. At idle there is no powered steering, and a stationary boat cannot turn. Propeller wash
moves opposite the thrust direction, independently of the spreading wake. The model uses normalized
mass and tunable forces, not calibrated hydrodynamics. Forward drag combines linear and quadratic
terms. Lateral resistance at each hull station increases with forward speed and local sideways
velocity, including rotation. A passive force across the angled submerged outboard uses the local
stern velocity and acts at the stern, producing both drag and turning torque. It works at idle
while moving and fades to zero with water-relative motion. Calm water remains the baseline. A
rate-limited, hysteretic planing blend reduces forward hull resistance above cruising speed and
returns smoothly to displacement handling while slowing. Six damped buoyancy supports provide
physical heave, pitch, and roll; planing redistributes support to raise the bow during the
transition and settle at cruise. Hull, cabin, and outboard share the physical pose in both
renderers, including GPU reflections and shadows. The camera keeps a level horizon.
`--waves` enables gentle crossing waves with water-relative resistance and matching visual
surfaces. Currents and crossable wakes from another vessel remain unimplemented.
A spreading, fading wake traces the travelled path; planing broadens it and adds symmetric spray
even on straight runs. Lateral slip throws turning spray from the sliding side of the hull. Two orange markers provide turning
references but do not collide. Rendering is 320x200 with nearest-neighbour integer scaling. Losing
focus pauses movement.

The simulation uses fixed 120 Hz ticks. Handling constants live in `src/boat.c`. Screenshot mode
runs a deterministic example without a graphics context when using CPU rendering; GPU captures
require a desktop graphics context. See the README for commands.
The 240x160-world-unit basin is a handling test area, not yet a representation of Gothenburg.
It is four times wider and deeper than the original basin. The camera follows the boat from
20 units behind and 14 units above, looking 5 units ahead. Its heading eases through turns on fixed
simulation ticks, including angle wraparound. `-`/`+` adjust distance (8..48), PageUp/PageDown adjust
height (4..36), and `C` restores the default view. The HUD shows both values; `R` resets the boat
without changing them. Settings are per-run; `--camera-distance` and `--camera-height` also configure
interactive runs and screenshots. Eriksberg geographical scenery is implemented;
collision splashes belong to later stages. Player evaluation of handling is the
next design input: especially steering responsiveness, the amount of slide, and coasting into a turn.

## Direction

Turn Proteus's waterfront into a small interactive boating playground. The central appeal is
being able to boat around Gothenburg and its surrounding archipelago: moving through a place,
finding passages, carving turns, and enjoying the water.

Start with a satisfying water-skimming toy. A small arcade game can grow from that foundation.
Free exploration is a first-class experience; racing is an optional reason to learn the handling
and revisit routes. Neither a detailed boat simulator nor a large open world is required.

Keep the demo spirit through low-resolution visuals, tracker music, bold effects, and expressive
motion. Player actions give those effects a purpose: wakes describe movement, spray communicates
sliding, and the waterfront becomes somewhere to go.

## Setting and scale

The longer-term setting is Gothenburg's waterfront and nearby archipelago. Begin with one compact
area inspired by the existing Eriksberg scene, with enough open water for sweeping turns and a few
obstacles or passages to explore. Reuse existing scenery where practical.

Prefer a small, recognisable interpretation over exhaustive geographic accuracy. The degree of
real-world fidelity remains open. Do not make map imports or modelling the entire archipelago a
prerequisite for playing.

If the first area works, explore a small island area next: rocky shores, narrow channels, sheltered
water, and a few distinctive landmarks. Decide later whether areas connect continuously or are
selected separately. An enjoyable harbour and one island route would already be a meaningful game.

## First playable baseline

- One craft, one compact area, free driving, and instant reset.
- Left/right adjust a spring-centred wheel; up/down adjust a persistent throttle within
  -100..100%; a separate reset key centres the wheel and idles the throttle.
- Momentum persists through turns. Stern thrust creates torque, and rotation builds with inertia.
  Water resistance along the hull damps sliding and rotation.
- Throttle affects both forward push and turning force. Reducing it cuts thrust; momentum remains.
  Defer a dedicated brake until play suggests it is needed.
- Shore contacts cause forgiving rebounds; buoys are non-colliding markers. Collision splashes
  remain a presentation proposal.
- Follow the boat from behind and above, with a stable horizon and gently smoothed heading.

The first success criterion: driving in circles and carving around a buoy is enjoyable for a minute,
even without a score. Tune movement before adding content if this does not hold.

## First timed course

`./build/boat_study --course` selects a basin-only S course. The boat spawns south of a
start line; a forward crossing between the posts starts the timer. Two ordered, offset buoy
gates require broad turns before a chequered finish line stops the clock. Yellow marks the
next gate, and a HUD route map shows progress. Gates use swept boat-centre crossings toward
north, with interpolated start/finish times on fixed simulation ticks. Skipping a gate cannot
finish a run. Missed gates can be retried while the clock continues. Markers do not collide.
R resets both the boat and attempt; focus loss pauses both. Free boating is still the default.
Completed runs are recorded as poses at 120 Hz with interpolated START/FINISH samples and gate
splits. The fastest run persists across launches and becomes a non-colliding cyan ghost on retry;
calm water and gentle waves have separate bests. G toggles the ghost. Before START or after FINISH,
P/B replays the last/best run; Space pauses, Enter restarts, and R returns to racing. A retry discards
only the unfinished recording. Replay omits water particles and allows the usual camera controls. Left/Right seek one second
(Shift: five seconds), Home/End jump to START/FINISH, and brackets select 0.25× through 4× playback.
A clickable timeline marks gate splits; clicking/dragging seeks and pauses. Finish pauses playback;
Space starts again from the beginning. Seeking and speed changes preserve the recording and best.
Portable, versioned run files can be exported with `--save-run`, watched with `--replay`, or selected
as a ghost with `--ghost`; see the [README controls and storage notes](../README.md#boating-sketch).
There is one saved best per water preset; a broader leaderboard is deferred. This small time trial
was implemented ahead of the proposed minute-long scoring mode and remaining physics experiments.
The time-trial foundation is now in place; it is not an outstanding prerequisite for physics D1.

## Optional arcade challenge

Deferred proposal: a score-loop alternative to the implemented S time trial. It would add
five or six ordered buoy gates forming a short loop. Clearly highlight the next gate. Include a broad sweeping turn, a tighter corner, and an acceleration straight.

The proposed challenge is a 60-second score run:

- Passing gates in order earns points.
- Completing a lap earns a bonus.
- Keep looping until time expires, then show the score and allow immediate restart.
- Keep the best score in memory initially.

Exact scoring values are tuning decisions. No opponents, enemy AI, or complicated combo system are
needed. Free boating remains available without a timer or mandatory gates.

## Presentation and feedback

The current craft, wake, and turning/planing spray provide visual movement feedback.
The table below is a presentation wishlist, not a list of implemented effects: the study has
no audio, buoy impacts, gate sounds, score laps, or impact camera kick. Preserve
nearest-neighbour, integer-scaled presentation while refining feedback.

| Player action | Feedback |
| --- | --- |
| Accelerate | Wake lengthens; engine pitch rises |
| Turn hard | Spray fans out from the craft |
| Bump a buoy or shore | Brief splash, impact sound, and restrained camera kick |
| Pass a gate | Buoys flash and a short musical sound plays |
| Complete a lap | A larger ripple or colour pulse crosses the water |

Steering and gameplay feedback stay immediate. Music may drive decorative pulses and motion;
gate sounds should fit the track. This does not require rhythm-game timing. Explore whether quieter
audio suits free boating once the basic experience exists.

## Implementation stages

These stages describe the design sequence; current implementation is recorded explicitly below.

1. **Handling sketch (implemented):** simple craft, water, steering, thrust, momentum,
   boundaries, and reset.
   Evaluate the pleasure and readability of movement; keep this cheap to revise.
2. **Water toy (implemented except buoy contacts and collision splashes):** wake, turning
   spray, forgiving bumps, and a few buoys in a compact waterfront area.
   Evaluate whether movement feels expressive and whether cruising is enjoyable on its own.
3. **Timed challenge (S time trial implemented):** ordered gates, timer, fast restart, persistent
   bests, ghosts, and replay. The original minute-long score-loop remains deferred.
   Evaluate whether another attempt is appealing without adding progression rewards.
4. **First archipelago slice (proposed):** only after the core works, try a small island area
   and the camera needed to explore it. Evaluate the appeal of discovering routes and places.

The separate `boat_study` executable reuses the scene and rendering foundation. Its controls
and launch options live in the [README](../README.md#boating-sketch). Preserve the main demo
and its soundtrack timeline while developing this experimental direction.

## Later water-physics experiments

The accepted next-step roadmap is [Boat physics implementation plan](BOAT_PHYSICS_PLAN.md).
It breaks the ambitions below into independently testable tasks suitable for one coding-agent
session each. Continue those experiments in bounded steps; the initial timed S course is now available
as a separate mode. Do not implement the whole roadmap at once.

Keep calm water as a baseline for comparisons. The following summarizes the implemented
experiments and remaining direction; the physics plan owns task status and acceptance.
These are tunable gameplay models, not calibrated hydrodynamics.

- **Speed-dependent steering (first approximation implemented):** tune stern thrust, passive
  outboard resistance, and speed-dependent hull resistance through coasting turns and low-speed
  manoeuvring. The current model uses forces and torque, not a prescribed speed-to-turn curve.
- **Planing (implemented):** a smooth resistance transition and physical support redistribution
  raise the bow, then settle it into cruise.
- **Current (D1, pending):** introduce drift that makes routes through channels and approaches
  to shore matter.
- **Gentle waves (C1–C3, implemented and accepted):** physical buoyancy and encounter resistance
  depend on speed and heading. Visual shelter/exposure varies independently of the wave physics.
- **Crossable wake (D2, pending):** one deterministic moving wake with matching rendering and forces.

Judge each experiment through driving before combining it with the others. Wake and spray should
help reveal movement through water as these models evolve.

## Scope boundaries

The first complete game can be one arena, one craft, one course, one music track, free driving, and
one score mode. The archipelago is the direction for expansion, not a requirement to build a vast map.

Defer upgrades, unlocks, economy, building systems, missions, multiplayer, procedural terrain,
a level editor, full fluid simulation, and broader progression saves. Course personal bests
and portable run files already persist. Incremental boat physics follows the
separate physics plan rather than being a prerequisite for a complete game. Jumps and boost wait until ordinary
turning is satisfying, since they would materially change course design.

## Questions to revisit through play

- How much sliding feels playful while still feeling like a boat?
- Does relaxed exploration or fast gate running become the stronger experience?
- Does the implemented Eriksberg-to-Älvsborgsbron area offer enough useful routes and landmarks?
- How much geographic fidelity contributes to the pleasure of being there?
- What follow-camera distance and angle best balance scenery and steering clarity?
- Can a few carefully shaped routes provide enough variety without more systems?

## Decision record

- 2026-10-18: Prioritize an inhabited harbor through two dressed berths. Build the moored
  fleet with shared parameterized boat recipes, laying a visual foundation for a possible
  later skiff-to-speedboat upgrade path. Keep gameplay progression and player handling changes
  separate from this first harbor step; preserve free exploration and the basin course.

- 2026-10-17: Prioritize learnable, predictable handling over exhaustive simulation. Adopt the
  [step-by-step physics roadmap](BOAT_PHYSICS_PLAN.md): reverse and docking, planing, buoyancy and
  gentle waves, currents and crossing wakes, then optional trim. Keep calm water, forgiving recovery,
  and stable camera behaviour. Break implementation into bounded tasks with tests and play gates.
- 2026-10-11: Adopt a small playable boating toy as the proposed successor direction for the demo,
  with handling first and an optional short arcade challenge.
- 2026-10-11: Make boating around Gothenburg and its surrounding archipelago a central aspiration.
  Keep the first prototype local and small, and preserve free exploration alongside timed play.
- 2026-10-11: Keep planing, refined speed-dependent steering, currents, and waves as future
  experiments. Implement wake and turning spray first to make the current movement easier to read.
- 2026-10-11: Replace direct heading control with steerable stern thrust, yaw inertia, and water
  resistance at bow/stern stations. Show the thrust direction through an outboard and propeller wash.
  Physical motivation: directed thrust in the [Coast Guard Boat Crew Handbook](https://rdept.cgaux.org/documents/BoatCrewHandbooks/BoatCrewBCH16114.4.pdf).
- 2026-10-13: Add nonlinear hull resistance and passive coasting steering from water crossing the
  angled outboard. Keep these forces dissipative, including their rotational work. The scripted
  preview now holds its wheel angle while lowering throttle to show an unpowered turning coast.
  Quadratic drag is motivated by the [standard fluid drag equation](https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/drag-equation/);
  coefficients remain gameplay tuning parameters, not measured properties of a specific boat.

### Playable gentle waves (physics C1–C3)

`--waves` opts into three deterministic crossing waves in all locations on CPU/GPU.
Flat water remains the default, and reset preserves the launch option while restarting wave time.
Six damped hull supports add physical heave, pitch and roll, including the planing attitude;
attachments share that pose while the follow camera stays level. Foam follows local water height.
GPU wave normals distort restrained planar reflections; CPU uses subdued slope colors.
Use `--camera-distance 12 --camera-height 6` for a closer view, and add `--demo` for a scripted run.
Local water-relative normal resistance now couples wave encounters to horizontal motion and all
three angular rates. Faster crossings increase the load and bow spray; slowing down softens the
response. Foam follows the local surface slope as well as height. Flat mode retains C2 handling.
This remains a support-spring/normal-damping model, without displaced hull volumes, orbital flow,
currents, ventilation, or capsize behavior. The user accepted Phase C after playtesting; preserve
this baseline for Phase D. See
[the physics plan](BOAT_PHYSICS_PLAN.md#phase-c--buoyancy-and-gentle-waves).

The GPU's boat-level water pass now uses a clipped near patch and a much smaller flat
far surface with analytic wave shading. In Eriksberg, shading shifts gradually from
quieter dock reflections to more broken river water. This is visual exposure only;
physical waves and existing recordings are unchanged. The CPU renderer retains the full
wave reference. Geometry, comparisons, and verification are in the
[04f water-surface record](WATER_REVIEW.md#04f-implementation-boat-level-surface-and-bounded-geometry).
