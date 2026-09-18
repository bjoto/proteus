#include "waterfront_corner.h"
#include "waterfront_scene.h"

#include "raymath.h"

#include <math.h>
#include <stdlib.h>

// Local u follows the north facade east; v points out into the courtyard north.
// Ground is 3.5 m. The mapped west facade runs from v=0 to v=-13.9.
Vector3 waterfront_corner_position(float u, float height, float v)
{
    return (Vector3) { -71.82f + .99284f * u - .11952f * v, 3.5f + height,
        105.9f + .11952f * u + .99284f * v };
}

static WaterfrontCornerBox boxes[16384];
static BoundingBox bounds[16384];
static size_t count;
static bool initialized, overflow;
static Vector3 origin = { -71.82f, 3.5f, 105.9f };
static Vector3 axis_u = { .99284f, 0, .11952f }, axis_v = { -.11952f, 0, .99284f };
static int entrances, shops, parked_cars, benches, bicycles, quay_sections, sidewalk_sections;
static int forecourts, trees;
static const Color stone = { 171, 171, 158, 255 };
static const Color metal = { 69, 83, 89, 255 };
static const Color wood = { 167, 114, 72, 255 };
static const Color green = { 72, 120, 101, 255 };
static const Color cream = { 232, 210, 161, 255 };

static Vector3 place(Vector3 p)
{
    Vector3 q
        = Vector3Add(origin, Vector3Add(Vector3Scale(axis_u, p.x), Vector3Scale(axis_v, p.z)));
    q.y += p.y;
    return q;
}

static void frame(Vector3 p, Vector3 u, Vector3 v)
{
    origin = p;
    origin.y = 3.5f;
    axis_u = u;
    axis_v = v;
}

static void box(Vector3 p, Vector3 size, Color color, bool emissive)
{
    if (count == sizeof(boxes) / sizeof(boxes[0])) {
        overflow = true;
        return;
    }
    WaterfrontCornerBox *b = &boxes[count++];
    *b = (WaterfrontCornerBox) { .color = color, .emissive = emissive };
    static const Vector3 corners[] = { { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 },
        { -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 } };
    for (int i = 0; i < 8; ++i) {
        Vector3 q = Vector3Add(p, Vector3Scale(Vector3Multiply(size, corners[i]), .5f));
        b->vertices[i] = place(q);
    }
}

static void beam(Vector3 a, Vector3 b, float thickness, Color color)
{
    if (count == sizeof(boxes) / sizeof(boxes[0])) {
        overflow = true;
        return;
    }
    Vector3 direction = Vector3Normalize(Vector3Subtract(b, a));
    Vector3 reference = fabsf(direction.y) > .9f ? (Vector3) { 1, 0, 0 } : (Vector3) { 0, 1, 0 };
    Vector3 right = Vector3Scale(
        Vector3Normalize(Vector3CrossProduct(reference, direction)), thickness * .5f);
    Vector3 up
        = Vector3Scale(Vector3Normalize(Vector3CrossProduct(direction, right)), thickness * .5f);
    WaterfrontCornerBox *out = &boxes[count++];
    *out = (WaterfrontCornerBox) { .color = color };
    for (int i = 0; i < 8; ++i) {
        int corner = i % 4;
        Vector3 p = Vector3Add(i < 4 ? a : b,
            Vector3Add(Vector3Scale(right, corner == 0 || corner == 3 ? -1 : 1),
                Vector3Scale(up, corner < 2 ? -1 : 1)));
        out->vertices[i] = place(p);
    }
}

static void chair(float u, float v, float facing)
{
    box((Vector3) { u, .64f, v }, (Vector3) { .65f, .12f, .65f }, wood, false);
    box((Vector3) { u, 1.04f, v + facing * .27f }, (Vector3) { .65f, .65f, .1f }, green, false);
    for (int side = -1; side <= 1; side += 2)
        box((Vector3) { u + side * .25f, .4f, v }, (Vector3) { .09f, .5f, .55f }, metal, false);
}

