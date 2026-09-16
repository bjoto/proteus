#!/usr/bin/env python3
"""Prepare a flat-shaded storyboard from the retained OSM snapshot, entirely offline.

Requires pyproj 3.8.0 and shapely 2.1.2. Normal C builds use the checked-in output.
This deliberately uses an assumed flat quay at 3.5 m; it is not a surveyed city model.
"""
import argparse
import gzip
import hashlib
import json
import math
from pathlib import Path

from pyproj import Transformer
from shapely import constrained_delaunay_triangles
from shapely.geometry import LineString, Point, Polygon, box
from shapely.geometry.polygon import orient
from shapely.ops import unary_union

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs/eriksberg/osm-survey.json.gz"
EXPECTED_HASH = "9349d7014692dba93203e03d842b9b1402a481f77503514cb68f38902eb23b54"
GROUND = 3.5
CROP = box(-400, -400, 550, 500)


def join_chains(chains):
    chains = [list(c) for c in chains if c]
    while True:
        merged = False
        for i, a in enumerate(chains):
            for j in range(i + 1, len(chains)):
                b = chains[j]
                if a[-1] == b[0]:
                    c = a + b[1:]
                elif a[-1] == b[-1]:
                    c = a + list(reversed(b))[1:]
                elif a[0] == b[-1]:
                    c = b + a[1:]
                elif a[0] == b[0]:
                    c = list(reversed(b)) + a[1:]
                else:
                    continue
                chains[i] = c
                chains.pop(j)
                merged = True
                break
            if merged:
                break
        if not merged:
            return chains


def polygons(geometry):
    if geometry.is_empty:
        return []
    if geometry.geom_type == "Polygon":
        return [geometry]
    if not hasattr(geometry, "geoms"):
        return []
    return [p for g in geometry.geoms for p in polygons(g)]


def triangulate(poly):
    triangles = list(constrained_delaunay_triangles(poly).geoms)
    assert abs(sum(t.area for t in triangles) - poly.area) < max(1e-5, poly.area * 1e-8)
    assert all(poly.covers(t.representative_point()) for t in triangles)
    return triangles


def self_test():
    # Area preservation and an empty courtyard must survive triangulation.
    concave = Polygon([(0, 0), (6, 0), (6, 2), (2, 2), (2, 6), (0, 6)])
    courtyard = Polygon([(0, 0), (10, 0), (10, 10), (0, 10)],
                        [[(3, 3), (7, 3), (7, 7), (3, 7)]])
    for shape in [concave, courtyard]:
        result = unary_union(triangulate(shape))
        assert result.symmetric_difference(shape).area < 1e-8
    assert building_height({"height": "17"}, 10) == (17.0, "height")
    assert building_height({"building:levels": "6"}, 10) == (19.0, "floors * 3 + 1")
    assert building_height({}, 10) == (4.0, "small-footprint fallback")
    assert building_height({}, 200) == (16.0, "fallback")
    assert building_height({"height": "21,5"}, 200) == (21.5, "height")
    # A taller part wins its overlap; lower parts and uncovered parent area remain.
    parent = box(0, 0, 10, 10)
    parts = [(box(0, 0, 6, 10), 20, "a"), (box(4, 0, 8, 10), 30, "b")]
    resolved = resolve_parts(parent, parts)
    assert abs(sum(shape.area for shape, _, _ in resolved) - 100) < 1e-8
    assert next(shape.area for shape, _, key in resolved if key == "b") == 40
    assert next(shape.area for shape, _, key in resolved if key == "a") == 40
    assert next(shape.area for shape, _, key in resolved if key is None) == 20


def building_height(tags, area):
    if "height" in tags:
        value = float(tags["height"].replace(",", "."))
        if not math.isfinite(value) or value <= 0 or value > 200:
            raise ValueError(f"Invalid building height: {tags['height']}")
        return value, "height"
    if "building:levels" in tags:
        floors = float(tags["building:levels"])
        if not math.isfinite(floors) or floors <= 0 or floors > 60:
            raise ValueError(f"Invalid floor count: {floors}")
        return floors * 3 + 1, "floors * 3 + 1"
    if area < 80 or tags.get("building") in {"garage", "garages", "shed"}:
        return 4.0, "small-footprint fallback"
    return 16.0, "fallback"


