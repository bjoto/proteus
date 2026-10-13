# Gothenburg boating playground

Status: working design, saved 2026-10-11. Stage 1 and the wake/spray portion of stage 2 are implemented
as `boat_study`; other features remain proposals. Revise this plan as we discover what is enjoyable
to play.

## Current prototype

Run `./build/boat_study`. Left/right or A/D adjust a persistent wheel setting, clamped from full
left to full right. Centre to either limit takes one second. Releasing keys holds the wheel;
opposite input brings it back through centre. A rotating wheel and left/right percentage show
the setting. Up/W raises the throttle, down/S lowers it; releasing the keys holds the setting.
The throttle range is 0% (idle) to 100%, with two seconds of held
input covering the full range. A percentage and bar show the setting independently of speed.
Lower to idle to coast; reverse is not implemented. R resets the boat, centres the wheel, and idles
the throttle; Esc exits.
The silent handling sketch has a third-person follow camera, stern-applied engine thrust, rotational
inertia, water resistance at two hull stations, and forgiving basin-wall collisions. Steering
angles the outboard; its lateral thrust pushes the stern and creates torque. The outboard holds
the wheel setting when keys are released. Returning the wheel to centre lets rotation settle
through water resistance. Countersteering can catch
a turn. At idle there is no powered steering, and a stationary boat cannot turn. Propeller wash
moves opposite the thrust direction, independently of the spreading wake. The model uses normalized
mass and tunable forces, not calibrated hydrodynamics. Forward drag combines linear and quadratic
terms. Lateral resistance at each hull station increases with forward speed and local sideways
velocity, including rotation. A passive force across the angled submerged outboard uses the local
stern velocity and acts at the stern, producing both drag and turning torque. It works at idle
while moving and fades to zero with water-relative motion. Calm water remains the baseline;
planing, currents, and waves are not implemented.
A spreading, fading wake traces the travelled path; lateral
slip throws turning spray from the sliding side of the hull. Two orange markers provide turning
references but do not collide. Rendering is 320x200 with nearest-neighbour integer scaling. Losing
focus pauses movement.

The simulation uses fixed 120 Hz ticks. Handling constants live in `src/boat.c`. Screenshot mode
runs a deterministic example without a graphics context; see the README for commands.
The 240x160-world-unit basin is a handling test area, not yet a representation of Gothenburg.
It is four times wider and deeper than the original basin. The camera follows the boat from
20 units behind and 14 units above, looking 5 units ahead. Its heading eases through turns on fixed
simulation ticks, including angle wraparound. `-`/`+` adjust distance (8..48), PageUp/PageDown adjust
height (4..36), and `C` restores the default view. The HUD shows both values; `R` resets the boat
without changing them. Settings are per-run; `--camera-distance` and `--camera-height` also configure
interactive runs and screenshots. Geographical scenery,
collision splashes, and timed courses belong to later stages. Player evaluation of handling is the
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

## First playable version

- One craft, one compact area, free driving, and instant reset.
- Left/right adjust a persistent wheel setting; up/down adjust a persistent throttle within
  0..100%; a separate reset key centres the wheel and idles the throttle.
- Momentum persists through turns. Stern thrust creates torque, and rotation builds with inertia.
  Water resistance along the hull damps sliding and rotation.
- Throttle affects both forward push and turning force. Reducing it cuts thrust; momentum remains.
  Defer a dedicated brake until play suggests it is needed.
- Shore and buoy collisions cause forgiving bumps and splashes, with quick recovery.
- Follow the boat from behind and above, with a stable horizon and gently smoothed heading.

The first success criterion: driving in circles and carving around a buoy is enjoyable for a minute,
even without a score. Tune movement before adding content if this does not hold.

## Optional arcade challenge

After free driving feels good, add five or six ordered buoy gates forming a short loop. Clearly
highlight the next gate. Include a broad sweeping turn, a tighter corner, and an acceleration straight.

The initial challenge is a 60-second score run:

- Passing gates in order earns points.
- Completing a lap earns a bonus.
- Keep looping until time expires, then show the score and allow immediate restart.
- Keep the best score in memory initially.

Exact scoring values are tuning decisions. No opponents, enemy AI, or complicated combo system are
needed. Free boating remains available without a timer or mandatory gates.

## Presentation and feedback

Use a chunky craft, strong silhouettes, a small palette, and exaggerated water effects. Preserve
nearest-neighbour, integer-scaled presentation. Put visual effort into movement and response.

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

1. **Handling sketch:** simple craft, water, steering, thrust, momentum, boundaries, and reset.
   Evaluate the pleasure and readability of movement; keep this cheap to revise.
2. **Water toy:** wake, turning spray, forgiving bumps, and a few buoys in a compact waterfront area.
   Evaluate whether movement feels expressive and whether cruising is enjoyable on its own.
3. **Minute-long game:** ordered gates, timer, scoring, and fast restart, alongside free driving.
   Evaluate whether another attempt is appealing without adding progression rewards.
4. **First archipelago slice:** only after the core works, try a small island area and the camera
   needed to explore it. Evaluate the appeal of discovering routes and places.

Develop in a separate launch mode initially, reusing rendering and audio where useful. Choose the
actual command-line interface during implementation and document it then. Preserve the existing
demo and its soundtrack timeline while this direction is experimental.

## Later water-physics experiments

Explore these individually after the wake and spray make the existing handling easy to read.
Keep calm water as a baseline for comparisons. These are proposed gameplay models to try, not a
commitment to a comprehensive boat simulation or claims of physically accurate behaviour.

- **Speed-dependent steering (first approximation implemented):** tune stern thrust, passive
  outboard resistance, and speed-dependent hull resistance through coasting turns and low-speed
  manoeuvring. The current model uses forces and torque, not a prescribed speed-to-turn curve.
- **Planing:** explore a distinct acceleration transition, with the bow rising and then settling
  into a faster glide. Test whether reaching open water becomes more satisfying.
- **Current:** introduce drift that makes routes through channels and approaches to shore matter.
- **Waves:** experiment with boat motion and handling that depend on travel direction relative to
  waves. Compare sheltered water with an exposed crossing.

Judge each experiment through driving before combining it with the others. Wake and spray should
help reveal movement through water as these models evolve.

## Scope boundaries

The first complete game can be one arena, one craft, one course, one music track, free driving, and
one score mode. The archipelago is the direction for expansion, not a requirement to build a vast map.

Defer upgrades, unlocks, economy, building systems, missions, multiplayer, procedural terrain,
a level editor, complex boat physics, and persistent saves. Jumps and boost also wait until ordinary
turning is satisfying, since they would materially change course design.

## Questions to revisit through play

- How much sliding feels playful while still feeling like a boat?
- Does relaxed exploration or fast gate running become the stronger experience?
- Which small piece of Gothenburg should be the first recognisable playable place?
- How much geographic fidelity contributes to the pleasure of being there?
- What follow-camera distance and angle best balance scenery and steering clarity?
- Can a few carefully shaped routes provide enough variety without more systems?

## Decision record

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
