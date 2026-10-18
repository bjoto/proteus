# Documentation and current plans

Current status reconciled on 2026-10-18. The main demo is a four-minute Eriksberg
journey following five gulls to “Below the surface” at 120 BPM. The separate, silent
`boat_study` provides free boating and a basin time trial. Both default to GPU rendering;
the CPU reference supports display-free captures.

## Current work

| Area and owning document | Implemented | Remaining / next task |
| --- | --- | --- |
| [Boating design](BOATING_PLAN.md) | Basin, Eriksberg, and river locations; timed S course, recordings, persistent bests, ghosts, and replays | Playtesting and presentation; score-loop and archipelago ideas remain proposals |
| [Boat physics](BOAT_PHYSICS_PLAN.md) | A–C: reverse/docking, hull contacts, planing, buoyancy, and gentle waves; Phase C accepted by the user | D1 spatial current, then D2 one crossable wake; optional trim/loading after D |
| [Living city](LIVING_CITY_PLAN.md) | 01–04h: coverage, dressing, materials, waterline fittings, and generated moored boats at two dressed berths; flying gulls also implemented | 05 shared actor evaluation before new interacting actors; people, varied traffic, building activity, ferry, perched gulls, and final polish follow |
| [River / Älvsborgsbron](eriksberg/bridge/README.md) | Separate source/crop, navigable bridge blockout, blocking tower legs, and grounded road approaches | Bridge/shoreline/building refinement; human handling and camera review |
| [GPU migration](GPU_WATERFRONT_PLAN.md) | Steps 01–12 implemented; GPU default, lighting, shadows, atmosphere, reflections, and regression checks | Migration is implemented; other-platform and human visual acceptance remain separate from automated checks |
| [Soundtrack](SOUNDTRACK_PLAN.md) | Approved proof v03, full composition/mix candidate, editable XM, and runtime integration | Timestamped listening feedback and full composition/mix acceptance; continue editing in FastTracker |

These are separate work tracks, not a single execution queue. A next task in one plan
does not authorize advancing every plan. Use the requested track's scope and acceptance checks.

## Guides and evidence

- [Project README](../README.md): build, launch, controls, renderer options, course/replay storage.
- [3D renderer and scene guide](3D.md): current scene evaluation, CPU API, GPU materials, geometry.
- [Water review](WATER_REVIEW.md): dated 04e baseline followed by implemented 04f/04g records.
- [Generated boats and berths](HARBOR_BOATS.md): visual parameters, mooring/wave evaluation,
  contact envelopes, review commands, and the boundary for future playable boat variants.
- [Music workflow](MUSIC.md) and [FastTracker guide](../soundtrack/alternate/fasttracker/README.md):
  working XM, section cues, editing, and runtime selection.
- [Windows builds](WINDOWS_BUILD.md), [dance study](DANCE_STUDY.md), and [Emacs setup](emacs.md).
- [Original waterfront plan](WATERFRONT_PLAN.md), [Eriksberg survey](eriksberg/README.md), and
  [living-city baseline audit](LIVING_CITY_AUDIT.md): historical design and evidence, with links
  to the current owners above. Dated progress entries retain results for their original revisions.

## Verification baseline

On the development Mac, 2026-10-18, Debug configure/build and all 49 CTests passed,
including 13 graphics-context tests with `PROTEUS_GPU_TESTS=ON`. This verifies the
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