static void planter(float u, float v, float width, float depth)
{
    box((Vector3) { u, .46f, v }, (Vector3) { width, .62f, depth }, wood, false);
    box((Vector3) { u, .98f, v }, (Vector3) { width - .15f, .55f, depth - .15f },
        (Color) { 88, 134, 75, 255 }, false);
}

static void bicycle(float u, float v, Color color)
{
    ++bicycles;
    // Eight-sided wheels and a diamond frame; slight thickness survives near shots.
    for (int wheel = -1; wheel <= 1; wheel += 2) {
        float z = v + wheel * .64f;
        for (int i = 0; i < 8; ++i) {
            float a = i * PI / 4, b = (i + 1) * PI / 4;
            beam((Vector3) { u, .56f + .34f * sinf(a), z + .34f * cosf(a) },
                (Vector3) { u, .56f + .34f * sinf(b), z + .34f * cosf(b) }, .09f, metal);
        }
    }
    Vector3 rear = { u, .56f, v - .64f }, front = { u, .56f, v + .64f };
    Vector3 crank = { u, .5f, v }, seat = { u, 1.06f, v - .22f };
    Vector3 head = { u, 1.06f, v + .42f };
    beam(rear, crank, .08f, color);
    beam(rear, seat, .08f, color);
    beam(crank, seat, .08f, color);
    beam(seat, head, .08f, color);
    beam(crank, head, .08f, color);
    beam(head, front, .08f, color);
    box(seat, (Vector3) { .22f, .1f, .35f }, metal, false);
    beam(head, (Vector3) { u, 1.3f, v + .45f }, .08f, metal);
    box((Vector3) { u, 1.3f, v + .45f }, (Vector3) { .5f, .08f, .09f }, metal, false);
}

