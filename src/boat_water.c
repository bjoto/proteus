#include "boat_water.h"
#include "raymath.h"
#include "waterfront_scene.h"

#include <math.h>
#include <stdlib.h>

// Three low crossing components. Frequencies approximate deep-water dispersion at this scale.
BoatWater boat_water_gentle(void)
{
    return (BoatWater) { .amplitude = .13f,
        .wavelength = 16,
        .angular_speed = 1.96f,
        .direction = { .6f, -.8f },
        .crossing = { { .amplitude = .06f,
                          .wavelength = 9,
                          .angular_speed = 2.62f,
                          .direction = { -.8f, -.6f } },
            { .amplitude = .025f,
                .wavelength = 5.5f,
                .angular_speed = 3.35f,
                .direction = { .96f, .28f } } } };
}

BoatWaterSample boat_water_query(BoatWater water, Vector2 position, float time)
{
    BoatWaterSample sample = { .normal = { 0, 1, 0 } };
    if (water.amplitude == 0)
        return sample;
    const BoatWave waves[]
        = { { water.amplitude, water.wavelength, water.angular_speed, water.direction },
              water.crossing[0], water.crossing[1] };
    Vector2 gradient = { 0 };
    for (int i = 0; i < 3; ++i) {
        BoatWave w = waves[i];
        if (w.amplitude == 0 || w.wavelength <= 0)
            continue;
        float k = 2 * PI / w.wavelength;
        float phase = k * Vector2DotProduct(w.direction, position) - w.angular_speed * time;
        float slope = w.amplitude * k * cosf(phase);
        sample.height += w.amplitude * sinf(phase);
        gradient = Vector2Add(gradient, Vector2Scale(w.direction, slope));
        sample.velocity.y -= w.amplitude * w.angular_speed * cosf(phase);
    }
    sample.normal = Vector3Normalize((Vector3) { -gradient.x, 1, -gradient.y });
    // Vertical surface motion only; no current or orbital drift.
    return sample;
}

Vector3 boat_water_resistance(
    BoatWaterSample water, Vector3 point_velocity, float damping, float support)
{
    if (support <= 0 || damping <= 0 || water.normal.y <= 0)
        return (Vector3) { 0 };
    Vector3 relative = Vector3Subtract(point_velocity, water.velocity);
    float force = -damping * Vector3DotProduct(relative, water.normal);
    force = fmaxf(force, -support / water.normal.y);
    return Vector3Scale(water.normal, force);
}

typedef struct {
    Vector3 *vertices;
    Triangle3D *triangles;
    size_t count, capacity;
    Mesh3D mesh;
} Surface;
static Surface surfaces[2];
static Surface patches[2];
static Vector2 patch_centers[2];

static bool surface_triangle(Surface *s, Vector3 a, Vector3 b, Vector3 c)
{
    if (s->count == s->capacity) {
        size_t capacity = s->capacity ? s->capacity * 2 : 1024;
        Vector3 *v = realloc(s->vertices, capacity * 3 * sizeof(*v));
        if (!v)
            return false;
        s->vertices = v;
        Triangle3D *t = realloc(s->triangles, capacity * sizeof(*t));
        if (!t)
            return false;
        s->triangles = t;
        s->capacity = capacity;
    }
    unsigned int i = (unsigned int)(s->count * 3);
    s->vertices[i] = a;
    s->vertices[i + 1] = b;
    s->vertices[i + 2] = c;
    s->triangles[s->count++] = (Triangle3D) { .vertices = { i, i + 1, i + 2 } };
    return true;
}

