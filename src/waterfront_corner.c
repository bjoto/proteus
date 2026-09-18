#include "waterfront_corner.h"

#include "raymath.h"

#include <math.h>

// Local u follows the north facade east; v points out into the courtyard north.
// Ground is 3.5 m. The mapped west facade runs from v=0 to v=-13.9.
Vector3 waterfront_corner_position(float u, float height, float v)
{
    return (Vector3) { -71.82f + .99284f * u - .11952f * v, 3.5f + height,
        105.9f + .11952f * u + .99284f * v };
}

static WaterfrontCornerBox boxes[512];
static size_t count;
static bool initialized, overflow;
static const Color stone = { 171, 171, 158, 255 };
static const Color metal = { 69, 83, 89, 255 };
static const Color wood = { 167, 114, 72, 255 };
static const Color green = { 72, 120, 101, 255 };
static const Color cream = { 232, 210, 161, 255 };

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
        b->vertices[i] = waterfront_corner_position(q.x, q.y, q.z);
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
        out->vertices[i] = waterfront_corner_position(p.x, p.y, p.z);
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

static void build(void)
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
        Vector3 low = items[i].vertices[0], high = low;
        for (int v = 1; v < 8; ++v) {
            low = Vector3Min(low, items[i].vertices[v]);
            high = Vector3Max(high, items[i].vertices[v]);
        }
        if (Vector3Distance(position, Vector3Clamp(position, low, high)) < clearance)
            return false;
    }
    return true;
}