static void build_cafe(void)
{
    // Street centre is about u=-7.6. Leave both traffic lanes clear. The north terrace
    // stops short of the existing planted garden, which starts around v=5.
    box((Vector3) { -2.65f, .08f, -5 }, (Vector3) { 5.1f, .16f, 26 }, stone, false);
    box((Vector3) { 8.4f, .08f, 2.0f }, (Vector3) { 16.4f, .16f, 3.8f }, stone, false);
    box((Vector3) { -5.2f, .15f, -5 }, (Vector3) { .24f, .3f, 26 }, cream, false);
    // Broad paving joints, not a high-frequency texture. Keep the walking strip open.
    for (int i = 0; i < 12; ++i)
        box((Vector3) { -2.6f, .165f, -17 + i * 2.1f }, (Vector3) { 4.95f, .01f, .035f },
            (Color) { 119, 125, 128, 255 }, false);

    // A dark shop surround masks the generic ground-floor apartment windows.
    box((Vector3) { -.14f, 1.95f, -7 }, (Vector3) { .24f, 3.7f, 13.4f }, green, false);
    for (int bay = 0; bay < 4; ++bay) {
        float v = -11.5f + bay * 2.85f;
        box((Vector3) { -.285f, 1.95f, v }, (Vector3) { .05f, 2.4f, 2.2f },
            (Color) { 212, 154, 84, 255 }, true);
        box((Vector3) { -.325f, 1.95f, v }, (Vector3) { .05f, 2.4f, .1f }, green, false);
        box((Vector3) { -.36f, 1.03f, v }, (Vector3) { .12f, .12f, 2.2f }, wood, false);
    }
    // Striped canvas and its fascia establish a cafe silhouette even when text is tiny.
    for (int stripe = 0; stripe < 14; ++stripe) {
        Color color = stripe % 2 ? cream : green;
        box((Vector3) { -1.2f, 3.62f, -12.75f + stripe * .92f }, (Vector3) { 2.6f, .18f, .92f },
            color, false);
        box((Vector3) { -2.47f, 3.43f, -12.75f + stripe * .92f }, (Vector3) { .12f, .42f, .92f },
            color, false);
    }
    // Recessed doorway, glass and transom on the short north return.
    box((Vector3) { 3.4f, 1.8f, .12f }, (Vector3) { 4.7f, 3.5f, .24f }, green, false);
    box((Vector3) { 3.3f, 1.58f, .26f }, (Vector3) { 1.55f, 2.8f, .05f },
        (Color) { 119, 91, 65, 255 }, true);
    box((Vector3) { 3.3f, 3.13f, .26f }, (Vector3) { 1.55f, .22f, .05f }, cream, true);
    box((Vector3) { 3.88f, 1.25f, .33f }, (Vector3) { .08f, .42f, .09f }, cream, false);
    box((Vector3) { 3.3f, .2f, .65f }, (Vector3) { 2.1f, .1f, .9f }, metal, false);
    box((Vector3) { 3.3f, 3.72f, 1.0f }, (Vector3) { 5.0f, .2f, 2.0f }, green, false);

    // KAFFE in a five-row pixel alphabet on a projecting sign facing the approach.
    box((Vector3) { -2.0f, 3.12f, .45f }, (Vector3) { 2.95f, .98f, .18f }, green, false);
    static const unsigned char letters[5][5] = { { 5, 5, 6, 5, 5 }, { 2, 5, 7, 5, 5 },
        { 7, 4, 6, 4, 4 }, { 7, 4, 6, 4, 4 }, { 7, 4, 6, 4, 7 } };
    for (int letter = 0; letter < 5; ++letter)
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 3; ++col)
                if (letters[letter][row] & (4 >> col))
                    // Match the waterfront's horizontally corrected presentation.
                    box((Vector3) { -.8f - (letter * 4 + col) * .13f, 3.39f - row * .13f, .555f },
                        (Vector3) { .12f, .12f, .025f }, cream, true);

    // Two sidewalk tables and one courtyard table leave a continuous curbside walking strip.
    for (int i = 0; i < 3; ++i) {
        float u = i == 2 ? 8 : -1.4f;
        float v = i == 2 ? 2.0f : -10.0f + i * 5.0f;
        box((Vector3) { u, .94f, v }, (Vector3) { 1.25f, .12f, 1.25f }, cream, false);
        box((Vector3) { u, .55f, v }, (Vector3) { .18f, .7f, .18f }, metal, false);
        chair(u, v - 1.1f, -1);
        chair(u, v + 1.1f, 1);
    }
    planter(5.3f, 3.6f, 1.6f, .7f);
    planter(15.3f, 3.2f, 1.6f, .7f);
    planter(-4.1f, -15.8f, 1.25f, 1.4f);
    // Bench, waste bin and menu board beside the cafe, not in its doorway.
    box((Vector3) { 5.3f, .67f, 1.3f }, (Vector3) { .65f, .16f, 1.8f }, wood, false);
    box((Vector3) { 5.55f, 1.05f, 1.3f }, (Vector3) { .12f, .65f, 1.8f }, wood, false);
    for (int i = -1; i <= 1; i += 2)
        box((Vector3) { 5.3f, .39f, 1.3f + i * .65f }, (Vector3) { .5f, .4f, .15f }, metal, false);
    box((Vector3) { 15.5f, .65f, 1.3f }, (Vector3) { .65f, 1, .65f }, green, false);
    box((Vector3) { -1.25f, .92f, 2.0f }, (Vector3) { .18f, 1.45f, .85f }, wood, false);
    box((Vector3) { -1.36f, 1.05f, 2.0f }, (Vector3) { .035f, 1.0f, .65f }, metal, false);
    for (int i = 0; i < 3; ++i)
        box((Vector3) { -1.39f, 1.3f - i * .2f, 2.0f }, (Vector3) { .025f, .04f, .45f - i * .08f },
            cream, false);
    // Cycle stands occupy the terrace's east edge. Parked bikes remain static in step 04.
    for (int i = 0; i < 3; ++i) {
        float u = 12 + i * 1.1f;
        beam((Vector3) { u, .2f, 1 }, (Vector3) { u, 1.0f, 1 }, .1f, metal);
        beam((Vector3) { u, 1.0f, 1 }, (Vector3) { u, 1.0f, 2.3f }, .1f, metal);
        beam((Vector3) { u, 1.0f, 2.3f }, (Vector3) { u, .2f, 2.3f }, .1f, metal);
        if (i < 2)
            bicycle(u + .18f, 1.65f, i ? cream : (Color) { 191, 81, 53, 255 });
    }
}

