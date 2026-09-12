#include "waterfront.h"

#include "raymath.h"

#include <math.h>
#include <stdio.h>
float waterfront_duration(bool short_route) { return waterfront_scene_duration(short_route); }

View3D waterfront_camera(float time, bool short_route)
{
    WaterfrontSceneFrame frame;
    waterfront_scene_evaluate(time, short_route, &frame);
    return frame.camera;
}

// The crane proxy is deliberately open. Beam dimensions are authored assumptions;
// overall placement and approximate height come from the retained OSM feature.
static Vector3 crane_point(float x, float y, float z)
{
    const float angle = 0.056f;
    return (Vector3) { x * cosf(angle) - z * sinf(angle), y, x * sinf(angle) + z * cosf(angle) };
}

static void beam(Renderer3D *r, Vector3 a, Vector3 b, float width, float depth)
{
    Vector3 direction = Vector3Normalize(Vector3Subtract(b, a));
    Vector3 reference = fabsf(direction.y) > 0.9f ? (Vector3) { 1, 0, 0 } : (Vector3) { 0, 1, 0 };
    Vector3 right
        = Vector3Scale(Vector3Normalize(Vector3CrossProduct(reference, direction)), width / 2);
    Vector3 up = Vector3Scale(Vector3Normalize(Vector3CrossProduct(direction, right)), depth / 2);
    Vector3 vertices[8];
    for (int i = 0; i < 8; ++i) {
        Vector3 center = i < 4 ? a : b;
        int corner = i % 4;
        vertices[i] = Vector3Add(center,
            Vector3Add(Vector3Scale(right, corner == 0 || corner == 3 ? -1 : 1),
                Vector3Scale(up, corner < 2 ? -1 : 1)));
    }
    const unsigned int indices[12][3] = {
        { 0, 2, 1 },
        { 0, 3, 2 },
        { 5, 7, 4 },
        { 5, 6, 7 },
        { 4, 3, 0 },
        { 4, 7, 3 },
        { 1, 6, 5 },
        { 1, 2, 6 },
        { 3, 6, 2 },
        { 3, 7, 6 },
        { 4, 1, 5 },
        { 4, 0, 1 },
    };
    Triangle3D triangles[12];
    for (int i = 0; i < 12; ++i)
        triangles[i] = (Triangle3D) { { indices[i][0], indices[i][1], indices[i][2] },
            { 107, 67, 62, 255 } };
    Mesh3D mesh = { vertices, triangles, 12 };
    renderer3d_draw_mesh(r, &mesh, (Transform3D) { .scale = { 1, 1, 1 } });
}

static void crane(Renderer3D *r)
{
    for (int side = -1; side <= 1; side += 2) {
        float x = (float)side;
        for (int face = -1; face <= 1; face += 2) {
            float z = (float)face;
            beam(r, crane_point(x * 49, 3.5f, z * 5), crane_point(x * 45, 73, z * 3), 3, 3);
        }
        beam(r, crane_point(x * 49, 5, -5), crane_point(x * 49, 5, 5), 4, 4);
        beam(r, crane_point(x * 47, 35, -4), crane_point(x * 47, 35, 4), 2, 2);
    }
    beam(r, crane_point(-55, 76, 0), crane_point(55, 76, 0), 8, 6);
    beam(r, crane_point(-15, 81, 0), crane_point(3, 81, 0), 8, 4);
    beam(r, crane_point(5, 75, 0), crane_point(5, 48, 0), .4f, .4f);
    beam(r, crane_point(2, 48, 0), crane_point(8, 48, 0), 1.5f, 1.5f);
}

// Authored traffic follows retained Monsungatan and Ostindiefararen centreline vertices.
// Speeds, lane offsets, lamps and occupancy are artistic, not geographic source data.

static void glow_quad(Renderer3D *r, Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color)
{
    Vector3 vertices[] = { a, b, c, d };
    Triangle3D triangles[] = { { { 0, 1, 2 }, color }, { { 0, 2, 3 }, color } };
    Mesh3D mesh = { vertices, triangles, 2 };
    renderer3d_draw_emissive_mesh(r, &mesh, (Transform3D) { .scale = { 1, 1, 1 } });
}

