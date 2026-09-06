#include "scenes.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    SceneId id;
    float duration;
} Scene;

typedef struct {
    float x;
    float y;
    float z;
} Vec3;

typedef struct {
    float x;
    float y;
    float depth;
    Color color;
} ScreenVertex;

enum {
    NOISE_TABLE_SIZE = 128,
    NOISE_TABLE_MASK = NOISE_TABLE_SIZE - 1,
    FIRE_BLOCK_SIZE = 1,
    FIRE_PALETTE_SIZE = 256,
    FIRE_SAMPLE_STEP = 2,
};

static float noise_table[NOISE_TABLE_SIZE * NOISE_TABLE_SIZE];
static Color fire_palette_table[FIRE_PALETTE_SIZE];

// At 125 BPM with six ticks per row, each 64-row XM pattern lasts 7.68 seconds.
static const float PATTERN_DURATION = 7.68f;
static const Scene SCENES[] = {
    { SCENE_STARSCROLL, PATTERN_DURATION * 2.0f },
    { SCENE_FIRE, PATTERN_DURATION * 2.0f },
    { SCENE_PLASMA, PATTERN_DURATION * 2.0f },
    { SCENE_CUBE, PATTERN_DURATION * 2.0f },
};

const char *scene_name(SceneId id)
{
    switch (id) {
    case SCENE_STARSCROLL:
        return "starscroll";
    case SCENE_PLASMA:
        return "plasma";
    case SCENE_FIRE:
        return "fire";
    case SCENE_CUBE:
        return "cube";
    }
    return "unknown";
}

float scene_duration(SceneId id)
{
    for (size_t i = 0; i < sizeof(SCENES) / sizeof(SCENES[0]); ++i) {
        if (SCENES[i].id == id)
            return SCENES[i].duration;
    }
    return 0.0f;
}

float scene_start_time(SceneId id)
{
    float start_time = 0.0f;
    for (size_t i = 0; i < sizeof(SCENES) / sizeof(SCENES[0]); ++i) {
        if (SCENES[i].id == id)
            return start_time;
        start_time += SCENES[i].duration;
    }
    return 0.0f;
}

float scene_timeline_duration(void)
{
    float duration = 0.0f;
    for (size_t i = 0; i < sizeof(SCENES) / sizeof(SCENES[0]); ++i)
        duration += SCENES[i].duration;
    return duration;
}

bool scene_parse(const char *name, SceneId *id)
{
    for (size_t i = 0; i < sizeof(SCENES) / sizeof(SCENES[0]); ++i) {
        if (strcmp(name, scene_name(SCENES[i].id)) == 0) {
            *id = SCENES[i].id;
            return true;
        }
    }
    return false;
}

static float smoothstep(float value) { return value * value * (3.0f - 2.0f * value); }

static float clamp01(float value) { return fmaxf(0.0f, fminf(value, 1.0f)); }

