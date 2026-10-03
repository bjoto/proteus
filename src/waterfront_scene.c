#include "waterfront_scene.h"

#include "raymath.h"

#include <math.h>

#include "../assets/geometry/eriksberg/geometry.h"

static Vector3 sky_mix(Vector3 a, Vector3 b, float t)
{
    t = Clamp(t, 0, 1);
    return Vector3Lerp(a, b, t * t * (3 - 2 * t));
}

Image waterfront_sky_image(void)
{
    enum { WIDTH = 1024, HEIGHT = 512 };
    static Color pixels[WIDTH * HEIGHT];
    static bool ready;
    if (!ready) {
        for (int y = 0; y < HEIGHT; ++y) {
            float elevation = 1 - 2 * ((float)y + .5f) / HEIGHT;
            for (int x = 0; x < WIDTH; ++x) {
                float azimuth = (((float)x + .5f) / WIDTH - .5f) * 2 * PI;
                float warmth = .5f + .5f * cosf(azimuth - .3f);
                Vector3 horizon = { 119 + 43 * warmth, 93 + 15 * warmth, 116 - 12 * warmth };
                Vector3 violet = { 76, 64, 100 };
                Vector3 indigo = { 28, 34, 65 };
                Vector3 color = elevation < .16f
                    ? sky_mix(horizon, violet, elevation / .16f)
                    : sky_mix(violet, indigo, (elevation - .16f) / .38f);
                color = sky_mix(color, (Vector3) { 15, 23, 46 }, (elevation - .54f) / .46f);
                // Small, fixed moon toward the opening street view; no screen-space drift.
                float moon_x = remainderf(azimuth - 3.04f, 2 * PI) * .95f;
                float moon_y = elevation - .32f;
                float moon_distance = sqrtf(moon_x * moon_x + moon_y * moon_y);
                color = sky_mix(color, (Vector3) { 147, 145, 163 },
                    .12f * (1 - Clamp(moon_distance / .065f, 0, 1)));
                color = sky_mix(color, (Vector3) { 222, 214, 192 },
                    Clamp((.017f - moon_distance) / .003f, 0, 1));
                // Broken, elongated cloud banks wrap seamlessly around the panorama.
                for (int layer = 0; layer < 3; ++layer) {
                    float phase = azimuth + layer * 1.7f;
                    float center
                        = .095f + layer * .115f + .016f * sinf(phase * 3) + .008f * sinf(phase * 7);
                    float thickness = .009f + .009f * (.5f + .5f * sinf(phase * 5));
                    float edge = (elevation - center) / thickness;
                    float coverage
                        = Clamp((sinf(phase * 2) + .45f * sinf(phase * 5) - .05f) * 2, 0, 1);
                    float density = (1 - Clamp(fabsf(edge), 0, 1)) * coverage;
                    Vector3 cloud = sky_mix((Vector3) { 43, 40, 64 }, (Vector3) { 141, 94, 102 },
                        Clamp(-edge, 0, 1) * warmth * .7f);
                    color = sky_mix(color, cloud, density * .85f);
                }
                // Subtle ordered grain at panorama resolution, fixed to the sky itself.
                float grain = ((x & 1) == (y & 1)) ? .6f : -.6f;
                pixels[y * WIDTH + x] = (Color) { (unsigned char)roundf(color.x + grain),
                    (unsigned char)roundf(color.y + grain), (unsigned char)roundf(color.z + grain),
                    255 };
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
}

const Mesh3D *waterfront_scene_city_mesh(void)
{
    static const Mesh3D mesh = { waterfront_vertices, waterfront_triangles,
        sizeof(waterfront_triangles) / sizeof(waterfront_triangles[0]) };
    return &mesh;
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
