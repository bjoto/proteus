#include "boat.h"
#include "boat_audio.h"
#include "boat_gpu.h"
#include "boat_map.h"
#include "boat_replay.h"
#include "profiler.h"
#include "raymath.h"
#include "xm_audio.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void)
{
    puts(
        "Usage: boat_study [--demo] [--frames COUNT] [--screenshot FILE [--time SECONDS]]\n"
        "Left/Right or A/D: adjust wheel | Up/Down or W/S: adjust thrust (-100..100%)\n"
        "Wheel springs to centre when released; thrust holds. R: reset and centre wheel | Esc: "
        "close\n"
        "+/-: closer/farther | PageUp/PageDown: raise/lower camera | C: reset view\n"
        "--player-wake / --no-player-wake: physical player trail (default on in free boating)\n"
        "--current / --crossing-wake: opt-in authored tests in all locations; R restarts passage\n"
        "--camera-distance UNITS: 8..48 (default 20; river 32)\n"
        "--camera-height UNITS: 4..36 (default 14; river 6)\n"
        "--location basin|eriksberg|river (default basin; river extends to Alvsborgsbron)\n"
        "--waves: gentle crossing waves and a floating hull\n"
        "--course: timed S course in the basin; cross highlighted gates in order | R: retry\n"
        "G: ghost on/off | P/B: replay last/best | Space: pause replay | Enter: restart replay\n"
        "Replay: Left/Right seek 1s (Shift: 5s) | Home/End | [ / ]: speed 0.25x..4x\n"
        "Click/drag the replay timeline to seek and pause; Space resumes.\n"
        "--replay FILE: watch a saved course run (also supports --screenshot --time)\n"
        "--ghost FILE: race a compatible saved run | --save-run FILE: export each finish\n"
        "--data-dir DIR: override personal-best storage directory\n"
        "--renderer gpu|cpu (default gpu) | --gpu: select GPU | --gpu-aa: 4x supersampling\n"
        "F3: toggle debug profiler | --profiler: show it on startup\n"
        "CPU screenshots need no window; GPU screenshots require a graphics context.\n"
        "--time is screenshot-only (0..60, or 0..600 with --replay); --frames limits a smoke run.\n"
        "--demo drives the screenshot example in a window; R restarts it.");
}

static BoatWater study_water(bool waves, bool current, bool crossing, bool player, bool mapped)
{
    BoatWater water = boat_water_test(waves, current, crossing);
    water.player_wake_enabled = player;
    if (mapped) {
        water.current_center = -12;
        water.wake_offset = (Vector2) { -140, -220 };
    }
    return water;
}

