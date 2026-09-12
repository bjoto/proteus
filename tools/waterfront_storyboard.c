#include "waterfront.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    puts("Usage: waterfront_storyboard [--route long|short] [--time SECONDS]\n"
         "       [--screenshot FILE] [--resolution 320x180|320x200|320x240] [--visible]\n"
         "       waterfront_storyboard --check\n"
         "Long route: 30.72 seconds. Short route: 15.36 seconds. Time clamps at the endpoint.\n"
         "With no screenshot or --check, plays one windowed pass at 60 FPS.\n"
         "Map data (c) OpenStreetMap contributors, ODbL 1.0.\n"
         "https://www.openstreetmap.org/copyright");
}

static bool verify(void)
{
    if (!waterfront_check_routes())
        return false;
    Color pixels[320 * 240], reference[320 * 240];
    Renderer3D renderer = { 0 };
    bool ok = true;
    for (int route = 0; route < 2 && ok; ++route) {
        for (int height = 180; height <= 240 && ok; height += height == 180 ? 20 : 40) {
            float time = waterfront_duration(route != 0) * .6f;
            ok = waterfront_draw(&renderer, reference, 320, height, time, route != 0)
                && waterfront_draw(&renderer, pixels, 320, height, 0, route != 0)
                && waterfront_draw(&renderer, pixels, 320, height, time, route != 0)
                && memcmp(reference, pixels, (size_t)320 * height * sizeof(Color)) == 0;
        }
    }
    renderer3d_destroy(&renderer);
    if (ok)
        puts("Direct seeks match repeated renders at every resolution.");
    return ok;
}

int main(int argc, char **argv)
{
    bool short_route = false, visible = false, check = false;
    const char *screenshot = NULL;
    float time = 0;
    int width = 320, height = 200;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        } else if (strcmp(argv[i], "--route") == 0 && i + 1 < argc) {
            const char *route = argv[++i];
            if (strcmp(route, "short") != 0 && strcmp(route, "long") != 0)
                goto invalid;
            short_route = strcmp(route, "short") == 0;
        } else if (strcmp(argv[i], "--time") == 0 && i + 1 < argc) {
            char *end;
            const char *value = argv[++i];
            time = strtof(value, &end);
            if (value == end || *end || !isfinite(time) || time < 0)
                goto invalid;
        } else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot = argv[++i];
        } else if (strcmp(argv[i], "--resolution") == 0 && i + 1 < argc) {
            const char *value = argv[++i];
            if (strcmp(value, "320x180") == 0)
                height = 180;
            else if (strcmp(value, "320x200") == 0)
                height = 200;
            else if (strcmp(value, "320x240") == 0)
                height = 240;
            else
                goto invalid;
        } else if (strcmp(argv[i], "--visible") == 0) {
            visible = true;
        } else if (strcmp(argv[i], "--check") == 0) {
            check = true;
        } else {
            goto invalid;
        }
    }
    if (check)
        return verify() ? 0 : 1;
    SetTraceLogLevel(LOG_WARNING);
    Color *pixels = malloc((size_t)width * height * sizeof(Color));
    if (!pixels)
        return 1;
    Renderer3D renderer = { 0 };
    float duration = waterfront_duration(short_route);
    if (time > duration)
        time = duration;
    if (!waterfront_draw(&renderer, pixels, width, height, time, short_route)) {
        free(pixels);
        renderer3d_destroy(&renderer);
        return 1;
    }
    Image image = { pixels, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    int result = 0;
    if (screenshot) {
        result = ExportImage(image, screenshot) ? 0 : 1;
        printf("route=%s\ntime=%.3f\nduration=%.2f\nscreenshot=%s\n",
            short_route ? "short" : "long", time, duration, screenshot);
    }
    if (!screenshot || visible) {
        InitWindow(width * 3, height * 3 + 52, "Eriksberg / Monsungatan storyboard");
        SetTargetFPS(60);
        Texture2D texture = LoadTextureFromImage(image);
        SetTextureFilter(texture, TEXTURE_FILTER_POINT);
        int frames = (int)ceilf((duration - time) * 60) + 1;
        for (int frame = 0; frame < frames && !WindowShouldClose(); ++frame) {
            float local_time = fminf(duration, time + (float)frame / 60);
            if (!waterfront_draw(&renderer, pixels, width, height, local_time, short_route)) {
                result = 1;
                break;
            }
            UpdateTexture(texture, pixels);
            BeginDrawing();
            ClearBackground(BLACK);
            DrawTextureEx(texture, (Vector2) { 0, 0 }, 0, 3, WHITE);
            DrawText(TextFormat("MONSUNGATAN  /  %s  /  %.1f / %.2f s  /  Esc closes",
                         short_route ? "SHORT" : "LONG", local_time, duration),
                10, height * 3 + 7, 14, LIGHTGRAY);
            DrawText("Map data (c) OpenStreetMap contributors | openstreetmap.org/copyright", 10,
                height * 3 + 29, 12, GRAY);
            EndDrawing();
        }
        UnloadTexture(texture);
        CloseWindow();
    }
    renderer3d_destroy(&renderer);
    free(pixels);
    return result;
invalid:
    usage();
    return 2;
}
