# Arcade gameplay review implementation

Owner: [wake plan](WAKE_REWRITE_PLAN.md). Started 2026-10-22 in response to the
user's instruction to address [the review](WAKE_ARCADE_REVIEW.md) in order.
That review remains a dated baseline; its findings are not descriptions of this revision.

## Current status and rules

2026-10-24: the [stability and steering-roll study](ARCADE_STABILITY.md) supersedes the
unassisted object-mode air attitude and 1.8 m default described in the dated baseline below.
It targets fewer accidental capsizes, a 1.2 m default and deliberate steering air rolls;
the first candidate remains pending user play.

User feedback, 2026-10-23: boating is fun, particularly jumping, landing and capsizing.
The [multiplayer plan](MULTIPLAYER_PLAN.md) preserves that baseline and owns the new planning
request. This establishes the desired feel; it does not establish a tight-turn fix, multiplayer
acceptance or the still-unverified platform checks below.

Implemented findings 1–5 in order, followed by the crest-readability/geometry pass and
stationary-camera tight-turn investigation. Verification is complete for the checks listed below.
Next on this track: readability feedback, particularly the tight-turn pattern; platform
playback/performance checks remain separate. The implementation-time evidence below did not
claim user acceptance; later feedback is recorded above. Documentation link targets and `git diff --check` pass.

1. **Flight, landing, capsize.** Six support locations determine whole-hull water contact.
   At least .05 s without support establishes airborne state; renewed support records a
   landing and shows a .35 s landing indicator. The transformed hull-up vector, rather
   than an angle range, determines inversion. Contact with up-dot < -.25 for .6 s latches
   capsize, disables propulsion and shows `CAPSIZED / R TO RESTART`. Brief inverted flight
   is allowed. Normal turn assistance remains; it fades between up-dot .75 and .5, and
   the passive roll guard acts only with wet contact, using the wrapped bank angle.
2. **Separate air handling.** No thrust, hull drag, lateral grip, skeg or automatic banking
   acts in wholly dry flight. Wheel input supplies yaw-only .3 rad/s² at full lock.
   Airborne horizontal momentum is conserved; gravity and existing angular momentum
   continue. Propulsion/skeg additionally require stern support and upright orientation.
   Partial hull contact retains the accepted full water-grip model; this is deliberately
   a binary arcade contact gate rather than a new submerged-volume simulation.
3. **Height-aware contacts.** Traffic and berth hull/cabin/mast bounds are transformed by
   their shared wave pose. A vertically clear jump ignores their planar footprint; tall
   obstacles still block. Pontoon and quay heights are separate. Horizontal rejection
   precedes wave-pose queries. Basin edges, mapped shore and bridge-leg land footprints
   remain intentional boundaries at all heights. These are conservative envelopes;
   there is no full 3D impact solver, walkable deck or vertical landing-on-boat response.
