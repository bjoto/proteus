#include "raylib.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef PROTEUS_BUILD_TYPE
#define PROTEUS_BUILD_TYPE "unknown"
#endif

typedef struct {
    int width;
    int height;
} VirtualSize;

typedef enum {
    SCENE_STARSCROLL,
    SCENE_PLASMA,
    SCENE_FIRE,
    SCENE_TRIANGLE,
} SceneId;

typedef struct {
    SceneId id;
    float duration;
} Scene;

typedef struct {
    SceneId id;
    float time;
    float progress;
} SceneFrame;

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
    PROFILER_HISTORY_COUNT = 240,
    NOISE_TABLE_SIZE = 128,
    NOISE_TABLE_MASK = NOISE_TABLE_SIZE - 1,
};

static float noise_table[NOISE_TABLE_SIZE * NOISE_TABLE_SIZE];

typedef struct {
    float frame_ms[PROFILER_HISTORY_COUNT];
    float scene_ms[PROFILER_HISTORY_COUNT];
    float present_ms[PROFILER_HISTORY_COUNT];
    int next;
    int count;
} Profiler;

typedef struct {
    float latest;
    float minimum;
    float average;
    float percentile_95;
    float maximum;
} ProfileStats;

typedef struct {
    ProfileStats frame;
    ProfileStats scene_cost;
    ProfileStats present;
    SceneFrame scene;
    float timeline_time;
    float observed_fps;
    float music_played;
    float music_length;
    int raylib_fps;
    int monitor_refresh_rate;
    int sample_count;
    int window_width;
    int window_height;
    int render_width;
    int render_height;
    int scale;
    int framebuffer_kib;
    bool music_playing;
} ProfilerSnapshot;

typedef struct {
    bool enabled;
    bool visible;
    bool show_help;
    SceneId scene_id;
    int frames;
    int warmup_frames;
    int size_index;
} ProfileOptions;

static const VirtualSize VIRTUAL_SIZES[] = {
    { 320, 180 },
    { 320, 200 },
    { 320, 240 },
};

// At 125 BPM with six ticks per row, each 64-row XM pattern lasts 7.68 seconds.
static const float PATTERN_DURATION = 7.68f;
static const Scene SCENES[] = {
    { SCENE_STARSCROLL, PATTERN_DURATION * 2.0f },
    { SCENE_FIRE, PATTERN_DURATION * 2.0f },
    { SCENE_PLASMA, PATTERN_DURATION * 2.0f },
    { SCENE_TRIANGLE, PATTERN_DURATION * 2.0f },
};

static const char *scene_name(SceneId id)
{
    switch (id) {
    case SCENE_STARSCROLL:
        return "starscroll";
    case SCENE_PLASMA:
        return "plasma";
    case SCENE_FIRE:
        return "fire";
    case SCENE_TRIANGLE:
        return "triangle";
    }
    return "unknown";
}

static float scene_duration(SceneId id)
{
    for (size_t i = 0; i < sizeof(SCENES) / sizeof(SCENES[0]); ++i) {
        if (SCENES[i].id == id)
            return SCENES[i].duration;
    }
    return 0.0f;
}

static bool parse_scene_id(const char *name, SceneId *id)
{
    for (size_t i = 0; i < sizeof(SCENES) / sizeof(SCENES[0]); ++i) {
        if (strcmp(name, scene_name(SCENES[i].id)) == 0) {
            *id = SCENES[i].id;
            return true;
        }
    }
    return false;
}

static bool parse_count(const char *text, int minimum, int *value)
{
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (text[0] == '\0' || *end != '\0' || parsed < minimum || parsed > INT_MAX)
        return false;
    *value = (int)parsed;
    return true;
}