// Ground queries are only used during construction. Keep the actual map as the
// authority: no furniture on roofs, roads, water, or the existing planted areas.
typedef struct {
    Vector3 a, b, c, low, high;
} Surface;
static Surface *surfaces;
static size_t surface_count;

static float cross_xz(Vector3 a, Vector3 b, Vector3 p)
{
    return (b.x - a.x) * (p.z - a.z) - (b.z - a.z) * (p.x - a.x);
}

static float surface_height(Vector3 p)
{
    float height = -100;
    for (size_t i = 0; i < surface_count; ++i) {
        const Surface *s = &surfaces[i];
        if (s->a.y <= height || p.x < s->low.x || p.x > s->high.x || p.z < s->low.z
            || p.z > s->high.z)
            continue;
        float a = cross_xz(s->a, s->b, p), b = cross_xz(s->b, s->c, p);
        float c = cross_xz(s->c, s->a, p);
        if ((a >= -.001f && b >= -.001f && c >= -.001f) || (a <= .001f && b <= .001f && c <= .001f))
            height = s->a.y;
    }
    return height;
}

static bool reserved(Vector3 p)
{
    // Preserve the authored cafe and the crane supports.
    return (p.x > -82 && p.x < -52 && p.z > 86 && p.z < 119)
        || (fabsf(p.x) > 39 && fabsf(p.x) < 56 && fabsf(p.z) < 14);
}

static bool space(float u, float v, float width, float depth)
{
    // Sample the footprint at <= 1 m intervals, not just its centre.
    int nx = (int)ceilf(width), nz = (int)ceilf(depth);
    for (int x = 0; x <= nx; ++x)
        for (int z = 0; z <= nz; ++z) {
            Vector3 p = place((Vector3) {
                u + width * ((float)x / nx - .5f), 0, v + depth * ((float)z / nz - .5f) });
            float height = surface_height(p);
            if (reserved(p) || fabsf(height - 3.5f) > .01f)
                return false;
        }
    return true;
}

static void bench(float u, float v)
{
    ++benches;
    box((Vector3) { u, .6f, v }, (Vector3) { 2.3f, .16f, .65f }, wood, false);
    box((Vector3) { u, 1.02f, v + .28f }, (Vector3) { 2.3f, .65f, .12f }, wood, false);
    for (int side = -1; side <= 1; side += 2)
        box((Vector3) { u + side * .8f, .32f, v }, (Vector3) { .15f, .6f, .55f }, metal, false);
}

static void tree(float u, float v)
{
    ++trees;
    planter(u, v, 2.1f, 2.1f);
    box((Vector3) { u, 2.1f, v }, (Vector3) { .3f, 3.6f, .3f }, wood, false);
    box((Vector3) { u, 4.25f, v }, (Vector3) { 3.0f, 2.4f, 2.6f }, (Color) { 76, 109, 73, 255 },
        false);
    box((Vector3) { u + .35f, 5.4f, v }, (Vector3) { 2.2f, .9f, 2 }, (Color) { 98, 130, 78, 255 },
        false);
}

