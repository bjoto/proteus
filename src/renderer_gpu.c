#include "renderer_gpu.h"

#include "raymath.h"
#include "rlgl.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float *positions;
    float *normals;
    float *texcoords;
    unsigned char *colors;
    int count;
    int capacity;
} MeshBuilder;

// A low dusk key produces readable building and vehicle shadows at virtual resolution while
// retaining the cool blue-hour palette. This direction points from a shaded surface to the light.
static const Vector3 DIRECTIONAL_LIGHT_DIRECTION = { -.45f, .55f, -.35f };
static const Color LAMP_LENS_COLOR = { 255, 218, 132, 255 };
static const Color HEADLIGHT_LENS_COLOR = { 255, 246, 202, 255 };

static void builder_destroy(MeshBuilder *builder)
{
    free(builder->positions);
    free(builder->normals);
    free(builder->texcoords);
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
    float *texcoords = malloc((size_t)capacity * 2 * sizeof(*texcoords));
    unsigned char *colors = malloc((size_t)capacity * 4 * sizeof(*colors));
    if (positions == NULL || normals == NULL || texcoords == NULL || colors == NULL) {
        free(positions);
        free(normals);
        free(texcoords);
        free(colors);
        return false;
    }
    memcpy(positions, builder->positions, (size_t)builder->count * 3 * sizeof(*positions));
    memcpy(normals, builder->normals, (size_t)builder->count * 3 * sizeof(*normals));
    memcpy(texcoords, builder->texcoords, (size_t)builder->count * 2 * sizeof(*texcoords));
    memcpy(colors, builder->colors, (size_t)builder->count * 4 * sizeof(*colors));
    free(builder->positions);
    free(builder->normals);
    free(builder->texcoords);
    free(builder->colors);
    builder->positions = positions;
    builder->normals = normals;
    builder->texcoords = texcoords;
    builder->colors = colors;
    builder->capacity = capacity;
    return true;
}

// The imported city vertices are already in world space.  A conservative density keeps the
// 32-pixel detail textures legible at virtual resolution without creating fine distant shimmer.
static Vector2 world_uv(Vector3 point, Vector3 normal)
{
    const float density = .04f;
    if (fabsf(normal.y) > fabsf(normal.x) && fabsf(normal.y) > fabsf(normal.z))
        return (Vector2) { point.x * density, point.z * density };
    if (fabsf(normal.x) > fabsf(normal.z))
        return (Vector2) { point.z * density, -point.y * density };
    return (Vector2) { point.x * density, -point.y * density };
}

static bool builder_triangle(MeshBuilder *builder, Vector3 a, Vector3 b, Vector3 c, Color color)
{
    if (!builder_reserve(builder, 3))
        return false;
    Vector3 normal
        = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
    Vector3 points[] = { a, b, c };
    for (int i = 0; i < 3; ++i) {
        int vertex = builder->count++;
        Vector2 uv = world_uv(points[i], normal);
        builder->positions[vertex * 3] = points[i].x;
        builder->positions[vertex * 3 + 1] = points[i].y;
        builder->positions[vertex * 3 + 2] = points[i].z;
        builder->normals[vertex * 3] = normal.x;
        builder->normals[vertex * 3 + 1] = normal.y;
        builder->normals[vertex * 3 + 2] = normal.z;
        builder->texcoords[vertex * 2] = uv.x;
        builder->texcoords[vertex * 2 + 1] = uv.y;
        builder->colors[vertex * 4] = color.r;
        builder->colors[vertex * 4 + 1] = color.g;
        builder->colors[vertex * 4 + 2] = color.b;
        builder->colors[vertex * 4 + 3] = color.a;
    }
    return true;
}

static bool builder_quad(
    MeshBuilder *builder, Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color)
{
    return builder_triangle(builder, a, b, c, color) && builder_triangle(builder, a, c, d, color);
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
                points[faces[i][2]], (Color) { 107, 67, 62, 255 }))
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
        .texcoords = builder->texcoords,
        .colors = builder->colors };
    builder->positions = NULL;
    builder->normals = NULL;
    builder->texcoords = NULL;
    builder->colors = NULL;
    UploadMesh(&mesh, false);
    *builder = (MeshBuilder) { 0 };
    return mesh;
}

typedef enum {
    CITY_SURFACE_FLAT,
    CITY_SURFACE_FACADE,
    CITY_SURFACE_ROOF,
    CITY_SURFACE_QUAY,
} CitySurface;

static CitySurface city_surface(Vector3 a, Vector3 b, Vector3 c)
{
    Vector3 normal
        = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
    float top = fmaxf(a.y, fmaxf(b.y, c.y));
    if (normal.y > .95f)
        return top > 4.0f ? CITY_SURFACE_ROOF : CITY_SURFACE_QUAY;
    if (fabsf(normal.y) < .15f && top >= 9.0f)
        return CITY_SURFACE_FACADE;
    return CITY_SURFACE_FLAT;
}

