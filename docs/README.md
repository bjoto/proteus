# Documentation and current plans

Current status reconciled on 2026-10-19. The main demo is a four-minute Eriksberg
journey following five gulls to “Below the surface” at 120 BPM. The separate
`boat_study`, with quiet engine/water audio and a gentle XM background loop, provides free boating and a basin time trial.
Both default to GPU rendering; the CPU reference supports display-free captures.

## Current work

| Area and owning document | Implemented | Remaining / next task |
| --- | --- | --- |
| [Boating foundations](BOATING_FOUNDATIONS_PLAN.md) | P1 frame/tick instrumentation implemented; later phases pending | P2: repeatable benchmark/report tool and renderer counters; then baseline, clock correction, explicit world ownership, and simulation/presentation separation |
| [Boating design](BOATING_PLAN.md) | Basin, Eriksberg, and river locations; timed S course, recordings, persistent bests, ghosts, replays, full-window presentation with modal `?` help, and quiet engine/water audio with a joyful repeating XM | [Handling/speed review](BOATING_PLAN.md#handling-and-speed-review--2026-10-19): research and first tuning pass implemented and user-accepted; optional closer-camera comparison next; score-loop and archipelago remain proposals |
| [Boat physics](BOAT_PHYSICS_PLAN.md) | A–D3: reverse/docking, hull contacts, planing, buoyancy, gentle waves, current/scripted wake options in all locations and default player wakes; C accepted and D provisionally accepted | [H1/H2 feel pass](BOAT_PHYSICS_PLAN.md#handling-feel-pass--2026-10-19): quicker countersteering and speed view, verified and user-accepted; retain baseline and collect feedback; optional trim/loading not started |
| [Living city](LIVING_CITY_PLAN.md) | 01–05: city dressing, materials, berths, shared actors; travelling boats and a compact ferry brought forward; demo flock and 32 reactive boating gulls | 06 people/cyclists; varied road traffic, building activity, ferry polish, perched gulls, and final review remain |
| [River / Älvsborgsbron](eriksberg/bridge/README.md) | Separate source/crop, navigable bridge blockout, blocking tower legs, and grounded road approaches | Bridge/shoreline/building refinement; human handling and camera review |
| [GPU migration](GPU_WATERFRONT_PLAN.md) | Steps 01–12 implemented; GPU default, lighting, shadows, atmosphere, reflections, and regression checks | Migration is implemented; other-platform and human visual acceptance remain separate from automated checks |
| [Soundtrack](SOUNDTRACK_PLAN.md) | Approved proof v03, full composition/mix candidate, editable XM, and runtime integration | Timestamped listening feedback and full composition/mix acceptance; continue editing in FastTracker |
| [macOS app builds](MACOS_BUILD.md) | Apple Silicon action, relocatable app bundle, preferred river launch, ad-hoc signing, ZIP/checksum; local checks and first GitHub run passed | Browser-download security approval; macOS 13 execution and user playback review |
| [Windows builds](WINDOWS_BUILD.md) | Portable x64 action; reversal correction, bundled music, relocated CPU check; latest basin impact-bound correction implemented | Verify Windows CI after impact-bound correction; human Windows GPU/audio/control review |
| [Automated releases](RELEASES.md) | Version-tag/UI release workflow reuses both platform builds, uploads ZIPs/checksums, then publishes; offline publication tests passed | Push workflow and verify the first chosen version's full release/download path |
| [Browser boating](WEB_BUILD.md) | Emscripten/WebGL 2 build, audio start gesture, IndexedDB bests, local server, `v*` tag Pages workflow; Chrome automation/native suite passed and user confirmed local startup | Enable Pages, allow version tags, and verify first tag deployment; Safari/Firefox, physical GPU performance, and human playback review |

These are separate work tracks, not a single execution queue. A next task in one plan
does not authorize advancing every plan. Use the requested track's scope and acceptance checks.

## Guides and evidence

- [Architecture review](ARCHITECTURE_REVIEW.md): 2026-10-19 assessment of ownership, performance,
  iteration, and growth risks; the [foundations plan](BOATING_FOUNDATIONS_PLAN.md) owns the
  selected implementation sequence. These do not replace the gameplay plans.
- [Project README](../README.md): build, launch, controls, renderer options, course/replay storage.
- [3D renderer and scene guide](3D.md): current scene evaluation, CPU API, GPU materials, geometry.
- [Water review](WATER_REVIEW.md): dated 04e baseline followed by implemented 04f/04g records.
- [Generated boats and berths](HARBOR_BOATS.md): visual parameters, mooring/wave evaluation,
  contact envelopes, review commands, and the boundary for future playable boat variants.
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
ctest --test-dir build --output-on-failure
```

GPU tests require a desktop graphics context. In display-free environments configure a separate
build with `-DPROTEUS_GPU_TESTS=OFF` and report that GPU checks were omitted.

## Keeping this accurate

Follow [AGENTS.md](../AGENTS.md#documentation-maintenance) at every step: update the owning
plan and affected guides together, reconcile superseded claims, and distinguish implemented,
verified, and user-accepted work. Refresh the overview when milestones or next tasks change.
Keep test results dated and historical entries explicitly scoped to their original revision.
Ignored editor autosaves are not additional plans.
