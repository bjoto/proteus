#include "waterfront_scene.h"

#include "raymath.h"

#include <math.h>
#include <stdint.h>

#include "../assets/geometry/eriksberg/geometry.h"
#include "../assets/geometry/river/geometry.h"

static bool river_enabled;

void waterfront_scene_select_river(bool enabled) { river_enabled = enabled; }

bool waterfront_scene_is_bridge_triangle(size_t index)
{
    return river_enabled && index >= river_bridge_first_triangle;
}

// Tiny, flat-coloured meshes keep the toy-boat details identical in both renderers.
static void player_box(Vector3 *vertices, Triangle3D *triangles, int *count, Vector3 position,
    Vector3 size, Color color)
{
    static const unsigned int faces[][3]
        = { { 0, 2, 1 }, { 0, 3, 2 }, { 4, 5, 6 }, { 4, 6, 7 }, { 0, 1, 5 }, { 0, 5, 4 },
              { 3, 7, 6 }, { 3, 6, 2 }, { 0, 4, 7 }, { 0, 7, 3 }, { 1, 2, 6 }, { 1, 6, 5 } };
    static const Vector3 corners[] = { { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 },
        { -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 } };
    // Each box owns 36 vertices; this keeps the little mesh builder straightforward.
    for (int face = 0; face < 12; ++face) {
        unsigned int base = (unsigned int)*count * 3;
        for (int v = 0; v < 3; ++v) {
            Vector3 corner = corners[faces[face][v]];
            vertices[base + v] = Vector3Add(position,
                (Vector3) {
                    corner.x * size.x * .5f, corner.y * size.y * .5f, corner.z * size.z * .5f });
        }
        triangles[(*count)++] = (Triangle3D) { { base, base + 1, base + 2 }, color };
    }
}

const Mesh3D *waterfront_player_hull_mesh(void)
{
    static Vector3 vertices[360];
    static Triangle3D triangles[120];
    static Mesh3D mesh = { vertices, triangles, 0 };
    if (mesh.triangle_count == 0) {
        const Vector2 outline[]
            = { { 0, -1.65f }, { -.65f, -.85f }, { -.75f, .9f }, { .75f, .9f }, { .65f, -.85f } };
        // Continue the hull up, across a broad cream lip, and down to the inset deck.
        const float heights[] = { .05f, .36f, .46f, .55f, .68f, .68f, .55f };
        const Color colors[]
            = { { 225, 83, 70, 255 }, { 255, 222, 143, 255 }, { 39, 104, 120, 255 },
                  { 39, 104, 120, 255 }, { 255, 237, 191, 255 }, { 191, 143, 85, 255 } };
        for (int ring = 0; ring < 7; ++ring)
            for (int v = 0; v < 5; ++v) {
                float taper = ring == 0 || ring >= 5 ? .82f : 1;
                vertices[ring * 5 + v]
                    = (Vector3) { outline[v].x * taper, heights[ring], outline[v].y * taper };
            }
        for (int ring = 0; ring < 6; ++ring)
            for (unsigned int v = 0; v < 5; ++v) {
                unsigned int a = ring * 5 + v, b = ring * 5 + (v + 1) % 5;
                triangles[mesh.triangle_count++] = (Triangle3D) { { a, b, b + 5 }, colors[ring] };
                triangles[mesh.triangle_count++]
                    = (Triangle3D) { { a, b + 5, a + 5 }, colors[ring] };
            }
        for (unsigned int v = 1; v < 4; ++v) {
            triangles[mesh.triangle_count++]
                = (Triangle3D) { { 30, 30 + v, 31 + v }, { 218, 163, 94, 255 } };
            triangles[mesh.triangle_count++] = (Triangle3D) { { 0, v + 1, v }, colors[0] };
        }
        int count = (int)mesh.triangle_count;
        // Raised side treads and a little foredeck hatch sit clear of the deck surface.
        for (int side = -1; side <= 1; side += 2)
            player_box(vertices, triangles, &count, (Vector3) { side * .49f, .565f, -.05f },
                (Vector3) { .12f, .03f, 1.1f }, (Color) { 247, 208, 137, 255 });
        player_box(vertices, triangles, &count, (Vector3) { 0, .57f, -.94f },
            (Vector3) { .48f, .04f, .42f }, (Color) { 39, 104, 120, 255 });
        player_box(vertices, triangles, &count, (Vector3) { 0, .6f, -.94f },
            (Vector3) { .36f, .02f, .3f }, (Color) { 255, 190, 57, 255 });
        mesh.triangle_count = (size_t)count;
    }
    return &mesh;
}