static bool build_city(Mesh *city_mesh, Mesh *facades, Mesh *roofs, Mesh *quay)
{
    const Mesh3D *city = waterfront_scene_city_mesh();
    MeshBuilder builders[] = { { 0 }, { 0 }, { 0 }, { 0 } };
    for (size_t i = 0; i < city->triangle_count; ++i) {
        const Triangle3D *triangle = &city->triangles[i];
        Vector3 a = city->vertices[triangle->vertices[0]];
        Vector3 b = city->vertices[triangle->vertices[1]];
        Vector3 c = city->vertices[triangle->vertices[2]];
        CitySurface surface = city_surface(a, b, c);
        if (!builder_triangle(&builders[surface], a, b, c, triangle->color))
            goto failed;
    }
    if (builders[CITY_SURFACE_FLAT].count == 0 || builders[CITY_SURFACE_FACADE].count == 0
        || builders[CITY_SURFACE_ROOF].count == 0 || builders[CITY_SURFACE_QUAY].count == 0)
        goto failed;
    *city_mesh = builder_finish(&builders[CITY_SURFACE_FLAT]);
    *facades = builder_finish(&builders[CITY_SURFACE_FACADE]);
    *roofs = builder_finish(&builders[CITY_SURFACE_ROOF]);
    *quay = builder_finish(&builders[CITY_SURFACE_QUAY]);
    return city_mesh->vaoId != 0 && facades->vaoId != 0 && roofs->vaoId != 0 && quay->vaoId != 0;
failed:
    for (size_t i = 0; i < sizeof(builders) / sizeof(builders[0]); ++i)
        builder_destroy(&builders[i]);
    return false;
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
                if (!builder_quad(&builder, p, Vector3Add(p, up), Vector3Add(q, up), q, color)) {
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
        if (!builder_triangle(
                &builder, points[faces[i][0]], points[faces[i][1]], points[faces[i][2]], WHITE)) {
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
            (Vector3) { .5f, 0, .5f }, (Vector3) { .5f, 0, -.5f }, WHITE)) {
        builder_destroy(&builder);
        return false;
    }
    *mesh = builder_finish(&builder);
    return mesh->vaoId != 0;
}

static bool load_detail_texture(
    Texture2D *texture, const char *fragment_shader_path, const char *asset_name)
{
    const char *slash = strrchr(fragment_shader_path, '/');
    if (slash == NULL || (size_t)(slash - fragment_shader_path) > 900)
        return false;
    char texture_path[1024];
    snprintf(texture_path, sizeof(texture_path), "%.*s/../textures/%s",
        (int)(slash - fragment_shader_path), fragment_shader_path, asset_name);
    *texture = LoadTexture(texture_path);
    if (texture->id == 0)
        return false;
    SetTextureFilter(*texture, TEXTURE_FILTER_POINT);
    SetTextureWrap(*texture, TEXTURE_WRAP_REPEAT);
    return true;
}

static bool init_detail_material(Material *material, Shader shader, Texture2D texture)
{
    *material = LoadMaterialDefault();
    if (material->maps == NULL)
        return false;
    material->shader = shader;
    material->maps[MATERIAL_MAP_DIFFUSE].texture = texture;
    return true;
}

static Shader load_city_shader(const char *fragment_shader_path)
{
    const char *slash = strrchr(fragment_shader_path, '/');
    if (slash == NULL || (size_t)(slash - fragment_shader_path) > 900)
        return (Shader) { 0 };
    char vertex_shader_path[1024];
    snprintf(vertex_shader_path, sizeof(vertex_shader_path), "%.*s/waterfront_city.vs",
        (int)(slash - fragment_shader_path), fragment_shader_path);
    return LoadShader(vertex_shader_path, fragment_shader_path);
}

static Shader load_shadow_shader(const char *fragment_shader_path)
{
    const char *slash = strrchr(fragment_shader_path, '/');
    if (slash == NULL || (size_t)(slash - fragment_shader_path) > 900)
        return (Shader) { 0 };
    char vertex_shader_path[1024];
    char fragment_path[1024];
    snprintf(vertex_shader_path, sizeof(vertex_shader_path), "%.*s/waterfront_shadow.vs",
        (int)(slash - fragment_shader_path), fragment_shader_path);
    snprintf(fragment_path, sizeof(fragment_path), "%.*s/waterfront_shadow.fs",
        (int)(slash - fragment_shader_path), fragment_shader_path);
    return LoadShader(vertex_shader_path, fragment_path);
}