static bool subdivide(Surface *s, Vector3 a, Vector3 b, Vector3 c, int depth)
{
    // CPU reference retains its original two-unit tessellation and displacement.
    float ab = Vector3DistanceSqr(a, b), bc = Vector3DistanceSqr(b, c);
    float ca = Vector3DistanceSqr(c, a);
    if (depth < 24 && fmaxf(ab, fmaxf(bc, ca)) > 4) {
        if (bc > ab && bc >= ca)
            return subdivide(s, b, c, a, depth);
        if (ca > ab && ca > bc)
            return subdivide(s, c, a, b, depth);
        Vector3 mid = Vector3Scale(Vector3Add(a, b), .5f);
        return subdivide(s, a, mid, c, depth + 1) && subdivide(s, mid, b, c, depth + 1);
    }
    return surface_triangle(s, a, b, c);
}

void boat_water_destroy(void)
{
    for (int i = 0; i < 2; ++i) {
        free(surfaces[i].vertices);
        free(surfaces[i].triangles);
        surfaces[i] = (Surface) { 0 };
        free(patches[i].vertices);
        free(patches[i].triangles);
        patches[i] = (Surface) { 0 };
    }
}

// Clip a convex polygon against one axis-aligned cell edge, retaining input winding.
static int clip_edge(
    const Vector3 *input, int count, Vector3 *output, int axis, float edge, bool greater)
{
    int written = 0;
    if (!count)
        return 0;
    Vector3 a = input[count - 1];
    float da = (axis ? a.z : a.x) - edge;
    for (int i = 0; i < count; ++i) {
        Vector3 b = input[i];
        float db = (axis ? b.z : b.x) - edge;
        bool inside_a = greater ? da >= 0 : da <= 0;
        bool inside_b = greater ? db >= 0 : db <= 0;
        if (inside_a != inside_b) {
            Vector3 p = Vector3Lerp(a, b, da / (da - db));
            if (axis)
                p.z = edge;
            else
                p.x = edge;
            output[written++] = p;
        }
        if (inside_b)
            output[written++] = b;
        a = b;
        da = db;
    }
    return written;
}

static bool patch_triangle(Surface *s, Vector3 a, Vector3 b, Vector3 c, Vector2 origin)
{
    const float spacing = 1.5f;
    int min_z = (int)floorf((fminf(a.z, fminf(b.z, c.z)) - origin.y) / spacing);
    int max_z = (int)floorf((fmaxf(a.z, fmaxf(b.z, c.z)) - origin.y) / spacing);
    min_z = min_z < 0 ? 0 : min_z;
    max_z = max_z > 127 ? 127 : max_z;
    for (int z = min_z; z <= max_z; ++z) {
        float bottom = origin.y + z * spacing;
        Vector3 triangle[3] = { a, b, c }, strip[12], temporary[12];
        int strip_count = clip_edge(triangle, 3, temporary, 1, bottom, true);
        strip_count = clip_edge(temporary, strip_count, strip, 1, bottom + spacing, false);
        if (strip_count < 3)
            continue;
        // A row-clipped triangle often spans only a few cells even when its whole AABB
        // is large. Avoid clipping every empty cell in long diagonal source triangles.
        float min = strip[0].x, max = strip[0].x;
        for (int j = 1; j < strip_count; ++j) {
            min = fminf(min, strip[j].x);
            max = fmaxf(max, strip[j].x);
        }
        int min_x = (int)floorf((min - origin.x) / spacing);
        int max_x = (int)floorf((max - origin.x) / spacing);
        min_x = min_x < 0 ? 0 : min_x;
        max_x = max_x > 127 ? 127 : max_x;
        for (int x = min_x; x <= max_x; ++x) {
            float left = origin.x + x * spacing;
            Vector3 p[12], q[12];
            int count = clip_edge(strip, strip_count, q, 0, left, true);
            count = clip_edge(q, count, p, 0, left + spacing, false);
            for (int j = 1; j + 1 < count; ++j) {
                Vector3 ab = Vector3Subtract(p[j], p[0]);
                Vector3 ac = Vector3Subtract(p[j + 1], p[0]);
                if (fabsf(ab.x * ac.z - ab.z * ac.x) < 1e-7f)
                    continue;
                if (!surface_triangle(s, p[0], p[j], p[j + 1]))
                    return false;
            }
        }
    }
    return true;
}