const Mesh3D *waterfront_player_cabin_mesh(void)
{
    static Vector3 vertices[720];
    static Triangle3D triangles[240];
    static Mesh3D mesh = { vertices, triangles, 0 };
    if (mesh.triangle_count == 0) {
        int count = 0;
        Color cream = { 255, 237, 191, 255 }, glass = { 55, 166, 181, 255 };
        Color yellow = { 255, 190, 57, 255 }, coral = { 234, 91, 64, 255 };
        player_box(
            vertices, triangles, &count, (Vector3) { 0, 0, 0 }, (Vector3) { 1, 1, 1 }, cream);
        player_box(vertices, triangles, &count, (Vector3) { 0, .57f, 0 },
            (Vector3) { 1.2f, .18f, 1.22f }, yellow);
        for (int side = -1; side <= 1; side += 2) {
            // Split windscreens and broad side windows, inset in chunky cream frames.
            for (int pane = -1; pane <= 1; pane += 2)
                player_box(vertices, triangles, &count,
                    (Vector3) { pane * .22f, .14f, side * .505f }, (Vector3) { .34f, .48f, .025f },
                    glass);
            player_box(vertices, triangles, &count, (Vector3) { side * .505f, .16f, -.03f },
                (Vector3) { .025f, .44f, .72f }, glass);
            // A blocky life preserver below each side window.
            for (int bar = -1; bar <= 1; bar += 2) {
                player_box(vertices, triangles, &count,
                    (Vector3) { side * .54f, -.28f + bar * .13f, .08f },
                    (Vector3) { .08f, .08f, .34f }, coral);
                player_box(vertices, triangles, &count,
                    (Vector3) { side * .54f, -.28f, .08f + bar * .13f },
                    (Vector3) { .08f, .18f, .08f }, yellow);
            }
        }
        mesh.triangle_count = (size_t)count;
    }
    return &mesh;
}

const Mesh3D *waterfront_player_engine_mesh(void)
{
    static Vector3 vertices[108];
    static Triangle3D triangles[36];
    static Mesh3D mesh = { vertices, triangles, 0 };
    if (mesh.triangle_count == 0) {
        int count = 0;
        // End the body at the cap's underside; coincident top faces cause z-fighting.
        player_box(vertices, triangles, &count, (Vector3) { 0, -.16f, 0 }, (Vector3) { 1, .68f, 1 },
            (Color) { 40, 68, 87, 255 });
        player_box(vertices, triangles, &count, (Vector3) { 0, .34f, 0 },
            (Vector3) { 1.08f, .32f, 1.04f }, (Color) { 234, 91, 64, 255 });
        player_box(vertices, triangles, &count, (Vector3) { 0, .1f, .505f },
            (Vector3) { .72f, .1f, .025f }, (Color) { 255, 237, 191, 255 });
        mesh.triangle_count = (size_t)count;
    }
    return &mesh;
}

static Vector3 sky_mix(Vector3 a, Vector3 b, float t)
{
    t = Clamp(t, 0, 1);
    return Vector3Lerp(a, b, t * t * (3 - 2 * t));
}

Vector3 waterfront_sun_direction(void) { return Vector3Normalize((Vector3) { .10f, .48f, -.87f }); }

// A fixed cloud deck sampled in world X/Z, so square tiles recede toward the horizon.
static float cloud_hash(int x, int z)
{
    uint32_t seed = (uint32_t)x * 374761393u + (uint32_t)z * 668265263u;
    seed = (seed ^ (seed >> 13)) * 1274126177u;
    return (float)((seed ^ (seed >> 16)) & 65535u) / 65535;
}

