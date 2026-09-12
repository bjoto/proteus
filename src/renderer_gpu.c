#include "renderer_gpu.h"

#include "raymath.h"
#include "rlgl.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float *positions;
    float *normals;
    unsigned char *colors;
    int count;
    int capacity;
} MeshBuilder;

static void builder_destroy(MeshBuilder *builder)
{
    free(builder->positions);
    free(builder->normals);
    free(builder->colors);
    *builder = (MeshBuilder) { 0 };
}

static bool builder_reserve(MeshBuilder *builder, int additional)
{
    if (additional <= builder->capacity - builder->count)
        return true;
    int capacity = builder->capacity == 0 ? 256 : builder->capacity;
    while (capacity - builder->count < additional)
        capacity *= 2;
    float *positions = malloc((size_t)capacity * 3 * sizeof(*positions));
    float *normals = malloc((size_t)capacity * 3 * sizeof(*normals));
    unsigned char *colors = malloc((size_t)capacity * 4 * sizeof(*colors));
    if (positions == NULL || normals == NULL || colors == NULL) {
        free(positions);
        free(normals);
        free(colors);
        return false;
    }
    memcpy(positions, builder->positions, (size_t)builder->count * 3 * sizeof(*positions));
    memcpy(normals, builder->normals, (size_t)builder->count * 3 * sizeof(*normals));
    memcpy(colors, builder->colors, (size_t)builder->count * 4 * sizeof(*colors));
    free(builder->positions);
    free(builder->normals);
    free(builder->colors);
    builder->positions = positions;
    builder->normals = normals;
    builder->colors = colors;
    builder->capacity = capacity;
    return true;
}

static Color shaded(Color color, Vector3 normal)
{
    Vector3 light = Vector3Normalize((Vector3) { -0.4f, .85f, -0.35f });
    float amount = .30f + .70f * fmaxf(0.0f, Vector3DotProduct(normal, light));
    return (Color) { (unsigned char)(color.r * amount), (unsigned char)(color.g * amount),
        (unsigned char)(color.b * amount), color.a };
}

static bool builder_triangle(
    MeshBuilder *builder, Vector3 a, Vector3 b, Vector3 c, Color color, bool emissive)
{
    if (!builder_reserve(builder, 3))
        return false;
    Vector3 normal
        = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
    if (!emissive)
        color = shaded(color, normal);
    Vector3 points[] = { a, b, c };
    for (int i = 0; i < 3; ++i) {
        int vertex = builder->count++;
        builder->positions[vertex * 3] = points[i].x;
        builder->positions[vertex * 3 + 1] = points[i].y;
        builder->positions[vertex * 3 + 2] = points[i].z;
        builder->normals[vertex * 3] = normal.x;
        builder->normals[vertex * 3 + 1] = normal.y;
        builder->normals[vertex * 3 + 2] = normal.z;
        builder->colors[vertex * 4] = color.r;
        builder->colors[vertex * 4 + 1] = color.g;
        builder->colors[vertex * 4 + 2] = color.b;
        builder->colors[vertex * 4 + 3] = color.a;
    }
    return true;
}

static bool builder_quad(
    MeshBuilder *builder, Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color, bool emissive)
{
    return builder_triangle(builder, a, b, c, color, emissive)
        && builder_triangle(builder, a, c, d, color, emissive);
}

