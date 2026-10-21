# Coherent, crossable wakes

Status, 2026-10-21: W1 baseline and connected-strip experiment complete; **strip rejected
for player integration** at folds and energy refinement. The isolated linear heightfield
fallback passes the measured periodic free-propagation gates. W2.1 now has an isolated
owned C field, energy ledger, deep snapshots and passing boundary fixtures. W2.1b improves
operator accuracy and passes modal/coarse-domain refinement checks, but measured cost and
accurate-grid domain proof still prevent runtime solver/domain selection.
[Current W2 evidence and limits](WAKE_W2_REFINEMENT_RESULTS.md). W2 is revised below.
The existing D3 runtime remains current. Parent: [boat physics](BOAT_PHYSICS_PLAN.md).
Measured evidence, declared tolerances, commands and limits: [W1 results](WAKE_W1_RESULTS.md).
Next task: remaining W2.1, a faster boundary-aware square-root evaluation and a domain
study at the selected accurate grid/order; then W2.2 source calibration and W2.3 shared queries. No player field,
replacement tuning or human play acceptance is claimed.

## Recommendation

The connected-front design below is the original 2026-10-20 proposal. W1 tested a
connected Hermite-strip reconstruction on 2026-10-21 and rejected that implementation
at folds and spatial energy convergence. The physical contract and W3–W5 integration/play
gates remain applicable; the representation and W2 deliverable are revised below after the
heightfield experiment. This is not authorization to integrate the rejected strip model.

Original proposal: use a staged replacement of the physical player wake with **connected, dispersive wave
fronts carrying an explicit energy budget**. Keep a small number of wavelength bands,
continuous crest/trough profiles, and the existing water-query boundary. This is a surface
wave approximation suitable for this C17 playground, not a volume-fluid engine.

Make wake crossings a core feature: crests can reinforce, a crest and trough can cancel,
and both trains continue beyond the intersection. Aim for readable ramps that can be
crossed again after a circuit, with jump height depending on approach speed and angle.
Do not prescribe an extra jump impulse in place of the existing hull/surface interaction.

Prefer several reviewable steps over a one-shot rewrite: changing propagation, overlap,
and source strength together changes the forces on the boat. Longer-lived waves also
invalidate the present fixed history and per-fragment evaluation budget.

## Code findings

Historical source-review baseline, 2026-10-20; see [W1 results](WAKE_W1_RESULTS.md)
for subsequent runtime measurements. These are source-based findings; visual causality has not been isolated by new captures.

| Finding | Evidence and consequence |
| --- | --- |
| Independent short patches | `src/boat.c:610` emits approximately every 0.4 s. `src/boat_water.c:174–196` confines each patch to a fixed path-axis support and sends it sideways along its own normal. Neighbours on a bend travel in different directions without a shared front or resampling, so separation and overlap can grow with age. |
| Approximate stern path | Both ends of each 0.4 s interval receive the same stern offset in `src/boat.c:617–621`. This does not record the actual stern trajectory through the turn. |
| Hard-coded disappearance | `src/boat_water.c:198–200` grows the wave at ages 0.4–1.4 s, then fades it at 14–20 s. No retained energy determines the fade. The 64-slot ring normally spans about 25.6 s of continuous emission, so it is not the first cause of the current 20 s expiry; extending lifetime alone would expose overwrite losses. |
| Short wave train | The transverse support is only 9 m wide with a 6 m carrier wavelength, and the cubic envelope narrows its useful part further. Crests travel through this small envelope at 3 m/s while it travels at 1.6 m/s. Individual crests appearing and disappearing is expected for dispersive waves, but this short train accentuates it. |
| Overlap suppresses waves | `src/boat_water.c:216–221` divides the signed sum by `sqrt(1 + weight²)`. That bounds height but is not physical superposition or energy conservation. Even an isolated packet is attenuated; independent crossing trains are suppressed together. |
| Energy is not represented | Amplitude depends on speed alone (`src/boat_water.c:79`), not emitted work, wetted load, represented area, or spreading. Neither phase interference nor source work is audited. |
| Rendering can exaggerate breakup | The GPU near mesh uses 1.5 m spacing; the CPU reference uses edges up to 2 m. A 6 m carrier has few geometry samples. GPU height fades between 48 and 80 m from the boat, while analytic slope shading remains active. These are additional suspects, not confirmed causes of the reported fragmentation. |
| Several different things are called wakes | `src/boat.c:435–451` creates short-lived cosmetic foam; `src/waterfront_traffic.c:102–145` creates fleet foam ribbons. Neither produces the jumpable surface. The scripted crossing source is another analytic recipe in `src/boat_water.c:125–153`. |

