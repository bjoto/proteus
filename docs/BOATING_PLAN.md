# Gothenburg boating playground

Status: working design, saved 2026-10-11. This describes a proposed direction, not implemented
features. Revise it as we discover what is enjoyable to play.

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
- Left/right steering and up for thrust; a separate reset key.
- Momentum persists through turns. Heading changes faster than travel direction, producing a
  controllable slide. Water resistance slows the craft; sideways grip keeps it manageable.
- Releasing thrust helps tighten turns. Defer a dedicated brake until play suggests it is needed.
- Shore and buoy collisions cause forgiving bumps and splashes, with quick recovery.
- Begin with a fixed, elevated camera that makes the test area easy to read. Revisit a following
  camera when exploring larger areas becomes the next concrete task.

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
- When is a following camera needed, and what angle best balances scenery and steering clarity?
- Can a few carefully shaped routes provide enough variety without more systems?

## Decision record

- 2026-10-11: Adopt a small playable boating toy as the proposed successor direction for the demo,
  with handling first and an optional short arcade challenge.
- 2026-10-11: Make boating around Gothenburg and its surrounding archipelago a central aspiration.
  Keep the first prototype local and small, and preserve free exploration alongside timed play.