static int resolution_index(const char *text)
{
    for (size_t i = 0; i < sizeof(VIRTUAL_SIZES) / sizeof(VIRTUAL_SIZES[0]); ++i) {
        char resolution[32];
        snprintf(resolution, sizeof(resolution), "%dx%d", VIRTUAL_SIZES[i].width,
            VIRTUAL_SIZES[i].height);
        if (strcmp(text, resolution) == 0)
            return (int)i;
    }
    return -1;
}

static void print_usage(const char *program)
{
    printf("Usage: %s [--profile SCENE [options]]\n", program);
    printf("Scenes: starscroll, fire, plasma, triangle\n");
    printf("Profile options:\n");
    printf("  --frames N              measured frames (default: 600)\n");
    printf("  --warmup N              unmeasured warm-up frames (default: 60)\n");
    printf("  --resolution WIDTHxHEIGHT (default: 320x200)\n");
    printf("  --visible               include upload/presentation using a visible window\n");
}

static bool parse_options(int argc, char **argv, ProfileOptions *options)
{
    *options = (ProfileOptions) {
        .scene_id = SCENE_STARSCROLL,
        .frames = 600,
        .warmup_frames = 60,
        .size_index = 1,
    };
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            options->show_help = true;
        } else if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc) {
            options->enabled = true;
            if (!parse_scene_id(argv[++i], &options->scene_id)) {
                fprintf(stderr, "Unknown scene: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            if (!parse_count(argv[++i], 1, &options->frames)) {
                fprintf(stderr, "Invalid frame count: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            if (!parse_count(argv[++i], 0, &options->warmup_frames)) {
                fprintf(stderr, "Invalid warm-up count: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--resolution") == 0 && i + 1 < argc) {
            options->size_index = resolution_index(argv[++i]);
            if (options->size_index < 0) {
                fprintf(stderr, "Unsupported resolution: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--visible") == 0) {
            options->visible = true;
        } else {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            return false;
        }
    }
    if (!options->enabled && argc > 1 && !options->show_help) {
        fprintf(stderr, "Profiling options require --profile SCENE\n");
        return false;
    }
    return true;
}

static int compare_floats(const void *left, const void *right)
{
    float a = *(const float *)left;
    float b = *(const float *)right;
    return (a > b) - (a < b);
}

static ProfileStats profile_stats(const float *samples, int next, int count)
{
    ProfileStats stats = { 0 };
    if (count == 0)
        return stats;

    float sorted[PROFILER_HISTORY_COUNT];
    stats.minimum = samples[0];
    stats.maximum = samples[0];
    for (int i = 0; i < count; ++i) {
        float sample = samples[i];
        sorted[i] = sample;
        stats.minimum = fminf(stats.minimum, sample);
        stats.maximum = fmaxf(stats.maximum, sample);
        stats.average += sample;
    }
    stats.average /= (float)count;
    qsort(sorted, (size_t)count, sizeof(sorted[0]), compare_floats);
    stats.percentile_95 = sorted[(count - 1) * 95 / 100];
    int latest = (next + PROFILER_HISTORY_COUNT - 1) % PROFILER_HISTORY_COUNT;
    stats.latest = samples[latest];
    return stats;
}

static void profiler_add(Profiler *profiler, float frame_ms, float scene_ms, float present_ms)
{
    profiler->frame_ms[profiler->next] = frame_ms;
    profiler->scene_ms[profiler->next] = scene_ms;
    profiler->present_ms[profiler->next] = present_ms;
    profiler->next = (profiler->next + 1) % PROFILER_HISTORY_COUNT;
    if (profiler->count < PROFILER_HISTORY_COUNT)
        ++profiler->count;
}

static ProfilerSnapshot profiler_snapshot(const Profiler *profiler, SceneFrame scene,
    float timeline_time, VirtualSize size, Rectangle destination, bool music_ready, Music music)
{
    ProfilerSnapshot snapshot = {
        .frame = profile_stats(profiler->frame_ms, profiler->next, profiler->count),
        .scene_cost = profile_stats(profiler->scene_ms, profiler->next, profiler->count),
        .present = profile_stats(profiler->present_ms, profiler->next, profiler->count),
        .scene = scene,
        .timeline_time = timeline_time,
        .music_played = music_ready ? GetMusicTimePlayed(music) : 0.0f,
        .music_length = music_ready ? GetMusicTimeLength(music) : 0.0f,
        .raylib_fps = GetFPS(),
        .monitor_refresh_rate = GetMonitorRefreshRate(GetCurrentMonitor()),
        .sample_count = profiler->count,
        .window_width = GetScreenWidth(),
        .window_height = GetScreenHeight(),
        .render_width = GetRenderWidth(),
        .render_height = GetRenderHeight(),
        .scale = size.width > 0 ? (int)destination.width / size.width : 0,
        .framebuffer_kib = size.width * size.height * (int)sizeof(Color) / 1024,
        .music_playing = music_ready && IsMusicStreamPlaying(music),
    };
    snapshot.observed_fps = snapshot.frame.average > 0.0f ? 1000.0f / snapshot.frame.average : 0.0f;
    return snapshot;
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

static void draw_fire(Color *pixels, VirtualSize size, float time, float beat)
{
    float pulse = 0.03f * sinf(beat * 3.14159265f);
    for (int y = 0; y < size.height; ++y) {
        float height = (float)(size.height - 1 - y) / (float)size.height;
        float drift = 0.07f * sinf(height * 13.0f - time * 1.8f);
        for (int x = 0; x < size.width; ++x) {
            float fx = (float)x / (float)size.width;
            float billow = fire_noise(fx * 7.0f + drift, height * 6.0f - time * 1.9f);
            float detail = value_noise(fx * 31.0f - time * 0.35f, height * 18.0f - time * 4.3f);
            float edge = 0.72f - height + (billow - 0.5f) * 0.62f + pulse;
            float flame = clamp01(edge * 5.0f);
            float heat = flame * clamp01(1.16f - height * 1.28f + (detail - 0.5f) * 0.25f);
            pixels[y * size.width + x] = fire_palette(heat);
        }
    }
}

static Vec3 rotate_triangle_vertex(Vec3 vertex, float yaw, float pitch)
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
    return (Vec3) {
        yawed.x,
        yawed.y * pitch_cosine - yawed.z * pitch_sine,
        yawed.y * pitch_sine + yawed.z * pitch_cosine,
    };
}

static ScreenVertex project_triangle_vertex(Vec3 vertex, Color color, VirtualSize size)
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

static void draw_triangle_3d(Color *pixels, VirtualSize size, float time)
{
    const Vec3 model_vertices[] = {
        { -1.0f, -0.75f, 0.0f },
        { 1.0f, -0.75f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
    };
    const Color vertex_colors[] = {
        { 255, 45, 190, 255 },
        { 25, 220, 255, 255 },
        { 245, 255, 110, 255 },
    };

    for (int y = 0; y < size.height; ++y) {
        unsigned char blue = (unsigned char)(10 + 12 * y / size.height);
        for (int x = 0; x < size.width; ++x)
            pixels[y * size.width + x] = (Color) { 2, 4, blue, 255 };
    }

    ScreenVertex projected[3];
    for (int i = 0; i < 3; ++i) {
        Vec3 transformed
            = rotate_triangle_vertex(model_vertices[i], time * 0.72f, sinf(time * 0.37f) * 0.42f);
        transformed.z += 3.4f;
        projected[i] = project_triangle_vertex(transformed, vertex_colors[i], size);
    }

    rasterize_triangle(pixels, size, projected[0], projected[1], projected[2]);
    Color edge_color = { 225, 245, 255, 255 };
    draw_line(pixels, size, (int)projected[0].x, (int)projected[0].y, (int)projected[1].x,
        (int)projected[1].y, edge_color);
    draw_line(pixels, size, (int)projected[1].x, (int)projected[1].y, (int)projected[2].x,
        (int)projected[2].y, edge_color);
    draw_line(pixels, size, (int)projected[2].x, (int)projected[2].y, (int)projected[0].x,
        (int)projected[0].y, edge_color);
}

static SceneFrame scene_at(float time)
{
    float total_duration = 0.0f;
    for (size_t i = 0; i < sizeof(SCENES) / sizeof(SCENES[0]); ++i)
        total_duration += SCENES[i].duration;

    float loop_time = fmodf(time, total_duration);
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

static void draw_scene(
    Color *pixels, VirtualSize size, SceneFrame scene, float timeline_time, float beat)
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
    case SCENE_TRIANGLE:
        draw_triangle_3d(pixels, size, scene.time);
        break;
    }
}

static Rectangle integer_scaled_destination(int screen_width, int screen_height, VirtualSize size)
{
    int scale_x = screen_width / size.width;
    int scale_y = screen_height / size.height;
    int scale = scale_x < scale_y ? scale_x : scale_y;
    if (scale < 1)
        scale = 1;
    float width = (float)(size.width * scale);
    float height = (float)(size.height * scale);
    return (Rectangle) {
        ((float)screen_width - width) * 0.5f,
        ((float)screen_height - height) * 0.5f,
        width,
        height,
    };
}

static float graph_y(Rectangle bounds, float milliseconds, float maximum)
{
    float amount = clamp01(milliseconds / maximum);
    return bounds.y + bounds.height - amount * bounds.height;
}

static void draw_profiler_graph(const Profiler *profiler, Rectangle bounds)
{
    DrawRectangleRec(bounds, (Color) { 4, 8, 14, 235 });
    DrawRectangleLinesEx(bounds, 1.0f, (Color) { 105, 125, 145, 255 });
    if (profiler->count == 0)
        return;

    float maximum = 35.0f;
    for (int i = 0; i < profiler->count; ++i)
        maximum = fmaxf(maximum, profiler->frame_ms[i] * 1.1f);

    const float budgets[] = { 1000.0f / 60.0f, 1000.0f / 30.0f };
    const Color budget_colors[] = {
        { 75, 105, 125, 210 },
        { 125, 75, 75, 210 },
    };
    for (int i = 0; i < 2; ++i) {
        float y = graph_y(bounds, budgets[i], maximum);
        DrawLine((int)bounds.x, (int)y, (int)(bounds.x + bounds.width), (int)y, budget_colors[i]);
        DrawText(TextFormat("%d", i == 0 ? 60 : 30), (int)bounds.x + 3, (int)y - 11, 10,
            budget_colors[i]);
    }

    int first = profiler->count == PROFILER_HISTORY_COUNT ? profiler->next : 0;
    Vector2 previous_frame = { 0 };
    Vector2 previous_scene = { 0 };
    Vector2 previous_present = { 0 };
    for (int i = 0; i < profiler->count; ++i) {
        int index = (first + i) % PROFILER_HISTORY_COUNT;
        float x = bounds.x + bounds.width
            - (float)(profiler->count - 1 - i) * (bounds.width - 1.0f)
                / (float)(PROFILER_HISTORY_COUNT - 1);
        Vector2 frame = { x, graph_y(bounds, profiler->frame_ms[index], maximum) };
        Vector2 scene = { x, graph_y(bounds, profiler->scene_ms[index], maximum) };
        Vector2 present = { x, graph_y(bounds, profiler->present_ms[index], maximum) };
        if (i > 0) {
            DrawLineV(previous_frame, frame, (Color) { 255, 210, 70, 255 });
            DrawLineV(previous_scene, scene, (Color) { 80, 245, 135, 255 });
            DrawLineV(previous_present, present, (Color) { 80, 190, 255, 255 });
        }
        previous_frame = frame;
        previous_scene = scene;
        previous_present = present;
    }

    DrawText(TextFormat("%.0f ms", maximum), (int)(bounds.x + bounds.width) - 47, (int)bounds.y + 3,
        10, GRAY);
}

static void draw_profiler(
    const Profiler *profiler, const ProfilerSnapshot *snapshot, VirtualSize size)
{
    const int x = 12;
    const int y = 12;
    const int width = 570;
    const int height = 318;
    DrawRectangle(x, y, width, height, (Color) { 2, 5, 10, 225 });
    DrawRectangleLines(x, y, width, height, (Color) { 150, 175, 195, 255 });
    DrawText("PROFILER [F3]", x + 10, y + 8, 18, RAYWHITE);
    DrawText(TextFormat("scene %-10s  %5.1f / %5.1f s  (%3.0f%%)   timeline %.1f s",
                 scene_name(snapshot->scene.id), snapshot->scene.time,
                 scene_duration(snapshot->scene.id), snapshot->scene.progress * 100.0f,
                 snapshot->timeline_time),
        x + 10, y + 31, 14, RAYWHITE);
    DrawText(TextFormat("frame ms    now %6.1f  avg %6.1f  p95 %6.1f  min %6.1f  max %6.1f",
                 snapshot->frame.latest, snapshot->frame.average, snapshot->frame.percentile_95,
                 snapshot->frame.minimum, snapshot->frame.maximum),
        x + 10, y + 51, 14, (Color) { 255, 210, 70, 255 });
    DrawText(TextFormat("scene ms    now %6.1f  avg %6.1f  p95 %6.1f  min %6.1f  max %6.1f",
                 snapshot->scene_cost.latest, snapshot->scene_cost.average,
                 snapshot->scene_cost.percentile_95, snapshot->scene_cost.minimum,
                 snapshot->scene_cost.maximum),
        x + 10, y + 70, 14, (Color) { 80, 245, 135, 255 });
    DrawText(
        TextFormat("present ms  now %6.1f  avg %6.1f  p95 %6.1f  min %6.1f  max %6.1f",
            snapshot->present.latest, snapshot->present.average, snapshot->present.percentile_95,
            snapshot->present.minimum, snapshot->present.maximum),
        x + 10, y + 89, 14, (Color) { 80, 190, 255, 255 });
    DrawText(TextFormat("FPS observed %5.1f  raylib %3d  monitor %d Hz  samples %d/%d",
                 snapshot->observed_fps, snapshot->raylib_fps, snapshot->monitor_refresh_rate,
                 snapshot->sample_count, PROFILER_HISTORY_COUNT),
        x + 10, y + 109, 14, RAYWHITE);
    DrawText(TextFormat("virtual %dx%d  window %dx%d  render %dx%d  integer scale %dx", size.width,
                 size.height, snapshot->window_width, snapshot->window_height,
                 snapshot->render_width, snapshot->render_height, snapshot->scale),
        x + 10, y + 128, 14, RAYWHITE);
    DrawText(TextFormat("framebuffer %d KiB  texture upload %d KiB/frame  audio %s %.1f/%.1f s",
                 snapshot->framebuffer_kib, snapshot->framebuffer_kib,
                 snapshot->music_playing ? "playing" : "off", snapshot->music_played,
                 snapshot->music_length),
        x + 10, y + 147, 14, RAYWHITE);
    DrawText("rolling 240 frames:  frame wall     scene CPU+upload     present/vsync", x + 10,
        y + 168, 13, LIGHTGRAY);
    DrawRectangle(x + 111, y + 172, 12, 2, (Color) { 255, 210, 70, 255 });
    DrawRectangle(x + 241, y + 172, 12, 2, (Color) { 80, 245, 135, 255 });
    DrawRectangle(x + 392, y + 172, 12, 2, (Color) { 80, 190, 255, 255 });
    draw_profiler_graph(profiler, (Rectangle) { x + 10, y + 187, width - 20, 120 });
}

static ProfileStats linear_profile_stats(float *samples, int count)
{
    ProfileStats stats = {
        .latest = samples[count - 1],
        .minimum = samples[0],
        .maximum = samples[0],
    };
    for (int i = 0; i < count; ++i) {
        stats.minimum = fminf(stats.minimum, samples[i]);
        stats.maximum = fmaxf(stats.maximum, samples[i]);
        stats.average += samples[i];
    }
    stats.average /= (float)count;
    qsort(samples, (size_t)count, sizeof(samples[0]), compare_floats);
    stats.percentile_95 = samples[(count - 1) * 95 / 100];
    return stats;
}

static void print_profile_stats(const char *name, ProfileStats stats)
{
    printf("%s_ms_latest=%.6f\n", name, stats.latest);
    printf("%s_ms_min=%.6f\n", name, stats.minimum);
    printf("%s_ms_average=%.6f\n", name, stats.average);
    printf("%s_ms_p95=%.6f\n", name, stats.percentile_95);
    printf("%s_ms_max=%.6f\n", name, stats.maximum);
}

static double profile_time(void)
{
    struct timespec time;
    timespec_get(&time, TIME_UTC);
    return (double)time.tv_sec + (double)time.tv_nsec / 1000000000.0;
}

static uint64_t framebuffer_checksum(const Color *pixels, size_t pixel_count)
{
    const uint8_t *bytes = (const uint8_t *)pixels;
    uint64_t checksum = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < pixel_count * sizeof(*pixels); ++i) {
        checksum ^= bytes[i];
        checksum *= UINT64_C(1099511628211);
    }
    return checksum;
}

static int run_profile(ProfileOptions options)
{
    VirtualSize size = VIRTUAL_SIZES[options.size_index];
    Texture2D framebuffer = { 0 };
    if (options.visible) {
        SetTraceLogLevel(LOG_WARNING);
        InitWindow(size.width, size.height, "Proteus profile");
        SetExitKey(KEY_NULL);
        SetTargetFPS(0);
        Image image = GenImageColor(size.width, size.height, BLACK);
        framebuffer = LoadTextureFromImage(image);
        UnloadImage(image);
        SetTextureFilter(framebuffer, TEXTURE_FILTER_POINT);
    }

    size_t pixel_count = (size_t)size.width * (size_t)size.height;
    Color *pixels = malloc(pixel_count * sizeof(*pixels));
    float *frame_samples = malloc((size_t)options.frames * sizeof(*frame_samples));
    float *generation_samples = malloc((size_t)options.frames * sizeof(*generation_samples));
    float *upload_samples = malloc((size_t)options.frames * sizeof(*upload_samples));
    float *present_samples = malloc((size_t)options.frames * sizeof(*present_samples));
    if (pixels == NULL || frame_samples == NULL || generation_samples == NULL
        || upload_samples == NULL || present_samples == NULL) {
        fprintf(stderr, "Unable to allocate profiling buffers\n");
        free(present_samples);
        free(upload_samples);
        free(generation_samples);
        free(frame_samples);
        free(pixels);
        if (options.visible) {
            UnloadTexture(framebuffer);
            CloseWindow();
        }
        return 1;
    }

    Rectangle bounds = { 0, 0, (float)size.width, (float)size.height };
    int64_t total_frames = (int64_t)options.warmup_frames + options.frames;
    for (int64_t frame_index = 0; frame_index < total_frames; ++frame_index) {
        float timeline_time = (float)frame_index / 60.0f;
        float duration = scene_duration(options.scene_id);
        float local_time = fmodf(timeline_time, duration);
        SceneFrame scene = {
            .id = options.scene_id,
            .time = local_time,
            .progress = local_time / duration,
        };
        float beat = timeline_time * 125.0f / 60.0f;
        double frame_started_at = profile_time();
        double generation_started_at = profile_time();
        draw_scene(pixels, size, scene, timeline_time, beat);
        float generation_ms = (float)((profile_time() - generation_started_at) * 1000.0);
        float upload_ms = 0.0f;
        float present_ms = 0.0f;

        if (options.visible) {
            double upload_started_at = profile_time();
            UpdateTexture(framebuffer, pixels);
            upload_ms = (float)((profile_time() - upload_started_at) * 1000.0);
            double present_started_at = profile_time();
            BeginDrawing();
            ClearBackground(BLACK);
            DrawTexturePro(framebuffer, bounds, bounds, (Vector2) { 0, 0 }, 0, WHITE);
            EndDrawing();
            present_ms = (float)((profile_time() - present_started_at) * 1000.0);
        }
        float frame_ms = (float)((profile_time() - frame_started_at) * 1000.0);

        if (frame_index >= options.warmup_frames) {
            int sample_index = (int)(frame_index - options.warmup_frames);
            frame_samples[sample_index] = frame_ms;
            generation_samples[sample_index] = generation_ms;
            upload_samples[sample_index] = upload_ms;
            present_samples[sample_index] = present_ms;
        }
    }

    uint64_t checksum = framebuffer_checksum(pixels, pixel_count);
    free(pixels);
    if (options.visible) {
        UnloadTexture(framebuffer);
        CloseWindow();
    }

    ProfileStats frame = linear_profile_stats(frame_samples, options.frames);
    ProfileStats generation = linear_profile_stats(generation_samples, options.frames);
    ProfileStats upload = linear_profile_stats(upload_samples, options.frames);
    ProfileStats present = linear_profile_stats(present_samples, options.frames);
    printf("proteus_profile_begin=1\n");
    printf("format_version=2\n");
    printf("build_type=%s\n", PROTEUS_BUILD_TYPE);
    printf("scene=%s\n", scene_name(options.scene_id));
    printf("frames=%d\n", options.frames);
    printf("warmup_frames=%d\n", options.warmup_frames);
    printf("virtual_width=%d\n", size.width);
    printf("virtual_height=%d\n", size.height);
    printf("graphics_enabled=%s\n", options.visible ? "true" : "false");
    printf("pacing=unbounded\n");
    printf("timeline_step_seconds=0.016666667\n");
    printf("noise_table_bytes=%zu\n", sizeof(noise_table));
    printf("framebuffer_bytes=%zu\n", pixel_count * sizeof(Color));
    printf("final_framebuffer_checksum=%016llx\n", (unsigned long long)checksum);
    printf("generation_scope=cpu_scene_generation\n");
    printf("upload_scope=update_texture_or_zero_when_graphics_disabled\n");
    printf("present_scope=begin_drawing_through_end_drawing_or_zero_when_graphics_disabled\n");
    printf("frame_scope=generation_and_optional_upload_and_present\n");
    print_profile_stats("generation", generation);
    print_profile_stats("upload", upload);
    print_profile_stats("present", present);
    print_profile_stats("frame", frame);
    printf("proteus_profile_end=1\n");

    free(present_samples);
    free(upload_samples);
    free(generation_samples);
    free(frame_samples);
    return 0;
}

int main(int argc, char **argv)
{
    ProfileOptions profile_options;
    if (!parse_options(argc, argv, &profile_options)) {
        print_usage(argv[0]);
        return 2;
    }
    if (profile_options.show_help) {
        print_usage(argv[0]);
        return 0;
    }

    initialize_noise_table();
    if (profile_options.enabled)
        return run_profile(profile_options);

    int size_index = 1; // 320x200: change live with keys 1, 2, and 3.
    VirtualSize size = VIRTUAL_SIZES[size_index];

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1280, 800, "Proteus");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    InitAudioDevice();

    Image image = GenImageColor(size.width, size.height, BLACK);
    Texture2D framebuffer = LoadTextureFromImage(image);
    UnloadImage(image);
    SetTextureFilter(framebuffer, TEXTURE_FILTER_POINT);
    Color *pixels = malloc((size_t)size.width * (size_t)size.height * sizeof(*pixels));
    if (pixels == NULL) {
        fprintf(stderr, "Unable to allocate the virtual framebuffer\n");
        return 1;
    }

    Music music = { 0 };
    bool music_ready = false;
    if (FileExists("assets/music/starter.xm")) {
        music = LoadMusicStream("assets/music/starter.xm");
        music_ready = IsMusicValid(music);
        if (music_ready) {
            music.looping = true;
            PlayMusicStream(music);
        }
    }
    // XM playback position advances by decoded audio buffers, so use a monotonic
    // clock for per-frame animation and align it with the start of playback.
    double animation_start_time = GetTime();
    double frame_started_at = GetTime();
    float previous_scene_ms = 0.0f;
    float previous_present_ms = 0.0f;
    bool completed_frame = false;
    bool profiler_visible = false;
    Profiler profiler = { 0 };
    ProfilerSnapshot snapshot = { 0 };
    double next_snapshot_time = 0.0;

    while (!WindowShouldClose()) {
        double current_frame_started_at = GetTime();
        if (completed_frame) {
            profiler_add(&profiler, (float)((current_frame_started_at - frame_started_at) * 1000.0),
                previous_scene_ms, previous_present_ms);
        }
        frame_started_at = current_frame_started_at;

        if (IsKeyPressed(KEY_ESCAPE))
            break;
        if (IsKeyPressed(KEY_F11))
            ToggleBorderlessWindowed();
        if (IsKeyPressed(KEY_F3)) {
            profiler_visible = !profiler_visible;
            if (profiler_visible)
                next_snapshot_time = 0.0;
        }

        int requested = size_index;
        if (IsKeyPressed(KEY_ONE))
            requested = 0;
        if (IsKeyPressed(KEY_TWO))
            requested = 1;
        if (IsKeyPressed(KEY_THREE))
            requested = 2;
        if (requested != size_index) {
            size_index = requested;
            size = VIRTUAL_SIZES[size_index];
            UnloadTexture(framebuffer);
            free(pixels);
            Image resized = GenImageColor(size.width, size.height, BLACK);
            framebuffer = LoadTextureFromImage(resized);
            UnloadImage(resized);
            SetTextureFilter(framebuffer, TEXTURE_FILTER_POINT);
            pixels = malloc((size_t)size.width * (size_t)size.height * sizeof(*pixels));
            if (pixels == NULL)
                break;
        }

        if (music_ready)
            UpdateMusicStream(music);
        float time = (float)(GetTime() - animation_start_time);
        float beat = time * 125.0f / 60.0f;
        SceneFrame scene = scene_at(time);
        double scene_started_at = GetTime();
        draw_scene(pixels, size, scene, time, beat);
        UpdateTexture(framebuffer, pixels);
        previous_scene_ms = (float)((GetTime() - scene_started_at) * 1000.0);

        int screen_width = GetScreenWidth();
        int screen_height = GetScreenHeight();
        Rectangle destination = integer_scaled_destination(screen_width, screen_height, size);
        Rectangle source = { 0, 0, (float)size.width, (float)size.height };
        if (current_frame_started_at >= next_snapshot_time) {
            snapshot
                = profiler_snapshot(&profiler, scene, time, size, destination, music_ready, music);
            next_snapshot_time = current_frame_started_at + 0.25;
        }

        double present_started_at = GetTime();
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(framebuffer, source, destination, (Vector2) { 0, 0 }, 0, WHITE);
        if (profiler_visible)
            draw_profiler(&profiler, &snapshot, size);
        DrawText(TextFormat("%dx%d  |  1/2/3 resolution  F3 profiler  F11 fullscreen  Esc quit",
                     size.width, size.height),
            12, screen_height - 28, 16, RAYWHITE);
        EndDrawing();
        previous_present_ms = (float)((GetTime() - present_started_at) * 1000.0);
        completed_frame = true;
    }

    if (music_ready)
        UnloadMusicStream(music);
    free(pixels);
    UnloadTexture(framebuffer);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
