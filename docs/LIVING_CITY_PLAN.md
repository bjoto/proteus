# Living Eriksberg city plan

Status: execution steps 01–04 complete, 2026-09-18; step 05 is next. This is the
visual-development plan after the [GPU migration](GPU_WATERFRONT_PLAN.md).

## Direction

An inhabited waterfront at blue hour: apartments above active ground floors, connected
streets, people with destinations, and a working harbour beyond. Preserve the crisp,
integer-scaled image and the contrast between warm rooms and cool outdoor space.
Prioritize depth and readable activity at 320x200; inspect 320x240 and 320x180 too.

The city should feel inhabited even in a still frame. Movement then supplies small stories:
a cyclist passes a waiting bus, someone walks home, a room lights up, a ferry slides behind
the waterfront. Most of the scene remains calm. Avoid making every light or object animate.

## Working and committing one step at a time

The user has requested incremental commits. Complete the requested step, verify it,
update this document's progress log, and create a focused local commit. Commit creation
is already authorized; do not ask again at each step. Pushing is not part of this workflow.
If a range is requested, make a separate commit for each completed step and continue
through the range. Otherwise stop after the assigned step.

The numbered steps below are the execution units; the six stages later in this document
describe their artistic goals. Make routine implementation choices autonomously. Preserve
unrelated user changes and stage only files belonging to the step. Keep captures and
profiling artifacts under `build/living-city/`, outside version control. Do not stage the
three existing root-level PNG captures.

Before each commit, run the build and CTest, plus the visual, motion, and profiling checks
relevant to the change. Record commands, results, inspected artifact paths, and limitations
in the progress log. If a required check fails or cannot run, investigate and record the
specific blocker; do not describe the step as verified or complete. Keep useful work and
report what remains. A purely documentary step does not require a new manual demo run.

| Step | Deliverable and completion evidence | Status |
| --- | --- | --- |
| 01 | Capture and inspect the existing short/long routes; audit map coverage, heights, empty parcels, and camera views. Record a prioritized list of exact areas to change, with still-frame evidence and a performance baseline. | Complete |
| 02 | Repair nearby street enclosure and give exposed ground areas an identity, using step 01 findings. Compare matching before/after stills and check camera clearance. | Complete |
| 03 | Extend surrounding blocks and the distant skyline where needed. Inspect the whole route for visible map edges, new gaps, and depth continuity. | Complete |
| 04 | Finish one visible street corner: kerbs, entrances, shop/cafe, planting, furniture, and parked bikes/vehicles. It must read as inhabited in a still frame. | Complete |
| 05 | Consolidate the actor evaluation needed for city life into the shared scene layer. Preserve existing behaviour and verify direct seeks before adding actors. | Pending |
| 06 | Add a small cast of pedestrians and cyclists around the completed corner and quay, with readable silhouettes and deterministic paths. Inspect motion and occlusion. | Pending |
| 07 | Add varied vehicles and one authored interaction, such as a bus stop or yielding car. Verify spacing, stops, and hidden route wraps. | Pending |
| 08 | Add sparse room activity, coordinated lamp flicker, and selective vehicle lighting. Verify stills, motion, and direct-seek consistency. | Pending |
| 09 | Stage a ferry proxy in a visible river shot and validate scale, route clearance, and camera composition before detailed modelling. Preserve the music slot. | Pending |
| 10 | Finish the ferry silhouette, livery, deck lights, wake, and reflection, plus selected quay details. Review the complete harbour shot. | Pending |
| 11 | Add gulls: a few perched birds and a small number of circling/gliding silhouettes. Keep flight paths deterministic and readable against the sky at 320x200. | Pending |
| 12 | Expand the successful street treatment selectively and add restrained atmosphere. Review both routes, all resolutions, seeking, and final performance. | Pending |