static bool cloud_tile(float x, float z)
{
    int tile_x = (int)floorf(x / .16f), tile_z = (int)floorf(z / .16f);
    float gx = tile_x / 5.0f, gz = tile_z / 5.0f;
    int ix = (int)floorf(gx), iz = (int)floorf(gz);
    float tx = gx - ix, tz = gz - iz;
    tx = tx * tx * (3 - 2 * tx);
    tz = tz * tz * (3 - 2 * tz);
    float a = Lerp(cloud_hash(ix, iz), cloud_hash(ix + 1, iz), tx);
    float b = Lerp(cloud_hash(ix, iz + 1), cloud_hash(ix + 1, iz + 1), tx);
    // Broad joined patches, notched edges, holes, and occasional detached little tiles.
    return Lerp(a, b, tz) * .85f + cloud_hash(tile_x + 137, tile_z - 59) * .15f > .59f;
}

Image waterfront_sky_image(void)
{
    enum { WIDTH = 1024, HEIGHT = 512 };
    static Color pixels[WIDTH * HEIGHT];
    static bool ready;
    if (!ready) {
        Vector3 sun = waterfront_sun_direction();
        for (int y = 0; y < HEIGHT; ++y) {
            float elevation = 1 - 2 * ((float)y + .5f) / HEIGHT;
            for (int x = 0; x < WIDTH; ++x) {
                float azimuth = (((float)x + .5f) / WIDTH - .5f) * 2 * PI;
                Vector3 color = sky_mix(
                    (Vector3) { 186, 229, 244 }, (Vector3) { 66, 158, 226 }, elevation / .85f);
                float sun_x
                    = remainderf(azimuth - atan2f(sun.x, sun.z), 2 * PI) * sqrtf(1 - sun.y * sun.y);
                float sun_y = (elevation - sun.y) / sqrtf(1 - sun.y * sun.y);
                float sun_distance = sqrtf(sun_x * sun_x + sun_y * sun_y);
                // A warm, crisp disc with a quiet halo, anchored to the lighting direction.
                color = sky_mix(color, (Vector3) { 255, 237, 165 },
                    .24f * (1 - Clamp(sun_distance / .16f, 0, 1)));
                color = sky_mix(color, (Vector3) { 255, 244, 184 },
                    Clamp((.047f - sun_distance) / .004f, 0, 1));
                if (elevation > .075f) {
                    float horizontal = sqrtf(1 - elevation * elevation) / elevation;
                    float cx = sinf(azimuth) * horizontal;
                    float cz = cosf(azimuth) * horizontal;
                    // Looking up through a shallow slab: its underside covers the distant rim.
                    // Two ray/plane samples give the cloud deck a little visible thickness.
                    bool underside = cloud_tile(cx, cz);
                    bool rim = cloud_tile(cx * 1.035f, cz * 1.035f);
                    if (underside || rim) {
                        Vector3 cloud
                            = underside ? (Vector3) { 225, 242, 244 } : (Vector3) { 205, 225, 238 };
                        color = sky_mix(color, cloud, (elevation - .075f) / .10f);
                    }
                }
                // Subtle ordered grain at panorama resolution, fixed to the sky itself.
                float grain = ((x & 1) == (y & 1)) ? .6f : -.6f;
                pixels[y * WIDTH + x]
                    = (Color) { (unsigned char)roundf(Clamp(color.x + grain, 0, 255)),
                          (unsigned char)roundf(Clamp(color.y + grain, 0, 255)),
                          (unsigned char)roundf(Clamp(color.z + grain, 0, 255)), 255 };
            }
        }
        ready = true;
    }
    return (Image) { .data = pixels,
        .width = WIDTH,
        .height = HEIGHT,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
}

void waterfront_sky_draw(Color *pixels, int width, int height, View3D camera)
{
    Image sky = waterfront_sky_image();
    const Color *colors = sky.data;
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(camera.up, forward));
    Vector3 up = Vector3CrossProduct(forward, right);
    float scale = 2 * tanf(camera.vertical_fov * .5f) / height;
    for (int y = 0; y < height; ++y) {
        Vector3 row = Vector3Add(forward, Vector3Scale(up, (height * .5f - y - .5f) * scale));
        for (int x = 0; x < width; ++x) {
            Vector3 ray = Vector3Normalize(
                Vector3Add(row, Vector3Scale(right, (x + .5f - width * .5f) * scale)));
            float u = atan2f(ray.x, ray.z) / (2 * PI) + .5f;
            int sx = (int)(u * sky.width) % sky.width;
            int sy = (int)Clamp((1 - ray.y) * .5f * sky.height, 0, sky.height - 1);
            pixels[y * width + x] = colors[sy * sky.width + sx];
        }
    }
}