static bool create_shadow_map(RenderTexture2D *target, int size)
{
    *target = (RenderTexture2D) { 0 };
    target->id = rlLoadFramebuffer();
    if (target->id == 0)
        return false;
    target->texture.width = size;
    target->texture.height = size;
    target->depth.id = rlLoadTextureDepth(size, size, false);
    target->depth.width = size;
    target->depth.height = size;
    target->depth.mipmaps = 1;
    if (target->depth.id == 0) {
        rlUnloadFramebuffer(target->id);
        *target = (RenderTexture2D) { 0 };
        return false;
    }
    rlEnableFramebuffer(target->id);
    rlFramebufferAttach(
        target->id, target->depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    bool complete = rlFramebufferComplete(target->id);
    rlDisableFramebuffer();
    if (!complete) {
        rlUnloadFramebuffer(target->id);
        *target = (RenderTexture2D) { 0 };
        return false;
    }
    SetTextureFilter(target->depth, TEXTURE_FILTER_POINT);
    return true;
}

bool waterfront_gpu_scene_init(WaterfrontGpuScene *scene, const char *fragment_shader_path)
{
    *scene = (WaterfrontGpuScene) { 0 };
    scene->shader = load_city_shader(fragment_shader_path);
    scene->shadow_shader = load_shadow_shader(fragment_shader_path);
    if (!IsShaderValid(scene->shader) || scene->shader.id == rlGetShaderIdDefault()
        || !IsShaderValid(scene->shadow_shader)
        || scene->shadow_shader.id == rlGetShaderIdDefault())
        goto failed;
    scene->city_material = LoadMaterialDefault();
    scene->life_material = LoadMaterialDefault();
    scene->shadow_material = LoadMaterialDefault();
    if (scene->city_material.maps == NULL || scene->life_material.maps == NULL
        || scene->shadow_material.maps == NULL)
        goto failed;
    scene->city_material.shader = scene->shader;
    scene->life_material.shader = scene->shader;
    scene->shadow_material.shader = scene->shadow_shader;
    if (!load_detail_texture(&scene->facade_texture, fragment_shader_path, "facade.png")
        || !load_detail_texture(&scene->roof_texture, fragment_shader_path, "roof.png")
        || !load_detail_texture(&scene->quay_texture, fragment_shader_path, "quay.png")
        || !init_detail_material(&scene->facade_material, scene->shader, scene->facade_texture)
        || !init_detail_material(&scene->roof_material, scene->shader, scene->roof_texture)
        || !init_detail_material(&scene->quay_material, scene->shader, scene->quay_texture))
        goto failed;
    if (!build_city(&scene->city, &scene->facades, &scene->roofs, &scene->quay)
        || !build_windows(&scene->windows) || !build_crane(&scene->crane))
        goto failed;
    if (!build_unit_cube(&scene->bridge) || !build_unit_cube(&scene->cube)
        || !build_unit_plane(&scene->plane))
        goto failed;
    scene->shadow_map_size = WATERFRONT_GPU_SHADOW_MAP_SIZE;
    if (!create_shadow_map(&scene->shadow_map, scene->shadow_map_size))
        goto failed;
    scene->lighting_enabled = true;
    scene->shadows_enabled = true;
    scene->ready = true;
    return true;
failed:
    waterfront_gpu_scene_destroy(scene);
    return false;
}

static Matrix transform_matrix(Transform3D transform)
{
    // MatrixMultiply(A, B) composes B before A in raymath. This matches the CPU renderer's
    // explicit order: scale a mesh vertex, rotate it, then add its world position.
    return MatrixMultiply(
        MatrixMultiply(MatrixScale(transform.scale.x, transform.scale.y, transform.scale.z),
            MatrixRotateXYZ(transform.rotation)),
        MatrixTranslate(transform.position.x, transform.position.y, transform.position.z));
}

static void set_object_color(Shader shader, Color color)
{
    float values[] = {
        (float)color.r / 255.0f,
        (float)color.g / 255.0f,
        (float)color.b / 255.0f,
        (float)color.a / 255.0f,
    };
    int location = GetShaderLocation(shader, "objectColor");
    if (location >= 0)
        SetShaderValue(shader, location, values, SHADER_UNIFORM_VEC4);
}

static void set_emissive(Shader shader, bool emissive)
{
    int location = GetShaderLocation(shader, "emissive");
    if (location >= 0) {
        int value = emissive ? 1 : 0;
        SetShaderValue(shader, location, &value, SHADER_UNIFORM_INT);
    }
}

static void draw_colored_mesh(
    Mesh mesh, Material material, Transform3D transform, Color color, bool emissive)
{
    set_object_color(material.shader, color);
    set_emissive(material.shader, emissive);
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

typedef enum {
    LIGHT_CRANE,
    LIGHT_LAMP,
    LIGHT_HEADLIGHT,
} LightKind;

typedef struct {
    int id;
    WaterfrontGpuLight light;
    float score;
    LightKind kind;
} LightCandidate;

static bool light_was_selected(const WaterfrontGpuScene *scene, int id)
{
    for (int i = 0; i < scene->local_light_count; ++i) {
        if (scene->local_light_ids[i] == id)
            return true;
    }
    return false;
}

static void add_light_candidate(
    LightCandidate *candidates, int *count, int id, WaterfrontGpuLight light, LightKind kind)
{
    if (*count >= 72)
        return;
    candidates[*count] = (LightCandidate) { .id = id, .light = light, .kind = kind };
    ++*count;
}

static int select_light_class(const WaterfrontGpuScene *scene, const LightCandidate *candidates,
    int candidate_count, WaterfrontSceneFrame frame, LightKind kind, LightCandidate *selected,
    int capacity)
{
    Vector3 view_direction
        = Vector3Normalize(Vector3Subtract(frame.camera.target, frame.camera.position));
    int selected_count = 0;
    for (int candidate = 0; candidate < candidate_count; ++candidate) {
        LightCandidate current = candidates[candidate];
        if (current.kind != kind)
            continue;
        Vector3 from_camera = Vector3Subtract(current.light.position, frame.camera.position);
        float view_depth = Vector3DotProduct(from_camera, view_direction);
        if (view_depth < -current.light.radius)
            continue;
        // The target is the authored shot focus, unlike the eye position which may sit far away
        // or above the street. It keeps the selected lights relevant to the visible composition.
        current.score = Vector3DistanceSqr(current.light.position, frame.camera.target);
        if (light_was_selected(scene, current.id))
            current.score *= .35f;
        int slot = selected_count;
        while (slot > 0 && current.score < selected[slot - 1].score) {
            if (slot < capacity)
                selected[slot] = selected[slot - 1];
            --slot;
        }
        if (slot < capacity) {
            selected[slot] = current;
            if (selected_count < capacity)
                ++selected_count;
        }
    }
    return selected_count;
}

// Every authored lamp stays in the forward-light list, so its pool cannot pop off while the camera
// approaches. The remaining slots retain the three crane lights and nearby moving headlights.
static void select_local_lights(WaterfrontGpuScene *scene, WaterfrontSceneFrame frame)
{
    LightCandidate candidates[72];
    int candidate_count = 0;
    add_light_candidate(candidates, &candidate_count, 10,
        (WaterfrontGpuLight) { .position = crane_point(-42, 75, -5),
            .direction = Vector3Normalize(crane_point(.2f, -1, .3f)),
            .color = { 255, 92, 48, 255 },
            .radius = 34,
            .intensity = 1.45f,
            .inner_cone_cos = .78f,
            .outer_cone_cos = .48f },
        LIGHT_CRANE);
    add_light_candidate(candidates, &candidate_count, 11,
        (WaterfrontGpuLight) { .position = crane_point(5, 75, -5),
            .direction = Vector3Normalize(crane_point(0, -1, .15f)),
            .color = { 106, 135, 255, 255 },
            .radius = 26,
            .intensity = 1.35f,
            .inner_cone_cos = .82f,
            .outer_cone_cos = .55f },
        LIGHT_CRANE);
    add_light_candidate(candidates, &candidate_count, 12,
        (WaterfrontGpuLight) { .position = crane_point(40, 75, -5),
            .direction = Vector3Normalize(crane_point(-.2f, -1, .3f)),
            .color = { 255, 78, 40, 255 },
            .radius = 31,
            .intensity = 1.3f,
            .inner_cone_cos = .78f,
            .outer_cone_cos = .48f },
        LIGHT_CRANE);
    for (int street_index = 0; street_index < 2; ++street_index) {
        WaterfrontStreet street = waterfront_scene_street(street_index);
        float length = street_length(street);
        int lamp_index = 0;
        for (float distance = 20; distance < length - 12; distance += 27, ++lamp_index) {
            Vector3 direction;
            Vector3 position = waterfront_scene_street_position(street, distance, &direction);
            Vector3 side = { direction.z, 0, -direction.x };
            position = Vector3Add(position, Vector3Scale(side, 2.6f));
            position.y = 8.5f;
            add_light_candidate(candidates, &candidate_count, 100 + street_index * 100 + lamp_index,
                (WaterfrontGpuLight) { .position = position,
                    .direction = { 0, -1, 0 },
                    .color = { 255, 202, 116, 255 },
                    .radius = 18,
                    .intensity = 1.0f,
                    .inner_cone_cos = -1,
                    .outer_cone_cos = -1 },
                LIGHT_LAMP);
        }
        for (int car = 0; car < 10; ++car) {
            int sign = car % 2 ? 1 : -1;
            float distance = fmodf(
                car * length / 10 + frame.time * (5.5f + street_index) * sign + length, length);
            Vector3 forward;
            Vector3 position = waterfront_scene_street_position(street, distance, &forward);
            forward = Vector3Scale(forward, (float)sign);
            Vector3 right = { forward.z, 0, -forward.x };
            position = Vector3Add(position, Vector3Scale(right, 1.25f));
            Vector3 direction = Vector3Normalize(Vector3Add(forward, (Vector3) { 0, -.22f, 0 }));
            for (int side = -1; side <= 1; side += 2) {
                Vector3 headlight = Vector3Add(position,
                    Vector3Add(Vector3Scale(forward, 2.0f), Vector3Scale(right, side * .6f)));
                headlight.y = 4.3f;
                add_light_candidate(candidates, &candidate_count,
                    1000 + street_index * 100 + car * 2 + (side > 0),
                    (WaterfrontGpuLight) { .position = headlight,
                        .direction = direction,
                        .color = { 255, 233, 174, 255 },
                        .radius = 18,
                        .intensity = 1.25f,
                        .inner_cone_cos = .90f,
                        .outer_cone_cos = .72f },
                    LIGHT_HEADLIGHT);
            }
        }
    }

    LightCandidate selected[WATERFRONT_GPU_MAX_LOCAL_LIGHTS];
    int selected_count
        = select_light_class(scene, candidates, candidate_count, frame, LIGHT_CRANE, selected, 3);
    selected_count += select_light_class(scene, candidates, candidate_count, frame, LIGHT_LAMP,
        &selected[selected_count], WATERFRONT_GPU_MAX_LOCAL_LIGHTS - selected_count - 4);
    selected_count += select_light_class(scene, candidates, candidate_count, frame, LIGHT_HEADLIGHT,
        &selected[selected_count], WATERFRONT_GPU_MAX_LOCAL_LIGHTS - selected_count);
    scene->local_light_count = selected_count;
    for (int i = 0; i < selected_count; ++i) {
        scene->local_light_ids[i] = selected[i].id;
        scene->local_lights[i] = selected[i].light;
    }
}

static void set_lighting_uniforms(WaterfrontGpuScene *scene)
{
    static const float ambient_color[] = { .07f, .09f, .18f };
    static const float directional_color[] = { .32f, .38f, .62f };
    float directional_direction[] = { DIRECTIONAL_LIGHT_DIRECTION.x, DIRECTIONAL_LIGHT_DIRECTION.y,
        DIRECTIONAL_LIGHT_DIRECTION.z };
    int location = GetShaderLocation(scene->shader, "ambientColor");
    if (location >= 0)
        SetShaderValue(scene->shader, location, ambient_color, SHADER_UNIFORM_VEC3);
    location = GetShaderLocation(scene->shader, "directionalColor");
    if (location >= 0)
        SetShaderValue(scene->shader, location, directional_color, SHADER_UNIFORM_VEC3);
    location = GetShaderLocation(scene->shader, "directionalDirection");
    if (location >= 0)
        SetShaderValue(scene->shader, location, directional_direction, SHADER_UNIFORM_VEC3);
    location = GetShaderLocation(scene->shader, "directionalIntensity");
    if (location >= 0) {
        float intensity = .72f;
        SetShaderValue(scene->shader, location, &intensity, SHADER_UNIFORM_FLOAT);
    }
    int shadows_enabled = scene->shadows_enabled && scene->shadow_map.depth.id != 0;
    location = GetShaderLocation(scene->shader, "shadowsEnabled");
    if (location >= 0)
        SetShaderValue(scene->shader, location, &shadows_enabled, SHADER_UNIFORM_INT);
    if (shadows_enabled) {
        location = GetShaderLocation(scene->shader, "lightViewProjection");
        if (location >= 0)
            SetShaderValueMatrix(scene->shader, location, scene->light_view_projection);
        location = GetShaderLocation(scene->shader, "shadowMap");
        if (location >= 0) {
            int texture_slot = 10;
            rlEnableShader(scene->shader.id);
            rlActiveTextureSlot(texture_slot);
            rlEnableTexture(scene->shadow_map.depth.id);
            rlSetUniform(location, &texture_slot, SHADER_UNIFORM_INT, 1);
            rlActiveTextureSlot(0);
        }
    }
    int light_count = scene->lighting_enabled ? scene->local_light_count : 0;
    location = GetShaderLocation(scene->shader, "localLightCount");
    if (location >= 0)
        SetShaderValue(scene->shader, location, &light_count, SHADER_UNIFORM_INT);
    if (light_count == 0)
        return;

    float positions[WATERFRONT_GPU_MAX_LOCAL_LIGHTS * 4];
    float colors[WATERFRONT_GPU_MAX_LOCAL_LIGHTS * 4];
    float directions[WATERFRONT_GPU_MAX_LOCAL_LIGHTS * 4];
    float cones[WATERFRONT_GPU_MAX_LOCAL_LIGHTS * 4];
    for (int i = 0; i < light_count; ++i) {
        WaterfrontGpuLight light = scene->local_lights[i];
        float *position = &positions[i * 4];
        float *color = &colors[i * 4];
        float *direction = &directions[i * 4];
        float *cone = &cones[i * 4];
        position[0] = light.position.x;
        position[1] = light.position.y;
        position[2] = light.position.z;
        position[3] = light.radius;
        color[0] = (float)light.color.r / 255.0f;
        color[1] = (float)light.color.g / 255.0f;
        color[2] = (float)light.color.b / 255.0f;
        color[3] = light.intensity;
        direction[0] = light.direction.x;
        direction[1] = light.direction.y;
        direction[2] = light.direction.z;
        direction[3] = 0;
        cone[0] = light.inner_cone_cos;
        cone[1] = light.outer_cone_cos;
        cone[2] = 0;
        cone[3] = 0;
    }
    location = GetShaderLocation(scene->shader, "localLightPositionRadius[0]");
    if (location >= 0)
        SetShaderValueV(scene->shader, location, positions, SHADER_UNIFORM_VEC4, light_count);
    location = GetShaderLocation(scene->shader, "localLightColorIntensity[0]");
    if (location >= 0)
        SetShaderValueV(scene->shader, location, colors, SHADER_UNIFORM_VEC4, light_count);
    location = GetShaderLocation(scene->shader, "localLightDirection[0]");
    if (location >= 0)
        SetShaderValueV(scene->shader, location, directions, SHADER_UNIFORM_VEC4, light_count);
    location = GetShaderLocation(scene->shader, "localLightCone[0]");
    if (location >= 0)
        SetShaderValueV(scene->shader, location, cones, SHADER_UNIFORM_VEC4, light_count);
}

static void draw_rectangle(const WaterfrontGpuScene *scene, Material material, Vector3 center,
    Vector3 forward, float width, float length, Color color)
{
    draw_colored_mesh(scene->plane, material,
        (Transform3D) { .position = center,
            .rotation = { 0, atan2f(forward.x, forward.z), 0 },
            .scale = { width, 1, length } },
        color, false);
}

static void draw_life(
    const WaterfrontGpuScene *scene, Material material, WaterfrontSceneFrame frame, bool emissive)
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
                (Color) { 58, 66, 79, 255 }, false);
            if (emissive)
                draw_colored_mesh(scene->cube, material,
                    (Transform3D) { .position = { p.x, 8.5f, p.z }, .scale = { .85f, .38f, .85f } },
                    LAMP_LENS_COLOR, true);
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
                colors[car % 4], false);
            draw_colored_mesh(scene->cube, material,
                (Transform3D) { .position = { p.x, 4.9f, p.z },
                    .rotation = { 0, yaw, 0 },
                    .scale = { 1.4f, .65f, 1.9f } },
                (Color) { 49, 65, 81, 255 }, false);
            if (emissive) {
                // Source lenses are always emissive. The bounded forward-light selection only
                // decides which nearby headlights affect surrounding surfaces this frame.
                for (int end = -1; end <= 1; end += 2) {
                    for (int side = -1; side <= 1; side += 2) {
                        Vector3 lamp = Vector3Add(p,
                            Vector3Add(Vector3Scale(forward, end * 2.0f),
                                Vector3Scale(right, side * .6f)));
                        lamp.y = 4.3f;
                        draw_colored_mesh(scene->cube, material,
                            (Transform3D) { .position = lamp, .scale = { .48f, .34f, .48f } },
                            end > 0 ? HEADLIGHT_LENS_COLOR : (Color) { 240, 56, 34, 255 }, true);
                    }
                }
            }
        }
    }
    float z = frame.boat_z;
    draw_colored_mesh(scene->cube, material,
        (Transform3D) { .position = { 12, .7f, z }, .scale = { 3.2f, 1.3f, 7 } },
        (Color) { 164, 177, 185, 255 }, false);
    draw_colored_mesh(scene->cube, material,
        (Transform3D) { .position = { 12, 1.9f, z + .5f }, .scale = { 2.5f, 1.3f, 3.2f } },
        (Color) { 69, 91, 111, 255 }, false);
    if (emissive)
        draw_colored_mesh(scene->cube, material,
            (Transform3D) { .position = { 12, 2, z - 1.15f }, .scale = { 2, .6f, .1f } },
            (Color) { 247, 191, 108, 255 }, true);
    if (emissive) {
        for (int i = 0; i < 8; ++i) {
            float drift = fmodf(frame.time * 2 + i * 1.8f, 14);
            draw_rectangle(scene, material, (Vector3) { 12, .04f, z + 3.95f + drift },
                (Vector3) { 0, 0, 1 }, (1.4f + drift * .14f) * 2, .3f,
                (Color) { 63, 98, 117, 255 });
        }
    }
}