static void parked_car(float u, int variant)
{
    static const Color paint[] = { { 126, 60, 52, 255 }, { 159, 162, 153, 255 },
        { 63, 90, 115, 255 }, { 178, 165, 121, 255 } };
    ++parked_cars;
    box((Vector3) { u, .025f, 0 }, (Vector3) { 2.5f, .05f, 5.6f }, (Color) { 98, 106, 112, 255 },
        false);
    for (int side = -1; side <= 1; side += 2) {
        box((Vector3) { u + side * 1.2f, .06f, 0 }, (Vector3) { .1f, .03f, 5.6f }, cream, false);
        for (int axle = -1; axle <= 1; axle += 2)
            box((Vector3) { u + side * .91f, .39f, axle * 1.32f }, (Vector3) { .19f, .57f, .65f },
                (Color) { 36, 42, 49, 255 }, false);
        box((Vector3) { u + side * .59f, .73f, -2.13f }, (Vector3) { .4f, .2f, .07f }, cream,
            false);
    }
    box((Vector3) { u, .75f, 0 }, (Vector3) { 1.85f, .7f, 4.2f }, paint[variant % 4], false);
    box((Vector3) { u, 1.29f, .18f }, (Vector3) { 1.55f, .65f, 2.35f }, (Color) { 43, 65, 79, 255 },
        false);
    box((Vector3) { u, 1.63f, .18f }, (Vector3) { 1.65f, .12f, 2.4f }, paint[variant % 4], false);
}

static void build_streets(void)
{
    for (int index = 0; index < 2; ++index) {
        WaterfrontStreet street = waterfront_scene_street(index);
        float length = 0;
        for (int i = 1; i < street.point_count; ++i)
            length += Vector3Distance(street.points[i - 1], street.points[i]);
        for (float distance = 3; distance < length - 3; distance += 3) {
            Vector3 along;
            Vector3 p = waterfront_scene_street_position(street, distance, &along);
            frame(p, (Vector3) { along.z, 0, -along.x }, along);
            for (int side = -1; side <= 1; side += 2) {
                if (!space(side * 5.0f, 0, 2.6f, 3.0f))
                    continue;
                ++sidewalk_sections;
                box((Vector3) { side * 5.0f, .075f, 0 }, (Vector3) { 2.6f, .15f, 3.0f }, stone,
                    false);
                box((Vector3) { side * 3.75f, .14f, 0 }, (Vector3) { .18f, .28f, 3.0f }, cream,
                    false);
            }
        }
        for (int cluster = 0; cluster * 23 + 12 < length - 6; ++cluster) {
            Vector3 along;
            Vector3 p = waterfront_scene_street_position(street, cluster * 23 + 12, &along);
            frame(p, (Vector3) { along.z, 0, -along.x }, along);
            for (int side = -1; side <= 1; side += 2) {
                float u = side * 8.1f;
                if (!space(u, 0, 3.4f, 6))
                    continue;
                int variant = (cluster + index * 3 + (side > 0)) % 5;
                if (variant < 3)
                    parked_car(u, cluster + index);
                else if (variant == 3)
                    tree(u, 0);
                else {
                    bench(u, -1.8f);
                    bicycle(u, .9f, green);
                    planter(u, 2.3f, 2.5f, 1);
                }
            }
        }
    }
}

static float street_distance(Vector3 p)
{
    float best = 10000;
    for (int i = 0; i < 2; ++i) {
        WaterfrontStreet street = waterfront_scene_street(i);
        for (int j = 1; j < street.point_count; ++j) {
            Vector3 a = street.points[j - 1], delta = Vector3Subtract(street.points[j], a);
            Vector3 offset = Vector3Subtract(p, a);
            float t = Clamp(Vector3DotProduct(offset, delta) / Vector3LengthSqr(delta), 0, 1);
            best = fminf(best, Vector3Distance(p, Vector3Add(a, Vector3Scale(delta, t))));
        }
    }
    return best;
}