static void light_box(Renderer3D *r, Vector3 position, Vector3 scale, Color color)
{
    const unsigned char texel = 0;
    Color palette[256] = { 0 };
    palette[240] = color;
    Material3D material = { &texel, palette, 1, 1, 1, true };
    renderer3d_draw_textured_box(
        r, (Transform3D) { .position = position, .scale = scale }, &material);
}

// Layered opaque light patches keep the palette crisp and participate in depth testing.
static void light_pool(Renderer3D *r, Vector3 p, Vector3 forward, float width, float length)
{
    Vector3 right = { forward.z, 0, -forward.x };
    for (int layer = 0; layer < 3; ++layer) {
        float factor = 1 - (float)layer * .25f;
        Vector3 side = Vector3Scale(right, width * factor);
        Vector3 tip = Vector3Add(p, Vector3Scale(forward, length * factor));
        Vector3 base = Vector3Scale(side, .25f);
        p.y += .006f;
        tip.y = p.y;
        glow_quad(r, Vector3Subtract(p, base), Vector3Subtract(tip, side), Vector3Add(tip, side),
            Vector3Add(p, base),
            (Color) { (unsigned char)(79 + layer * 15), (unsigned char)(76 + layer * 12),
                (unsigned char)(65 + layer * 7), 255 });
    }
}

static void windows(Renderer3D *r)
{
    // Each imported wall starts with a triangle a,c,b (two ground vertices).
    // Derive inset windows from that edge, preserving arbitrary facade orientation.
    const Mesh3D *city = waterfront_scene_city_mesh();
    for (size_t i = 0; i < city->triangle_count; ++i) {
        const Triangle3D *t = &city->triangles[i];
        Vector3 a = city->vertices[t->vertices[0]];
        Vector3 c = city->vertices[t->vertices[1]];
        Vector3 b = city->vertices[t->vertices[2]];
        if (fabsf(a.y - 3.5f) > .001f || fabsf(b.y - a.y) > .001f || c.y < 9)
            continue;
        Vector3 edge = Vector3Subtract(b, a);
        float length = Vector3Length(edge);
        Vector3 along = Vector3Scale(edge, 1 / length);
        Vector3 normal = { along.z, 0, -along.x };
        if (Vector3DotProduct(normal, Vector3Subtract(r->camera.position, a)) <= 0
            || Vector3Distance(r->camera.position, a) > 430)
            continue;
        for (int floor = 0; 6.0f + floor * 3.2f < c.y - 1; ++floor) {
            for (int column = 0; 2.5f + column * 4 < length - 1.5f; ++column) {
                unsigned int seed = (unsigned int)i * 1664525u + (unsigned int)column * 1013904223u
                    + (unsigned int)floor * 374761393u;
                Color color = seed % 7 < 3 ? (Color) { 230, 169, 88, 255 }
                    : seed % 7 == 3        ? (Color) { 139, 161, 177, 255 }
                                           : (Color) { 32, 43, 58, 255 };
                Vector3 p = Vector3Add(Vector3Add(a, Vector3Scale(along, 2.5f + column * 4)),
                    Vector3Scale(normal, .035f));
                p.y = 5.0f + floor * 3.2f;
                Vector3 q = Vector3Add(p, Vector3Scale(along, floor == 0 ? 2.2f : 1.5f));
                Vector3 up = { 0, floor == 0 ? 1.9f : 1.5f, 0 };
                glow_quad(r, p, Vector3Add(p, up), Vector3Add(q, up), q, color);
            }
        }
    }
}

