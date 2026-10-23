# Documentation and current plans

Current status reconciled on 2026-10-22. The main demo is a four-minute Eriksberg
journey following five gulls to “Below the surface” at 120 BPM. The separate
`boat_study`, with quiet engine/water audio and a gentle XM background loop, provides free boating and a basin time trial.
Both default to GPU rendering; the CPU reference supports display-free captures.

## Current work

| Area and owning document | Implemented | Remaining / next task |
| --- | --- | --- |
| [Boating foundations](BOATING_FOUNDATIONS_PLAN.md) | P1 instrumentation, P2a benchmark and P3a current arcade baseline implemented; [report contract/evidence](BOATING_PERFORMANCE.md) | C1: integer tick authority and replay revision; full P2/P3 workload and renderer matrix remains pending |
| [Boating design](BOATING_PLAN.md) | Basin, Eriksberg, and river locations; timed S course, recordings, persistent bests, ghosts, replays, full-window presentation with modal `?` help, quiet engine/water audio with a joyful repeating XM, and a user-accepted wind-driven Swedish ensign | [Boat visual improvement](BOATING_PLAN.md#boat-visual-improvement--saved-2026-10-20) saved for later: cabin/hull shapes and palette; optional closer-camera comparison, score-loop, and archipelago remain proposals |
| [Boat physics](BOAT_PHYSICS_PLAN.md) | A–D3 plus accepted H1/H2 countersteering and speed view; T1–T5 load-driven banking and quicker recovery; user-accepted H3 firmer planing hull grip; arcade flight/contact rules advance compatibility to revision 5 | [H3 carving pass](BOAT_PHYSICS_PLAN.md#h3--stern-led-carving-feel-2026-10-20): feel accepted on 2026-10-20; exhaustive hands-on/platform checks unverified; optional trim/loading not started |
| [Wake rewrite](WAKE_REWRITE_PLAN.md) | Arcade default; bounded 128-object pool, smoother pieces; [review implementation](WAKE_ARCADE_IMPLEMENTATION.md): flight/landing/capsize, height-aware contacts, dynamic rival, crossing outcomes and crest readability | User driving/readability feedback; tight-turn overlapping pattern resembles the stuck-wave report, exact reproduction/fix unconfirmed; platform/fleet costs remain open |
| [Living city](LIVING_CITY_PLAN.md) | 01–05: city dressing, materials, berths, shared actors; travelling boats and a compact ferry brought forward; demo flock and 32 reactive boating gulls | 06 people/cyclists; varied road traffic, building activity, ferry polish, perched gulls, and final review remain |
| [River / Älvsborgsbron](eriksberg/bridge/README.md) | Separate source/crop, navigable bridge blockout, blocking tower legs, and grounded road approaches | Bridge/shoreline/building refinement; human handling and camera review |
| [GPU migration](GPU_WATERFRONT_PLAN.md) | Steps 01–12 implemented; GPU default, lighting, shadows, atmosphere, reflections, and regression checks | Migration is implemented; other-platform and human visual acceptance remain separate from automated checks |
| [Soundtrack](SOUNDTRACK_PLAN.md) | Approved proof v03, full composition/mix candidate, editable XM, and runtime integration | Timestamped listening feedback and full composition/mix acceptance; continue editing in FastTracker |
| [macOS app builds](MACOS_BUILD.md) | Apple Silicon action, relocatable app bundle, preferred river launch, ad-hoc signing, ZIP/checksum; local checks and first GitHub run passed | Browser-download security approval; macOS 13 execution and user playback review |
| [Windows builds](WINDOWS_BUILD.md) | Portable x64 action; reversal and basin impact-bound corrections, bundled music, relocated CPU check; latest MSVC timing-header fix awaits CI | Verify workflow after 2026-10-20 timing-header fix; then human Windows GPU/audio/control review |
| [Automated releases](RELEASES.md) | Version-tag/UI release workflow reuses both platform builds, uploads ZIPs/checksums, then publishes; offline publication tests passed | Push workflow and verify the first chosen version's full release/download path |
| [Browser boating](WEB_BUILD.md) | Emscripten/WebGL 2 build, IndexedDB bests, `v*` Pages workflow; deployed gameplay user-confirmed; user-accepted compact screenshot/PLAY/settings launcher; build-version footer and README live link; AA/fullscreen default on; canvas focus-border fix | Review version footer and focus-border correction after deployment; Safari/Firefox, physical GPU performance, and human audio review |

These are separate work tracks, not a single execution queue. A next task in one plan
does not authorize advancing every plan. Use the requested track's scope and acceptance checks.

## Guides and evidence

- [Boating performance](BOATING_PERFORMANCE.md): headless arcade benchmark contract, P2a verification and P3a baseline.
- [Architecture review](ARCHITECTURE_REVIEW.md): 2026-10-19 assessment of ownership, performance,
  iteration, and growth risks; the [foundations plan](BOATING_FOUNDATIONS_PLAN.md) owns the
  selected implementation sequence. These do not replace the gameplay plans.
- [Project README](../README.md): build, launch, controls, renderer options, course/replay storage.
- [3D renderer and scene guide](3D.md): current scene evaluation, CPU API, GPU materials, geometry.
- [Water review](WATER_REVIEW.md): dated 04e baseline followed by implemented 04f/04g records.
- [Generated boats and berths](HARBOR_BOATS.md): visual parameters, mooring/wave evaluation,
  contact envelopes, review commands, and the boundary for future playable boat variants.
- [Player flag](BOAT_FLAG.md): breeze, apparent wind, cloth, playback, and close review commands.
- [Harbor gulls](HARBOR_GULLS.md): boating flocks, social cues, vessel reactions, and review commands.
- [Harbor traffic](HARBOR_TRAFFIC.md): travelling craft, ferry, routes, moving contacts, and visual wakes.
- [Music workflow](MUSIC.md) and [FastTracker guide](../soundtrack/alternate/fasttracker/README.md):
  working XM, section cues, editing, and runtime selection.
- [Windows builds](WINDOWS_BUILD.md), [macOS app builds](MACOS_BUILD.md),
  [automated releases](RELEASES.md), [browser boating](WEB_BUILD.md),
  [dance study](DANCE_STUDY.md), and [Emacs setup](emacs.md).
- [Original waterfront plan](WATERFRONT_PLAN.md), [Eriksberg survey](eriksberg/README.md), and
  [living-city baseline audit](LIVING_CITY_AUDIT.md): historical design and evidence, with links
  to the current owners above. Dated progress entries retain results for their original revisions.

## Verification baseline

Arcade review implementation, 2026-10-22, M1 Max/macOS: Debug build and all 83 active
CTests (20 GPU) pass in 121.79 s; fifteen retired field cases stay disabled. The Emscripten
build, inspected captures and visible scripted basin-rival/river runs pass. A subsequent
[local browser smoke](WEB_BUILD.md#current-arcade-build-local-smoke--2026-10-22) verifies
Chrome/SwiftShader river/basin startup, rendering and help/reset without runtime errors. The
[implementation record](WAKE_ARCADE_IMPLEMENTATION.md) owns outcome sweeps, full-pool GPU
cost, the tight-turn overlap investigation and remaining human/platform checks.

Routine test selection and timings are owned by [the test workflow](TESTING.md).
On 2026-10-22, M1 Max/macOS, all 83 active cases (20 GPU) pass in 97.40 s, down from
748.21 s for the old 98-case serial suite. Fifteen retired physical-field experiments
are disabled by default; arcade/gameplay coverage remains active. Debug numerical
validators retain assertions with optimization. Use four CPU jobs; graphics cases
retain serial scheduling.


Wake continuity revision, 2026-10-22, macOS / M1 Max: Debug/optimized builds, all 97
CTest cases (23 GPU checks), final focused object checks, portable ASan/UBSan and the
Emscripten build pass. Before/after basin/river GPU and current basin CPU captures were
inspected; visible scripted jumping/landing completes. [Continuity results](WAKE_OBJECT_SMOOTHING.md)
own the denser/smoother source, 128-piece bound, .37–.40 ms observed cost increase,
verification and limits. The user subsequently enjoyed this revision and requested it as
the default; [default promotion](WAKE_OBJECT_SMOOTHING.md#default-promotion) owns the switch
and the remaining stuck-wave report.

Initial arcade wake objects, 2026-10-22, macOS / M1 Max: Debug/optimized builds and all 97 CTest
cases pass after correcting/rerunning the object GPU comparison, including 23 GPU checks.
Portable ASan/UBSan, bounded four-emitter stress, actual jumping/recovery, CPU/GPU recipe
parity, inspected captures, a visible scripted run and the Emscripten build pass. [Object results](WAKE_OBJECT_RESULTS.md)
own exact evidence and scope. The user subsequently reported enjoying the mode;
[continuity follow-up](WAKE_OBJECT_SMOOTHING.md) owns the subsequent revision and default promotion.

Earlier compact packet experiment, 2026-10-22, macOS / M1 Max: Debug build and all 92 CTest
cases pass across split runs, including 22 GPU checks after resuming with desktop access.
Portable packet ASan/UBSan contracts, four quadrature sweeps, doubled-reference control,
inspected diagnostic plots and documentation checks pass. [Packet results](WAKE_PACKET_RESULTS.md)
record the fixed-quadrature rejection, strong returning encounter and remaining limits.
At that checkpoint large-world runtime acceptance and the 2 ms target remained open.
The subsequent arcade direction above supersedes its proposed next task; D3 remained default
at this historical checkpoint.

Wake performance follow-up, 2026-10-21, macOS / M1 Max: Debug configure/build and all
91 CTests passed, including 22 GPU checks (349.92 s). Native and portable sanitizer checks,
identical CPU/GPU captures, scripted desktop animation and the Emscripten baseline build
pass. [Scaling results](WAKE_SCALING_RESULTS.md) record the 29.2% measured physics reduction,
frame percentiles, size sweep and remaining limits. At that checkpoint the 2 ms target
failed and large-world simulation remained open. The subsequent [packet experiment](WAKE_PACKET_RESULTS.md)
then narrowed the physical-track proposal; arcade object play review is now the active task.

Wake candidate, 2026-10-21, macOS / M1 Max: Debug configure/build and all 91 CTests passed,
including 22 GPU checks. Matched 80 s boundary/island and 160 s basin-domain proofs,
source/query convergence, physical crossing/recovery, CPU/GPU captures, Instruments profiling,
scripted desktop animation, portable sanitizers and browser-baseline smoke checks are recorded
in [candidate results](WAKE_CANDIDATE_RESULTS.md). The user reports revision 3 looks and feels
much better; the subsequent [performance/large-water follow-up](WAKE_SCALING_RESULTS.md)
takes priority over water appearance. At that checkpoint the 2 ms wake target failed and D3 remained
default, and broader scenario/platform review is not inferred from these checks.

Swedish flag, 2026-10-20, macOS: Debug build and all 77 CTests passed, including 20 GPU
checks; seven affected checks passed again after shortening the staff. CPU/GPU captures
and scripted desktop animation were checked. The user accepted the final flag;
[the boating record](BOATING_PLAN.md#swedish-ensign--2026-10-20) owns details and limits.

Handling/speed pass, 2026-10-19, macOS: Debug configure/build completed; all 69 CTests
passed across the full run and targeted handling rerun, including 18 GPU checks. CPU/GPU
frames were inspected and an Eriksberg GPU+AA desktop demo smoke run completed. Countersteer
recovery improved in deterministic trials. The user accepted the handling/speed-view pass on
2026-10-19: "Feels great!" Old personal-best files are preserved separately from handling
revision 2. Exhaustive scenario and platform coverage remain unverified. Detailed measurements and limits are in the
[physics feel-pass record](BOAT_PHYSICS_PLAN.md#handling-feel-pass--2026-10-19).

All-location/default-wake update, 2026-10-18, macOS: Debug build and all 69 CTests passed,
including 18 GPU checks. Default and explicit player-wake captures match byte for byte;
the opt-out matches the earlier calm frame. Mapped profile/source-clearance checks,
CPU/GPU captures, and a desktop river smoke run passed. Courses/replays retain their
original settings. Details and continuing manual/platform limits belong to the
[physics handoff log](BOAT_PHYSICS_PLAN.md#handoff-log).


User acceptance, 2026-10-18: Phase D handling is provisionally accepted as feeling good,
with continued evaluation over time. Revised player-wake motion was also accepted.
Preserve this baseline; optional Phase E has not started. This supersedes pending
handling status in the historical verification entries below. See the
[physics handoff log](BOAT_PHYSICS_PLAN.md#handoff-log) for scope and limitations.


Player-wake motion refinement, 2026-10-18, macOS: Debug build and all 66 CTests passed,
including 18 GPU checks. A new propagation test tracks crest travel independently of the
packet; CPU/GPU parity and loop/recovery checks passed. Desktop scripted playback and
sampled native captures passed. The user accepted the revised player-wake motion on
2026-10-18; broader Phase D handling review remains pending. See the
[physics handoff log](BOAT_PHYSICS_PLAN.md#handoff-log) for current speeds and evidence.


Player-path wake follow-up, 2026-10-18, macOS: Debug build and all 66 CTests passed,
including 18 GPU checks. The expanded full-history shader parity probe and environment
checks also passed afterward. Scripted loop/recovery and desktop playback passed; sampled
CPU/GPU basin and mapped captures were inspected. Human handling/artistic acceptance is
pending. Details and limitations belong to the
[physics handoff log](BOAT_PHYSICS_PLAN.md#handoff-log).


Phase D, 2026-10-18, macOS: Debug build and all 62 CTests passed, including 16 GPU
checks. The authored basin current/wake profiles passed scripted recovery and desktop
playback checks; 160 calm-profile CPU frames matched the pre-D baseline byte for byte.
Human handling/artistic acceptance remains pending. See the
[physics handoff log](BOAT_PHYSICS_PLAN.md#handoff-log) for detailed evidence and limits.


Boating music, 2026-10-18: Debug build and all 54 CTests passed across CPU and
unsandboxed desktop graphics runs (14 GPU tests); the final XM was rechecked after
regeneration. Desktop boating audio smoke passed. The user accepted the joyful music
on 2026-10-18; exhaustive focus/replay listening remains unverified.
See [the boating music record](BOATING_PLAN.md#joyful-background-music--2026-10-18).

Boating-audio change, 2026-10-18: a peaceful water-led mix supersedes the earlier engine
revisions following user feedback and a second Astra consultation. Debug build and offline
engine/water validation, all 53 CTests (including 14 graphics tests), and the desktop CPU
wave-demo smoke run passed. Detailed evidence is tracked in
[the audio record](BOATING_PLAN.md#peaceful-water-mix-verification--2026-10-18).
The user accepted the peaceful mix on 2026-10-18; it is the audio baseline.
Exhaustive manual control/replay coverage remains unverified.

On the development Mac, 2026-10-18, Debug configure/build and all 52 CTests passed,
including 14 graphics-context tests with `PROTEUS_GPU_TESTS=ON`; rerun after the
boating-gull addition (see the living-city log). This verifies the
automated suite at this revision; it does not establish Windows behavior, human motion/audio
assessment, or musical acceptance. See the owning plans for existing manual evidence and limits.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DPROTEUS_GPU_TESTS=ON
cmake --build build -j
ctest --test-dir build -j 4 --output-on-failure
```

GPU tests require a desktop graphics context. In display-free environments configure a separate
build with `-DPROTEUS_GPU_TESTS=OFF` and report that GPU checks were omitted.

## Keeping this accurate

Follow [AGENTS.md](../AGENTS.md#documentation-maintenance) at every step: update the owning
plan and affected guides together, reconcile superseded claims, and distinguish implemented,
verified, and user-accepted work. Refresh the overview when milestones or next tasks change.
Keep test results dated and historical entries explicitly scoped to their original revision.
Ignored editor autosaves are not additional plans.