static bool builder_box(MeshBuilder *builder, Vector3 a, Vector3 b, float width, float depth)
{
    Vector3 direction = Vector3Normalize(Vector3Subtract(b, a));
    Vector3 reference = fabsf(direction.y) > .9f ? (Vector3) { 1, 0, 0 } : (Vector3) { 0, 1, 0 };
    Vector3 right
        = Vector3Scale(Vector3Normalize(Vector3CrossProduct(reference, direction)), width / 2);
    Vector3 up = Vector3Scale(Vector3Normalize(Vector3CrossProduct(direction, right)), depth / 2);
    Vector3 points[8];
    for (int i = 0; i < 8; ++i) {
        Vector3 center = i < 4 ? a : b;
        int corner = i % 4;
        points[i] = Vector3Add(center,
            Vector3Add(Vector3Scale(right, corner == 0 || corner == 3 ? -1 : 1),
                Vector3Scale(up, corner < 2 ? -1 : 1)));
    }
    static const unsigned int faces[][3]
        = { { 0, 2, 1 }, { 0, 3, 2 }, { 5, 7, 4 }, { 5, 6, 7 }, { 4, 3, 0 }, { 4, 7, 3 },
              { 1, 6, 5 }, { 1, 2, 6 }, { 3, 6, 2 }, { 3, 7, 6 }, { 4, 1, 5 }, { 4, 0, 1 } };
    for (size_t i = 0; i < sizeof(faces) / sizeof(faces[0]); ++i) {
        if (!builder_triangle(builder, points[faces[i][0]], points[faces[i][1]],
                points[faces[i][2]], (Color) { 107, 67, 62, 255 }, false))
            return false;
    }
    return true;
}

static Mesh builder_finish(MeshBuilder *builder)
{
    Mesh mesh = { .vertexCount = builder->count,
        .triangleCount = builder->count / 3,
        .vertices = builder->positions,
        .normals = builder->normals,
        .colors = builder->colors };
    builder->positions = NULL;
    builder->normals = NULL;
    builder->colors = NULL;
    UploadMesh(&mesh, false);
    *builder = (MeshBuilder) { 0 };
    return mesh;
}

static bool build_city(Mesh *mesh)
{
    const Mesh3D *city = waterfront_scene_city_mesh();
    MeshBuilder builder = { 0 };
    for (size_t i = 0; i < city->triangle_count; ++i) {
        const Triangle3D *triangle = &city->triangles[i];
        if (!builder_triangle(&builder, city->vertices[triangle->vertices[0]],
                city->vertices[triangle->vertices[1]], city->vertices[triangle->vertices[2]],
                triangle->color, false)) {
            builder_destroy(&builder);
            return false;
        }
    }
    *mesh = builder_finish(&builder);
    return mesh->vaoId != 0;
}

static bool build_windows(Mesh *mesh)
{
    const Mesh3D *city = waterfront_scene_city_mesh();
    MeshBuilder builder = { 0 };
    for (size_t i = 0; i < city->triangle_count; ++i) {
        const Triangle3D *triangle = &city->triangles[i];
        Vector3 a = city->vertices[triangle->vertices[0]];
        Vector3 c = city->vertices[triangle->vertices[1]];
        Vector3 b = city->vertices[triangle->vertices[2]];
        if (fabsf(a.y - 3.5f) > .001f || fabsf(b.y - a.y) > .001f || c.y < 9)
            continue;
        Vector3 edge = Vector3Subtract(b, a);
        float length = Vector3Length(edge);
        Vector3 along = Vector3Scale(edge, 1 / length);
        Vector3 normal = { along.z, 0, -along.x };
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
                if (!builder_quad(
                        &builder, p, Vector3Add(p, up), Vector3Add(q, up), q, color, true)) {
                    builder_destroy(&builder);
                    return false;
                }
            }
        }
    }
    *mesh = builder_finish(&builder);
    return mesh->vaoId != 0;
}

static Vector3 crane_point(float x, float y, float z)
{
    const float angle = .056f;
    return (Vector3) { x * cosf(angle) - z * sinf(angle), y, x * sinf(angle) + z * cosf(angle) };
}