static uint32_t hash_u32(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

static float random_unit(uint32_t seed)
{
    return (float)(hash_u32(seed) & 0x00ffffffu) / 16777215.0f;
}

static void initialize_noise_table(void)
{
    for (int y = 0; y < NOISE_TABLE_SIZE; ++y) {
        for (int x = 0; x < NOISE_TABLE_SIZE; ++x) {
            uint32_t seed = (uint32_t)x * 0x1f123bb5u ^ (uint32_t)y * 0x5f356495u;
            uint8_t sample = (uint8_t)(hash_u32(seed) & UINT8_MAX);
            noise_table[y * NOISE_TABLE_SIZE + x] = (float)sample / (float)UINT8_MAX;
        }
    }
}

static float value_noise(float x, float y)
{
    int ix = (int)floorf(x);
    int iy = (int)floorf(y);
    float fx = smoothstep(x - (float)ix);
    float fy = smoothstep(y - (float)iy);
    int x0 = (int)((uint32_t)ix & NOISE_TABLE_MASK);
    int x1 = (x0 + 1) & NOISE_TABLE_MASK;
    int y0 = (int)((uint32_t)iy & NOISE_TABLE_MASK);
    int y1 = (y0 + 1) & NOISE_TABLE_MASK;
    int top_row = y0 * NOISE_TABLE_SIZE;
    int bottom_row = y1 * NOISE_TABLE_SIZE;
    float top = noise_table[top_row + x0] * (1.0f - fx) + noise_table[top_row + x1] * fx;
    float bottom = noise_table[bottom_row + x0] * (1.0f - fx) + noise_table[bottom_row + x1] * fx;
    return top * (1.0f - fy) + bottom * fy;
}

static float fire_noise(float x, float y)
{
    float value = value_noise(x, y) * 0.57f;
    value += value_noise(x * 2.03f + 17.0f, y * 2.03f - 11.0f) * 0.29f;
    value += value_noise(x * 4.11f - 7.0f, y * 4.11f + 23.0f) * 0.14f;
    return value;
}

static void put_pixel(Color *pixels, VirtualSize size, int x, int y, Color color)
{
    if (x >= 0 && x < size.width && y >= 0 && y < size.height)
        pixels[y * size.width + x] = color;
}

static void draw_line(Color *pixels, VirtualSize size, int x0, int y0, int x1, int y1, Color color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        put_pixel(pixels, size, x0, y0, color);
        if (x0 == x1 && y0 == y1)
            break;
        int twice_error = error * 2;
        if (twice_error >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice_error <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

static bool project_star(
    float x, float y, float z, float pitch, VirtualSize size, int *screen_x, int *screen_y)
{
    float sine = sinf(pitch);
    float cosine = cosf(pitch);
    float camera_y = y * cosine + z * sine;
    float camera_z = -y * sine + z * cosine;
    if (camera_z < 0.35f)
        return false;

    float focal_length = (float)size.width * 0.72f;
    *screen_x = size.width / 2 + (int)(x * focal_length / camera_z);
    *screen_y = size.height / 2 - (int)(camera_y * focal_length / camera_z);
    return true;
}

static void draw_starscroll(Color *pixels, VirtualSize size, float time, float progress)
{
    const int star_count = 700;
    const float field_depth = 42.0f;
    const float speed = 7.0f;
    const float initial_pitch = 0.78f;
    const float trail_time = 0.035f;

    for (int i = 0; i < size.width * size.height; ++i)
        pixels[i] = (Color) { 0, 1, 7, 255 };

    float turn = smoothstep(progress);
    float pitch = initial_pitch * (1.0f - turn);
    float previous_progress = fmaxf(0.0f, progress - trail_time / PATTERN_DURATION);
    float previous_pitch = initial_pitch * (1.0f - smoothstep(previous_progress));

    for (int i = 0; i < star_count; ++i) {
        uint32_t seed = (uint32_t)i * 3u;
        float x = (random_unit(seed + 1u) - 0.5f) * 38.0f;
        float y = (random_unit(seed + 2u) - 0.5f) * 88.0f;
        float original_z = random_unit(seed + 3u) * field_depth;
        float z = fmodf(original_z - time * speed, field_depth);
        if (z < 0.0f)
            z += field_depth;
        z += 0.45f;

        int x0;
        int y0;
        int x1;
        int y1;
        if (!project_star(x, y, z, pitch, size, &x0, &y0)
            || !project_star(x, y, z + speed * trail_time, previous_pitch, size, &x1, &y1))
            continue;
        if ((x0 < 0 && x1 < 0) || (x0 >= size.width && x1 >= size.width) || (y0 < 0 && y1 < 0)
            || (y0 >= size.height && y1 >= size.height))
            continue;

        float brightness = 1.0f - fminf(z / field_depth, 1.0f);
        unsigned char level = (unsigned char)(90.0f + 165.0f * brightness);
        Color color = { level, level, (unsigned char)fminf(255.0f, level + 24.0f), 255 };
        draw_line(pixels, size, x1, y1, x0, y0, color);
        if (brightness > 0.72f) {
            put_pixel(pixels, size, x0 + 1, y0, color);
            put_pixel(pixels, size, x0, y0 + 1, color);
        }
    }
}

static Color palette(float value)
{
    const float tau = 6.28318530718f;
    unsigned char r = (unsigned char)(128.0f + 127.0f * sinf(tau * (value + 0.00f)));
    unsigned char g = (unsigned char)(128.0f + 127.0f * sinf(tau * (value + 0.33f)));
    unsigned char b = (unsigned char)(128.0f + 127.0f * sinf(tau * (value + 0.67f)));
    return (Color) { r, g, b, 255 };
}

static void draw_plasma(Color *pixels, VirtualSize size, float time, float beat)
{
    for (int y = 0; y < size.height; ++y) {
        for (int x = 0; x < size.width; ++x) {
            float fx = (float)x / (float)size.width;
            float fy = (float)y / (float)size.height;
            float dx = fx - 0.5f;
            float dy = fy - 0.5f;
            float v = sinf(fx * 25.0f + time * 1.7f);
            v += sinf(fy * 19.0f - time * 1.3f);
            v += sinf(sqrtf(dx * dx + dy * dy) * 42.0f - time * 3.0f);
            v = v / 6.0f + beat * 0.0025f;
            pixels[y * size.width + x] = palette(v);
        }
    }
}

static Color fire_palette(float heat)
{
    heat = clamp01(heat);
    if (heat < 0.2f) {
        float amount = heat / 0.2f;
        return (Color) { (unsigned char)(70.0f * amount), 0, 3, 255 };
    }
    if (heat < 0.48f) {
        float amount = (heat - 0.2f) / 0.28f;
        return (Color) { (unsigned char)(70.0f + 185.0f * amount), (unsigned char)(65.0f * amount),
            0, 255 };
    }
    if (heat < 0.78f) {
        float amount = (heat - 0.48f) / 0.30f;
        return (Color) { 255, (unsigned char)(65.0f + 160.0f * amount),
            (unsigned char)(35.0f * amount), 255 };
    }
    float amount = (heat - 0.78f) / 0.22f;
    return (Color) { 255, (unsigned char)(225.0f + 30.0f * amount),
        (unsigned char)(35.0f + 190.0f * amount), 255 };
}

static void initialize_fire_palette(void)
{
    for (int i = 0; i < FIRE_PALETTE_SIZE; ++i)
        fire_palette_table[i] = fire_palette((float)i / (float)(FIRE_PALETTE_SIZE - 1));
}

static Color average_color(Color left, Color right)
{
    return (Color) { (unsigned char)(((int)left.r + (int)right.r) / 2),
        (unsigned char)(((int)left.g + (int)right.g) / 2),
        (unsigned char)(((int)left.b + (int)right.b) / 2), 255 };
}

static void draw_fire(Color *pixels, VirtualSize size, float time, float beat)
{
    const float inverse_width = 1.0f / (float)size.width;
    const float inverse_height = 1.0f / (float)size.height;
    float pulse = 0.03f * sinf(beat * 3.14159265f);
    for (int y = 0; y < size.height; y += FIRE_SAMPLE_STEP) {
        float height = (float)(size.height - 1 - y) * inverse_height;
        float drift = 0.07f * sinf(height * 13.0f - time * 1.8f);
        Color *row = pixels + y * size.width;
        for (int x = 0; x < size.width; x += FIRE_SAMPLE_STEP) {
            float fx = (float)x * inverse_width;
            float billow = fire_noise(fx * 7.0f + drift, height * 6.0f - time * 1.9f);
            float detail = value_noise(fx * 31.0f - time * 0.35f, height * 18.0f - time * 4.3f);
            float edge = 0.72f - height + (billow - 0.5f) * 0.62f + pulse;
            float flame = clamp01(edge * 5.0f);
            float heat = flame * clamp01(1.16f - height * 1.28f + (detail - 0.5f) * 0.25f);
            row[x] = fire_palette_table[(int)(heat * (float)(FIRE_PALETTE_SIZE - 1))];
        }
    }

    for (int y = 0; y < size.height; y += FIRE_SAMPLE_STEP) {
        Color *row = pixels + y * size.width;
        for (int x = FIRE_BLOCK_SIZE; x < size.width; x += FIRE_SAMPLE_STEP) {
            Color right = x + 1 < size.width ? row[x + 1] : row[x - 1];
            row[x] = average_color(row[x - 1], right);
        }
    }

    for (int y = FIRE_BLOCK_SIZE; y < size.height; y += FIRE_SAMPLE_STEP) {
        Color *row = pixels + y * size.width;
        Color *top = row - size.width;
        Color *bottom = y + 1 < size.height ? row + size.width : top;
        for (int x = 0; x < size.width; ++x)
            row[x] = average_color(top[x], bottom[x]);
    }
}

static Vec3 rotate_vertex(Vec3 vertex, float yaw, float pitch, float roll)
{
    float yaw_sine = sinf(yaw);
    float yaw_cosine = cosf(yaw);
    float pitch_sine = sinf(pitch);
    float pitch_cosine = cosf(pitch);
    Vec3 yawed = {
        vertex.x * yaw_cosine + vertex.z * yaw_sine,
        vertex.y,
        -vertex.x * yaw_sine + vertex.z * yaw_cosine,
    };
    Vec3 pitched = {
        yawed.x,
        yawed.y * pitch_cosine - yawed.z * pitch_sine,
        yawed.y * pitch_sine + yawed.z * pitch_cosine,
    };
    float roll_sine = sinf(roll);
    float roll_cosine = cosf(roll);
    return (Vec3) {
        pitched.x * roll_cosine - pitched.y * roll_sine,
        pitched.x * roll_sine + pitched.y * roll_cosine,
        pitched.z,
    };
}

static ScreenVertex project_vertex(Vec3 vertex, Color color, VirtualSize size)
{
    float focal_length = (float)(size.width < size.height ? size.width : size.height) * 1.35f;
    return (ScreenVertex) {
        (float)size.width * 0.5f + vertex.x * focal_length / vertex.z,
        (float)size.height * 0.5f - vertex.y * focal_length / vertex.z,
        vertex.z,
        color,
    };
}

static float triangle_edge(ScreenVertex a, ScreenVertex b, float x, float y)
{
    return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
}

static void rasterize_triangle(
    Color *pixels, VirtualSize size, ScreenVertex a, ScreenVertex b, ScreenVertex c)
{
    float area = triangle_edge(a, b, c.x, c.y);
    if (fabsf(area) < 0.001f)
        return;

    int min_x = (int)floorf(fminf(a.x, fminf(b.x, c.x)));
    int max_x = (int)ceilf(fmaxf(a.x, fmaxf(b.x, c.x)));
    int min_y = (int)floorf(fminf(a.y, fminf(b.y, c.y)));
    int max_y = (int)ceilf(fmaxf(a.y, fmaxf(b.y, c.y)));
    min_x = min_x < 0 ? 0 : min_x;
    max_x = max_x >= size.width ? size.width - 1 : max_x;
    min_y = min_y < 0 ? 0 : min_y;
    max_y = max_y >= size.height ? size.height - 1 : max_y;

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            float sample_x = (float)x + 0.5f;
            float sample_y = (float)y + 0.5f;
            float weight_a = triangle_edge(b, c, sample_x, sample_y) / area;
            float weight_b = triangle_edge(c, a, sample_x, sample_y) / area;
            float weight_c = triangle_edge(a, b, sample_x, sample_y) / area;
            if (weight_a < 0.0f || weight_b < 0.0f || weight_c < 0.0f)
                continue;

            Color color = {
                (unsigned char)(weight_a * a.color.r + weight_b * b.color.r + weight_c * c.color.r),
                (unsigned char)(weight_a * a.color.g + weight_b * b.color.g + weight_c * c.color.g),
                (unsigned char)(weight_a * a.color.b + weight_b * b.color.b + weight_c * c.color.b),
                255,
            };
            put_pixel(pixels, size, x, y, color);
        }
    }
}

typedef struct {
    int vertices[4];
    Color color;
    float depth;
} CubeFace;

static Color shade_color(Color color, float brightness)
{
    return (Color) {
        (unsigned char)((float)color.r * brightness),
        (unsigned char)((float)color.g * brightness),
        (unsigned char)((float)color.b * brightness),
        255,
    };
}

static void draw_cube(Color *pixels, VirtualSize size, float time)
{
    const Vec3 model_vertices[] = {
        { -1.0f, -1.0f, -1.0f },
        { 1.0f, -1.0f, -1.0f },
        { 1.0f, 1.0f, -1.0f },
        { -1.0f, 1.0f, -1.0f },
        { -1.0f, -1.0f, 1.0f },
        { 1.0f, -1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f },
        { -1.0f, 1.0f, 1.0f },
    };
    CubeFace faces[] = {
        { { 0, 1, 2, 3 }, { 255, 45, 190, 255 }, 0.0f },
        { { 5, 4, 7, 6 }, { 35, 100, 255, 255 }, 0.0f },
        { { 4, 0, 3, 7 }, { 25, 220, 255, 255 }, 0.0f },
        { { 1, 5, 6, 2 }, { 255, 105, 35, 255 }, 0.0f },
        { { 3, 2, 6, 7 }, { 245, 255, 110, 255 }, 0.0f },
        { { 4, 5, 1, 0 }, { 135, 75, 255, 255 }, 0.0f },
    };

    for (int y = 0; y < size.height; ++y) {
        unsigned char blue = (unsigned char)(10 + 12 * y / size.height);
        for (int x = 0; x < size.width; ++x)
            pixels[y * size.width + x] = (Color) { 2, 4, blue, 255 };
    }

    Vec3 transformed[8];
    ScreenVertex projected[8];
    float yaw = time * 0.72f;
    float pitch = time * 0.47f + 0.38f;
    float roll = sinf(time * 0.31f) * 0.22f;
    for (int i = 0; i < 8; ++i) {
        transformed[i] = rotate_vertex(model_vertices[i], yaw, pitch, roll);
        transformed[i].z += 4.5f;
        projected[i] = project_vertex(transformed[i], WHITE, size);
    }

    for (int face = 0; face < 6; ++face) {
        faces[face].depth = 0.0f;
        for (int corner = 0; corner < 4; ++corner)
            faces[face].depth += transformed[faces[face].vertices[corner]].z;
    }
    for (int i = 1; i < 6; ++i) {
        CubeFace face = faces[i];
        int position = i;
        while (position > 0 && faces[position - 1].depth < face.depth) {
            faces[position] = faces[position - 1];
            --position;
        }
        faces[position] = face;
    }

    const Vec3 light = { -0.36f, 0.48f, -0.80f };
    const Color edge_color = { 225, 245, 255, 255 };
    for (int face = 0; face < 6; ++face) {
        int a = faces[face].vertices[0];
        int b = faces[face].vertices[1];
        int c = faces[face].vertices[2];
        int d = faces[face].vertices[3];
        if (triangle_edge(projected[a], projected[b], projected[c].x, projected[c].y) <= 0.0f)
            continue;

        Vec3 side_ab = {
            transformed[b].x - transformed[a].x,
            transformed[b].y - transformed[a].y,
            transformed[b].z - transformed[a].z,
        };
        Vec3 side_ac = {
            transformed[c].x - transformed[a].x,
            transformed[c].y - transformed[a].y,
            transformed[c].z - transformed[a].z,
        };
        Vec3 normal = {
            side_ab.y * side_ac.z - side_ab.z * side_ac.y,
            side_ab.z * side_ac.x - side_ab.x * side_ac.z,
            side_ab.x * side_ac.y - side_ab.y * side_ac.x,
        };
        float light_amount = (normal.x * light.x + normal.y * light.y + normal.z * light.z) * 0.25f;
        float brightness = 0.38f + 0.62f * fmaxf(0.0f, light_amount);
        Color color = shade_color(faces[face].color, brightness);
        ScreenVertex corners[] = { projected[a], projected[b], projected[c], projected[d] };
        for (int corner = 0; corner < 4; ++corner)
            corners[corner].color = color;

        rasterize_triangle(pixels, size, corners[0], corners[1], corners[2]);
        rasterize_triangle(pixels, size, corners[0], corners[2], corners[3]);
        for (int corner = 0; corner < 4; ++corner) {
            ScreenVertex start = corners[corner];
            ScreenVertex end = corners[(corner + 1) % 4];
            draw_line(pixels, size, (int)start.x, (int)start.y, (int)end.x, (int)end.y, edge_color);
        }
    }
}

SceneFrame scene_at(float time)
{
    float loop_time = fmodf(time, scene_timeline_duration());
    for (size_t i = 0; i < sizeof(SCENES) / sizeof(SCENES[0]); ++i) {
        if (loop_time < SCENES[i].duration) {
            return (SceneFrame) {
                SCENES[i].id,
                loop_time,
                loop_time / SCENES[i].duration,
            };
        }
        loop_time -= SCENES[i].duration;
    }
    return (SceneFrame) { SCENES[0].id, 0.0f, 0.0f };
}

void scene_draw(Color *pixels, VirtualSize size, SceneFrame scene, float timeline_time, float beat)
{
    switch (scene.id) {
    case SCENE_STARSCROLL:
        draw_starscroll(pixels, size, scene.time, scene.progress);
        break;
    case SCENE_PLASMA:
        draw_plasma(pixels, size, timeline_time, beat);
        break;
    case SCENE_FIRE:
        draw_fire(pixels, size, timeline_time, beat);
        break;
    case SCENE_CUBE:
        draw_cube(pixels, size, scene.time);
        break;
    }
}

void scenes_initialize(void)
{
    initialize_noise_table();
    initialize_fire_palette();
}

size_t scenes_noise_table_bytes(void) { return sizeof(noise_table); }