typedef struct {
    Vector3 position;
    Vector3 target;
} CameraKey;

static const CameraKey keys[] = {
    { { -85, 17.5f, 170 }, { -78, 5, 100 } },
    { { -78, 21.5f, 90 }, { -70, 5, 10 } },
    { { -70, 28.5f, 0 }, { -15, 5, 90 } },
    { { -30, 35.5f, -130 }, { 30, 8, 130 } },
    { { 120, 41.5f, -200 }, { 20, 8, 140 } },
};
static const float key_distances[] = { 0, 80.306f, 170.661f, 306.676f, 472.205f };

static const Vector3 west_street[] = { { -99.4f, 3.56f, 308.7f }, { -86.9f, 3.56f, 186.7f },
    { -80.6f, 3.56f, 116.5f }, { -73.9f, 3.56f, 53.6f }, { -68.8f, 3.56f, -5.7f },
    { -64.8f, 3.56f, -35.8f }, { -61.5f, 3.56f, -69.6f } };
static const Vector3 east_street[] = { { 67.4f, 3.56f, 406.2f }, { 82.5f, 3.56f, 269.5f },
    { 92.7f, 3.56f, 167.2f }, { 95.6f, 3.56f, 133.7f }, { 103.7f, 3.56f, 67.1f } };

static void evaluate_life(float time, WaterfrontSceneFrame *frame)
{
    static const Color colors[] = { { 153, 69, 57, 255 }, { 170, 178, 181, 255 },
        { 51, 88, 117, 255 }, { 183, 142, 72, 255 } };
    frame->lamp_count = 0;
    for (int street = 0; street < 2; ++street) {
        WaterfrontStreet route = waterfront_scene_street(street);
        float length = 0;
        for (int i = 1; i < route.point_count; ++i)
            length += Vector3Distance(route.points[i - 1], route.points[i]);
        int lamp_index = 0;
        for (float distance = 20; distance < length - 12; distance += 27, ++lamp_index) {
            // Capacity covers the two authored routes; validated by the scene test.
            if (frame->lamp_count == WATERFRONT_MAX_LAMPS)
                break;
            Vector3 direction;
            Vector3 p = waterfront_scene_street_position(route, distance, &direction);
            Vector3 side = { direction.z, 0, -direction.x };
            p = Vector3Add(p, Vector3Scale(side, 2.6f));
            frame->lamps[frame->lamp_count++]
                = (WaterfrontLampState) { .id = 100 + street * 100 + lamp_index,
                      .street = street,
                      .position = p,
                      .pool_direction = Vector3Negate(side),
                      .pole = { .position = { p.x, 6, p.z }, .scale = { .16f, 5, .16f } },
                      .lens = { p.x, 8.5f, p.z } };
        }
        for (int car = 0; car < 10; ++car) {
            int sign = car % 2 ? 1 : -1;
            // Preserve the original arithmetic, phase, speed, lane, and wrap behavior.
            float distance
                = fmodf(car * length / 10 + time * (5.5f + street) * sign + length, length);
            if (distance < 0)
                distance += length;
            Vector3 forward;
            Vector3 p = waterfront_scene_street_position(route, distance, &forward);
            forward = Vector3Scale(forward, (float)sign);
            Vector3 right = { forward.z, 0, -forward.x };
            p = Vector3Add(p, Vector3Scale(right, 1.25f));
            float yaw = atan2f(forward.x, forward.z);
            WaterfrontCarState *out = &frame->cars[street * 10 + car];
            *out = (WaterfrontCarState) { .id = 1000 + street * 100 + car * 2,
                .street = street,
                .position = p,
                .forward = forward,
                .right = right,
                .body = { .position = { p.x, 4.2f, p.z },
                    .rotation = { 0, yaw, 0 },
                    .scale = { 1.7f, .9f, 3.9f } },
                .cabin = { .position = { p.x, 4.9f, p.z },
                    .rotation = { 0, yaw, 0 },
                    .scale = { 1.4f, .65f, 1.9f } },
                .color = colors[car % 4],
                .headlight_direction
                = Vector3Normalize(Vector3Add(forward, (Vector3) { 0, -.22f, 0 })),
                .pool_position = Vector3Add(p, Vector3Scale(forward, 2.05f)) };
            for (int end = -1; end <= 1; end += 2)
                for (int side = -1; side <= 1; side += 2) {
                    Vector3 lens = Vector3Add(p,
                        Vector3Add(
                            Vector3Scale(forward, end * 2.0f), Vector3Scale(right, side * .6f)));
                    lens.y = 4.3f;
                    out->lenses[end > 0][side > 0] = lens;
                }
        }
    }
    float z = 85 - time * 2;
    frame->boat.hull = (Transform3D) { .position = { 12, .7f, z }, .scale = { 3.2f, 1.3f, 7 } };
    frame->boat.cabin
        = (Transform3D) { .position = { 12, 1.9f, z + .5f }, .scale = { 2.5f, 1.3f, 3.2f } };
    frame->boat.window
        = (Transform3D) { .position = { 12, 2, z - 1.15f }, .scale = { 2, .6f, .1f } };
    for (int i = 0; i < WATERFRONT_WAKE_COUNT; ++i) {
        float drift = fmodf(time * 2 + i * 1.8f, 14);
        // Retain legacy float operation order for the CPU edges and GPU centre.
        frame->boat.wake[i] = (WaterfrontWakeState) { .half_width = 1.4f + drift * .14f,
            .z_start = z + 3.8f + drift,
            .z_center = z + 3.95f + drift };
    }
}