Steps 02–04 establish the city in a frozen frame before the new moving cast arrives.
Gulls are a planned deliverable, with gliding and occasional wingbeats rather than a dense
particle flock. During step 01, note possible ferry framing; detailed proxy work belongs
to step 09 unless explicitly brought forward.

To start, use this prompt in the current or a fresh session:

```text
Read AGENTS.md and docs/LIVING_CITY_PLAN.md. Complete execution step 01 only.
Use the existing GPU renderer and retained geographic data. Inspect both camera routes
and identify the concrete changes needed to make the city convincing in a frozen frame.
Follow the plan's verification requirements, record findings and artifact paths in its
progress log, and commit the completed step. Preserve unrelated changes. Do not implement
later steps. Report the commit, findings, and any remaining verification limitations.
```

For subsequent work, replace `01` and the task-specific sentence with the next step's
deliverable, or request a range such as `02–04, with one commit per step`.

### Progress log

- Step 04 (2026-09-18): added an authored cafe at the northwest corner of mapped way
  282574767 (local origin X -71.82 / Z 105.90). Includes raised pavement and kerb,
  striped canvas awning, readable KAFFE sign, warm display glazing and a north entrance,
  three tables/six chairs, three planters, bench, bin, menu board, three cycle stands,
  and two parked bicycles. The existing north garden and the traffic lanes remain clear.
  The destination reads during the opening approach, especially t=1–2.5 on the short
  route. Camera position, music, actor motion, and generated geographic assets are unchanged.
  Static scene-owned box/beam data lives in `src/waterfront_corner.[ch]`; the GPU uploads
  two batches and reuses them for colour, reflection, and opaque shadows (five additional
  mesh submissions with all passes enabled). One reserved shop light fits the existing
  32-light limit, keeping the street lamps and at least three headlight slots.
  The CPU reference remains visually unchanged; its checksum tests pass without updates.
  Camera clearance now includes all new props, sampled at 120 Hz for both routes.
  Debug configure/build and CTest passed 13/13, including GPU review. Matching captures
  and one-second sweeps for both routes were inspected at 320x200/240, with 320x180
  framing checks. Windowed short and long GPU passes completed. Final artifacts are in
  `build/living-city/step04/`; `compare-corner-2.png` pairs step 03 and step 04 at t=2.
  CPU submission mean 1.317 ms / p95 1.421 ms (923 frames after 120 warm-up frames).
  This does not measure GPU execution time. As before, live human motion observation
  and exact GPU seek parity with history-dependent light selection remain unverified.
  Small furniture naturally becomes a few pixels on the fly-through; the awning, sign,
  doorway and warm pavement carry the main visual identity. People and moving cyclists
  remain work for later steps.

- Step 03: enlarged the retained-data crop from X -400..550 / Z -400..500 to
  X -700..950 / Z -600..800 m. Building features increase from 204 to 523 without a new
  download or authored building footprints. The river assembly still verifies that its
  artificial closing edges lie outside the crop. Geometry is now 20,434 vertices /
  25,512 triangles; regeneration into `build/living-city/step03/repro` matches the bundled
  geometry and manifest byte-for-byte. The new context uses existing colour, shadow,
  reflection, and atmosphere passes without new per-object draw calls.
  Inspected matching before/after captures at all supported resolutions and one-second
  sweeps of both full routes (17 short / 32 long frames including endpoints). Added
  `--sweep` and compact paired views to the review script. No exposed rectangular crop
  edge was apparent in the reviewed sweep; real open water and park space remain.
  CPU frames at t=0/15 were reviewed before updating the reference checksum to
  `5ebe525b6d2f6f83`. Build and all 13 tests pass, including GPU review, CPU seek checks,
  and 2 m route clearance. Both windowed GPU routes completed.
  The denser t=5 skyline leaves only the top sky band exposed in both renderers; the
  semantic review now requires that top band and ordered exposed sky rather than two
  bands, retaining coverage limits and the independent orientation/depth fixture.
  Final artifacts: `build/living-city/step03/`, with baseline-left/current-right comparison
  sheets and `compare-short-0.png`, `compare-short-10.png`, `compare-long-30.72.png`.
  CPU submission mean 1.328 ms / p95 1.517 ms versus baseline 1.317 / 1.405 ms (Debug;
  noisy submission measurements, not GPU execution timings). Live human motion review,
  GPU execution timing/memory measurement, and exact GPU seeking with the existing
  history-dependent light selection are not established by these checks.
  Remaining sparseness: broad undressed quay/paving, simple flat ground, repetitive
  facades, and no human destinations or activity. Step 04 should concentrate on a readable
  street corner, entrances, kerbs, parked bikes, and cafe/stop furniture.