const Mesh3D *boat_water_patch(Vector2 world_center, bool eriksberg)
{
    Surface *s = &patches[eriksberg];
    Vector2 center = { roundf(world_center.x / 12) * 12, roundf(world_center.y / 12) * 12 };
    if (s->count && Vector2Equals(center, patch_centers[eriksberg]))
        return &s->mesh;
    s->count = 0;
    Vector2 origin = { center.x - 96, center.y - 96 };
    bool ok = true;
    if (eriksberg) {
        const Mesh3D *city = waterfront_scene_city_mesh();
        for (size_t i = 0; ok && i < city->triangle_count; ++i) {
            Triangle3D t = city->triangles[i];
            Vector3 a = city->vertices[t.vertices[0]], b = city->vertices[t.vertices[1]];
            Vector3 c = city->vertices[t.vertices[2]];
            if (a.y != 0 || b.y != 0 || c.y != 0)
                break;
            ok = patch_triangle(s, a, b, c, origin);
        }
    } else {
        ok = patch_triangle(s, (Vector3) { -120, 0, -80 }, (Vector3) { -120, 0, 80 },
                 (Vector3) { 120, 0, 80 }, origin)
            && patch_triangle(s, (Vector3) { -120, 0, -80 }, (Vector3) { 120, 0, 80 },
                (Vector3) { 120, 0, -80 }, origin);
    }
    if (!ok) {
        s->count = 0;
        return NULL;
    }
    patch_centers[eriksberg] = center;
    s->mesh = (Mesh3D) { s->vertices, s->triangles, s->count };
    return &s->mesh;
}

const Mesh3D *boat_water_surface(BoatWater water, float time, bool eriksberg)
{
    Surface *s = &surfaces[eriksberg];
    if (!s->count) {
        bool ok = true;
        if (eriksberg) {
            const Mesh3D *city = waterfront_scene_city_mesh();
            for (size_t i = 0; ok && i < city->triangle_count; ++i) {
                Triangle3D t = city->triangles[i];
                Vector3 a = city->vertices[t.vertices[0]], b = city->vertices[t.vertices[1]];
                Vector3 c = city->vertices[t.vertices[2]];
                if (a.y != 0 || b.y != 0 || c.y != 0)
                    break; // Imported water triangles precede land.
                ok = subdivide(s, a, b, c, 0);
            }
        } else {
            ok = subdivide(s, (Vector3) { -120, 0, -80 }, (Vector3) { -120, 0, 80 },
                     (Vector3) { 120, 0, 80 }, 0)
                && subdivide(s, (Vector3) { -120, 0, -80 }, (Vector3) { 120, 0, 80 },
                    (Vector3) { 120, 0, -80 }, 0);
        }
        if (!ok) {
            boat_water_destroy();
            return NULL;
        }
    }
    for (size_t i = 0; i < s->count; ++i) {
        Vector3 centre = { 0 };
        for (int j = 0; j < 3; ++j) {
            Vector3 *v = &s->vertices[i * 3 + j];
            v->y = boat_water_query(water, (Vector2) { -v->x, v->z }, time).height;
            centre = Vector3Add(centre, Vector3Scale(*v, 1.0f / 3));
        }
        BoatWaterSample q = boat_water_query(water, (Vector2) { -centre.x, centre.z }, time);
        // Restrained slope light; geometry and GPU reflections carry the wave shape.
        float light = Clamp(.95f + .55f * (-q.normal.x + q.normal.z), .85f, 1.05f);
        Color base = eriksberg ? (Color) { 45, 79, 104, 255 } : (Color) { 36, 107, 125, 255 };
        s->triangles[i].color = (Color) { (unsigned char)(base.r * light),
            (unsigned char)(base.g * light), (unsigned char)(base.b * light), 255 };
    }
    s->mesh = (Mesh3D) { s->vertices, s->triangles, s->count };
    return &s->mesh;
}