static void draw_crane_lights(const WaterfrontGpuScene *scene, Material material)
{
    // Source fixtures stay intentionally chunky: at virtual resolution they read as the coloured
    // work lights in the reference rather than as a row of sub-pixel bulbs.
    draw_colored_mesh(scene->cube, material,
        (Transform3D) { .position = crane_point(-42, 75, -5), .scale = { 1.3f, .45f, .7f } },
        (Color) { 255, 92, 48, 255 }, true);
    draw_colored_mesh(scene->cube, material,
        (Transform3D) { .position = crane_point(5, 75, -5), .scale = { .85f, .85f, .85f } },
        (Color) { 122, 145, 255, 255 }, true);
    draw_colored_mesh(scene->cube, material,
        (Transform3D) { .position = crane_point(40, 75, -5), .scale = { 1.15f, .4f, .65f } },
        (Color) { 255, 86, 44, 255 }, true);
}

static Camera3D shadow_camera(const WaterfrontGpuScene *scene, WaterfrontSceneFrame frame)
{
    const float coverage = 500.0f;
    const float texel_size = coverage / scene->shadow_map_size;
    Vector3 light_direction = Vector3Normalize(DIRECTIONAL_LIGHT_DIRECTION);
    Vector3 right = Vector3Normalize(Vector3CrossProduct((Vector3) { 0, 1, 0 }, light_direction));
    Vector3 up = Vector3CrossProduct(light_direction, right);
    Vector3 target = frame.camera.target;
    float right_offset = roundf(Vector3DotProduct(target, right) / texel_size) * texel_size
        - Vector3DotProduct(target, right);
    float up_offset = roundf(Vector3DotProduct(target, up) / texel_size) * texel_size
        - Vector3DotProduct(target, up);
    target = Vector3Add(
        target, Vector3Add(Vector3Scale(right, right_offset), Vector3Scale(up, up_offset)));
    return (Camera3D) { .position = Vector3Add(target, Vector3Scale(light_direction, 600)),
        .target = target,
        .up = up,
        .fovy = coverage,
        .projection = CAMERA_ORTHOGRAPHIC };
}

