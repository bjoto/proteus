"""Authored Älvsborgsbron blockout in the shared CPU/GPU city mesh.

Mapped tower legs, alignment, and approach roads; assumed 45 m deck underside,
3 m deck thickness, 107.6 m tower tops. Road slopes, embankments, cable sag, and
beam sizes are artistic assumptions against the scene's flat 3.5 m ground.
Called by prepare_waterfront.py --area river; no separate runtime dependency.
"""
import math

from shapely.geometry import LineString, Point, Polygon
from shapely.geometry.polygon import orient
from shapely.ops import unary_union
from prepare_waterfront import triangulate


def bridge_layout(elements, coordinates):
    ways = {e["id"]: e for e in elements if e["type"] == "way"}
    def point(way):
        p = Polygon(coordinates(ways[way]["geometry"])).centroid
        return (p.x, p.y)

    north, south = point(1244572192), point(1244572188)
    length = math.dist(north, south)
    along = ((south[0]-north[0])/length, (south[1]-north[1])/length)
    across = (-along[1], along[0])
    outline = coordinates(ways[387711139]["geometry"])
    ts = [(x-north[0])*along[0]+(z-north[1])*along[1] for x, z in outline]
    return ways, north, length, along, across, min(ts), max(ts)


def bridge_approaches(elements, coordinates, land):
    """Reserve grounded approach corridors along the mapped carriageways.

    The 7 m offset puts the shared road near the centre of the two carriageways.
    Elevation is authored against the flat scene, not inferred from OSM ele tags.
    """
    ways, north, length, along, across, start, end = bridge_layout(elements, coordinates)
    chains = [
        ("north", start, [4303432, 1022940008, 1022940007, 155516496, 165809806]),
        ("south", end, [43048092, 1022939987, 1022939986, 25039557, 25039606,
                        1022939983, 1022939982, 39408748]),
    ]
    approaches = []
    for name, t, ids in chains:
        points = []
        for way in ids:
            part = coordinates(ways[way]["geometry"])
            if points:
                assert math.dist(points[-1], part[0]) < .01, "Disconnected mapped approach road"
                part = part[1:]
            points.extend(part)
        source_line = LineString(points)
        line = source_line.offset_curve(7, join_style=2)
        assert line.geom_type == "LineString"
        points = list(line.coords)
        if name == "south":
            # Land the ramp before the existing crop edge; the source road continues.
            clipped = []
            for a, b in zip(points, points[1:]):
                clipped.append(a)
                if b[1] <= -1545:
                    u = (-1545-a[1])/(b[1]-a[1])
                    clipped.append((a[0]+u*(b[0]-a[0]), -1545))
                    break
            points = clipped
        deck_end = (north[0]+t*along[0], north[1]+t*along[1])
        line = LineString([deck_end, *points])
        # The complete landform, not just the road centre, must stay on dry land.
        footprint = line.buffer(47, cap_style=2, join_style=2)
        assert land.covers(footprint), f"{name} approach grading leaves mapped land/crop"
        assert Point(points[-1]).distance(source_line) < 8
        approaches.append({"name": name, "line": line, "footprint": footprint, "ways": ids})
    return approaches, unary_union([a["footprint"] for a in approaches])


