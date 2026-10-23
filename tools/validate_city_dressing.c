#include "waterfront_corner.h"
#include "waterfront_scene.h"

#include <math.h>
#include <stdio.h>

// Wall-mounted fittings must stay within half a metre of an actual imported quay,
// including the submerged ladder ends. A relaxed global Y bound alone misses floating props.
static bool near_quay(Vector3 p)
{
    const Mesh3D *city = waterfront_city_mesh();
    for (size_t i = 0; i < city->triangle_count; ++i) {
        const Triangle3D *t = &city->triangles[i];
        Vector3 a = city->vertices[t->vertices[0]], c = city->vertices[t->vertices[1]];
        Vector3 b = city->vertices[t->vertices[2]];
        if (a.y != 0 || b.y != 0 || c.y != 3.5f)
            continue;
        float dx = b.x - a.x, dz = b.z - a.z;
        float length = dx * dx + dz * dz;
        if (length < .001f)
            continue;
        float u = fmaxf(0, fminf(1, ((p.x - a.x) * dx + (p.z - a.z) * dz) / length));
        if (hypotf(p.x - a.x - u * dx, p.z - a.z - u * dz) <= .5f)
            return true;
    }
    return false;
}

int main(void)
{
    size_t count = 0;
    const WaterfrontCornerBox *boxes = waterfront_corner_boxes(&count);
    if (!boxes || count < 4000 || count > 16384) {
        fprintf(stderr, "Missing or unbounded city dressing: %zu boxes\n", count);
        return 1;
    }
    int waterline = 0;
    int west = 0, east = 0, north = 0, south = 0, emissive = 0;
    for (size_t i = 0; i < count; ++i) {
        Vector3 centre = { 0 };
        for (int v = 0; v < 8; ++v) {
            Vector3 p = boxes[i].vertices[v];
            if (!isfinite(p.x) || !isfinite(p.y) || !isfinite(p.z)
                || p.y < (boxes[i].waterline ? -.36f : 3.49f) || p.y > 12 || fabsf(p.x) > 500
                || p.z < -300 || p.z > 550) {
                fprintf(stderr, "Invalid city dressing vertex: box %zu corner %d\n", i, v);
                return 1;
            }
            if (boxes[i].waterline && p.y < 3.49f && !near_quay(p)) {
                fprintf(stderr, "Waterline fitting detached from quay: box %zu\n", i);
                return 1;
            }
            centre.x += p.x / 8;
            centre.z += p.z / 8;
        }
        waterline += boxes[i].waterline;
        west += centre.x < -100;
        east += centre.x > 100;
        north += centre.z > 250;
        south += centre.z < 0;
        emissive += boxes[i].emissive;
    }
    // Deliberately broad coverage thresholds: a detailed cafe alone cannot pass.
    if (west < 100 || east < 100 || north < 100 || south < 100 || emissive < 200) {
        fprintf(stderr, "Insufficient coverage W/E/N/S/emissive: %d/%d/%d/%d/%d\n", west, east,
            north, south, emissive);
        return 1;
    }
    if (waterline < 100) {
        fprintf(stderr, "Missing waterline fittings: %d boxes\n", waterline);
        return 1;
    }
    Vector3 inside = waterfront_corner_position(-2.65f, .08f, -5);
    if (waterfront_corner_clear(inside, .1f)
        || !waterfront_corner_clear((Vector3) { 0, 100, 0 }, 2)) {
        fprintf(stderr, "City dressing clearance query failed\n");
        return 1;
    }
    size_t again = 0;
    if (waterfront_corner_boxes(&again) != boxes || again != count) {
        fprintf(stderr, "City dressing changed after initialization\n");
        return 1;
    }
    printf("Waterline fittings: %d boxes\n", waterline);
    printf("Static city coverage W/E/N/S: %d/%d/%d/%d; %zu boxes, %d emissive\n", west, east, north,
        south, count, emissive);
    return 0;
}