static void street_life(Renderer3D *r, float time)
{
    for (int street = 0; street < 2; ++street) {
        WaterfrontStreet route = waterfront_scene_street(street);
        float length = 0;
        for (int i = 1; i < route.point_count; ++i)
            length += Vector3Distance(route.points[i - 1], route.points[i]);
        for (float distance = 20; distance < length - 12; distance += 27) {
            Vector3 direction;
            Vector3 p = waterfront_scene_street_position(route, distance, &direction);
            Vector3 side = { direction.z, 0, -direction.x };
            p = Vector3Add(p, Vector3Scale(side, 2.6f));
            renderer3d_draw_box(r,
                (Transform3D) { .position = { p.x, 6, p.z }, .scale = { .16f, 5, .16f } },
                (Color) { 58, 66, 79, 255 });
            light_box(r, (Vector3) { p.x, 8.5f, p.z }, (Vector3) { .65f, .3f, .65f },
                (Color) { 255, 202, 116, 255 });
            light_pool(r, p, Vector3Negate(side), 3, 5);
        }
        for (int car = 0; car < 10; ++car) {
            int sign = car % 2 ? 1 : -1;
            float distance
                = fmodf(car * length / 10 + time * (5.5f + street) * sign + length, length);
            Vector3 forward;
            Vector3 p = waterfront_scene_street_position(route, distance, &forward);
            forward = Vector3Scale(forward, (float)sign);
            Vector3 right = { forward.z, 0, -forward.x };
            p = Vector3Add(p, Vector3Scale(right, 1.25f));
            float yaw = atan2f(forward.x, forward.z);
            Color colors[] = { { 153, 69, 57, 255 }, { 170, 178, 181, 255 }, { 51, 88, 117, 255 },
                { 183, 142, 72, 255 } };
            renderer3d_draw_box(r,
                (Transform3D) { .position = { p.x, 4.2f, p.z },
                    .rotation = { 0, yaw, 0 },
                    .scale = { 1.7f, .9f, 3.9f } },
                colors[car % 4]);
            renderer3d_draw_box(r,
                (Transform3D) { .position = { p.x, 4.9f, p.z },
                    .rotation = { 0, yaw, 0 },
                    .scale = { 1.4f, .65f, 1.9f } },
                (Color) { 49, 65, 81, 255 });
            Vector3 front = Vector3Add(p, Vector3Scale(forward, 2.05f));
            light_pool(r, front, forward, 2.3f, 8);
            for (int end = -1; end <= 1; end += 2) {
                for (int side = -1; side <= 1; side += 2) {
                    Vector3 lamp = Vector3Add(p,
                        Vector3Add(
                            Vector3Scale(forward, end * 2.0f), Vector3Scale(right, side * .6f)));
                    lamp.y = 4.3f;
                    light_box(r, lamp, (Vector3) { .35f, .28f, .35f },
                        end > 0 ? (Color) { 255, 233, 174, 255 } : (Color) { 240, 56, 34, 255 });
                }
            }
        }
    }
}

static void harbour_life(Renderer3D *r, float time, float z)
{
    // A slow launch stays inside the basin, south of Dockbron, for both route lengths.
    renderer3d_draw_box(r, (Transform3D) { .position = { 12, .7f, z }, .scale = { 3.2f, 1.3f, 7 } },
        (Color) { 164, 177, 185, 255 });
    renderer3d_draw_box(r,
        (Transform3D) { .position = { 12, 1.9f, z + .5f }, .scale = { 2.5f, 1.3f, 3.2f } },
        (Color) { 69, 91, 111, 255 });
    light_box(r, (Vector3) { 12, 2, z - 1.15f }, (Vector3) { 2, .6f, .1f },
        (Color) { 247, 191, 108, 255 });
    for (int i = 0; i < 8; ++i) {
        float drift = fmodf(time * 2 + i * 1.8f, 14);
        float w = 1.4f + drift * .14f;
        float zz = z + 3.8f + drift;
        glow_quad(r, (Vector3) { 12 - w, .04f, zz }, (Vector3) { 12 - w, .04f, zz + .3f },
            (Vector3) { 12 + w, .04f, zz + .3f }, (Vector3) { 12 + w, .04f, zz },
            (Color) { 63, 98, 117, 255 });
    }
}

