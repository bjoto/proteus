#include "boat.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    puts("Usage: boat_study [--demo] [--frames COUNT] [--screenshot FILE [--time SECONDS]]\n"
         "Left/Right or A/D: adjust wheel | Up/Down or W/S: adjust throttle (0..100%)\n"
         "Wheel and throttle hold when released. R: reset and centre wheel | Esc: close\n"
         "+/-: closer/farther | PageUp/PageDown: raise/lower camera | C: reset view\n"
         "--camera-distance UNITS: 8..48 (default 20)\n"
         "--camera-height UNITS: 4..36 (default 14)\n"
         "Screenshots run a deterministic handling example without a window.\n"
         "--time is screenshot-only (0..60); --frames limits an interactive smoke run.\n"
         "--demo drives the screenshot example in a window; R restarts it.");
}

static void demo_step(Boat *boat, int tick)
{
    // Keep the wheel turned after lowering throttle to demonstrate passive coasting steering.
    float target_wheel = tick >= 120 ? 0.65f : 0;
    float wheel_change = fmaxf(-1, fminf(1, (target_wheel - boat->wheel) / BOAT_STEP));
    boat_step(boat, wheel_change, tick < 240 ? 1 : tick >= 720 ? -1 : 0);
}

static void draw_wheel(float setting, int hud_y)
{
    Vector2 centre = { 44, hud_y + 78 };
    Color accent = { 232, 180, 102, 255 };
    DrawCircleLines((int)centre.x, (int)centre.y, 24, LIGHTGRAY);
    DrawCircleLines((int)centre.x, (int)centre.y, 21, LIGHTGRAY);
    // A coloured top spoke and fixed centre mark make the held rotation readable.
    DrawLine(44, hud_y + 46, 44, hud_y + 50, accent);
    for (int spoke = 0; spoke < 3; ++spoke) {
        float angle = setting * 0.75f * PI + spoke * 2 * PI / 3 - PI / 2;
        Vector2 end = { centre.x + cosf(angle) * 22, centre.y + sinf(angle) * 22 };
        DrawLineEx(centre, end, 2, spoke == 0 ? accent : LIGHTGRAY);
    }
    DrawCircleV(centre, 4, accent);
    const char *label = fabsf(setting) < 0.005f
        ? "Wheel centred"
        : TextFormat("Wheel %s %2.0f%%", setting < 0 ? "L" : "R", fabsf(setting) * 100);
    DrawText(label, 84, hud_y + 61, 16, RAYWHITE);
    DrawText("Holds when released", 84, hud_y + 83, 12, LIGHTGRAY);
}