static void demo_step(Boat *boat, int tick)
{
    if (boat->water_settings.player_wake_enabled) {
        float throttle = tick < 1440 ? 1 : 0;
        float wheel = tick >= 420 && tick < 1440 ? 1 : 0;
        boat_step(boat, Clamp((wheel - boat->wheel) / BOAT_STEP, -1, 1),
            Clamp((throttle - boat->throttle) / (.5f * BOAT_STEP), -1, 1));
        return;
    }
    if (boat->water_settings.wake_amplitude != 0
        || Vector2LengthSqr(boat->water_settings.current) > 0) {
        // Let the source become visible, approach its spreading wake, then recover in neutral.
        float target = tick < 720 ? 0 : tick < 1200 ? .65f : 0;
        boat_step(boat, 0, Clamp((target - boat->throttle) / (.5f * BOAT_STEP), -1, 1));
        return;
    }
    // Ease through neutral into reverse with the same held steering.
    float target_wheel = tick >= 360 ? 0.65f : 0;
    float wheel_change = fmaxf(-1, fminf(1, (target_wheel - boat->wheel) / BOAT_STEP));
    float throttle_change = tick < 240 ? 1 : tick < 480 ? 0 : tick < 960 ? -1 : 0;
    boat_step(boat, wheel_change, throttle_change);
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

static Rectangle replay_timeline(int width)
{
    return (Rectangle) { 16, 160, fminf(416, width - 32), 12 };
}

static void course_runs_hud(
    const BoatReplayRuns *runs, const Boat *boat, const Boat *ghost, int width)
{
    DrawRectangle(8, 72, width > 460 ? 440 : width - 16, runs->playing ? 120 : 84,
        (Color) { 14, 23, 32, 245 });
    if (runs->best.complete)
        DrawText(TextFormat("BEST %.2fs", runs->best.splits[BOAT_COURSE_GATES - 1]), 16, 76, 16,
            (Color) { 90, 240, 255, 255 });
    if (!runs->playing && runs->opponent.complete && boat->course.next_gate > 1) {
        int gate = boat->course.next_gate - 1;
        DrawText(
            TextFormat("Split %+.2fs", boat->course.splits[gate] - runs->opponent.splits[gate]), 16,
            96, 16, RAYWHITE);
    }
    if (runs->playing) {
        double duration = runs->playing->splits[BOAT_COURSE_GATES - 1];
        DrawText("Left/Right: seek | Shift: 5s | Home/End | [ / ]: speed", 16, 96, 12, RAYWHITE);
        DrawText(TextFormat("REPLAY %s %.2gx  %.2f / %.2fs", runs->paused ? "PAUSED" : "",
                     boat_replay_speed(runs), runs->replay_time, duration),
            16, 116, 16, (Color) { 90, 240, 255, 255 });
        DrawText("Space: pause | Enter: restart | R: race", 16, 138, 12, RAYWHITE);
        Rectangle bar = replay_timeline(width);
        DrawRectangleRec(bar, (Color) { 40, 58, 68, 255 });
        float playhead = bar.x + bar.width * (float)(runs->replay_time / duration);
        DrawRectangleRec((Rectangle) { bar.x, bar.y, playhead - bar.x, bar.height },
            (Color) { 46, 130, 146, 255 });
        for (int i = 0; i < BOAT_COURSE_GATES; ++i) {
            int x = (int)(bar.x + bar.width * (float)(runs->playing->splits[i] / duration));
            DrawLine(x, (int)bar.y - 2, x, (int)(bar.y + bar.height), LIGHTGRAY);
            DrawText(i == 0                      ? "S"
                    : i == BOAT_COURSE_GATES - 1 ? "F"
                                                 : TextFormat("%d", i),
                x - 3, (int)(bar.y + bar.height + 3), 10, LIGHTGRAY);
        }
        DrawCircle((int)playhead, (int)(bar.y + bar.height / 2), 3, (Color) { 90, 240, 255, 255 });
    } else {
        DrawText(boat->course.next_gate > 0 && boat->course.next_gate < BOAT_COURSE_GATES
                ? "G: ghost on/off | R: retry"
                : "G: ghost | P: replay last | B: replay best",
            16, 116, 12, RAYWHITE);
        if (runs->notice)
            DrawText(runs->notice, 16, 138, 12, RAYWHITE);
    }
    if (ghost && width >= 368) {
        int left = width - 128;
        int x = left + 60 + (int)fmaxf(-54, fminf(54, ghost->position.x));
        int y = 38 + (int)fmaxf(0, fminf(112, ghost->position.y + 60));
        DrawCircleLines(x, y, 4, (Color) { 90, 240, 255, 255 });
    }
}

int main(int argc, char **argv)
{
    const char *screenshot = NULL;
    const char *replay_path = NULL, *ghost_path = NULL, *data_dir = NULL, *export_path = NULL;
    float time = 0;
    BoatCamera camera_settings = BOAT_CAMERA_DEFAULT;
    bool camera_distance_set = false, camera_height_set = false;
    bool time_set = false, demo = false;
    bool eriksberg = false, course = false, waves = false;
    bool river = false, current = false, crossing_wake = false, player_wake = true;
    bool player_wake_set = false;
    bool gpu_enabled = true, gpu_aa = false;
    bool profiler_visible = false;
    int frame_limit = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        } else if (strcmp(argv[i], "--location") == 0 && i + 1 < argc) {
            const char *location = argv[++i];
            river = strcmp(location, "river") == 0;
            if (strcmp(location, "eriksberg") == 0 || river)
                eriksberg = true;
            else if (strcmp(location, "basin") == 0)
                eriksberg = false;
            else
                goto invalid;
        } else if (strcmp(argv[i], "--renderer") == 0 && i + 1 < argc) {
            const char *name = argv[++i];
            if (strcmp(name, "gpu") == 0)
                gpu_enabled = true;
            else if (strcmp(name, "cpu") == 0)
                gpu_enabled = false;
            else
                goto invalid;
        } else if (strcmp(argv[i], "--gpu") == 0) {
            gpu_enabled = true;
        } else if (strcmp(argv[i], "--gpu-aa") == 0) {
            gpu_aa = true;
        } else if (strcmp(argv[i], "--profiler") == 0) {
            profiler_visible = true;
        } else if (strcmp(argv[i], "--player-wake") == 0) {
            player_wake = true;
            player_wake_set = true;
        } else if (strcmp(argv[i], "--no-player-wake") == 0) {
            player_wake = false;
            player_wake_set = true;
        } else if (strcmp(argv[i], "--current") == 0) {
            current = true;
        } else if (strcmp(argv[i], "--crossing-wake") == 0) {
            crossing_wake = true;
        } else if (strcmp(argv[i], "--waves") == 0) {
            waves = true;
        } else if (strcmp(argv[i], "--replay") == 0 && i + 1 < argc) {
            replay_path = argv[++i];
            course = true;
        } else if (strcmp(argv[i], "--ghost") == 0 && i + 1 < argc) {
            ghost_path = argv[++i];
            course = true;
        } else if (strcmp(argv[i], "--save-run") == 0 && i + 1 < argc) {
            export_path = argv[++i];
        } else if (strcmp(argv[i], "--data-dir") == 0 && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (strcmp(argv[i], "--course") == 0) {
            course = true;
        } else if (strcmp(argv[i], "--demo") == 0) {
            demo = true;
        } else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot = argv[++i];
        } else if (strcmp(argv[i], "--time") == 0 && i + 1 < argc) {
            char *end;
            const char *value = argv[++i];
            time = strtof(value, &end);
            if (end == value || *end || !isfinite(time) || time < 0
                || time > BOAT_REPLAY_MAX_SECONDS)
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
            if (distance) {
                camera_settings.distance = parsed;
                camera_distance_set = true;
            } else {
                camera_settings.height = parsed;
                camera_height_set = true;
            }
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
    if ((time_set && !screenshot) || (screenshot && frame_limit) || (!replay_path && time > 60))
        goto invalid;
    if ((!course && (data_dir || export_path)) || (replay_path && ghost_path))
        goto invalid;
    if (course && (eriksberg || demo)) {
        fprintf(stderr, "--course requires the basin and cannot be combined with --demo.\n");
        goto invalid;
    }
    if ((current || crossing_wake) && (course || ghost_path)) {
        fprintf(stderr,
            "--current and --crossing-wake require free boating, without course/replay/ghost "
            "modes.\n");
        goto invalid;
    }
    if (player_wake_set && player_wake && (course || ghost_path)) {
        fprintf(
            stderr, "--player-wake requires free boating, without course/replay/ghost modes.\n");
        goto invalid;
    }
    if (course || ghost_path)
        player_wake = false; // Existing course/replay comparisons retain their original water.
    const BoatCamera camera_default = river ? (BoatCamera) { 32, 6 } : BOAT_CAMERA_DEFAULT;
    if (!camera_distance_set)
        camera_settings.distance = camera_default.distance;
    if (!camera_height_set)
        camera_settings.height = camera_default.height;

    SetTraceLogLevel(LOG_WARNING);
    waterfront_scene_select_river(river);
    enum { WIDTH = 320, HEIGHT = 200, HUD_HEIGHT = 200 };
    Color *pixels = malloc(WIDTH * HEIGHT * sizeof(*pixels));
    if (!pixels)
        return 1;
    Renderer3D renderer = { 0 };
    BoatGpu gpu = { 0 };
    Texture2D texture = { 0 };
    bool window_ready = false;
    BoatAudio audio = { 0 };
    Music music = { 0 };
    bool music_ready = false;
    Boat boat;
    if (course)
        boat_reset_course(&boat);
    else
        boat_reset_location(&boat, eriksberg);
    boat.water_settings = study_water(waves, current, crossing_wake, player_wake, eriksberg);
    int result = 1;
    BoatReplayRuns runs = { .ghost_visible = true, .export_path = export_path };
    if (replay_path) {
        if (!boat_replay_load(&runs.last, replay_path) || (waves && !runs.last.waves)) {
            fprintf(stderr, "Invalid or incompatible course replay: %s\n", replay_path);
            goto cleanup;
        }
        waves = runs.last.waves;
        boat.water_settings = study_water(waves, current, crossing_wake, player_wake, eriksberg);
    }
    if (course) {
        char directory[1800];
        if (data_dir || boat_replay_data_directory(directory, sizeof(directory))) {
            int n = snprintf(runs.best_path, sizeof(runs.best_path), "%s/course-%s.run",
                data_dir ? data_dir : directory, waves ? "waves" : "calm");
            if (n < 0 || (size_t)n >= sizeof(runs.best_path))
                runs.best_path[0] = 0;
        }
        if (runs.best_path[0] && FileExists(runs.best_path)
            && (!boat_replay_load(&runs.best, runs.best_path) || runs.best.waves != waves)) {
            boat_replay_destroy(&runs.best);
            runs.notice = "Saved best is incompatible or damaged; ignored";
            fprintf(stderr, "%s: %s\n", runs.notice, runs.best_path);
        }
        if (ghost_path
            && (!boat_replay_load(&runs.reference, ghost_path) || runs.reference.waves != waves)) {
            fprintf(stderr, "Invalid or incompatible course ghost: %s\n", ghost_path);
            goto cleanup;
        }
        boat_replay_runs_reset(&runs);
        if (replay_path)
            boat_replay_play(&runs, &runs.last);
    }
    if (eriksberg && !boat_map_water(boat.position, 2.5f)) {
        fprintf(stderr, "Eriksberg spawn is not navigable.\n");
        goto cleanup;
    }
    if (screenshot && runs.playing) {
        boat_replay_sample(runs.playing, time, &boat);
    } else if (screenshot) {
        for (int tick = 0; tick < (int)roundf(time / BOAT_STEP); ++tick)
            demo_step(&boat, tick);
    }
    Boat ghost;
    const Boat *ghost_view = NULL;
    float ghost_opacity = 0;
    if (course && !runs.playing && runs.opponent.complete
        && boat_replay_sample(&runs.opponent, boat.course.elapsed, &ghost)) {
        ghost_view = &ghost;
        ghost_opacity = .5f;
    }
    if (!gpu_enabled
        && !boat_draw_with_ghost(
            &renderer, pixels, WIDTH, HEIGHT, &boat, camera_settings, ghost_view, ghost_opacity))
        goto cleanup;
    Image image = { pixels, WIDTH, HEIGHT, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    if (screenshot && !gpu_enabled) {
        result = ExportImage(image, screenshot) ? 0 : 1;
        goto cleanup;
    }

    SetConfigFlags(screenshot ? FLAG_WINDOW_HIDDEN : FLAG_WINDOW_RESIZABLE);
    InitWindow(WIDTH * 3, HEIGHT * 3 + HUD_HEIGHT, "Proteus / boating sketch");
    if (!IsWindowReady())
        goto cleanup;
    window_ready = true;
    SetWindowMinSize(WIDTH, HEIGHT + HUD_HEIGHT);
    SetTargetFPS(60);
    if (gpu_enabled) {
        if (!boat_gpu_init(&gpu, gpu_aa)) {
            fprintf(stderr, "Could not initialize GPU boating resources.\n");
            goto cleanup;
        }
        texture = gpu.target.texture;
        if (screenshot) {
            boat_gpu_draw_with_ghost(&gpu, &boat, camera_settings, ghost_view, ghost_opacity);
            Image capture = LoadImageFromTexture(texture);
            // CPU camera basis is horizontally mirrored relative to native OpenGL.
            // Normalize X here (and in presentation) to preserve steering and map alignment.
            ImageFlipVertical(&capture);
            ImageFlipHorizontal(&capture);
            result = ExportImage(capture, screenshot) ? 0 : 1;
            UnloadImage(capture);
            goto cleanup;
        }
    } else {
        texture = LoadTextureFromImage(image);
        if (!IsTextureValid(texture))
            goto cleanup;
    }
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    boat_audio_init(&audio, BOAT_ENGINE_DEFAULT);
    const char *music_path = PROTEUS_ASSET_DIR "/music/a-little-boat.xm";
    if (IsAudioDeviceReady()) {
        music = LoadMusicStream(music_path);
        music_ready = IsMusicValid(music);
        if (music_ready && !xm_music_start_at(&music, music_path, 0)) {
            UnloadMusicStream(music);
            music_ready = false;
        }
        if (music_ready) {
            music.looping = true;
            SetMusicVolume(music, .35f);
            PlayMusicStream(music);
        } else {
            TraceLog(LOG_WARNING, "Boating background music could not be loaded");
        }
    }
    double accumulator = 0;
    int frames = 0, demo_tick = 0;
    bool replay_dragging = false;
    Profiler profiler = { 0 };
    ProfilerSnapshot snapshot = { 0 };
    VirtualSize size = { WIDTH, HEIGHT };
    double frame_started_at = GetTime();
    double next_snapshot_time = 0;
    float previous_scene_ms = 0, previous_present_ms = 0;
    result = 0;
    while (!WindowShouldClose() && (!frame_limit || frames < frame_limit)) {
        double current_frame_started_at = GetTime();
        if (frames > 0)
            profiler_add(&profiler, (float)((current_frame_started_at - frame_started_at) * 1000),
                previous_scene_ms, previous_present_ms);
        frame_started_at = current_frame_started_at;
        if (IsKeyPressed(KEY_F3)) {
            profiler_visible = !profiler_visible;
            next_snapshot_time = 0;
        }
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
                camera_settings = camera_default;
        }
        bool replay_adjusted = false;
        if (!IsWindowFocused())
            replay_dragging = false;
        if (course && IsWindowFocused()) {
            if (IsKeyPressed(KEY_G))
                runs.ghost_visible = !runs.ghost_visible;
            if (boat.course.next_gate == 0 || boat.course.next_gate == BOAT_COURSE_GATES
                || runs.playing) {
                const BoatReplay *selected = IsKeyPressed(KEY_P) ? &runs.last
                    : IsKeyPressed(KEY_B)                        ? &runs.best
                                                                 : NULL;
                if (selected && boat_replay_play(&runs, selected)) {
                    accumulator = 0;
                    replay_dragging = false;
                    replay_adjusted = true;
                }
            }
            if (runs.playing) {
                double duration = runs.playing->splits[BOAT_COURSE_GATES - 1];
                if (IsKeyPressed(KEY_SPACE)) {
                    if (runs.replay_time == duration)
                        boat_replay_seek(&runs, 0);
                    runs.paused = !runs.paused;
                }
                if (IsKeyPressed(KEY_ENTER)) {
                    boat_replay_seek(&runs, 0);
                    runs.paused = false;
                    replay_adjusted = true;
                }
                if (IsKeyPressed(KEY_LEFT_BRACKET))
                    boat_replay_speed_step(&runs, -1);
                if (IsKeyPressed(KEY_RIGHT_BRACKET))
                    boat_replay_speed_step(&runs, 1);
                int seek = (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT))
                    - (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT));
                bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                if (seek) {
                    boat_replay_seek(&runs, runs.replay_time + seek * (shift ? 5 : 1));
                    replay_adjusted = true;
                }
                if (IsKeyPressed(KEY_HOME) || IsKeyPressed(KEY_END)) {
                    boat_replay_seek(&runs, IsKeyPressed(KEY_END) ? duration : 0);
                    replay_adjusted = true;
                }
                Rectangle bar = replay_timeline(GetScreenWidth());
                Rectangle hit = { bar.x - 4, bar.y - 6, bar.width + 8, bar.height + 12 };
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
                    && CheckCollisionPointRec(GetMousePosition(), hit))
                    replay_dragging = true;
                if (replay_dragging) {
                    boat_replay_seek(&runs, (GetMouseX() - bar.x) / bar.width * duration);
                    runs.paused = true;
                    replay_adjusted = true;
                    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
                        replay_dragging = false;
                }
            }
        }
        if (IsKeyPressed(KEY_R)) {
            if (course)
                boat_reset_course(&boat);
            else
                boat_reset_location(&boat, eriksberg);
            boat.water_settings
                = study_water(waves, current, crossing_wake, player_wake, eriksberg);
            if (course)
                boat_replay_runs_reset(&runs);
            accumulator = 0;
            demo_tick = 0;
            replay_dragging = false;
        } else if (runs.playing) {
            accumulator = 0;
            boat_replay_advance(&runs, replay_adjusted ? 0 : GetFrameTime(), IsWindowFocused());
        } else if (IsWindowFocused()) {
            float steering = (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
                - (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A));
            float throttle_change = (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))
                - (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S));
            accumulator += fmin(GetFrameTime(), 0.1);
            while (accumulator >= BOAT_STEP) {
                if (demo)
                    demo_step(&boat, demo_tick++);
                else {
                    BoatReplaySample before = boat_replay_pose(&boat);
                    boat_step(&boat, steering, throttle_change);
                    if (course)
                        boat_replay_runs_step(&runs, before, &boat);
                }
                accumulator -= BOAT_STEP;
            }
        } else {
            accumulator = 0;
        }
        Boat playback;
        const Boat *view = &boat;
        if (runs.playing && boat_replay_sample(runs.playing, runs.replay_time, &playback))
            view = &playback;
        Vector2 flow = boat_water_current(view->water_settings, view->position);
        float ground_speed = hypotf(view->velocity.x, view->velocity.y);
        float water_speed = hypotf(view->velocity.x - flow.x, view->velocity.y - flow.y);
        BoatAudioInput audio_input = { .thrust = view->engine_response,
            .speed = water_speed,
            .planing = view->planing,
            .wave_entry = view->wave_spray };
        boat_audio_update(
            &audio, audio_input, IsWindowFocused() && (!runs.playing || !runs.paused));
        if (music_ready) {
            if (IsWindowFocused() && (!runs.playing || !runs.paused))
                ResumeMusicStream(music);
            else
                PauseMusicStream(music);
            UpdateMusicStream(music);
        }
        ghost_view = NULL;
        ghost_opacity = 0;
        if (course && !runs.playing && runs.ghost_visible && runs.opponent.complete) {
            double fade
                = 1 - fmax(0, boat.course.elapsed - runs.opponent.splits[BOAT_COURSE_GATES - 1]);
            if (fade > 0 && boat_replay_sample(&runs.opponent, boat.course.elapsed, &ghost)) {
                ghost_view = &ghost;
                ghost_opacity = .5f * (float)fade;
            }
        }
        double scene_started_at = GetTime();
        if (gpu_enabled) {
            boat_gpu_draw_with_ghost(&gpu, view, camera_settings, ghost_view, ghost_opacity);
        } else if (!boat_draw_with_ghost(&renderer, pixels, WIDTH, HEIGHT, view, camera_settings,
                       ghost_view, ghost_opacity)) {
            result = 1;
            break;
        }
        if (!gpu_enabled)
            UpdateTexture(texture, pixels);
        previous_scene_ms = (float)((GetTime() - scene_started_at) * 1000);
        int scale_x = GetScreenWidth() / WIDTH;
        int scale_y = (GetScreenHeight() - HUD_HEIGHT) / HEIGHT;
        int scale = scale_x < scale_y ? scale_x : scale_y;
        if (scale < 1)
            scale = 1;
        Rectangle destination = { (GetScreenWidth() - WIDTH * scale) / 2,
            (GetScreenHeight() - HUD_HEIGHT - HEIGHT * scale) / 2, WIDTH * scale, HEIGHT * scale };
        if (profiler_visible && current_frame_started_at >= next_snapshot_time) {
            snapshot = profiler_snapshot(&profiler, (SceneFrame) { .time = boat.time }, boat.time,
                size, destination, false, (Music) { 0 }, (AudioSpectrum) { 0 });
            snapshot.scene_label = river ? "river" : eriksberg ? "eriksberg" : "basin";
            snapshot.renderer_label = gpu_enabled ? (gpu_aa ? "GPU 4x SSAA" : "GPU") : "CPU";
            snapshot.texture_upload_kib = gpu_enabled ? 0 : snapshot.framebuffer_kib;
            next_snapshot_time = current_frame_started_at + 0.25;
        }
        double present_started_at = GetTime();
        BeginDrawing();
        ClearBackground((Color) { 14, 23, 32, 255 });
        DrawTexturePro(texture,
            (Rectangle) { 0, 0, gpu_enabled ? -WIDTH : WIDTH, gpu_enabled ? -HEIGHT : HEIGHT },
            destination, (Vector2) { 0 }, 0, WHITE);
        int hud_y = GetScreenHeight() - HUD_HEIGHT + 10;
        DrawText(river      ? "ERIKSBERG / ALVSBORGSBRON"
                : eriksberg ? "ERIKSBERG"
                            : "BOATING SKETCH",
            12, hud_y, 20, RAYWHITE);
        DrawText(TextFormat("Ground %.1f | Water %.1f", ground_speed, water_speed), 210, hud_y + 3,
            14, (Color) { 232, 180, 102, 255 });
        if (current || crossing_wake)
            DrawText(TextFormat("Test water: %s %s | R restarts source",
                         current ? "current channel" : "", crossing_wake ? "crossing wake" : ""),
                12, 12, 14, RAYWHITE);
        DrawText(TextFormat("Thrust %+.0f%%", view->throttle * 100), 12, hud_y + 27, 16, RAYWHITE);
        DrawRectangle(150, hud_y + 28, 156, 12, (Color) { 40, 58, 68, 255 });
        int thrust_width = (int)(78 * fabsf(view->throttle));
        DrawRectangle(228 + (view->throttle < 0 ? -thrust_width : 0), hud_y + 28, thrust_width, 12,
            view->throttle < 0 ? (Color) { 104, 190, 220, 255 } : (Color) { 232, 180, 102, 255 });
        DrawText(TextFormat("Engine %+.0f%%", view->engine_response * 100), 12, hud_y + 47, 16,
            (Color) { 232, 180, 102, 255 });
        draw_wheel(view->wheel, hud_y);
        const char *demo_label = player_wake ? "DEMO: loop across your wake, neutral"
            : (current || crossing_wake)     ? "DEMO: wait, wake approach, neutral"
                                             : "DEMO: forward, neutral, reverse";
        DrawText(demo ? demo_label : "Left/Right A/D: wheel | Up/Down W/S: thrust", 12, hud_y + 113,
            12, LIGHTGRAY);
        DrawText(demo ? "R: replay | Esc: close" : "Both hold | R: reset + centre | Esc: close", 12,
            hud_y + 131, 12, LIGHTGRAY);
        DrawText(TextFormat("View %.1f back / %.1f high | +/-: zoom", camera_settings.distance,
                     camera_settings.height),
            12, hud_y + 153, 12, LIGHTGRAY);
        DrawText(
            "PgUp/PgDn: height | C: reset view | F3: profiler", 12, hud_y + 171, 12, LIGHTGRAY);
        boat_course_hud(&view->course, view->position, GetScreenWidth());
        if (course)
            course_runs_hud(&runs, view, ghost_view, GetScreenWidth());
        if (!IsWindowFocused())
            DrawText("PAUSED", 12, 12, 20, RAYWHITE);
        if (eriksberg)
            DrawText("Map: OpenStreetMap contributors (ODbL)", 12, 28, 10, RAYWHITE);
        if (profiler_visible)
            profiler_draw(&profiler, &snapshot, size);
        EndDrawing();
        previous_present_ms = (float)((GetTime() - present_started_at) * 1000);
        ++frames;
    }
cleanup:
    if (music_ready)
        UnloadMusicStream(music);
    boat_audio_destroy(&audio);
    boat_replay_runs_destroy(&runs);
    if (window_ready) {
        if (gpu_enabled)
            boat_gpu_destroy(&gpu);
        else if (texture.id)
            UnloadTexture(texture);
        CloseWindow();
    }
    boat_water_destroy();
    boat_map_destroy();
    renderer3d_destroy(&renderer);
    free(pixels);
    return result;
invalid:
    usage();
    return 1;
}
