#include "scenes.h"
#include "renderer3d.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    SceneId id;
    float duration;
} Scene;

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

static Renderer3D cube_renderer;

void scenes_shutdown(void) { renderer3d_destroy(&cube_renderer); }

static void draw_cube(Color *pixels, VirtualSize size, float time)
{
    for (int y = 0; y < size.height; ++y) {
        unsigned char blue = (unsigned char)(18 + 22 * y / size.height);
        for (int x = 0; x < size.width; ++x)
            pixels[y * size.width + x] = (Color) { 5, 9, blue, 255 };
    }

    // One slow camera arc per two-pattern scene; all motion is scene-local and seekable.
    float phase = time / scene_duration(SCENE_CUBE) * 2.0f * PI;
    View3D camera = {
        .position = { sinf(phase) * 10.0f, 5.5f + sinf(phase) * 0.8f, -cosf(phase) * 12.0f },
        .target = { 0, 1.2f, 0 },
        .up = { 0, 1, 0 },
        .vertical_fov = 55.0f * DEG2RAD,
        .near_plane = 0.15f,
        .far_plane = 80.0f,
    };
    if (!renderer3d_begin(&cube_renderer, pixels, size.width, size.height, camera,
            (Vector3) { -0.4f, 0.8f, -0.5f }))
        return;

    renderer3d_draw_box(&cube_renderer,
        (Transform3D) { .position = { 0, -0.3f, 0 }, .scale = { 24, 0.6f, 30 } },
        (Color) { 45, 57, 78, 255 });
    renderer3d_draw_box(&cube_renderer,
        (Transform3D) { .position = { 0, 0.015f, 0 }, .scale = { 4, 0.03f, 30 } },
        (Color) { 21, 28, 44, 255 });

    // A small, deterministic blockout exercises shared geometry, depth and nonuniform scales.
    for (int row = 0; row < 5; ++row) {
        for (int side = -1; side <= 1; side += 2) {
            float height = 1.5f + (float)((row * 7 + (side + 1) * 3) % 5) * 0.7f;
            Color color = side < 0 ? (Color) { 55, 165, 205, 255 } : (Color) { 170, 95, 185, 255 };
            Transform3D building = {
                .position = { (float)side * 5.0f, height * 0.5f, (float)(row - 2) * 5.0f },
                .scale = { 3, height, 3 },
            };
            renderer3d_draw_box(&cube_renderer, building, color);
        }
    }

    // Keep the original spinning cube as a landmark while the surrounding scene grows.
    renderer3d_draw_box(&cube_renderer,
        (Transform3D) { .position = { 0, 0.3f, 0 }, .scale = { 2.8f, 0.6f, 2.8f } },
        (Color) { 95, 110, 145, 255 });
    renderer3d_draw_box(&cube_renderer,
        (Transform3D) {
            .position = { 0, 2.6f, 0 },
            .rotation = { time * 0.47f + 0.38f, time * 0.72f, sinf(time * 0.31f) * 0.22f },
            .scale = { 2, 2, 2 },
        },
        (Color) { 255, 165, 60, 255 });
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