4. **One dynamic rival.** `--wake-objects-crossing` retains the basin craft's real `Boat`
   buoyancy, inertia and shared wake emission, adding mutual SAT separation and contact
   impulses. As implemented here, player/rival normalized impact masses were 1/1.6 and a
   capsized rival restarted at its fixed spawn. Since multiplayer M2 (2026-10-23) both craft
   are slots of one [fleet](MULTIPLAYER_PLAN.md#m2-local-authority-handoff--2026-10-23): equal
   masses, simultaneous contacts, the player's water for both, and a safe-spawn reset 2 s
   after a capsize. Neither craft is immune to the other's wake. Its emitter starts a new
   span while retaining live old wakes and ring position. Ordinary harbor/traffic vessels
   remain kinematic.
5. **Outcome envelope.** A permanent 108-case sweep covers speeds 6/12/18 m/s, angles
   0/45/90°, ages 1/4/8 s and 1–4 local source slots. Each case runs 35 s and must finish
   upright/waterborne or explicitly capsized, with settled vertical motion and expired
   wakes. It does not accept an inverted boat continuing normally. A separate 40 s local
   four-boat drive shares wakes and mutual impacts, complementing the older remote
   storage stress. No hidden contact-height clamp or wave/source attenuation was added.

The powered straight and steady-turn H3 reference trajectories still pass unchanged.
Disengaging the skeg during dry-stern intervals changes the later coast endpoint by
about .22 m in the 15 s fixture; its stored reference is updated. Banking force probes
now independently predict the missing skeg force. Handling/replay compatibility is
**revision 5**; physics4 and earlier saved bests remain on disk separately.

## Readability and the stationary-looking pattern

Object-mode crest foam uses a .45 m crest-coordinate half-width, the same birth/age/
expiry and longitudinal windows as the wave, and two moving breakup components.
CPU/GPU masks agree; foam never changes height, slope or contact force. Positive faces
are no longer broadly whitened by the legacy `.38 m` height saturation.

The object-mode GPU patch uses .75 m cells within a bounded 48 m central square and
1.5 m outer cells. Shared nonuniform axis partitions avoid T junctions; clipping retains
shore geometry. The patch still follows the 12 m snapped origin, so density transitions
move on recentering; exhaustive perceptual review of that transition remains open.
Other water modes retain their original mesh; CPU reference geometry remains coarser.
GPU normals include the displacement-distance fade derivative; object slopes are no
longer flattened by exposure. Integer nearest-neighbour presentation is retained.

A controlled 10 m-radius turn at 12 m/s creates sixteen pieces, freezes the source and
camera, then follows the field for two seconds. Fourteen eligible individual crests move
2.40–2.44 m over one second, and leave their previous locations. The stationary-camera
captures show similar-looking overlapping patterns persisting around the turn despite
moving individual crests. The user reports this sounds very similar to their experience;
this connects the controlled case to the reported symptom, without establishing an exact
reproduction or user acceptance of a fix. The source/propagation recipe is retained.
Next visual work should use this case to judge whether a change to curved-front support
is needed; moving foam alone does not establish a solved propagation/readability issue.

Artifacts: `build/wake-curve-{0,60,120}-gpu.png`. `wake_objects_review --gpu` regenerates
these with a stationary camera; `--desktop` also shows the two-second sequence after
its scripted jumping/landing run.

## Verification and limits

2026-10-22, development M1 Max/macOS, Debug with optimized numerical validators:

- Debug configure/build pass; final project-source incremental build is warning-clean.
  A dependency rebuild reported existing raylib/SDK warnings; these are not project-source
  warnings. The first full suite exposed an outdated dry-skeg force assumption in the
  banking fixture, which was corrected against independent local-flow force calculations.
- The complete 83-active-case suite, including 20 active GPU cases, passes; fifteen
  retired field cases remain disabled. The final full run takes **121.79 s**; log:
  `/tmp/proteus-arcade-ctest-last.log`. The initial sandboxed graphics run stalled during
  initialization, was stopped, and subsequent complete runs used desktop access.
- Emscripten `cmake --build build-web --target web --parallel 4` passes. This checks the
  build/package. A subsequent [local Chrome/SwiftShader smoke](WEB_BUILD.md#current-arcade-build-local-smoke--2026-10-22)
  verifies current river/basin startup, rendering, help/reset and no runtime errors;
  it does not establish browser arcade outcome coverage or hardware GPU performance.
- Basin CPU/GPU, river GPU+AA and stationary-camera turn captures were inspected. GPU
  crests retain the launch silhouette with narrower foam; the CPU reference remains
  visibly faceted. The river time-9 camera looks across a quay and is useful for boundary
  clipping, not a complete unobstructed wake-readability assessment.
- Visible GPU scripted runs complete for the dynamic basin rival (600 frames / 1238
  ticks) and river (180 frames / 382 ticks). These verify windowed scripted progression
  and asset loading; they do not establish hands-on control/restart or listening review.

Logs are `/tmp/proteus-arcade-*`; build artifacts are local and uncommitted.

The 108-case sweep records 84 flights with renewed water contact, 43 final capsizes,
45.49 m maximum hull height and 28.16 m/s peak upward velocity. Cases without capsize
settle upright. These large extremes use duplicated seeded local ridges, not claims
about ordinary driving frequency. The actual own-trail loop reaches 6.167 m hull height
and records whole-hull flight and a landing.

The local four-boat run emits 167 pieces, peaks at 98 live, records 147 contact ticks
and 30 transitions into whole-hull flight. Contacts are required by the fixture, not merely
permitted; the first pair starts on a reachable head-on approach.

A final-suite one-boat/23-live-object diagnostic measures two physics/source ticks at
.1580 ms median, renderer 3.0111 ms, combined 3.1746 ms median / 3.5981 ms p95 / 4.2480 ms
observed maximum. Live count and trajectory differ from the original review, so this is
not an isolated visual-change comparison.

A separate frozen radius-15 m/16 s circular-history probe duplicates the path into four
emitters, freezes camera/world, renders 180 frames, discards 60 and measures 120 using
`glFinish`. It includes scene construction/upload/rendering and excludes physics, city
traffic, presentation and cold shader compilation:

| Local records | Rendering | Median | p95 | Observed max |
| --- | --- | ---: | ---: | ---: |
| 32 | 320x200 | 1.3345 ms | 1.4710 ms | 1.5855 ms |
| 128 | 320x200 | 2.5415 ms | 2.6572 ms | 2.8229 ms |
| 32 | 1280x800 supersampled to 320x200 | 2.1040 ms | 2.1979 ms | 2.3552 ms |
| 128 | 1280x800 supersampled to 320x200 | 5.2113 ms | 5.4112 ms | 5.6506 ms |

The last case is slower than the original review's 4.4879 ms median. These single-run
measurements are encouraging for bounded storage, but do not meet a 2 ms whole-wake
budget at full local overlap or supersampling and do not establish browser/river fleet
cost. The temporary probe source/executable are `build/wake-arcade-bench.{c,o}` and
`build/wake-arcade-bench`; log: `/tmp/proteus-arcade-bench.log`. The emitter count remains
four. No spatial shortlist, map-sized solver or deletion
of offscreen live wakes was introduced.

Targeted flight tests cover ballistic motion, limited air yaw, either flip axis, odd/even
multiples of pi, sustained inversion, upright landing and reset. Mutual collision tests
cover vertical clearance, momentum, dissipated energy, unequal-mass separation and
angular response. Traffic/berth tests cover low/high/below-water intervals, masts, low
pontoons and taller quays. Fine/ordinary patch checks cover bounded size, local edge
length, shoreline occupancy and existing ordinary-mesh recentering stability.

Known limits: extreme synthetic launches remain possible; the sweep is not a calibrated
player difficulty curve. It samples combinations rather than exhaustively covering all
speeds, headings, source histories or curved-front interference. The local four-boat
check is an automated basin workload, not a rendered river/browser fleet benchmark.
Hands-on driving, restart key handling, audio listening, exhaustive browser gameplay,
Windows behavior and new user acceptance require further review. No Instruments trace is
claimed; routine measurements have not exposed an unexplained spike requiring attribution.