def add_bridge(elements, coordinates, triangle, approaches):
    ways, north, length, along, across, start, end = bridge_layout(elements, coordinates)
    steel, concrete, cable = (104, 126, 139), (161, 167, 166), (67, 83, 94)
    asphalt, marking, grass = (70, 78, 88), (192, 190, 164), (72, 105, 79)

    def p(t, side, height):
        return (north[0]+t*along[0]+side*across[0], height,
                north[1]+t*along[1]+side*across[1])

    def beam(a, b, width, depth, color):
        direction = tuple(b[i]-a[i] for i in range(3))
        size = math.sqrt(sum(v*v for v in direction))
        direction = tuple(v/size for v in direction)
        # Horizontal width, with a stable basis for vertical tower legs.
        side = (direction[2], 0, -direction[0])
        side_size = math.hypot(side[0], side[2])
        side = tuple(v/side_size for v in side) if side_size > 1e-6 else (across[0], 0, across[1])
        up = (side[1]*direction[2]-side[2]*direction[1],
              side[2]*direction[0]-side[0]*direction[2],
              side[0]*direction[1]-side[1]*direction[0])
        corners = [tuple(end[i]+s*width*.5*side[i]+u*depth*.5*up[i] for i in range(3))
                   for end in [a, b] for s, u in [(-1, -1), (1, -1), (1, 1), (-1, 1)]]
        # Check face winding against its box centre instead of assuming a basis sign.
        center = tuple((a[i]+b[i])*.5 for i in range(3))
        for face in [(0, 1, 2, 3), (4, 7, 6, 5), (0, 4, 5, 1),
                     (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)]:
            q = [corners[i] for i in face]
            ab, ac = [q[1][i]-q[0][i] for i in range(3)], [q[2][i]-q[0][i] for i in range(3)]
            normal = (ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0])
            if sum(normal[i]*(q[0][i]-center[i]) for i in range(3)) < 0:
                q.reverse()
            triangle([q[0], q[1], q[2]], color)
            triangle([q[0], q[2], q[3]], color)

    def quad(a, b, c, d, color):
        triangle([a, b, c], color)
        triangle([a, c, d], color)

    # Follow the mapped deck limits along the line through the two tower centres.
    beam(p(start, 0, 46.5), p(end, 0, 46.5), 31, 3, steel)
    quad(p(start, -13.5, 48.03), p(start, 13.5, 48.03),
         p(end, 13.5, 48.03), p(end, -13.5, 48.03), asphalt)
    for side in [-13, 0, 13]:
        quad(p(start, side-.12, 48.045), p(start, side+.12, 48.045),
             p(end, side+.12, 48.045), p(end, side-.12, 48.045), marking)
    for side in [-15.5, 15.5]:
        beam(p(start, side, 49), p(end, side, 49), .8, 2, steel)
    for leg in [1244572189, 1244572190, 1244572193, 1356632936]:
        shape = Polygon(coordinates(ways[leg]["geometry"]))
        ring = list(orient(shape, sign=1).exterior.coords)
        for (x, z), (xx, zz) in zip(ring, ring[1:]):
            a, b, c, d = (x, 0, z), (xx, 0, zz), (xx, 107.6, zz), (x, 107.6, z)
            triangle([a, c, b], concrete)
            triangle([a, d, c], concrete)
        # Cap each leg, using the same surface triangulation as the city importer.
        for t in triangulate(shape):
            pts = list(orient(t, sign=-1).exterior.coords)[:3]
            triangle([(x, 107.6, z) for x, z in pts], concrete)
    for t in [0, length]:
        beam(p(t, -17, 104.6), p(t, 17, 104.6), 4, 6, concrete)
        beam(p(t, -17, 41), p(t, 17, 41), 4, 4, concrete)
    for side in [-15, 15]:
        for lo, hi, sections in [(start, 0, 12), (0, length, 28), (length, end, 12)]:
            def height(t):
                if lo == 0:
                    u = t/length
                    return 56 + (107.6-56)*(2*u-1)**2
                u = (t-lo)/(hi-lo)
                return (49*(1-u)+107.6*u if hi == 0 else 107.6*(1-u)+49*u) - 9*4*u*(1-u)
            for i in range(sections):
                t, tt = lo+(hi-lo)*i/sections, lo+(hi-lo)*(i+1)/sections
                beam(p(t, side, height(t)), p(tt, side, height(tt)), .7, .7, cable)
                if i and height(t) > 49.5:
                    beam(p(t, side, 49.5), p(t, side, height(t)), .35, .35, cable)

    # A solid earth embankment supports each descending road slab. At the bridge
    # end, a concrete abutment reaches the deck underside instead of leaving air.
    for approach in approaches:
        line = approach["line"]
        count = math.ceil(line.length/10)
        sections = []
        for i in range(count+1):
            distance = line.length*i/count
            center = line.interpolate(distance)
            a, b = line.interpolate(max(0, distance-.1)), line.interpolate(min(line.length, distance+.1))
            dx, dz = b.x-a.x, b.y-a.y
            size = math.hypot(dx, dz)
            right = (dz/size, -dx/size)
            if i == 0:
                # Exact same cross-section as the suspended deck, including its width.
                right = across if approach["name"] == "north" else (-across[0], -across[1])
            u = i/count
            k = .12
            descent = (.5*u*u/k if u < k else
                       1-k-.5*(1-u)**2/k if u > 1-k else u-k*.5)/(1-k)
            height = 3.5+(48-3.5)*(1-descent)
            base = max(3.5, height-3)
            width = 19.5+min(27, (base-3.5)*.65)

            def q(side, y):
                return (center.x+side*right[0], y, center.y+side*right[1])

            sections.append({"road": [q(s, y) for s, y in [(-15.5, base), (-15.5, height),
                                                          (15.5, height), (15.5, base)]],
                             "earth": [q(-width, 3.5), q(-19.5, base),
                                       q(19.5, base), q(width, 3.5)],
                             "surface": [q(-13.5, height+.03), q(13.5, height+.03)],
                             "center": center, "right": right, "height": height})
        for a, b in zip(sections, sections[1:]):
            # Faces are outward relative to the direction of travel away from the bridge.
            for j in range(3):
                quad(a["earth"][j], b["earth"][j], b["earth"][j+1], a["earth"][j+1], grass)
                quad(a["road"][j], b["road"][j], b["road"][j+1], a["road"][j+1], steel)
            quad(a["surface"][0], b["surface"][0], b["surface"][1], a["surface"][1], asphalt)
            for side in [-13, 0, 13]:
                def stripe(section, offset):
                    c, r = section["center"], section["right"]
                    return (c.x+offset*r[0], section["height"]+.045, c.y+offset*r[1])
                quad(stripe(a, side-.12), stripe(b, side-.12),
                     stripe(b, side+.12), stripe(a, side+.12), marking)
        first = sections[0]
        assert abs(first["height"]-48) < 1e-8
        assert abs(sections[-1]["height"]-3.5) < 1e-8
        # Cap the exposed earth and slab at the shared bridge cross-section.
        e, r = first["earth"], first["road"]
        quad(e[0], e[1], e[2], e[3], grass)
        quad(r[0], r[1], r[2], r[3], steel)
        # Outward face toward the river, with visible wing walls beside the road.
        t = start if approach["name"] == "north" else end
        beam(p(t, -17, 24.25), p(t, 17, 24.25), 5, 41.5, concrete)