static void draw_shadow_casters(
    const WaterfrontGpuScene *scene, Material material, WaterfrontSceneFrame frame)
{
    set_object_color(scene->shadow_shader, WHITE);
    set_emissive(scene->shadow_shader, false);
    rlEnableBackfaceCulling();
    DrawMesh(scene->city, material, MatrixIdentity());
    DrawMesh(scene->facades, material, MatrixIdentity());
    DrawMesh(scene->roofs, material, MatrixIdentity());
    DrawMesh(scene->quay, material, MatrixIdentity());
    draw_colored_mesh(scene->bridge, material, frame.bridge, frame.bridge_color, false);
    // Cars, boats, and lamp poles matter to the road shadow, but their lenses and wake do not.
    rlDisableBackfaceCulling();
    draw_life(scene, material, frame, false);
    rlEnableBackfaceCulling();
    DrawMesh(scene->crane, material, MatrixIdentity());
}

static void render_shadow_map(WaterfrontGpuScene *scene, WaterfrontSceneFrame frame)
{
    Camera3D camera = shadow_camera(scene, frame);
    unsigned int previous_framebuffer = rlGetActiveFramebuffer();
    int previous_width = rlGetFramebufferWidth();
    int previous_height = rlGetFramebufferHeight();
    BeginTextureMode(scene->shadow_map);
    ClearBackground(WHITE);
    BeginMode3D(camera);
    rlSetMatrixProjection(MatrixOrtho(-250, 250, -250, 250, .25, 1400));
    Matrix view = rlGetMatrixModelview();
    Matrix projection = rlGetMatrixProjection();
    draw_shadow_casters(scene, scene->shadow_material, frame);
    EndMode3D();
    EndTextureMode();
    // Begin/EndTextureMode does not stack: ending the depth pass always binds the window target.
    // Restore the caller's virtual framebuffer before submitting the visible city pass.
    if (previous_framebuffer != 0)
        rlEnableFramebuffer(previous_framebuffer);
    else
        rlDisableFramebuffer();
    rlViewport(0, 0, previous_width, previous_height);
    rlSetFramebufferWidth(previous_width);
    rlSetFramebufferHeight(previous_height);
    // raylib composes matrices in this order for its shaders; retain the pinned example's
    // convention so this matrix maps world positions into the depth texture's clip space.
    scene->light_view_projection = MatrixMultiply(view, projection);
}