float waterfront_scene_duration(bool short_route) { return short_route ? 15.36f : 30.72f; }

static Vector3 tangent(int index, bool target)
{
    int before = index > 0 ? index - 1 : index;
    int after = index < 4 ? index + 1 : index;
    Vector3 a = target ? keys[before].target : keys[before].position;
    Vector3 b = target ? keys[after].target : keys[after].position;
    return Vector3Scale(Vector3Subtract(b, a), 1 / (key_distances[after] - key_distances[before]));
}

static Vector3 interpolate(int index, float u, bool target)
{
    float span = key_distances[index + 1] - key_distances[index];
    Vector3 a = target ? keys[index].target : keys[index].position;
    Vector3 b = target ? keys[index + 1].target : keys[index + 1].position;
    float u2 = u * u, u3 = u2 * u;
    return Vector3Add(
        Vector3Add(Vector3Scale(a, 2 * u3 - 3 * u2 + 1), Vector3Scale(b, -2 * u3 + 3 * u2)),
        Vector3Add(Vector3Scale(tangent(index, target), (u3 - 2 * u2 + u) * span),
            Vector3Scale(tangent(index + 1, target), (u3 - u2) * span)));
}

void waterfront_scene_evaluate(float time, bool short_route, WaterfrontSceneFrame *frame)
{
    *frame = (WaterfrontSceneFrame) { 0 };
    time = Clamp(time, 0, waterfront_scene_duration(short_route));
    frame->time = time;
    float progress = Clamp(time / waterfront_scene_duration(short_route), 0, 1);
    float distance = short_route ? Lerp(0, 340, progress) : progress * key_distances[4];
    int index = 0;
    while (index < 3 && distance > key_distances[index + 1])
        ++index;
    float u = (distance - key_distances[index]) / (key_distances[index + 1] - key_distances[index]);
    frame->camera = (View3D) { .position = interpolate(index, u, false),
        .target = interpolate(index, u, true),
        .up = { 0, 1, 0 },
        .vertical_fov = 62 * DEG2RAD,
        .near_plane = .25f,
        .far_plane = 1400 };
    frame->bridge = (Transform3D) {
        .position = { 11, 7, 126 }, .rotation = { 0, -.08f, 0 }, .scale = { 68, 1, 3 }
    };
    frame->bridge_color = (Color) { 119, 128, 145, 255 };
    evaluate_life(time, frame);
    waterfront_harbor_evaluate((BoatWater) { 0 }, time, &frame->harbor);
}