def resolve_parts(parent, parts):
    """Return disjoint ground-based parts, highest first, plus the uncovered parent."""
    remaining = parent
    result = []
    for shape, height, key in sorted(parts, key=lambda item: (-item[1], item[2])):
        clipped = shape.intersection(remaining)
        if clipped.area > 1e-6:
            result.append((clipped, height, key))
        remaining = remaining.difference(shape)
    if remaining.area > 1e-6:
        result.append((remaining, None, None))
    return result


def prepare(output):
    raw = gzip.decompress(SOURCE.read_bytes())
    assert hashlib.sha256(raw).hexdigest() == EXPECTED_HASH, "Unexpected source snapshot"
    data = json.loads(raw)
    elements = data["elements"]
    transform = Transformer.from_crs(4326, 3007, always_xy=True)
    ox, oy = transform.transform(11.9089495, 57.6973921)

    def coordinates(geometry):
        return [(x - ox, z - oy) for p in geometry if p and "lat" in p
                for x, z in [transform.transform(p["lon"], p["lat"])]]

    def footprint(e):
        if e["type"] == "way":
            points = coordinates(e.get("geometry", []))
            if len(points) < 4 or points[0] != points[-1]:
                raise ValueError(f"Incomplete building {e['id']}")
            return Polygon(points)
        outers = join_chains([coordinates(m.get("geometry", [])) for m in e["members"]
                             if m.get("role") == "outer"])
        inners = join_chains([coordinates(m.get("geometry", [])) for m in e["members"]
                             if m.get("role") == "inner"])
        assert all(r[0] == r[-1] for r in outers + inners), e["id"]
        shape = unary_union([Polygon(r) for r in outers])
        if inners:
            shape = shape.difference(unary_union([Polygon(r) for r in inners]))
        return shape

    river = next(e for e in elements if e["type"] == "relation" and e["id"] == 3329487)
    banks = sorted(join_chains([coordinates(m.get("geometry", [])) for m in river["members"]]),
                   key=len, reverse=True)
    a, b = banks[:2]
    if math.dist(a[-1], b[0]) > math.dist(a[-1], b[-1]):
        b = list(reversed(b))
    # The retained river relation is clipped. Its two artificial closing edges must
    # remain completely outside our crop; never let a guessed edge become the quay.
    assert not LineString([a[-1], b[0]]).intersects(CROP)
    assert not LineString([b[-1], a[0]]).intersects(CROP)
    water = Polygon(a + b)
    assert water.is_valid, "Invalid river-bank assembly"
    water = water.intersection(CROP)
    land = CROP.difference(water)

    vertices, triangles, lookup = [], [], {}

    def triangle(points, color):
        ids = []
        for p in points:
            p = tuple(round(v, 4) for v in p)
            if p not in lookup:
                lookup[p] = len(vertices)
                vertices.append(p)
            ids.append(lookup[p])
        triangles.append((ids, color))

    def surface(shape, y, color):
        for poly in polygons(shape):
            for t in triangulate(poly):
                # Clockwise in X/Z means upward-facing in X/Y/Z.
                pts = list(orient(t, sign=-1).exterior.coords)[:3]
                triangle([(x, y, z) for x, z in pts], color)

    def walls(shape, bottom, top, color):
        for poly in polygons(shape):
            poly = orient(poly, sign=1)
            for ring in [poly.exterior, *poly.interiors]:
                for (x, z), (xx, zz) in zip(ring.coords, list(ring.coords)[1:]):
                    a, b, c, d = (x, bottom, z), (xx, bottom, zz), (xx, top, zz), (x, top, z)
                    triangle([a, c, b], color)
                    triangle([a, d, c], color)

    surface(water, 0, (45, 79, 104))
    surface(land, GROUND, (135, 139, 144))
    walls(land, 0, GROUND, (94, 102, 114))
    buildings = [e for e in elements if "building" in e.get("tags", {})]
    members = {m["ref"] for e in buildings if e["type"] == "relation"
               for m in e["members"] if m["type"] == "way"}
    parts = []
    for e in elements:
        if "building:part" not in e.get("tags", {}) or "bridge:support" in e["tags"]:
            continue
        shape = footprint(e)
        if not shape.intersects(CROP):
            continue
        height, _ = building_height(e["tags"], shape.area)
        bottom = float(e["tags"].get("min_height", "0").replace(",", "."))
        assert math.isfinite(bottom) and 0 <= bottom < height, e["id"]
        parts.append((e, shape.intersection(CROP), height, bottom))
    manifest_buildings, solids = [], []
    for e in sorted(buildings, key=lambda e: (e["type"], e["id"])):
        if e["tags"].get("man_made") == "crane" or (e["type"] == "way" and e["id"] in members):
            continue
        shape = footprint(e)
        if not shape.intersects(CROP):
            continue
        assert shape.is_valid, f"Invalid footprint {e['id']}"
        shape = shape.intersection(CROP)
        if shape.area < 1:
            continue
        height, basis = building_height(e["tags"], shape.area)
        tint = [(166, 173, 195), (185, 183, 191), (154, 166, 188), (190, 177, 163)][e["id"] % 4]
        # Parts remain within their parent footprint. Use original unsimplified outlines
        # for partitioning so shared edges do not grow slivers from separate simplification.
        matching = [(p, h, part["id"]) for part, p, h, bottom in parts
                    if bottom == 0 and shape.intersection(p).area > p.area * .9]
        for section, part_height, _ in resolve_parts(shape, matching):
            top = GROUND + (height if part_height is None else part_height)
            walls(section, GROUND, top, tint)
            surface(section, top, tuple(max(0, c - 30) for c in tint))
        for part, p, part_height, bottom in parts:
            if bottom > 0 and shape.intersection(p).area > p.area * .9:
                section = p.intersection(shape)
                walls(section, GROUND + bottom, GROUND + part_height, tint)
                surface(section, GROUND + part_height, tuple(max(0, c - 30) for c in tint))
        solids.append(shape)
        manifest_buildings.append({"osm": f"{e['type']}/{e['id']}", "height": height,
                                   "basis": basis, "courtyards": sum(len(p.interiors) for p in polygons(shape)),
                                   "parts": [key for _, _, key in matching]})
    solid_union = unary_union(solids)
    roads = []
    for e in elements:
        tags = e.get("tags", {})
        if "highway" not in tags or tags.get("bridge") == "yes" or tags.get("tunnel") == "yes":
            continue
        pts = coordinates(e.get("geometry", []))
        if len(pts) < 2:
            continue
        width = 2 if tags["highway"] in {"path", "footway", "steps", "cycleway"} else 6
        roads.append(LineString(pts).buffer(width / 2, cap_style=2, join_style=2))
    road_union = unary_union(roads)
    surface(road_union.intersection(land).difference(solid_union), GROUND + .03, (83, 89, 99))

    # Authored landscape zones, not additional surveyed buildings. Keep mapped paths,
    # building entrances, and water clear, and turn unused slabs into legible gardens.
    gardens = [("west park", Polygon([(-275, -65), (-155, -65), (-120, 25),
                                      (-140, 87), (-268, 82)]))]
    gardens += [(f"west dock garden {i}", box(-62 - i * 3.5, 79 + i * 33,
                                              -33 - i * 3.5, 90 + i * 33))
                for i in range(7)]
    gardens += [(f"east dock garden {i}", box(57 - i * 3, 90 + i * 33,
                                              83 - i * 3, 100 + i * 33))
                for i in range(5)]
    garden_manifest = []
    for name, zone in gardens:
        shape = zone.intersection(land).difference(solid_union.buffer(1)).difference(
            road_union.buffer(1.2)).simplify(.15, preserve_topology=True)
        surface(shape, GROUND + .08, (72, 105, 79))
        garden_manifest.append({"name": name, "bounds": list(zone.bounds),
                                "area": round(shape.area, 2), "basis": "authored landscape"})
        # Small low planting islands provide volume without introducing street props.
        for poly in polygons(shape.buffer(-2.5)):
            if poly.area < 10:
                continue
            walls(poly, GROUND + .08, GROUND + .65, (61, 91, 69))
            surface(poly, GROUND + .65, (72, 105, 79))
        # Coarse park canopy masses establish landscape scale; detailed street trees
        # and furniture belong to the later street-corner pass. Positions are authored.
        trees = 0
        minx, minz, maxx, maxz = zone.bounds
        for row, z in enumerate(range(math.ceil(minz + 6), math.floor(maxz - 4), 17)):
            for col, x in enumerate(range(math.ceil(minx + 6), math.floor(maxx - 4), 19)):
                x += (row % 2) * 4
                z_offset = z + (col % 3) * 1.5
                center = Point(x, z_offset)
                if not shape.buffer(-4).covers(center):
                    continue
                trees += 1
                trunk = center.buffer(.4, quad_segs=2)
                walls(trunk, GROUND + .08, GROUND + 4, (95, 78, 65))
                radius = 3.2 + (col + row) % 3 * .4
                ring = [(x + radius * math.cos(i * math.tau / 6), GROUND + 4.5,
                         z_offset + radius * math.sin(i * math.tau / 6)) for i in range(6)]
                for i in range(6):
                    triangle([ring[i], (x, GROUND + 7.5, z_offset), ring[(i + 1) % 6]],
                             (72, 105, 79))
                    triangle([ring[(i + 1) % 6], (x, GROUND + 2.5, z_offset), ring[i]],
                             (61, 91, 69))
        garden_manifest[-1]["canopies"] = trees

    output.mkdir(parents=True, exist_ok=True)
    lines = ["// Generated by tools/prepare_waterfront.py. Do not edit.",
             "// Map data (c) OpenStreetMap contributors, ODbL 1.0; see manifest.json.",
             "static const Vector3 waterfront_vertices[] = {"]
    lines += ["    { " + ", ".join(f"{v:.4f}f" for v in p) + " }," for p in vertices]
    lines += ["};", "static const Triangle3D waterfront_triangles[] = {"]
    lines += [f"    {{ {{ {i[0]}, {i[1]}, {i[2]} }}, {{ {c[0]}, {c[1]}, {c[2]}, 255 }} }},"
              for i, c in triangles]
    lines += ["};", ""]
    content = "\n".join(lines)
    (output / "geometry.h").write_text(content)
    manifest = {"source": "docs/eriksberg/osm-survey.json.gz", "source_sha256": EXPECTED_HASH,
                "snapshot": data["osm3s"]["timestamp_osm_base"], "origin_epsg3007": [ox, oy],
                "license": "ODbL-1.0", "license_url": "https://opendatacommons.org/licenses/odbl/1-0/",
                "attribution": "Map data © OpenStreetMap contributors",
                "attribution_url": "https://www.openstreetmap.org/copyright",
                "ground": "Assumed flat quay at 3.5 m above water; terrain samples not used",
                "crop": list(CROP.bounds), "vertices": len(vertices), "triangles": len(triangles),
                "geometry_sha256": hashlib.sha256(content.encode()).hexdigest(),
                "buildings": manifest_buildings, "landscape": garden_manifest,
                "building_parts": [{"osm": f"{e['type']}/{e['id']}", "height": h,
                                    "min_height": bottom} for e, _, h, bottom in parts]}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Prepared {len(manifest_buildings)} buildings, {len(vertices)} vertices, {len(triangles)} triangles")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "assets/geometry/eriksberg")
    args = parser.parse_args()
    self_test()
    prepare(args.output)
