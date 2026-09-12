# Waterfront fly-through plan

Status: Route A evening city-life pass integrated into `cube` (2026-09-12).
The shortened 15.36-second route uses the existing two-pattern slot. A separate
`waterfront_storyboard` tool retains the full 30.72-second route for comparison.
Offline preparation, bundled geometry, sampled proxy clearance, and deterministic
seek checks are implemented. The evening-life pass adds facade windows, street lamps, two-way traffic and a launch
with a wake. Camera framing now prioritizes city life, with a muted background crane.
Broader surface materials and water reflections remain open.

See [the coverage report and camera sketches](eriksberg/README.md). Route A, Monsungatan to
the river, is implemented with sampled clearance checks against the proxy geometry.

The planning sections below retain the original milestone proposals. Current behavior and
remaining rendering limitations are documented in [the renderer notes](3D.md).

## Direction we have agreed on

Create a small, recognizable waterfront district using a fixed snapshot of open geographic
data. Interpret it through Proteus's low-resolution style: broad silhouettes, crisp textures,
restricted palette shading, and expressive moving light. The camera should travel from the
city toward a reveal over water, with a landmark or bridge providing a focal point.

Download and prepare data offline. Bundle the prepared scene so the demo runs without network
access. Preserve CPU rendering, deterministic seeking, headless screenshots, and integer-scaled
presentation. Spend additional computation on the scene and lighting at 320x200.

This extends the current city scene. It does not require a general map viewer, a whole-city
simulation, or a GIS library in the C runtime.

## Decisions to make together

| Decision | Starting proposal | Status |
| --- | --- | --- |
| Region | In or around Gothenburg | Agreed |
| Location | Eriksberg waterfront near the crane | Agreed |
| Exact crop and route | Route A: Monsungatan approach and crane reveal; 440 × 530 m detail plus 950 × 900 m context | Agreed; 3D framing and clearance remain |
| Geographic scope | Roughly four to eight blocks, adjacent water, and one visual landmark | Provisional; route determines final extent |
| Mood | Cool dusk with warm lights and dark water | Proposed |
| Fidelity | Preserve street layout, shoreline, major heights, and landmark silhouette; simplify small details | Proposed |
| Duration | Compare a shortened 15.36 s route with the full 30.72 s route | Route A selected; arrangement unchanged |
| First animated subject | One boat, or the existing flying beacon adapted to the route | Open |
| Reflections | Begin with an inexpensive stylized treatment; assess a CPU planar reflection later | Proposed |

The user has selected Eriksberg in Gothenburg. Dataset availability, useful height coverage,
and a good camera route must now be checked at the neighbourhood scale before committing to
an exact crop or importer.

### Chosen setting: Eriksberg

The selected shot follows Route A: approach between blocks along Monsungatan, reveal the quay
and crane, then glide over the water. The crane is a background landmark: preserve its
recognizable silhouette and negative space at 320x200, with a small authored mesh if the
geographic data cannot supply it.

The location and route are agreed; 3D framing, clearance, and timing remain to be validated.
Keep the extract local to the crane and approach, with only enough surrounding geometry to
support the skyline. Lindholmen is outside the current scope.

