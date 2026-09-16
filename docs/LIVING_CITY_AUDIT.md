# Living city baseline: execution step 01

Captured 2026-09-16, based on `e906687` with review-tool additions only. No scene
geometry, camera, shader output, or music changed in this step.

## Findings and next changes

1. **Restore the missing stepped apartments beside Monsungatan.** The snapshot contains
   46 `building:part` features, none consumed by the importer. In particular, ways
   503087590–503087622 contain 28 parts of the west dock frontage, at roughly
   X −94 to −24, Z 57 to 308. Their heights range from 19 to 36 m, but parent footprints
   currently receive the 16 m fallback. Part 503087591 uses `21,5`, which needs explicit
   decimal-comma handling. Resolve overlapping parts and retain uncovered parent regions;
   do not draw duplicate coplanar facades/roofs. Respect `min_height` on elevated parts.
2. **Give the open ground an identity.** Short-route t=0, 2.5, and 5 show large textured
   slabs around the street. The retained map shows genuine open land west of the towers
   (roughly X −280 to −115, Z −90 to 90), and gaps between dockside apartments. Preserve
   those openings. Add restrained planted areas and courtyard surfaces, clipped to land
   and away from mapped roads/buildings. Keep detailed furniture and storefronts for 04.
3. **Continue the district behind the first row.** The short-route look-back at t=10/15
   and long-route t=23.04/30.72 reveal a low, abruptly ending background. Current bounds
   are X −400..550, Z −400..500; the camera far plane is 1400 m. Inspect how far the retained
   snapshot can safely extend this crop before adding authored distant masses. Preserve
   real water and the north-bank street layout; avoid a rectangular ring of fake towers.
4. **Preserve foreground-to-water contrast.** The crane and dock are recognizable in the
   late frames. More enclosure should come from corrected nearby heights and background
   continuity, keeping the basin and river open. The long route exposes more shoreline
   and context than the short one and must be checked after both changes.
5. **Reserve ferry composition work.** The river is visible to the side of the crane in
   the middle turn and foreground of the final long view. A ferry near the departure
   camera could be behind the viewer. Step 09 must check an actual proxy in these views.

The existing 204 building features comprise 38 explicit heights, 62 floor-count heights,
47 general 16 m fallbacks, and 57 small-footprint fallbacks. Feature count is misleading:
small sheds count equally with apartment blocks, and omitted parts hide real roof variation.
The retained coverage map (`docs/eriksberg/coverage.png`) was inspected against both routes.

## Baseline artifacts and verification

- `build/living-city/before/`: GPU captures at short times 0, 2.5, 5, 10, 15 and long
  times 0, 7.68, 15.36, 23.04, 30.72, each at 320x200 and 320x240. Contact sheets of all
  these views were inspected. Both routes also have a 320x180 capture at t=10.
- `tools/living_city_review.py --output DIR` reproduces captures and a 923-frame profile
  after 120 warm-up frames. `--compare BEFORE_DIR` makes matching two-column contact
  sheets (before left, current right). Use `python3` from the repository root.
- `build/living-city/before/capture.log`: Debug, hidden GPU context, 320x200, shadows and
  reflections enabled. CPU submission mean 1.317 ms, p95 1.405 ms. These are submission
  timings, not GPU execution time or an FPS guarantee. Baseline city geometry is 7,388
  indexed source vertices and 9,083 triangles; GPU conversion duplicates face vertices.
  Total GPU memory, GPU timing, and draw-call counts are not instrumented by this profiler.
- Added `waterfront_storyboard --renderer gpu`, keeping CPU as the tool's default.
  GPU stills use the same scene evaluation, rendering, and export orientation as the app.
  Windowed short (15.36 s) and long (30.72 s) passes completed successfully. Automated
  access does not provide human observation of the live window; visual assessment here
  uses captured frames. Human motion review remains useful for subtle temporal artifacts.
- Configure/build completed; CTest passed 13/13 with `PROTEUS_GPU_TESTS=ON`. Enabling
  the GPU test initially found stale CPU sky-band assumptions in its GPU checks. The
  check now recognizes the GPU's existing row gradient, retaining the coverage bounds
  and independent depth/orientation fixture. No renderer change was needed.
- The route validator passes 2 m clearance against mapped geometry at 120 samples/s
  for both routes and checks CPU direct-seek consistency at all resolutions.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DPROTEUS_GPU_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
python3 tools/living_city_review.py --output build/living-city/before
./build/waterfront_storyboard --renderer gpu --route short
./build/waterfront_storyboard --renderer gpu --route long
```

GPU commands require access to the macOS graphics session outside the restricted sandbox.
Generated captures/logs remain under ignored `build/`; the original root PNGs are untouched.
