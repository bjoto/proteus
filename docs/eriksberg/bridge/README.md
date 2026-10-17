# Älvsborgsbron river expansion survey

This is the coverage proposal for extending `boat_study` from Eriksberg to
Älvsborgsbron. Runtime geometry, navigation boundaries, and the main demo are unchanged.

![Proposed river extension](coverage.png)

[Vector map](coverage.svg) · [Machine-readable findings](summary.json)

## Recommended coverage

Keep the crane origin and EPSG:3007 metre frame. Coordinates here are east/north;
boat simulation X points west, so negate it when converting to map coordinates.

| Coverage | West, south, east, north relative to crane | Size |
| --- | --- | --- |
| Current runtime crop | −700, −600, +950, +800 m | 1,650 × 1,400 m |
| Proposed runtime crop | −1,500, −1,600, +950, +800 m | 2,450 × 2,400 m |
| Download envelope | Approximately −1,900, −2,200, +1,250, +1,100 m | Source margin around proposal |

The proposed crop extends 800 m west and 1,000 m south. The mapped bridge outline
occupies approximately east −592…−255 m, north −1,191…−297 m. The current southern
boundary therefore cuts through the bridge area. The proposed crop contains the
entire mapped deck and space to sail beyond it.

The route drawn on the map starts at the existing dock spawn and travels about
1,649 m to a turnaround point. It crosses the mapped bridge deck around (−454, −678) m
and continues to (−1,100, −950) m. The complete route is inside the proposed water
polygon with at least 27.6 m of mapped shore/crop clearance. These are geometric
checks of a planning polyline, not a simulated voyage or verified overhead clearance.

## Source and reproducibility

Map data © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright),
[ODbL 1.0](https://opendatacommons.org/licenses/odbl/1-0/).

`osm-survey.json.gz` is a separate planning extract retrieved on 2026-10-17 from the
[OSM map API](https://api.openstreetmap.org/api/0.6/map.json?bbox=11.8770534,57.6776116,11.8946856,57.6875018).
It does not replace the original Eriksberg snapshot or its pinned hash. The source
envelope is longitude 11.8770534…11.9299499, latitude 57.6776116…57.7072822.

The envelope was divided into a 3 × 3 grid at longitude 11.8946856, 11.9123177
and latitude 57.6875018, 57.6973920. The southeast tile exceeded the API's 50,000-node
limit and was subdivided into four, at longitude 11.9211338 and latitude 57.6825567.
Each tile used `https://api.openstreetmap.org/api/0.6/map.json?bbox=west,south,east,north`.
Tiles were downloaded sequentially in time across several requests, not as an atomic
historical snapshot. Embedded tile bounds and raw-response SHA-256 hashes record provenance.

Responses were merged by element type/ID, preferring the highest version and requiring
equal contents for duplicate versions. Nodes were expanded into way geometry. The
retained subset includes buildings/parts, roads/paths, waterways, water/coastline,
bridges/supports, and cranes. Relation members include available way geometry; missing
members remain explicit. User identities and changeset IDs were omitted. The compressed
extract contains 16,895 features; its decompressed SHA-256 is pinned by `survey.py`.

Recreate all survey artifacts offline using Python with pyproj 3.8.0, shapely 2.1.2,
and matplotlib 3.10.8:

```sh
python docs/eriksberg/bridge/survey.py
```

The script reads the current crop from `tools/prepare_waterfront.py`, checks the source
hash, assembles the two main outer banks of river relation 3329487, and verifies:

- Artificial closing edges and omitted bank fragments lie outside the proposed crop.
- Any polygon repair preserves all mapped bank segments inside the proposed crop.
- The bridge outline lies inside the crop.
- The entire route has at least 8 m of water clearance and crosses the mapped bridge deck.

The map shows building way outlines for context. They are not a deduplicated building
inventory; relation-only buildings, courtyards, terrain, and detailed vegetation need
the runtime importer's handling in the next stage.

## Landmark findings

Use [bridge relation 5776295](https://www.openstreetmap.org/relation/5776295) for the
landmark and [way 387711139](https://www.openstreetmap.org/way/387711139) for its mapped
outline. The relation includes road/path alignments and pylon members. Avoid importing
the tagged bridge supports as ordinary solid building extrusions: the tall pylons
need open, authored geometry.

The relation tags say suspension, height 107.6 m, length 900.6 m, and marine clearance
45 m. The outline instead says cable-stayed. These tags are unverified and partly
contradictory; use the supplied photograph for the suspension silhouette and verify
structural dimensions before treating them as modeling measurements. Nearby tower
node 2495322719 also has a 331 m aviation-related height tag; do not use it as the
bridge's structural height. Path `ele=50` is not a deck profile or clearance survey.

## Next implementation stage

1. Adapt the importer to accept a separately pinned source and crop, preserving the
   existing Eriksberg output as the default. Keep the larger scene opt-in for the boat
   until the existing demo and city dressing have been checked against it.
2. Produce water/land and coarse buildings. Preserve the water-first triangle ordering
   required by `src/boat_map.c`. Handle incomplete unrelated relations explicitly;
   never close missing geometry across the playable crop.
3. Add one shared bridge blockout with deck and open towers, visible in CPU and GPU.
   Treat the deck as elevated geometry, not a cut in the navigable water. Use separate
   support footprints for boat collisions.
4. Add deterministic route captures and a repeatable boat traversal through the bridge.
   Check camera clipping, shore contacts, reflections, shadows, and CPU/GPU frame times.
   The boat camera currently has a 2,200 m far plane; review it from the turnaround
   point before increasing it. Profile linear water/contact queries with the larger mesh.
5. Refine cables, hangers, quays, shoreline rocks, and nearby waterfront buildings after
   the full trip works. Build, run CTest, inspect representative frames, and manually
   sail the route before calling the extension playable.