bool waterfront_draw(
    Renderer3D *r, Color *pixels, int width, int height, float time, bool short_route)
{
    WaterfrontSceneFrame frame;
    waterfront_scene_evaluate(time, short_route, &frame);
    time = Clamp(time, 0, waterfront_duration(short_route));
    for (int y = 0; y < height; ++y) {
        int band = y * 7 / height;
        Color sky = { (unsigned char)(20 + band * 5), (unsigned char)(29 + band * 5),
            (unsigned char)(52 + band * 6), 255 };
        for (int x = 0; x < width; ++x)
            pixels[y * width + x] = sky;
    }
    if (!renderer3d_begin(
            r, pixels, width, height, frame.camera, (Vector3) { -0.4f, 0.85f, -0.35f }))
        return false;
    renderer3d_draw_mesh(r, waterfront_scene_city_mesh(), (Transform3D) { .scale = { 1, 1, 1 } });
    // Dockbron: approximate mapped endpoints and assumed deck elevation, for composition.
    renderer3d_draw_box(r, frame.bridge, frame.bridge_color);
    windows(r);
    street_life(r, time);
    harbour_life(r, time, frame.boat_z);
    crane(r);
    return true;
}

static float segment_distance_squared(Vector3 p, Vector3 a, Vector3 b)
{
    Vector3 ab = Vector3Subtract(b, a);
    float length = Vector3LengthSqr(ab);
    float t = length > 0 ? Clamp(Vector3DotProduct(Vector3Subtract(p, a), ab) / length, 0, 1) : 0;
    return Vector3DistanceSqr(p, Vector3Add(a, Vector3Scale(ab, t)));
}

static float triangle_distance_squared(Vector3 p, Vector3 a, Vector3 b, Vector3 c)
{
    Vector3 ab = Vector3Subtract(b, a), ac = Vector3Subtract(c, a);
    Vector3 n = Vector3CrossProduct(ab, ac);
    float n2 = Vector3LengthSqr(n);
    if (n2 > 0.00001f) {
        float d = Vector3DotProduct(Vector3Subtract(p, a), n);
        Vector3 q = Vector3Subtract(p, Vector3Scale(n, d / n2));
        float sa = Vector3DotProduct(n, Vector3CrossProduct(ab, Vector3Subtract(q, a)));
        float sb = Vector3DotProduct(
            n, Vector3CrossProduct(Vector3Subtract(c, b), Vector3Subtract(q, b)));
        float sc = Vector3DotProduct(
            n, Vector3CrossProduct(Vector3Subtract(a, c), Vector3Subtract(q, c)));
        if (sa >= 0 && sb >= 0 && sc >= 0)
            return d * d / n2;
    }
    return fminf(segment_distance_squared(p, a, b),
        fminf(segment_distance_squared(p, b, c), segment_distance_squared(p, c, a)));
}

bool waterfront_check_routes(void)
{
    const Mesh3D *city = waterfront_scene_city_mesh();
    for (int route = 0; route < 2; ++route) {
        bool short_route = route != 0;
        float duration = waterfront_duration(short_route);
        int steps = (int)ceilf(duration * 120);
        for (int i = 0; i <= steps; ++i) {
            float time = duration * (float)i / (float)steps;
            Vector3 p = waterfront_camera(time, short_route).position;
            for (size_t j = 0; j < city->triangle_count; ++j) {
                const Triangle3D *t = &city->triangles[j];
                Vector3 a = city->vertices[t->vertices[0]];
                Vector3 b = city->vertices[t->vertices[1]];
                Vector3 c = city->vertices[t->vertices[2]];
                if (p.x < fminf(a.x, fminf(b.x, c.x)) - 2 || p.x > fmaxf(a.x, fmaxf(b.x, c.x)) + 2
                    || p.y < fminf(a.y, fminf(b.y, c.y)) - 2
                    || p.y > fmaxf(a.y, fmaxf(b.y, c.y)) + 2
                    || p.z < fminf(a.z, fminf(b.z, c.z)) - 2
                    || p.z > fmaxf(a.z, fmaxf(b.z, c.z)) + 2)
                    continue;
                if (triangle_distance_squared(p, a, b, c) < 4) {
                    fprintf(stderr, "Route %s at %.3f s is within 2 m of triangle %zu\n",
                        short_route ? "short" : "long", time, j);
                    return false;
                }
            }
            // Conservative crane envelope west of the nearest leg. The route turns
            // away before entering it; this also covers the assumed hook and crossbeam.
            if (p.x > -60 && p.x < 60 && fabsf(p.z) < 12 && p.y < 86)
                return false;
        }
    }
    puts("Both storyboard paths clear mapped surfaces by 2 m at 120 samples/second.");
    return true;
}