static void build_frontages(const Mesh3D *city)
{
    static const Color trims[] = { { 65, 112, 106, 255 }, { 113, 65, 60, 255 },
        { 64, 86, 113, 255 }, { 153, 133, 89, 255 } };
    for (size_t i = 0; i < city->triangle_count; ++i) {
        const Triangle3D *t = &city->triangles[i];
        Vector3 a = city->vertices[t->vertices[0]], c = city->vertices[t->vertices[1]];
        Vector3 b = city->vertices[t->vertices[2]];
        if (fabsf(a.y - 3.5f) > .001f || fabsf(b.y - a.y) > .001f || c.y < 12)
            continue;
        float length = Vector3Distance(a, b);
        Vector3 middle = Vector3Scale(Vector3Add(a, b), .5f);
        if (length < 9 || middle.x < -380 || middle.x > 470 || middle.z < -260 || middle.z > 510)
            continue;
        Vector3 along = Vector3Scale(Vector3Subtract(b, a), 1 / length);
        Vector3 normal = { along.z, 0, -along.x };
        frame(middle, Vector3Negate(along), normal);
        if (!space(0, 1.5f, 3.5f, 2.8f))
            continue; // Reject interior part walls and obstructed entrances.
        ++entrances;
        Color trim = trims[(i / 2) % 4];
        box((Vector3) { 0, 1.5f, .11f }, (Vector3) { 2.8f, 3, .22f }, trim, false);
        box((Vector3) { 0, 1.4f, .24f }, (Vector3) { 1.65f, 2.7f, .06f }, metal, false);
        box((Vector3) { 0, 2.65f, .28f }, (Vector3) { 1.5f, .38f, .04f },
            (Color) { 171, 137, 93, 255 }, true);
        box((Vector3) { -.55f, 1.2f, .31f }, (Vector3) { .09f, .4f, .08f }, cream, false);
        box((Vector3) { 0, 3.2f, .9f }, (Vector3) { 3.4f, .18f, 1.9f }, trim, false);
        box((Vector3) { 0, .075f, 1.5f }, (Vector3) { 3.4f, .15f, 2.8f }, stone, false);
        if (length > 14 && street_distance(middle) < 26 && space(4.5f, 1.25f, 5, 2.4f)) {
            ++shops;
            box((Vector3) { 4.5f, 1.8f, .14f }, (Vector3) { 5.2f, 3.5f, .28f }, trim, false);
            for (int bay = 0; bay < 2; ++bay)
                box((Vector3) { 3.25f + 2.5f * bay, 1.8f, .31f }, (Vector3) { 2.1f, 2.4f, .06f },
                    (Color) { 191, 148, 90, 255 }, true);
            box((Vector3) { 4.5f, 3.55f, 1.1f }, (Vector3) { 5.4f, .2f, 2.2f }, trim, false);
            box((Vector3) { 4.5f, 3.3f, 2.12f }, (Vector3) { 5.4f, .45f, .12f }, cream, false);
        }
        if (length > 15 && i % 4 == 0 && space(-4.6f, 2, 3.3f, 3.5f)) {
            planter(-4.6f, 2.8f, 2.8f, 1.1f);
            if (i % 8 == 0)
                bicycle(-4.6f, 1.2f, trim);
            else
                bench(-4.6f, 1.1f);
        }
    }
}

static bool furnished(float u, float v, float width, float depth)
{
    Vector3 low = { 10000, 0, 10000 }, high = { -10000, 0, -10000 };
    for (int x = -1; x <= 1; x += 2)
        for (int z = -1; z <= 1; z += 2) {
            Vector3 p = place((Vector3) { u + x * width * .5f, 0, v + z * depth * .5f });
            low = Vector3Min(low, p);
            high = Vector3Max(high, p);
        }
    for (size_t i = 0; i < count; ++i) {
        Vector3 a = boxes[i].vertices[0], b = a;
        for (int j = 1; j < 8; ++j) {
            a = Vector3Min(a, boxes[i].vertices[j]);
            b = Vector3Max(b, boxes[i].vertices[j]);
        }
        if (b.y > 3.9f && a.x < high.x && b.x > low.x && a.z < high.z && b.z > low.z)
            return true;
    }
    return false;
}