Reference: [Göteborg & Co's Eriksberg and Lindholmen guide](https://www.goteborg.com/en/guides/eriksberg-lindholmen).
Investigate [Göteborg's open geodata catalogue](https://catalog.goteborg.se/store/6/resource/971)
and its [downloadable terrain model](https://opengeodata.goteborg.se/files/hojdmodell/2022/hojdmodell_2022.html)
alongside the base map sources. Availability of a city 3D product does not establish that it
is open or suitable for bundling; verify the specific extract and terms during source selection.

## Camera story

Three visual beats, initially fitted into the existing two-pattern slot:

1. **Approach:** move through an open street with foreground architecture establishing scale.
2. **Reveal:** turn or rise past the last building to expose the shoreline and water.
3. **Departure:** glide along or over the water, framing a bridge, landmark, or skyline.

Keep the water visible long enough to register at 320x200. Avoid spending most of the shot
looking straight down at roads. Use foreground, middle distance, and background silhouettes
to make movement readable. A route can end at the scene transition; it need not return along
an artificial orbit. Seeking and repeating the demo must still be well-defined.

Exact timings and camera height remain editable. If 15.36 seconds feels rushed, first compare
a longer storyboard before changing music or the timeline arrangement.

## Milestone 1: inspect Eriksberg and frame the shot

- Compare two or three camera approaches within Eriksberg for shoreline shape, street
  approach, elevation changes, the crane reveal, and open sightlines.
- Inspect actual source coverage: footprints, water polygons, roads, heights or floor counts,
  and any terrain needed to explain the site.
- Prefer one base source, supplementing a specific gap only when it matters in the shot.
- Draw the geographic crop and a proposed camera path, including a margin for skyline geometry.
- Identify which landmark details need a small authored mesh rather than automatic extrusion.

Review artifact: a comparison of camera approaches, source links, and a map/storyboard with
three camera positions. Success means we can explain why the chosen route makes a good shot,
and know which important facts are supplied, missing, or intentionally stylized.

Candidate sources to inspect:

- [OpenStreetMap](https://www.openstreetmap.org/): streets, footprints, shoreline and landmarks;
  [3D building tags](https://wiki.openstreetmap.org/wiki/Simple_3D_Buildings) where mapped.
- [Overture buildings](https://docs.overturemaps.org/schema/reference/buildings/building/):
  an alternative or supplemental building source, with optional height and floor information.
- [Lantmäteriet](https://www.lantmateriet.se/sv/geodata/vara-produkter/): investigate applicable
  Swedish terrain and building products after choosing an area.
- [Göteborg open geodata](https://catalog.goteborg.se/store/6/resource/971): inspect local
  terrain and mapping coverage for the chosen waterfront.

These are candidates, not a commitment to combine all four.

## Milestone 2: reproducible asset preparation

Create a small offline preparation tool, likely Python, which converts the chosen extract into
simple scene data. Keep downloading separate from conversion: normal builds and rendering
must not fetch or refresh geographic data.

Record a source manifest alongside the scene:

- Provider, dataset release or snapshot timestamp, query or bounding box, and retrieval URL.
- Source file checksums, license/attribution information, and how the exact input is retained.
- Coordinate reference system, local origin, metre scale, and elevation convention.
- Converter version, simplification parameters, and deterministic fallback seed.
- Authored overrides and the source feature IDs they affect.

Use local metre coordinates near the scene origin, Y-up. Handle polygon winding, concave
footprints, courtyard holes, and multipart features during preparation. Triangulate and
simplify offline. Distinguish height above ground from absolute roof elevation. Missing
heights should use stable, explicit defaults; preserve which heights were inferred.

Keep water, land, roads, and buildings as separate semantic groups. Fix crossings and height
ordering deliberately rather than allowing coplanar road/water surfaces to fight in depth.
Do not combine overlapping building sources without choosing which features take precedence.

Proposed output: a compact versioned scene asset containing indexed geometry, material IDs,
object bounds, and source IDs, plus the manifest and separate authored overrides. Pick the
exact file representation after seeing the extract. Retain the small source snapshot where
its size and redistribution terms permit; a mutable URL alone is not reproducible input.

Review artifact: one frozen extract and a generated flat-shaded blockout. Success means
conversion can run twice from the same retained inputs and produce identical output, with no
network access. The packaged demo only needs the prepared output.

## Milestone 3: geography and camera prototype

- Load the simplified scene and render it through the CPU triangle path.
- Add only the renderer support needed for the asset: the current generic mesh path is
  flat-colored, while indexed textures are currently exposed only for boxes.
- Establish shoreline, simple water, buildings, and the landmark before surface detail.
- Author the three-beat camera path as a function of scene-local time, with smooth speed and
  orientation. Check clearance against buildings and terrain along the entire path.
- Simplify distant geometry and retain a modest skyline margin beyond the main crop.

Review artifact: screenshots at approach, reveal, and departure, plus an animated preview.
Success means the place and reveal read at native 320x200 even without detailed textures.
This is the first major visual review; revise the crop and camera here while changes are cheap.

## Milestone 4: surface character and water

- Apply a small family of palette materials based on building character, not random colors.
- Preserve broad facade shapes; add roof silhouettes and a few street-level cues.
- Give water a readable palette, deterministic ripples, and a restrained highlight treatment.
- Compare a stylized reflection with the cost and appearance of a simplified reflected scene.
  If actual planar reflections are added, clip reflected geometry correctly at the water plane.
- Check texture shimmer in motion before adding finer detail; consider simple mip levels if
  they improve the shot without softening the intended appearance.

Review artifact: the same camera frames with materials and water. Success means water reads
as water and the city remains legible during motion, without relying on higher resolution.

## Milestone 5: light and one moving subject

- Reuse the existing moving point light where it supports the camera story.
- Add one boat or beacon with a path derived directly from scene time.
- Let its light cross the quay, a facade, or the water to create a clear moving focal point.
- Assess one shadow-casting light only after the base composition works. The current point
  light has no occlusion or shadows, and emissive surfaces do not emit illumination themselves.
- Make reflections and shadows respect the same palette and low-resolution treatment.

Review artifact: an animated pass with a simple light-on/light-off comparison. Success means
the animation adds a readable event to the reveal and remains reproducible when seeking.
Traffic timetables, weather datasets, crowds, and multiple lights can wait for later iterations.

## Performance and validation throughout

Working target: keep optimized CPU scene generation comfortably inside a 60 Hz frame budget
on the development machine. Aim provisionally for p95 below 8 ms to leave presentation and
other demo work room; re-evaluate against a representative shot, not just the current blockout.
This is a development target, not a portability guarantee.

- Benchmark the entire route with fixed times, warm-up, resolution, and build type.
- Track scene generation separately from texture upload and presentation; record geometry
  counts and memory footprint when the imported scene is first available.
- Profile before adding complex culling, caching, SIMD, or threads. Simple object bounds and
  offline simplification are likely useful first steps as geographic scope grows.
- Test importer geometry using representative concave footprints, holes, and missing heights.
- Test malformed asset handling and valid geometry/depth behavior when introducing loading.
- Capture the three main camera beats at 320x180, 320x200, and 320x240.
- Check a full animated route for collisions, clipping gaps, shimmer, and abrupt motion.
- Verify repeated screenshots and direct seeks agree on a given build; retain source snapshots
  and conversion settings so asset regeneration is also reproducible.
- Keep build and CTest checks passing; inspect visual differences before changing checksums.

## Next review

Review the integrated short route with `./build/proteus --scene cube` and compare the
long route with `./build/waterfront_storyboard --route long`. The accepted city-life pass
prioritizes street traffic and lit apartments, with the crane as background setting.
The current flat quay and fallback heights are explicit artistic assumptions. The full
route remains a storyboard option; the music arrangement has not changed.