static bool build_crane(Mesh *mesh)
{
    MeshBuilder builder = { 0 };
    for (int side = -1; side <= 1; side += 2) {
        for (int face = -1; face <= 1; face += 2) {
            if (!builder_box(&builder, crane_point(side * 49, 3.5f, face * 5),
                    crane_point(side * 45, 73, face * 3), 3, 3))
                goto failed;
        }
        if (!builder_box(
                &builder, crane_point(side * 49, 5, -5), crane_point(side * 49, 5, 5), 4, 4)
            || !builder_box(
                &builder, crane_point(side * 47, 35, -4), crane_point(side * 47, 35, 4), 2, 2))
            goto failed;
    }
    if (!builder_box(&builder, crane_point(-55, 76, 0), crane_point(55, 76, 0), 8, 6)
        || !builder_box(&builder, crane_point(-15, 81, 0), crane_point(3, 81, 0), 8, 4)
        || !builder_box(&builder, crane_point(5, 75, 0), crane_point(5, 48, 0), .4f, .4f)
        || !builder_box(&builder, crane_point(2, 48, 0), crane_point(8, 48, 0), 1.5f, 1.5f))
        goto failed;
    *mesh = builder_finish(&builder);
    return mesh->vaoId != 0;
failed:
    builder_destroy(&builder);
    return false;
}

static bool build_light_patch(Mesh *mesh)
{
    MeshBuilder builder = { 0 };
    // Match the CPU pool shape: a narrow base widening toward its forward tip.
    if (!builder_quad(&builder, (Vector3) { -.25f, 0, 0 }, (Vector3) { -1, 0, 1 },
            (Vector3) { 1, 0, 1 }, (Vector3) { .25f, 0, 0 }, WHITE, true)) {
        builder_destroy(&builder);
        return false;
    }
    *mesh = builder_finish(&builder);
    return mesh->vaoId != 0;
}