const Mesh3D *waterfront_scene_city_mesh(void)
{
    static const Mesh3D mesh = { waterfront_vertices, waterfront_triangles,
        sizeof(waterfront_triangles) / sizeof(waterfront_triangles[0]) };
    static const Mesh3D river
        = { river_vertices, river_triangles, sizeof(river_triangles) / sizeof(river_triangles[0]) };
    return river_enabled ? &river : &mesh;
}

WaterfrontStreet waterfront_scene_street(int index)
{
    return index
        ? (WaterfrontStreet) { east_street, (int)(sizeof(east_street) / sizeof(east_street[0])) }
        : (WaterfrontStreet) { west_street, (int)(sizeof(west_street) / sizeof(west_street[0])) };
}

Vector3 waterfront_scene_street_position(
    WaterfrontStreet street, float distance, Vector3 *direction)
{
    for (int i = 1; i < street.point_count; ++i) {
        Vector3 delta = Vector3Subtract(street.points[i], street.points[i - 1]);
        float length = Vector3Length(delta);
        if (distance <= length || i == street.point_count - 1) {
            *direction = Vector3Scale(delta, 1 / length);
            return Vector3Add(street.points[i - 1], Vector3Scale(*direction, distance));
        }
        distance -= length;
    }
    return street.points[0];
}

// A sparse, low-poly gull: pointed body, swept wings, and charcoal outer feathers.
// Nose is local +X so wingbeats/banking rotate around X before heading around Y.
const Mesh3D *waterfront_gull_mesh(int part)
{
    static const Vector3 body[] = { { .65f, 0, 0 }, { -.55f, 0, 0 }, { 0, .16f, 0 },
        { 0, -.13f, 0 }, { 0, 0, .14f }, { 0, 0, -.14f } };
    static const Triangle3D body_faces[] = {
        { { 0, 2, 4 }, { 226, 225, 216, 255 } },
        { { 0, 5, 2 }, { 226, 225, 216, 255 } },
        { { 0, 4, 3 }, { 183, 190, 197, 255 } },
        { { 0, 3, 5 }, { 183, 190, 197, 255 } },
        { { 1, 4, 2 }, { 226, 225, 216, 255 } },
        { { 1, 2, 5 }, { 226, 225, 216, 255 } },
        { { 1, 3, 4 }, { 183, 190, 197, 255 } },
        { { 1, 5, 3 }, { 183, 190, 197, 255 } },
    };
    static const Vector3 left[] = { { .18f, 0, 0 }, { -.3f, 0, 0 }, { .08f, 0, -.8f },
        { -.34f, 0, -.9f }, { -.45f, 0, -1.35f } };
    static const Vector3 right[] = { { .18f, 0, 0 }, { -.3f, 0, 0 }, { .08f, 0, .8f },
        { -.34f, 0, .9f }, { -.45f, 0, 1.35f } };
    // Both sides are explicit so wings remain visible through a bank or upstroke.
    static const Triangle3D wing_faces[] = {
        { { 0, 1, 2 }, { 220, 222, 217, 255 } },
        { { 2, 1, 0 }, { 185, 195, 204, 255 } },
        { { 1, 3, 2 }, { 220, 222, 217, 255 } },
        { { 2, 3, 1 }, { 185, 195, 204, 255 } },
        { { 2, 3, 4 }, { 54, 63, 76, 255 } },
        { { 4, 3, 2 }, { 54, 63, 76, 255 } },
    };
    static const Mesh3D meshes[] = {
        { body, body_faces, sizeof(body_faces) / sizeof(body_faces[0]) },
        { left, wing_faces, sizeof(wing_faces) / sizeof(wing_faces[0]) },
        { right, wing_faces, sizeof(wing_faces) / sizeof(wing_faces[0]) },
    };
    return &meshes[part];
}

// Broad circuit above the street, out over the river, and back across the dock.
// Cue times follow Below the surface's arrangement; Hermite tangents keep turns smooth.
static const Vector3 flock_keys[]
    = { { -85, 62, 170 }, { -78, 69, 30 }, { -85, 95, -170 }, { 150, 100, -190 }, { 195, 100, 20 },
          { 175, 100, 245 }, { 80, 100, 335 }, { -90, 85, 290 }, { -130, 75, 110 } };
static const float flock_times[] = { 0, 24, 56, 88, 116, 148, 176, 216, 240 };