static void garden_court(float v)
{
    ++forecourts;
    box((Vector3) { 0, .08f, v }, (Vector3) { 10, .16f, 8 }, stone, false);
    box((Vector3) { 0, .2f, v + 1.5f }, (Vector3) { 8.5f, .24f, 4 }, (Color) { 76, 109, 73, 255 },
        false);
    for (int side = -1; side <= 1; side += 2) {
        tree(side * 3.4f, v + 1.5f);
        bench(side * 2.2f, v - 2.2f);
        planter(side * 4.4f, v - 2.8f, 1, 2);
    }
    box((Vector3) { 0, .9f, v + 1 }, (Vector3) { 2.2f, .16f, 1.2f }, wood, false);
    box((Vector3) { 0, .48f, v + 1 }, (Vector3) { 1.2f, .8f, .6f }, metal, false);
    for (int side = -1; side <= 1; side += 2) {
        chair(side * .65f, v - .1f, -1);
        chair(side * .65f, v + 2.1f, 1);
    }
}

static void build_forecourts(const Mesh3D *city)
{
    // Larger purposeful groups carry across a whole frame, where individual chairs
    // disappear. These are inferred uses of empty land, not surveyed businesses.
    for (size_t i = 0; i < city->triangle_count; ++i) {
        const Triangle3D *t = &city->triangles[i];
        Vector3 a = city->vertices[t->vertices[0]], c = city->vertices[t->vertices[1]];
        Vector3 b = city->vertices[t->vertices[2]];
        if (fabsf(a.y - 3.5f) > .001f || fabsf(b.y - a.y) > .001f || c.y < 12)
            continue;
        float length = Vector3Distance(a, b);
        Vector3 p = Vector3Scale(Vector3Add(a, b), .5f);
        if (length < 12 || p.x < -330 || p.x > 380 || p.z < -220 || p.z > 460)
            continue;
        Vector3 along = Vector3Scale(Vector3Subtract(b, a), 1 / length);
        frame(p, Vector3Negate(along), (Vector3) { along.z, 0, -along.x });
        if (!space(0, 8.5f, 10, 8) || furnished(0, 8.5f, 10, 8))
            continue;
        if (forecourts % 3 == 0 && street_distance(p) < 45) {
            ++forecourts;
            box((Vector3) { 0, .04f, 8.5f }, (Vector3) { 10, .08f, 8 }, stone, false);
            Vector3 parking_origin = place((Vector3) { 0, 0, 7.5f });
            Vector3 u = axis_u, v = axis_v;
            frame(parking_origin, u, v);
            for (int car = -1; car <= 1; ++car)
                parked_car(car * 3.2f, forecourts + car);
            planter(-3.2f, 3.7f, 2.5f, 1);
            planter(3.2f, 3.7f, 2.5f, 1);
        } else {
            garden_court(8.5f);
        }
    }
}