- Step 02: restored 43 non-bridge mapped building parts, including 28 stepped apartment
  sections beside Monsungatan. Taller overlapping parts take precedence within parent
  footprints; uncovered parent areas remain. Added 13 authored planted zones and 33 coarse
  park canopies, clipped away from roads, buildings, and water. Generated geometry now has
  10,249 vertices / 13,015 triangles. Converter self-checks cover height parsing, overlap
  partitioning, and triangulation; regeneration into `build/living-city/step02/repro`
  matches both bundled outputs byte-for-byte.
  Inspected before/after sheets for both routes at 320x200/240 and framing at 320x180;
  inspected CPU frames at t=0/5 before deliberately updating the CPU reference checksum
  to `0b17e8f1198856c6`. Shared geometry also changes the existing triangle-seeded window
  patterns. Both routes retain 2 m clearance and deterministic CPU seeks. Windowed GPU
  routes completed. Build and 13-test CTest verification includes GPU review.
  Captures/comparisons: `build/living-city/step02/`; CPU submission mean 1.301 ms,
  p95 1.366 ms (same Debug configuration as baseline; no measured GPU execution time).
  Near facades now have stronger enclosure and stepped roofs; background continuity and
  large undressed quay surfaces remain visibly sparse. Step 03 addresses the background.

- Step 01: [coverage audit and verification](LIVING_CITY_AUDIT.md) records exact missing
  building parts, priority areas, baseline captures, and profiling. Added GPU storyboard
  playback/capture for both routes and a repeatable comparison script. Corrected the
  GPU review's stale sky-band assumption. Build and all 13 tests passed, including GPU.
  Both windowed routes completed; live human motion observation and true GPU timing
  remain limitations noted in the audit. Scene output is unchanged.

- Planning: inspected the existing scene code, geometry manifest, and two saved captures;
  documented the artistic stages and commit workflow. `cmake --build build -j` and
  `ctest --test-dir build --output-on-failure` passed (12 tests). GPU tests were disabled
  in that configuration; fresh GPU captures, live route review, and performance baseline
  remain work for step 01. No runtime changes.

## What exists and what feels missing

The current GPU scene already has static apartment windows, 20 moving cars on two street
paths, street lamps, a small boat and wake, shadows, atmosphere, and water reflections.
The generated manifest contains 204 building features across a 950 by 900 metre crop.
That count includes small structures; it does not establish convincing street enclosure.

The three repository captures were available for review; the early and late captures
`proteus-cube-00047746.png` and `proteus-cube-00049029.png` were inspected. They show
large undifferentiated ground areas and a thin distant skyline around detailed foreground
facades. This supports a composition and continuity problem; it does not yet prove which
buildings are missing from the source or importer.

The importer already includes the surrounding context crop. First compare the retained
map, generated mesh, and camera views. Simply enabling an assumed unused context layer
will not solve this. Check omitted building parts, height assumptions, ground treatment,
and coverage beyond the crop before choosing extensions.

## Implementation sequence

### 1. Make the city continue beyond the camera

- Review the complete short route and the long storyboard. Mark exposed map edges,
  empty foreground parcels, weak street walls, and missing background layers in captures.