static Vector3 flock_tangent(int index)
{
    int before = index > 0 ? index - 1 : index;
    int after = index < 8 ? index + 1 : index;
    return Vector3Scale(Vector3Subtract(flock_keys[after], flock_keys[before]),
        1 / (flock_times[after] - flock_times[before]));
}

static Vector3 flock_position(float time)
{
    // Linear end extensions make the trailing camera and heading well-defined at either end.
    if (time < 0)
        return Vector3Add(flock_keys[0], Vector3Scale(flock_tangent(0), time));
    if (time > WATERFRONT_DEMO_DURATION)
        return Vector3Add(
            flock_keys[8], Vector3Scale(flock_tangent(8), time - WATERFRONT_DEMO_DURATION));
    int i = 0;
    while (i < 7 && time > flock_times[i + 1])
        ++i;
    float span = flock_times[i + 1] - flock_times[i];
    float u = (time - flock_times[i]) / span;
    float u2 = u * u, u3 = u2 * u;
    return Vector3Add(Vector3Add(Vector3Scale(flock_keys[i], 2 * u3 - 3 * u2 + 1),
                          Vector3Scale(flock_keys[i + 1], -2 * u3 + 3 * u2)),
        Vector3Add(Vector3Scale(flock_tangent(i), (u3 - 2 * u2 + u) * span),
            Vector3Scale(flock_tangent(i + 1), (u3 - u2) * span)));
}

void waterfront_demo_evaluate(float time, WaterfrontSceneFrame *frame)
{
    time = Clamp(time, 0, WATERFRONT_DEMO_DURATION);
    waterfront_scene_evaluate(0, true, frame);
    frame->time = time;
    evaluate_life(time, frame);
    waterfront_harbor_evaluate((BoatWater) { 0 }, time, &frame->harbor);
    Vector3 center = flock_position(time);
    Vector3 forward = Vector3Normalize(
        Vector3Subtract(flock_position(time + .25f), flock_position(time - .25f)));
    Vector3 side = Vector3Normalize(Vector3CrossProduct(forward, (Vector3) { 0, 1, 0 }));
    // Follow the route's recent history, with a stable horizon and a gentle look ahead.
    // This is stateless: direct seeks give exactly the same shot as continuous playback.
    Vector3 behind = Vector3Normalize(Vector3Subtract(center, flock_position(time - 3)));
    frame->camera.position
        = Vector3Add(center, Vector3Add(Vector3Scale(behind, -22), (Vector3) { 0, 2, 0 }));
    frame->camera.target = Vector3Add(center, Vector3Scale(forward, 10));
    frame->camera.vertical_fov = 58 * DEG2RAD;
    frame->gull_count = WATERFRONT_GULL_COUNT;
    static const Vector3 offsets[]
        = { { 0, 0, 2 }, { -3.5f, 1, -2 }, { 3, -.6f, -1 }, { -6.5f, -.3f, -5 }, { 6, 1.4f, -4 } };
    float yaw = atan2f(-forward.z, forward.x);
    Vector3 next = Vector3Normalize(Vector3Subtract(flock_position(time + 1), center));
    float bank = Clamp(Vector3CrossProduct(forward, next).y * -6, -.45f, .45f);
    for (int i = 0; i < WATERFRONT_GULL_COUNT; ++i) {
        float phase = time + i * 1.37f;
        Vector3 p = Vector3Add(center,
            Vector3Add(Vector3Scale(side, offsets[i].x + .35f * sinf(phase * .6f)),
                Vector3Scale(forward, offsets[i].z + .5f * sinf(phase * .37f))));
        p.y += offsets[i].y + .3f * sinf(phase * .8f);
        // A two-second flapping burst followed by six seconds of gliding, staggered by bird.
        float burst = fmodf(phase, 8);
        float flap = burst < 2 ? .6f * sinf(burst * PI * 3) * sinf(burst * PI / 2) : 0;
        for (int part = 0; part < WATERFRONT_GULL_PARTS; ++part) {
            float wing = part == 1 ? -.12f - flap : part == 2 ? .12f + flap : 0;
            frame->gulls[i][part] = (Transform3D) {
                .position = p, .rotation = { bank + wing, yaw, 0 }, .scale = { 1, 1, 1 }
            };
        }
    }
}
