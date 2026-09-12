#include "waterfront_scene.h"

#include "raymath.h"

#include <math.h>

#include "../assets/geometry/eriksberg/geometry.h"

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
    time = Clamp(time, 0, waterfront_scene_duration(short_route));
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
    frame->boat_z = 85 - time * 2;
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