- Establish three spatial layers: detailed street frontage, simpler surrounding blocks,
  and a low-detail distant city/industrial silhouette. Use real retained footprints where
  available; label authored infill and inferred heights as artistic assumptions.
- Add nearby buildings where the coverage audit supports them. Preserve legitimate
  courtyards, streets, squares, water, and views of the crane. Give open land an identity:
  planted courtyard, parking area, promenade, service yard, or small park.
- Extend context only where visible camera cones need it. If the retained snapshot is
  insufficient, choose a documented larger extract or authored distant masses. Keep
  source attribution and reproducible geometry generation.
- Vary roof heights, setbacks, facade widths, and roof silhouettes. Add balconies and
  entrances to the few buildings that pass closest to the camera.

Acceptance: representative stills read as a connected neighbourhood; map boundaries
are concealed by continuous geometry and distance treatment. Camera clearance remains valid.

### 2. Give streets places and purposes

- Build readable kerbs, pavements, crossings, planted strips, parking bays, and quay edges.
- Author three small activity centres along visible parts of the route: a lit corner
  shop/cafe, a bus stop, and a quay seating or fishing spot. Placement is artistic until
  checked against local references.
- Dress these with a restrained kit: benches, bicycle racks and parked bikes, bins,
  bollards, bus shelter, signs, trees, planters, railings, and mooring equipment.
- Add ground-floor shop windows, apartment entrance lights, door recesses, and a few
  awnings. Break the identical window-grid treatment between shops and apartments.
- Use parked cars and a delivery van to connect buildings to roads. Keep walking and
  driving routes clear. Group props by purpose instead of scattering them uniformly.

Acceptance: the viewer can identify a pavement, an entrance, and a destination in the
opening shot; open spaces look intentionally used even with animation frozen.

### 3. Put people and varied traffic into those places

- Start with a few clearly visible pedestrians: one pair walking, someone at the stop,
  a dog walker, and a person sitting or fishing. Expand only where they remain readable.
- Use simple low-poly silhouettes with modest limb motion and varied clothing. Position
  some against lit shopfronts so they register at the virtual resolution.
- Add cyclists, a bus, and a delivery vehicle. Vary traffic spacing and speeds; the
  existing 20 cars mean vehicle count alone is not the main deficiency.
- Author short route schedules: approach, stop, wait, depart. A bus stopping near waiting
  people and a car yielding at a crossing create relationships between moving objects.
- Use separated paths and timed reservations for the few interactions on screen. Hide
  route wraps beyond the shot or behind buildings. No general traffic simulation is needed.

Acceptance: people stay on usable surfaces, vehicles respect authored lanes and stops,
and no actor visibly teleports or intersects another during either camera route.

### 4. Let buildings and lights show activity

- Give apartments stable room states: dark, warm interior, cooler television light,
  partly closed curtains. Group related windows into apartments.
- Animate only a small visible subset: one room turns on, a silhouette crosses a window,
  a television changes softly, a stairwell light briefly comes on.
- Make one identifiable lamp intermittently faulty, with irregular short dimming and long
  stable periods. Change both its visible bulb and the illumination it casts together.
- Add selective brake lights, indicators, and the bus destination panel. Most lamps and
  windows remain steady; reserve actual local lights for useful nearby illumination.

Acceptance: activity is noticeable in motion without a twinkling-building effect;
window changes and lamp behaviour reproduce at a directly selected scene time.

### 5. Give the harbour a major presence

- Block out a large Stena Line ferry on the river outside the dock basin. Start with
  hull, layered decks, bridge, funnel, and a broad recognizable colour/livery treatment;
  inspect its silhouette before spending effort on windows or lettering.
- Treat its exact vessel, dimensions, route, and berth as unresolved reference work.
  This proposal is artistic staging, not a claim about current Stena operations.
