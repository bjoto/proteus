#include "raylib.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int width;
    int height;
} VirtualSize;

static const VirtualSize VIRTUAL_SIZES[] = {
    { 320, 180 },
    { 320, 200 },
    { 320, 240 },
};

static Color palette(float value)
{
    const float tau = 6.28318530718f;
    unsigned char r = (unsigned char)(128.0f + 127.0f * sinf(tau * (value + 0.00f)));
    unsigned char g = (unsigned char)(128.0f + 127.0f * sinf(tau * (value + 0.33f)));
    unsigned char b = (unsigned char)(128.0f + 127.0f * sinf(tau * (value + 0.67f)));
    return (Color) { r, g, b, 255 };
}

static void draw_demo(
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
        float time = music_ready ? GetMusicTimePlayed(music) : (float)GetTime();
        float beat = time * 125.0f / 60.0f;
        draw_demo(framebuffer, pixels, size, time, beat);

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