static void build_quays(const Mesh3D *city)
{
    for (size_t i = 0; i < city->triangle_count; ++i) {
        const Triangle3D *t = &city->triangles[i];
        Vector3 a = city->vertices[t->vertices[0]], c = city->vertices[t->vertices[1]];
        Vector3 b = city->vertices[t->vertices[2]];
        if (fabsf(a.y) > .001f || fabsf(b.y) > .001f || fabsf(c.y - 3.5f) > .001f)
            continue;
        float length = Vector3Distance(a, b);
        if (length < 1)
            continue;
        Vector3 along = Vector3Scale(Vector3Subtract(b, a), 1 / length);
        Vector3 inward = { -along.z, 0, along.x };
        int sections = (int)ceilf(length / 4);
        float span = length / sections;
        for (int j = 0; j < sections; ++j) {
            Vector3 p = Vector3Add(a, Vector3Scale(along, (j + .5f) * span));
            if (p.x < -330 || p.x > 360 || p.z < -270 || p.z > 340)
                continue;
            frame(p, along, inward);
            if (fabsf(surface_height(place((Vector3) { 0, 0, -1 }))) > .01f
                || !space(0, 1.5f, span, 2.5f))
                continue;
            ++quay_sections;
            box((Vector3) { 0, .065f, 1.5f }, (Vector3) { span, .13f, 2.5f }, stone, false);
            box((Vector3) { 0, .14f, .26f }, (Vector3) { span, .28f, .22f }, cream, false);
            box((Vector3) { 0, .52f, .6f }, (Vector3) { .24f, .9f, .24f }, metal, false);
            box((Vector3) { 0, .9f, .6f }, (Vector3) { .5f, .16f, .28f }, metal, false);
            if (quay_sections % 7 == 0 && space(0, 12, 10, 8) && !furnished(0, 12, 10, 8))
                garden_court(12);
            // Rail sections alternate with mooring/access openings, not a sealed fence.
            if (quay_sections % 4 != 0) {
                beam((Vector3) { -span * .5f, 1.08f, .6f }, (Vector3) { span * .5f, 1.08f, .6f },
                    .09f, metal);
                beam((Vector3) { -span * .5f, .55f, .6f }, (Vector3) { span * .5f, .55f, .6f },
                    .07f, metal);
            }
            if (quay_sections % 5 == 0 && space(0, 4.8f, 3.4f, 3.6f)
                && !furnished(0, 4.8f, 3.4f, 3.6f)) {
                if (quay_sections % 10 == 0)
                    tree(0, 4.8f);
                else {
                    bench(0, 4.2f);
                    planter(0, 5.7f, 2.8f, 1);
                }
            }
        }
    }
}

static void build(void)
{
    const Mesh3D *city = waterfront_scene_city_mesh();
    surfaces = calloc(city->triangle_count, sizeof(*surfaces));
    if (!surfaces) {
        overflow = true;
        return;
    }
    for (size_t i = 0; i < city->triangle_count; ++i) {
        const Triangle3D *t = &city->triangles[i];
        Vector3 a = city->vertices[t->vertices[0]], b = city->vertices[t->vertices[1]];
        Vector3 c = city->vertices[t->vertices[2]];
        if (fabsf(a.y - b.y) < .001f && fabsf(a.y - c.y) < .001f)
            surfaces[surface_count++] = (Surface) { a, b, c, Vector3Min(a, Vector3Min(b, c)),
                Vector3Max(a, Vector3Max(b, c)) };
    }
    build_cafe();
    build_streets();
    build_frontages(city);
    build_quays(city);
    build_forecourts(city);
    free(surfaces);
    surfaces = NULL;
    for (size_t i = 0; i < count; ++i) {
        bounds[i].min = bounds[i].max = boxes[i].vertices[0];
        for (int j = 1; j < 8; ++j) {
            bounds[i].min = Vector3Min(bounds[i].min, boxes[i].vertices[j]);
            bounds[i].max = Vector3Max(bounds[i].max, boxes[i].vertices[j]);
        }
    }
    TraceLog(LOG_INFO,
        "City dressing: %zu boxes, %d entrances, %d shops plus cafe, %d parked cars, "
        "%d benches, %d bicycles, %d pavement / %d quay sections",
        count, entrances, shops, parked_cars, benches + 1, bicycles, sidewalk_sections,
        quay_sections);
    TraceLog(
        LOG_INFO, "City dressing: %d shared forecourts, %d additional trees", forecourts, trees);
}

const WaterfrontCornerBox *waterfront_corner_boxes(size_t *box_count)
{
    if (!initialized) {
        build();
        initialized = true;
    }
    *box_count = overflow ? 0 : count;
    return overflow ? NULL : boxes;
}

bool waterfront_corner_clear(Vector3 position, float clearance)
{
    size_t n;
    const WaterfrontCornerBox *items = waterfront_corner_boxes(&n);
    if (!items)
        return false;
    for (size_t i = 0; i < n; ++i) {
        Vector3 low = bounds[i].min, high = bounds[i].max;
        if (Vector3Distance(position, Vector3Clamp(position, low, high)) < clearance)
            return false;
    }
    return true;
}