The physical recipe is repeated in `boat_water.c` and both `waterfront_water` shaders.
`boat_water_query()` already supplies height, normal and velocity to the six hull supports
in `buoyancy_step()`. Preserve this useful integration point. Current waves supply vertical
surface motion but no horizontal orbital wave velocity; the current field does not advect
the wake history. Adding current travel along the slope is not wave transport.

Existing environment checks cover derivatives, bounded overlap, crest travel, deterministic
loop encounters, copying/reset and CPU/GPU agreement. They do not establish conserved wave
energy, continuous fronts through bends, or physical interference. The exact 0.65 m overlap
cap and forced disappearance are implementation-specific expectations that must change.

## Physical contract

For small gravity waves at fixed depth, use `ω² = g k tanh(k h)`, with `k = 2π/λ`.
Start with a declared deep-water approximation: `ω² = g k`, crest speed `ω/k`, and energy
transport speed `dω/dk`, half the crest speed. A 6 m wave then travels at approximately
3.06 m/s, with its energy envelope at 1.53 m/s. The existing 3/1.6 values are already close;
speed retuning alone will not fix the structure. See [MIT's linear-wave lecture](https://www.ocw.mit.edu/courses/2-019-design-of-ocean-systems-spring-2011/0a8db4679928bbf62bc46addbc709432_MIT2_019S11_SK1.pdf).

Energy conservation does not mean constant crest height. For a narrow-band train, mean
energy per unit surface area is approximately `ρ g a² / 2`. For a normalized packet of
effective area `S`, preserve `E = ρ g a² S / 2` during free propagation. Thus doubling the
covered area reduces amplitude by `sqrt(2)`. Spreading and dispersion redistribute energy
even with heat loss disabled. This estimate includes the mean kinetic and potential energy
of a linear progressive wave; instantaneous height squared alone is not an energy audit.

The implementation contract is:

- Source work supplies energy; free propagation transports it. No age-based fade in the
  active play area. Track energy lost at absorbing shores, open boundaries, and deliberate
  resource retirement separately. Reset explicitly discards the field.
- Conservatively subdivide/resample fronts: children inherit their share of the parent's
  energy, footprint and phase. Adding samples must not make a brighter or weaker wave.
- Interpolate samples belonging to the **same** front with weights that sum to one.
  Add **independent** fronts with signed phase. Never divide their combined field by a
  global overlap count. At a crossing the local crest can grow without creating total energy.
- Reconstruct continuous height, spatial derivatives and time derivative from the same
  representation. Also derive horizontal orbital velocity consistently with the chosen
  wave bands before changing how the hull samples that velocity.
- Keep the mean water level stable: each emitted train must balance crests and troughs.
  A compact envelope times a sinusoid does not automatically have zero integral.
- Initially audit conservation in still water and uniform flow. Spatially varying currents
  require ray refraction and wave-action transport (`E/ω_intrinsic`), and can exchange energy
  with waves. Do not claim that simply translating nodes by local current conserves energy.

Connected wave fronts are informed by [Yuksel, House and Keyser's Wave Particles](https://www.cemyuksel.com/research/waveparticles/),
which describes local wave representations, reconstruction and subdivision. The proposed
energy accounting and dispersive bands still need validation; citing that method is not
proof that our implementation conserves energy.

This is approximate conservation for resolved surface-wave propagation, not exact conservation
of boat + water. The current buoyancy model does not remove work from waves when the hull
takes a jump. Start with that explicit one-way encounter approximation. Source energy can
be budgeted from a portion of existing water-relative resistance work without adding the
same drag twice; complete reciprocal wave/hull coupling is a separate follow-up. Do not
describe repeated jumps as an audited closed mechanical system until that exists.
The hull code expresses forces per unit mass; define consistent effective mass/density
units before claiming a source-work budget in joules. Track wave steepness as well as
energy: the linear approximation cannot represent arbitrary steep or breaking crests.

## Heightfield direction after W1

The fallback diagnostic uses a periodic Fourier grid of height η and surface potential ψ
with exact linear deep-water propagation. It removes the strip inverse/fold failure and
allows kinetic plus potential energy to be audited from reconstructed fields. The
[heightfield results](WAKE_W1_RESULTS.md#heightfield-fallback--2026-10-21) own its equations,
source, measurements and limits. This selects a propagation direction for W2, not a runtime
storage size, FFT library, performance budget or accepted wake appearance.

W2 should proceed in separately reviewable steps:

1. **Owned field and boundary proof.** Prototype a bounded C field behind `boat_wake.[ch]`
   with explicit initialization, destruction, reset and deep snapshot/copy semantics.
   Select an FFT implementation and physical domain from measured costs; the Python
   diagnostic's 384 m periodic box is not a gameplay choice. Demonstrate open-boundary
   absorption and island occlusion before any player switch. A damping mask alone is not
   evidence of island blocking: the nonlocal deep-water operator may communicate across
   land. Test a source on one side of a solid island and probes on the other, including
   leave/return and long propagation. Track signed injected work, retained energy and
   explicit absorbed/retired energy; separate numerical splitting residuals from sinks.
2. **Tick source and convergence.** Record actual wetted stern history at fixed simulation
   ticks, use double/integer time with bounded presentation phase, and define effective
   mass/density units before assigning resistance work. Replace the diagnostic's prescribed
   potential forcing with a documented moving-source approximation. Check the steady
   `ω = k·V` relation, accelerate/turn/reverse/stop paths, dry-flight suppression and
   energy plus field convergence under source resampling. Preserve zero-volume emission;
   report signed work without silently relabelling source removal as damping.
3. **Shared query contract.** Derive η, its gradient, η_t and the horizontal surface velocity
   `∇ψ` from the same state. Validate uniform-current translation. Spatially varying current,
   nonlinear breaking, shore reflection and reciprocal hull coupling remain unimplemented
   until separately designed and measured. Coordinate shared world ownership with foundations
   P4; do not add shallow pointer copies to `BoatWater`.

Then W3 measures CPU update/query/upload/render costs and selects shared CPU/GPU sampling,
near geometry and interpolation. W4 still requires jumping/crossing play feedback, and W5
still owns the default switch. The rejected strip's subdivision/pool implementation is no
longer W2's deliverable; its energy, continuity and explicit-retirement requirements survive
in field/refinement and domain accounting form.

## Original representation and scope (superseded by the heightfield direction)

Introduce a small `boat_wake.[ch]` module with emitter state, front connectivity, a bounded
node pool and spatial bins. A front element stores its position, propagation direction,
band/wavenumber, phase, energy and represented footprint. The envelope moves at group
velocity while phase advances according to the dispersion relation. Track a continuous
source phase across emissions and maintain the front through turns. Do not weld together
different fronts where they cross or fold.

Record actual wetted hull/stern positions at fixed simulation ticks. Resample by travelled
distance and curvature; use water-relative motion and hull load for source strength. Begin
with a modest set of outgoing directions and wavelength bands; verify the straight-run
pattern against a moving-source reference before tuning. A physically based reference uses
the steady-source relation `ω = k · V` in still water, rather than a fixed V opening angle
at every speed. Keep the generation model explicitly approximate for accelerating turns.

Use a longer coherent wave train, initially exploring 3–5 visible wavelengths, with smooth
edges and balanced displaced volume. Treat this as a prototype range, not accepted tuning.
Finite bandwidth should produce gradual dispersive widening. Keep foam separately short-lived;
a clean swell should remain jumpable after its foam fades.

Retain world-space wake state when it leaves the camera. Rendering culling must not erase a
wake the player can loop back into. Use conservative coarsening of old fronts and an explicit
outer simulation domain/resource budget. Report residual energy on retirement and never
silently overwrite a live high-energy front. Perfectly lossless waves in a closed basin with
an engine continually injecting energy cannot also remain bounded forever: use explicit
absorbing shores/open boundaries for the first release. Reflection is a later feature.

Spatial bins should let CPU hull queries inspect nearby fronts. Avoid extending the current
64-entry uniform loop into hundreds of fronts evaluated at every fragment. Prototype a
shared CPU-evaluated height/slope field for GPU upload, or compact per-tile front lists; select
using measured simulation/query/upload/render costs in W3. The GPU must consume the same
field and phase convention as the CPU. Preserve snapshot ownership, reset and deterministic
headless evaluation when introducing dynamic storage; a shallow pointer copy of `BoatWater`
would break the current by-value ownership guarantees.

## Implementation sequence

| Step | Deliverable | Acceptance gate |
| --- | --- | --- |
| W1 — Baseline and isolated prototypes (complete; strip rejected, periodic heightfield gates pass) | Top-down height/slope plots and side-view sequences for a straight run, a tight loop, an S turn and two crossing trains; a standalone connected-front prototype alongside the current model. | Measure present seams and lifetime. Prototype a spreading front, conservative resampling, phase/group travel and crossing interference before integrating the player. |
| W2 — Owned heightfield, boundaries and source | Owned height/potential field; measured FFT/domain choice; open/shore absorption and island occlusion; actual stern history, coherent finite-band source, energy/volume and source-work accounting, explicit retirement and safe snapshots. Use integer tick/double time with bounded presentation phases, coordinated with foundations P3. See the revised steps above. | Equivalent paths at different emitter sample densities give convergent fields/energy. No holes at bends, no young-wave overwrite, no waves passing through an island and reappearing. Uniform-current translation works. Spatial-current limitations are documented until implemented. |
| W3 — One visible, physical surface | CPU query and CPU/GPU surface reconstruction share the new field; normals, foam placement and wave velocity agree. Measure finer near geometry, starting around 0.75–1 m spacing for a 6 m minimum gameplay wavelength. | Crossings show the same crest/trough and slope used by the hull; no camera-distance seam or missed ramp. Profile full wake history in basin, Eriksberg and river on desktop and browser. Choose pool/grid sizes from these measurements. |
| W4 — Jumping and crossing play pass | Replace the scripted-source recipe with the common emitter. Tune straight crossings, oblique crossings, wake surfing, loop recrossings and two-source intersections with the existing six-support response. | A useful wake survives a measured circuit. Reinforcement yields a readable larger ramp; cancellation and the continuation of both trains are visible. Dry flight does not emit a full hull wake. Landings and neutral recovery stay controlled. Obtain human play feedback before accepting tuning. |
| W5 — Default and documentation | Switch the default after the gates pass; retire the old packet equations, revise old cap/expiry assertions, and update physics, boating, renderer and harbor guides. | Full build/CTest, CPU/GPU captures, desktop motion/audio smoke, explicit platform limits and separate user acceptance. Keep course/replay water settings intentional and review compatibility if those settings change. |

Traffic can use the same emitter after W4. Start with one controlled crossing vessel so there
is a reliable place to find intersecting wakes; enabling every existing harbor craft at once
would obscure source strength and performance. That extension is proposed, not implemented.
Coordinate shared water ownership with foundations P4 instead of keeping independent copies
of the same physical field in every boat. The rest of the foundations plan is not a mandatory
prerequisite to the isolated wake prototype.

## Numerical and play checks

W1 must define tolerances before adopting the replacement. Initial targets, subject to a
recorded resolution study:

- With source, sinks and damping disabled, less than 1% drift in integrated energy over 60 s
  for an isolated resolved train; repeat after halving node spacing and timestep. Test the
  reconstructed field as well as the stored energy sum so a broken renderer cannot pass by
  merely keeping an unchanged energy counter.
- Check crest and envelope speeds against the dispersion relation; check spreading amplitude
  against effective area and mean water level against zero-volume emission.
- Test straight, turning, accelerating, reversing and stopping emitters. Change emission
  spacing without changing the path or energy injected per second.
- For two finite crossing packets, test constructive/destructive phase, then compare each
  outgoing packet with its isolated reference. Audit kinetic plus potential energy including
  interference terms; summing independent packet energies alone cannot prove field conservation.
- Resampling and capacity stress must preserve the field within a specified error bound;
  audit retained, injected, absorbed and retired energy separately. Check source-off recovery
  after waves leave, rather than demanding rest while a long-lived wave is still passing.
- Check height/gradient/time-derivative consistency, CPU/GPU parity, pause/reset/copies,
  long-session phase precision, shoreline absorption and leave/return camera scenarios.
- Measure airborne time, peak heave/pitch/roll, re-entry loads and recovery for several
  approach speeds and angles. A numerical pass does not establish that the jump feels good.

If conservative front reconstruction fails at tight folds/focusing or needs pervasive amplitude
clamping, stop at W1 and choose a linear dispersive heightfield solver instead. That is the
more systematic alternative for phase interference and energy, but requires a surface grid,
boundary handling and a larger CPU/GPU integration change. Do not ship independent patches
with an energy counter and call the propagation solved. A full Navier–Stokes engine is not
needed for either option.

## Review evidence and limits

2026-10-20, macOS: source and existing validators reviewed; physical relationships checked
against the primary references linked above. No runtime implementation changed. Debug
configure/build and all 77 CTests passed, including 20 GPU checks with the existing
`PROTEUS_GPU_TESTS=ON` configuration. These verify the current implementation, not the proposed
rewrite. Local file targets in all four changed documents and `git diff --check` passed.
The CTest-generated CPU time-9 player-wake and combined scripted/player-wake frames were
inspected (`build/boat-player-wake.png`, `build/boat-crossing-wake.png`), along with
`build/boat-player-wake-gpu.png` and the combined current/ambient-wave GPU+AA variant
`build/boat-player-wake-gpu-aa.png`. Angular crest geometry is more apparent in the CPU
reference; GPU shading smooths its appearance. These still frames do not isolate motion,
sampling error or lifetime behavior, and the AA variant has different water options. No new
manual gameplay or before/after implementation comparison was performed. Patch divergence,
short-envelope turnover and render undersampling are diagnoses to isolate in W1, not measured
percentages of the observed artifact. Performance, conservation tolerances, source tuning,
variable-current transport and player acceptance remain unverified.

## W1 handoff — 2026-10-21

Implemented `wake_baseline`, which samples production water queries on controlled paths,
and `tools/wake_front_review.py`, a standard-library-only connected-front experiment with
reproducible height/slope/side-section plots and JSON measurements. It remains entirely
outside player water and shaders. [W1 results](WAKE_W1_RESULTS.md) owns the measurements,
method, numerical tolerances, failed physical gates and remaining limitations.

Completed: baseline lifetime and station/join probes; straight, loop, S-turn and crossing
sequences; spreading/area transport; conservative Hermite subdivision with field checks;
phase/group travel; signed crossing fields; potential-energy quadrature and reconstructed
kinetic-plus-potential audits including crossing terms and a resolution study.

Decision: reject this connected-strip reconstruction. Inner fronts fold and omitted invalid
cells would make holes; spatial energy differs by 12.565% under refinement. A 0.00587%
maximum temporal energy deviation over the sampled 60 s interval is not a conservation
pass. The next task at this initial checkpoint was the W1 fallback heightfield experiment,
completed in the continuation below, then a revised W2. No source,
hull, renderer or default switch was made.

Verification, macOS / Apple Silicon, 2026-10-21: Debug configure/build succeeded with
`PROTEUS_GPU_TESTS=ON`; all 78 CTests passed, including 20 GPU checks, in a desktop-access
run (`ctest --test-dir build --output-on-failure --timeout 180`). A sandboxed attempt had
passed its first 56 checks before stalling at the first GPU window; that attempt was
interrupted and the complete desktop rerun passed. The final sampler capacity guard was
rebuilt and the W1 CTest repeated afterwards. Numerical checks include an analytic
Fourier-mode energy reference, subdivision/reconstructed density, raster coverage, volume,
phase/group travel and signed crossing fields. All four top-down height/slope sequences
and their side sections were inspected. Commands and parameters are in the W1 results;
artifacts/logs are in `build/wake-w1/` and `build/wake-w1*.log`. Changed local file links
and `git diff --check` passed. No new manual gameplay, motion/audio playback,
other-platform checks or user acceptance.

Continuation checkpoint, 2026-10-21: Debug configure/build and all 58 CTests passed
with `PROTEUS_GPU_TESTS=OFF`. This verifies the baseline before the heightfield
fallback; it adds no GPU, manual playback or user-acceptance evidence.

## W1 heightfield continuation — 2026-10-21

Completed the fallback in `tools/wake_heightfield_review.py`: exact periodic linear
height/potential propagation, balanced finite-band potential forcing on the four W1 paths,
chronological signed source-work accounting, independent reconstructed Hamiltonian audits,
source/grid refinement, co-located field comparison, crossing interference and a dispersive
packet probe. [The results](WAKE_W1_RESULTS.md#heightfield-fallback--2026-10-21) own exact
parameters, measurements, commands and limitations.

All declared isolated gates pass: largest grid energy error 4.49e−10 relative, height L2
error 1.97e−5, source-time energy error 1.84e−8, and reconstructed energy drift 3.78e−14.
Select the height/potential field direction and the revised W2 steps above. This completes
W1's fallback, not W2 implementation. Next: W2.1 owned C field, safe snapshots and explicit
energy ledger, with boundary/island proof before player integration. The periodic experiment
has no shoreline or open-boundary solution, calibrated moving-hull source or runtime cost
measurement. The strip rejection and D3 gameplay baseline remain unchanged.

Verification, macOS / Apple Silicon, 2026-10-21: Debug configure/build and all 59 CTests
passed with `PROTEUS_GPU_TESTS=OFF`, using `ctest --test-dir build --output-on-failure -j 4`.
The final full review completed with no failed diagnostic gates; all four final PNG sequences
were inspected. Artifacts: `build/wake-heightfield-final/`; log:
`build/wake-heightfield-final.log`. Changed local file links and `git diff --check` passed.
GPU checks, new manual boating/playback, other platforms and user acceptance were not performed.

## W2.1 initial baseline — 2026-10-21 (superseded by the continuation below)

Completed the isolated owner, deep snapshots/reset, signed source work, explicit absorption/
retirement and separately measured propagation residual in `boat_wake.[ch]`. The new CTest
checks ownership and deterministic continuation, manufactured-mode phase, source-off energy/
mean level, timestep refinement, invalid forcing, and 80 s open/finite-island/full-barrier
controls. [W2 results](WAKE_W2_RESULTS.md) owns equations, thresholds, costs, commands and
verification. Diagnostic boundary images were inspected; the D3 runtime is unchanged.

The candidate uses a positive polynomial of a water-only graph Laplacian instead of masking
a global FFT. It blocks communication across disconnected water exactly, and the finite
island passes the declared shadow/return tests. This changes the spatial approximation:
continuum dispersion/order/grid convergence and production domain choice are still open.
At this checkpoint W2.1 was partially complete, with W2.1b solver/domain refinement next,
before W2.2 source calibration. No moving-hull source, current/query integration, renderer switch,
manual playback or user acceptance is claimed.

## W2.1b accuracy and domain experiments — 2026-10-21

Added a positive constant-preserving polynomial-tail closure and optional fourth/eighth
order water-only axis operators. Tests verify actual stepped modes, symmetry/positive
energy and constants on irregular land, narrow water segments, deep snapshots and one-cell
land blocking. A shared finite-band modal packet has <1% continuum and coarse/fine height
error at 16/76 s with dx .75, spatial order 8, polynomial order 256 and dt 1/120.
The coarse-domain study rejects 72x48 m and passes 144x96 m against a doubled reference
at the declared sampled central region/times. [Refinement results](WAKE_W2_REFINEMENT_RESULTS.md)
own equations, gates, costs, artifacts, verification and limits; initial results above are history.

The accurate 144x96 m configuration takes about 109 optimized ms per step. It has not had
the same full domain study at that accuracy. Keep the operator as a numerical reference;
**W2.1 remains open on faster evaluation and a domain proof at matching accuracy**. No
runtime solver/FFT selection, source calibration, player integration or user acceptance is
claimed. Next work must preserve the independent modal, topology and energy gates rather
than accepting the current cost or carrying a coarse-grid domain result into gameplay.