void waterfront_gpu_scene_draw(
    WaterfrontGpuScene *scene, WaterfrontSceneFrame frame, int width, int height)
{
    Camera3D camera = { .position = frame.camera.position,
        .target = frame.camera.target,
        .up = frame.camera.up,
        .fovy = frame.camera.vertical_fov * RAD2DEG,
        .projection = CAMERA_PERSPECTIVE };
    Material material = scene->city_material;
    Material life_material = scene->life_material;
    if (scene->shadows_enabled)
        render_shadow_map(scene, frame);
    select_local_lights(scene, frame);
    set_lighting_uniforms(scene);
    BeginMode3D(camera);
    rlSetMatrixProjection(MatrixPerspective(frame.camera.vertical_fov, (float)width / (float)height,
        frame.camera.near_plane, frame.camera.far_plane));
    rlEnableBackfaceCulling();
    set_object_color(scene->shader, WHITE);
    set_emissive(scene->shader, false);
    DrawMesh(scene->city, material, MatrixIdentity());
    DrawMesh(scene->facades, scene->facade_material, MatrixIdentity());
    DrawMesh(scene->roofs, scene->roof_material, MatrixIdentity());
    DrawMesh(scene->quay, scene->quay_material, MatrixIdentity());
    set_emissive(scene->shader, true);
    DrawMesh(scene->windows, material, MatrixIdentity());
    draw_colored_mesh(scene->bridge, life_material, frame.bridge, frame.bridge_color, false);
    // These small closed meshes use reusable, non-indexed buffers. Keep them two-sided while
    // retaining depth testing, so their visibility does not depend on backend cull-state defaults.
    rlDisableBackfaceCulling();
    draw_life(scene, life_material, frame, true);
    rlEnableBackfaceCulling();
    set_object_color(scene->shader, WHITE);
    set_emissive(scene->shader, false);
    DrawMesh(scene->crane, material, MatrixIdentity());
    rlDisableBackfaceCulling();
    draw_crane_lights(scene, life_material);
    rlEnableBackfaceCulling();
    EndMode3D();
}

