#include "raylib.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int width;
    int height;
} VirtualSize;

typedef enum {
    SCENE_STARSCROLL,
    SCENE_PLASMA,
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

static const VirtualSize VIRTUAL_SIZES[] = {
    { 320, 180 },
    { 320, 200 },
    { 320, 240 },
};

// At 125 BPM with six ticks per row, each 64-row XM pattern lasts 7.68 seconds.
static const float PATTERN_DURATION = 7.68f;
static const Scene SCENES[] = {
    { SCENE_STARSCROLL, PATTERN_DURATION },
    { SCENE_PLASMA, PATTERN_DURATION * 7.0f },
};

static float smoothstep(float value) { return value * value * (3.0f - 2.0f * value); }

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

static void draw_starscroll(
    Texture2D framebuffer, Color *pixels, VirtualSize size, float time, float progress)
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
    UpdateTexture(framebuffer, pixels);
}

static Color palette(float value)
{
    const float tau = 6.28318530718f;
    unsigned char r = (unsigned char)(128.0f + 127.0f * sinf(tau * (value + 0.00f)));
    unsigned char g = (unsigned char)(128.0f + 127.0f * sinf(tau * (value + 0.33f)));
    unsigned char b = (unsigned char)(128.0f + 127.0f * sinf(tau * (value + 0.67f)));
    return (Color) { r, g, b, 255 };
}

static void draw_plasma(
    Texture2D framebuffer, Color *pixels, VirtualSize size, float time, float beat)
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
    UpdateTexture(framebuffer, pixels);
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

static void draw_scene(Texture2D framebuffer, Color *pixels, VirtualSize size, SceneFrame scene,
    float timeline_time, float beat)
{
    switch (scene.id) {
    case SCENE_STARSCROLL:
        draw_starscroll(framebuffer, pixels, size, scene.time, scene.progress);
        break;
    case SCENE_PLASMA:
        draw_plasma(framebuffer, pixels, size, timeline_time, beat);
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

int main(void)
{
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

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE))
            break;
        if (IsKeyPressed(KEY_F11))
            ToggleBorderlessWindowed();

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
        draw_scene(framebuffer, pixels, size, scene_at(time), time, beat);

        int screen_width = GetScreenWidth();
        int screen_height = GetScreenHeight();
        Rectangle destination = integer_scaled_destination(screen_width, screen_height, size);
        Rectangle source = { 0, 0, (float)size.width, (float)size.height };

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(framebuffer, source, destination, (Vector2) { 0, 0 }, 0, WHITE);
        DrawText(TextFormat("%dx%d  |  1/2/3 resolution  F11 fullscreen  Esc quit", size.width,
                     size.height),
            12, 12, 16, RAYWHITE);
        EndDrawing();
    }

    if (music_ready)
        UnloadMusicStream(music);
    free(pixels);
    UnloadTexture(framebuffer);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