static bool build_unit_cube(Mesh *mesh)
{
    static const Vector3 points[] = {
        { -.5f, -.5f, -.5f },
        { .5f, -.5f, -.5f },
        { .5f, .5f, -.5f },
        { -.5f, .5f, -.5f },
        { -.5f, -.5f, .5f },
        { .5f, -.5f, .5f },
        { .5f, .5f, .5f },
        { -.5f, .5f, .5f },
    };
    static const unsigned char faces[][3] = {
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
    MeshBuilder builder = { 0 };
    for (size_t i = 0; i < sizeof(faces) / sizeof(faces[0]); ++i) {
        if (!builder_triangle(&builder, points[faces[i][0]], points[faces[i][1]],
                points[faces[i][2]], WHITE, true)) {
            builder_destroy(&builder);
            return false;
        }
    }
    *mesh = builder_finish(&builder);
    return mesh->vaoId != 0;
}

static bool build_unit_plane(Mesh *mesh)
{
    MeshBuilder builder = { 0 };
    if (!builder_quad(&builder, (Vector3) { -.5f, 0, -.5f }, (Vector3) { -.5f, 0, .5f },
            (Vector3) { .5f, 0, .5f }, (Vector3) { .5f, 0, -.5f }, WHITE, true)) {
        builder_destroy(&builder);
        return false;
    }
    *mesh = builder_finish(&builder);
    return mesh->vaoId != 0;
}

bool waterfront_gpu_scene_init(WaterfrontGpuScene *scene, const char *fragment_shader_path)
{
    *scene = (WaterfrontGpuScene) { 0 };
    scene->shader = LoadShader(NULL, fragment_shader_path);
    if (!IsShaderValid(scene->shader) || scene->shader.id == rlGetShaderIdDefault())
        goto failed;
    if (!build_city(&scene->city) || !build_windows(&scene->windows) || !build_crane(&scene->crane))
        goto failed;
    if (!build_unit_cube(&scene->bridge) || !build_unit_cube(&scene->cube)
        || !build_unit_plane(&scene->plane) || !build_light_patch(&scene->light_patch))
        goto failed;
    scene->ready = true;
    return true;
failed:
    waterfront_gpu_scene_destroy(scene);
    return false;
}

static Matrix transform_matrix(Transform3D transform)
{
    return MatrixMultiply(
        MatrixTranslate(transform.position.x, transform.position.y, transform.position.z),
        MatrixMultiply(MatrixRotateXYZ(transform.rotation),
            MatrixScale(transform.scale.x, transform.scale.y, transform.scale.z)));
}

static void draw_colored_mesh(Mesh mesh, Material material, Transform3D transform, Color color)
{
    material.maps[MATERIAL_MAP_DIFFUSE].color = color;
    DrawMesh(mesh, material, transform_matrix(transform));
}

static float street_length(WaterfrontStreet street)
{
    float length = 0;
    for (int i = 1; i < street.point_count; ++i)
        length += Vector3Distance(street.points[i - 1], street.points[i]);
    return length;
}

static void draw_patch(const WaterfrontGpuScene *scene, Material material, Vector3 start,
    Vector3 forward, float width, float length, Color color)
{
    draw_colored_mesh(scene->light_patch, material,
        (Transform3D) { .position = start,
            .rotation = { 0, atan2f(forward.x, forward.z), 0 },
            .scale = { width, 1, length } },
        color);
}

static void draw_rectangle(const WaterfrontGpuScene *scene, Material material, Vector3 center,
    Vector3 forward, float width, float length, Color color)
{
    draw_colored_mesh(scene->plane, material,
        (Transform3D) { .position = center,
            .rotation = { 0, atan2f(forward.x, forward.z), 0 },
            .scale = { width, 1, length } },
        color);
}

static void draw_pool(const WaterfrontGpuScene *scene, Material material, Vector3 p,
    Vector3 forward, float width, float length)
{
    for (int layer = 0; layer < 3; ++layer) {
        float factor = 1 - (float)layer * .25f;
        Vector3 start = p;
        start.y += .006f * (float)(layer + 1);
        draw_patch(scene, material, start, forward, width * factor, length * factor,
            (Color) { (unsigned char)(79 + layer * 15), (unsigned char)(76 + layer * 12),
                (unsigned char)(65 + layer * 7), 255 });
    }
}

static void draw_life(
    const WaterfrontGpuScene *scene, Material material, WaterfrontSceneFrame frame)
{
    static const Color colors[] = {
        { 153, 69, 57, 255 },
        { 170, 178, 181, 255 },
        { 51, 88, 117, 255 },
        { 183, 142, 72, 255 },
    };
    for (int street_index = 0; street_index < 2; ++street_index) {
        WaterfrontStreet street = waterfront_scene_street(street_index);
        float length = street_length(street);
        for (float distance = 20; distance < length - 12; distance += 27) {
            Vector3 direction;
            Vector3 p = waterfront_scene_street_position(street, distance, &direction);
            Vector3 side = { direction.z, 0, -direction.x };
            p = Vector3Add(p, Vector3Scale(side, 2.6f));
            draw_colored_mesh(scene->cube, material,
                (Transform3D) { .position = { p.x, 6, p.z }, .scale = { .16f, 5, .16f } },
                (Color) { 58, 66, 79, 255 });
            draw_colored_mesh(scene->cube, material,
                (Transform3D) { .position = { p.x, 8.5f, p.z }, .scale = { .65f, .3f, .65f } },
                (Color) { 255, 202, 116, 255 });
            draw_pool(scene, material, p, Vector3Negate(side), 3, 5);
        }
        for (int car = 0; car < 10; ++car) {
            int sign = car % 2 ? 1 : -1;
            float distance = fmodf(
                car * length / 10 + frame.time * (5.5f + street_index) * sign + length, length);
            Vector3 forward;
            Vector3 p = waterfront_scene_street_position(street, distance, &forward);
            forward = Vector3Scale(forward, (float)sign);
            Vector3 right = { forward.z, 0, -forward.x };
            p = Vector3Add(p, Vector3Scale(right, 1.25f));
            float yaw = atan2f(forward.x, forward.z);
            draw_colored_mesh(scene->cube, material,
                (Transform3D) { .position = { p.x, 4.2f, p.z },
                    .rotation = { 0, yaw, 0 },
                    .scale = { 1.7f, .9f, 3.9f } },
                colors[car % 4]);
            draw_colored_mesh(scene->cube, material,
                (Transform3D) { .position = { p.x, 4.9f, p.z },
                    .rotation = { 0, yaw, 0 },
                    .scale = { 1.4f, .65f, 1.9f } },
                (Color) { 49, 65, 81, 255 });
            draw_pool(
                scene, material, Vector3Add(p, Vector3Scale(forward, 2.05f)), forward, 2.3f, 8);
            for (int end = -1; end <= 1; end += 2) {
                for (int side = -1; side <= 1; side += 2) {
                    Vector3 lamp = Vector3Add(p,
                        Vector3Add(
                            Vector3Scale(forward, end * 2.0f), Vector3Scale(right, side * .6f)));
                    lamp.y = 4.3f;
                    draw_colored_mesh(scene->cube, material,
                        (Transform3D) { .position = lamp, .scale = { .35f, .28f, .35f } },
                        end > 0 ? (Color) { 255, 233, 174, 255 } : (Color) { 240, 56, 34, 255 });
                }
            }
        }
    }
    float z = frame.boat_z;
    draw_colored_mesh(scene->cube, material,
        (Transform3D) { .position = { 12, .7f, z }, .scale = { 3.2f, 1.3f, 7 } },
        (Color) { 164, 177, 185, 255 });
    draw_colored_mesh(scene->cube, material,
        (Transform3D) { .position = { 12, 1.9f, z + .5f }, .scale = { 2.5f, 1.3f, 3.2f } },
        (Color) { 69, 91, 111, 255 });
    draw_colored_mesh(scene->cube, material,
        (Transform3D) { .position = { 12, 2, z - 1.15f }, .scale = { 2, .6f, .1f } },
        (Color) { 247, 191, 108, 255 });
    for (int i = 0; i < 8; ++i) {
        float drift = fmodf(frame.time * 2 + i * 1.8f, 14);
        draw_rectangle(scene, material, (Vector3) { 12, .04f, z + 3.95f + drift },
            (Vector3) { 0, 0, 1 }, (1.4f + drift * .14f) * 2, .3f, (Color) { 63, 98, 117, 255 });
    }
}

void waterfront_gpu_scene_draw(
    const WaterfrontGpuScene *scene, WaterfrontSceneFrame frame, int width, int height)
{
    Camera3D camera = { .position = frame.camera.position,
        .target = frame.camera.target,
        .up = frame.camera.up,
        .fovy = frame.camera.vertical_fov * RAD2DEG,
        .projection = CAMERA_PERSPECTIVE };
    Material material = LoadMaterialDefault();
    material.shader = scene->shader;
    BeginMode3D(camera);
    rlSetMatrixProjection(MatrixPerspective(frame.camera.vertical_fov, (float)width / (float)height,
        frame.camera.near_plane, frame.camera.far_plane));
    DrawMesh(scene->city, material, MatrixIdentity());
    DrawMesh(scene->windows, material, MatrixIdentity());
    material.maps[MATERIAL_MAP_DIFFUSE].color = frame.bridge_color;
    DrawMesh(scene->bridge, material, transform_matrix(frame.bridge));
    draw_life(scene, material, frame);
    material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    DrawMesh(scene->crane, material, MatrixIdentity());
    EndMode3D();
}

void waterfront_gpu_scene_destroy(WaterfrontGpuScene *scene)
{
    if (scene->city.vaoId != 0)
        UnloadMesh(scene->city);
    if (scene->windows.vaoId != 0)
        UnloadMesh(scene->windows);
    if (scene->crane.vaoId != 0)
        UnloadMesh(scene->crane);
    if (scene->bridge.vaoId != 0)
        UnloadMesh(scene->bridge);
    if (scene->cube.vaoId != 0)
        UnloadMesh(scene->cube);
    if (scene->plane.vaoId != 0)
        UnloadMesh(scene->plane);
    if (scene->light_patch.vaoId != 0)
        UnloadMesh(scene->light_patch);
    if (scene->shader.id != 0 && scene->shader.id != rlGetShaderIdDefault())
        UnloadShader(scene->shader);
    *scene = (WaterfrontGpuScene) { 0 };
}