- Test visibility immediately with a cheap proxy. The existing camera looks back towards
  the neighbourhood during the water section, so a river ferry may be behind the viewer.
  If necessary, propose a small gaze adjustment that includes the ferry and buildings
  together, with camera-clearance checks. A longer shot would be a separate timing decision.
- Let the ferry move slowly relative to cars. Add a restrained wake, deck lights,
  navigation lights, and its reflection. Ensure river width, depth appearance, quay
  clearance, reflection clipping, and render bounds support its scale.
- Keep the existing small boat as a scale cue. Add moored craft, quay ladders, fenders,
  bollards, and a few distant industrial lights around the harbour.

Acceptance: the ferry is visible and unmistakably large in at least one composed shot,
without hiding the neighbourhood or appearing to fit inside the small dock basin.

### 6. Add atmosphere and finish the shot

Choose a few extras after the previous passes are legible:

| Detail | What it contributes |
| --- | --- |
| Gulls circling or perched on quay furniture | Harbour identity and motion above street level |
| Tree movement and fluttering flags | A shared sense of wind |
| Rooftop vents and faint drifting steam | Activity above otherwise plain roofs |
| Damp pavement and a few puddles | Selective light reflections at street level |
| Roadworks with a barrier and warning beacon | A reason for an irregular street layout |
| Cafe chairs, a closing shop, someone on a balcony | Different uses and rhythms within the same block |
| Distant industrial roofs, masts, and scattered lights | Depth beyond the residential frontage |
| A clock or simple illuminated shop sign | A recognizable landmark at human scale |

Treat rain, complex particles, detailed interiors, and extra environmental audio as later
options. The existing tracker music and its generated assets stay unchanged in this plan.

Acceptance: additional effects support the main silhouettes and stories; they do not
turn small details into distracting pixel noise.

## Technical boundaries and checks

- Keep the existing 15.36-second music slot and 30.72-second storyboard. City activity
  runs at plausible independent speeds; compose key moments against musical phrases.
  Do not make every window, walk cycle, or lamp pulse with the beat.
- Keep authored paths, stable IDs, schedules, and evaluated object states in the shared
  scene layer. `waterfront_scene.[ch]` already owns camera/time evaluation, but traffic
  placement currently repeats in GPU drawing/light code and the CPU renderer. Consolidate
  the relevant evaluation before adding interacting actors.
- Make actor and window states functions of explicit time and stable IDs. Seek directly
  into waiting, walking, braking, and light-change intervals without simulation warm-up.
  Current GPU light selection favours previously selected lights; review that history
  dependence when adding lights and test direct capture versus continuous playback.
- Preserve the CPU reference as a useful working mode. New visual embellishments may be
  GPU-only; shared geometry changes must have reviewed CPU baseline changes, not blind
  checksum updates. Do not require complete feature parity.
- Batch static props and reuse actor meshes. Introduce instancing only where profiling
  supports it. Keep window geometry resident and update compact state for active rooms.
- The present local-light cap is 32. Thousands of windows and ferry portholes should be
  emissive surfaces; select only the nearby lights that need to illuminate other objects.
- Use the same evaluated transforms in colour, shadow, and reflection passes. Measure
  the added work in all passes; collect a baseline before expanding scene complexity.
- At each implemented stage, build and run CTest. Capture times 0, 2.5, 5, 10, and 15 at
  320x200 and 320x240, plus a 320x180 framing check and event-specific frames. Inspect the
  full short and long routes in motion. GPU checks need a working graphics context.
- Compare fresh direct seeks with sequential playback and backward/forward seeks. Check
  route transitions, light selection, shadow/reflection coherence, and camera clearance.
  Report CPU submission and GPU timing separately where available, plus draw counts and
  memory growth; establish a performance target from measurements on the target machine.

Recommended first implementation sequence: execution steps 01–04 establish coverage,
density, and one complete street corner. Review these still frames before adding the new
moving cast. Reserve a place for the ferry during the initial composition audit, then
test its proxy in step 09 before modelling the vessel in detail.