void waterfront_gpu_scene_set_lighting_enabled(WaterfrontGpuScene *scene, bool enabled)
{
    scene->lighting_enabled = enabled;
}

void waterfront_gpu_scene_set_shadows_enabled(WaterfrontGpuScene *scene, bool enabled)
{
    scene->shadows_enabled = enabled;
}

bool waterfront_gpu_scene_set_shadow_map_size(WaterfrontGpuScene *scene, int size)
{
    if (size != 512 && size != WATERFRONT_GPU_SHADOW_MAP_SIZE)
        return false;
    if (size == scene->shadow_map_size)
        return true;
    RenderTexture2D replacement;
    if (!create_shadow_map(&replacement, size))
        return false;
    if (scene->shadow_map.id != 0)
        rlUnloadFramebuffer(scene->shadow_map.id);
    scene->shadow_map = replacement;
    scene->shadow_map_size = size;
    return true;
}

Texture2D waterfront_gpu_scene_shadow_map(const WaterfrontGpuScene *scene)
{
    return scene->shadow_map.depth;
}

void waterfront_gpu_scene_destroy(WaterfrontGpuScene *scene)
{
    if (scene->city.vaoId != 0)
        UnloadMesh(scene->city);
    if (scene->facades.vaoId != 0)
        UnloadMesh(scene->facades);
    if (scene->roofs.vaoId != 0)
        UnloadMesh(scene->roofs);
    if (scene->quay.vaoId != 0)
        UnloadMesh(scene->quay);
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
    if (scene->city_material.maps != NULL) {
        scene->city_material.shader.id = rlGetShaderIdDefault();
        UnloadMaterial(scene->city_material);
    }
    if (scene->facade_material.maps != NULL) {
        scene->facade_material.shader.id = rlGetShaderIdDefault();
        UnloadMaterial(scene->facade_material);
    }
    if (scene->roof_material.maps != NULL) {
        scene->roof_material.shader.id = rlGetShaderIdDefault();
        UnloadMaterial(scene->roof_material);
    }
    if (scene->quay_material.maps != NULL) {
        scene->quay_material.shader.id = rlGetShaderIdDefault();
        UnloadMaterial(scene->quay_material);
    }
    if (scene->life_material.maps != NULL) {
        scene->life_material.shader.id = rlGetShaderIdDefault();
        UnloadMaterial(scene->life_material);
    }
    if (scene->shadow_material.maps != NULL) {
        scene->shadow_material.shader.id = rlGetShaderIdDefault();
        UnloadMaterial(scene->shadow_material);
    }
    if (scene->facade_texture.id != 0)
        UnloadTexture(scene->facade_texture);
    if (scene->roof_texture.id != 0)
        UnloadTexture(scene->roof_texture);
    if (scene->quay_texture.id != 0)
        UnloadTexture(scene->quay_texture);
    if (scene->shadow_map.id != 0)
        rlUnloadFramebuffer(scene->shadow_map.id);
    if (scene->shader.id != 0 && scene->shader.id != rlGetShaderIdDefault())
        UnloadShader(scene->shader);
    if (scene->shadow_shader.id != 0 && scene->shadow_shader.id != rlGetShaderIdDefault())
        UnloadShader(scene->shadow_shader);
    *scene = (WaterfrontGpuScene) { 0 };
}