int main(int argc, char **argv)
{
    const char *screenshot = NULL;
    float time = 0;
    BoatCamera camera_settings = BOAT_CAMERA_DEFAULT;
    bool time_set = false, demo = false;
    int frame_limit = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        } else if (strcmp(argv[i], "--demo") == 0) {
            demo = true;
        } else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot = argv[++i];
        } else if (strcmp(argv[i], "--time") == 0 && i + 1 < argc) {
            char *end;
            const char *value = argv[++i];
            time = strtof(value, &end);
            if (end == value || *end || !isfinite(time) || time < 0 || time > 60)
                goto invalid;
            time_set = true;
        } else if ((strcmp(argv[i], "--camera-distance") == 0
                       || strcmp(argv[i], "--camera-height") == 0)
            && i + 1 < argc) {
            bool distance = strcmp(argv[i], "--camera-distance") == 0;
            char *end;
            const char *value = argv[++i];
            float parsed = strtof(value, &end);
            if (end == value || *end || !isfinite(parsed) || parsed < (distance ? 8 : 4)
                || parsed > (distance ? 48 : 36))
                goto invalid;
            if (distance)
                camera_settings.distance = parsed;
            else
                camera_settings.height = parsed;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            char *end;
            const char *value = argv[++i];
            long count = strtol(value, &end, 10);
            if (end == value || *end || count <= 0 || count > 10000000)
                goto invalid;
            frame_limit = (int)count;
        } else {
            goto invalid;
        }
    }
    if ((time_set && !screenshot) || (screenshot && frame_limit))
        goto invalid;

    SetTraceLogLevel(LOG_WARNING);
    enum { WIDTH = 320, HEIGHT = 200, HUD_HEIGHT = 200 };
    Color *pixels = malloc(WIDTH * HEIGHT * sizeof(*pixels));
    if (!pixels)
        return 1;
    Renderer3D renderer = { 0 };
    Boat boat;
    boat_reset(&boat);
    int result = 1;
    if (screenshot) {
        for (int tick = 0; tick < (int)roundf(time / BOAT_STEP); ++tick)
            demo_step(&boat, tick);
    }
    if (!boat_draw(&renderer, pixels, WIDTH, HEIGHT, &boat, camera_settings))
        goto cleanup;
    Image image = { pixels, WIDTH, HEIGHT, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    if (screenshot) {
        result = ExportImage(image, screenshot) ? 0 : 1;
        goto cleanup;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WIDTH * 3, HEIGHT * 3 + HUD_HEIGHT, "Proteus / boating sketch");
    if (!IsWindowReady())
        goto cleanup;
    SetWindowMinSize(WIDTH, HEIGHT + HUD_HEIGHT);
    SetTargetFPS(60);
    Texture2D texture = LoadTextureFromImage(image);
    if (!IsTextureValid(texture)) {
        CloseWindow();
        goto cleanup;
    }
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    double accumulator = 0;
    int frames = 0, demo_tick = 0;
    result = 0;
    while (!WindowShouldClose() && (!frame_limit || frames < frame_limit)) {
        if (IsWindowFocused()) {
            float dt = fminf(GetFrameTime(), 0.1f);
            int zoom = (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_KP_SUBTRACT))
                - (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_KP_ADD));
            int elevation = IsKeyDown(KEY_PAGE_UP) - IsKeyDown(KEY_PAGE_DOWN);
            camera_settings.distance
                = fmaxf(8, fminf(48, camera_settings.distance + zoom * 12 * dt));
            camera_settings.height
                = fmaxf(4, fminf(36, camera_settings.height + elevation * 8 * dt));
            if (IsKeyPressed(KEY_C))
                camera_settings = BOAT_CAMERA_DEFAULT;
        }
        if (IsKeyPressed(KEY_R)) {
            boat_reset(&boat);
            accumulator = 0;
            demo_tick = 0;
        } else if (IsWindowFocused()) {
            float steering = (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
                - (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A));
            float throttle_change = (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))
                - (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S));
            accumulator += fmin(GetFrameTime(), 0.1);
            while (accumulator >= BOAT_STEP) {
                if (demo)
                    demo_step(&boat, demo_tick++);
                else
                    boat_step(&boat, steering, throttle_change);
                accumulator -= BOAT_STEP;
            }
        } else {
            accumulator = 0;
        }
        if (!boat_draw(&renderer, pixels, WIDTH, HEIGHT, &boat, camera_settings)) {
            result = 1;
            break;
        }
        UpdateTexture(texture, pixels);
        int scale_x = GetScreenWidth() / WIDTH;
        int scale_y = (GetScreenHeight() - HUD_HEIGHT) / HEIGHT;
        int scale = scale_x < scale_y ? scale_x : scale_y;
        if (scale < 1)
            scale = 1;
        Rectangle destination = { (GetScreenWidth() - WIDTH * scale) / 2,
            (GetScreenHeight() - HUD_HEIGHT - HEIGHT * scale) / 2, WIDTH * scale, HEIGHT * scale };
        BeginDrawing();
        ClearBackground((Color) { 14, 23, 32, 255 });
        DrawTexturePro(
            texture, (Rectangle) { 0, 0, WIDTH, HEIGHT }, destination, (Vector2) { 0 }, 0, WHITE);
        int hud_y = GetScreenHeight() - HUD_HEIGHT + 10;
        DrawText("BOATING SKETCH", 12, hud_y, 20, RAYWHITE);
        DrawText(TextFormat("Speed %.1f", hypotf(boat.velocity.x, boat.velocity.y)), 210, hud_y + 3,
            16, (Color) { 232, 180, 102, 255 });
        DrawText(TextFormat("Throttle %3.0f%%", boat.throttle * 100), 12, hud_y + 27, 16, RAYWHITE);
        DrawRectangle(150, hud_y + 28, 156, 12, (Color) { 40, 58, 68, 255 });
        DrawRectangle(
            150, hud_y + 28, (int)(156 * boat.throttle), 12, (Color) { 232, 180, 102, 255 });
        draw_wheel(boat.wheel, hud_y);
        DrawText(demo ? "DEMO: throttle up, turn, throttle down"
                      : "Left/Right A/D: wheel | Up/Down W/S: throttle",
            12, hud_y + 113, 12, LIGHTGRAY);
        DrawText(demo ? "R: replay | Esc: close" : "Both hold | R: reset + centre | Esc: close", 12,
            hud_y + 131, 12, LIGHTGRAY);
        DrawText(TextFormat("View %.1f back / %.1f high | +/-: zoom", camera_settings.distance,
                     camera_settings.height),
            12, hud_y + 153, 12, LIGHTGRAY);
        DrawText("PgUp/PgDn: height | C: reset view", 12, hud_y + 171, 12, LIGHTGRAY);
        if (!IsWindowFocused())
            DrawText("PAUSED", 12, 12, 20, RAYWHITE);
        EndDrawing();
        ++frames;
    }
    UnloadTexture(texture);
    CloseWindow();
cleanup:
    renderer3d_destroy(&renderer);
    free(pixels);
    return result;
invalid:
    usage();
    return 1;
}
