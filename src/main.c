#include "audio_spectrum.h"
#include "profiler.h"
#include "raylib.h"
#include "scenes.h"
#include "xm_audio.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PROTEUS_BUILD_TYPE
#define PROTEUS_BUILD_TYPE "unknown"
#endif

typedef struct {
    bool profile_enabled;
    bool visible;
    bool show_help;
    bool scene_selected;
    SceneId scene_id;
    int frames;
    int warmup_frames;
    int size_index;
    const char *screenshot_path;
    float start_time;
} ProgramOptions;

typedef struct {
    VirtualSize size;
    Texture2D texture;
    Color *pixels;
} VirtualFramebuffer;

static const VirtualSize VIRTUAL_SIZES[] = {
    { 320, 180 },
    { 320, 200 },
    { 320, 240 },
};

static bool virtual_framebuffer_create(VirtualFramebuffer *framebuffer, VirtualSize size)
{
    Color *pixels = malloc((size_t)size.width * (size_t)size.height * sizeof(*pixels));
    if (pixels == NULL)
        return false;

    Image image = GenImageColor(size.width, size.height, BLACK);
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    *framebuffer = (VirtualFramebuffer) {
        .size = size,
        .texture = texture,
        .pixels = pixels,
    };
    return true;
}

static void virtual_framebuffer_destroy(VirtualFramebuffer *framebuffer)
{
    free(framebuffer->pixels);
    UnloadTexture(framebuffer->texture);
    *framebuffer = (VirtualFramebuffer) { 0 };
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

static bool parse_time(const char *text, float *value)
{
    char *end = NULL;
    float parsed = strtof(text, &end);
    if (text[0] == '\0' || *end != '\0' || !isfinite(parsed) || parsed < 0.0f)
        return false;
    *value = parsed;
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
    printf("Usage: %s [--time SECONDS] [--scene SCENE] [--resolution WIDTHxHEIGHT]\n", program);
    printf("       %s [--profile SCENE [options]]\n", program);
    printf("       %s --screenshot FILE [--time SECONDS] [--scene SCENE] [options]\n", program);
    printf("Scenes: starscroll, fire, plasma, cube\n");
    printf("Profile options:\n");
    printf("  --frames N              measured frames (default: 600)\n");
    printf("  --warmup N              unmeasured warm-up frames (default: 60)\n");
    printf("  --resolution WIDTHxHEIGHT (default: 320x200)\n");
    printf("  --visible               include upload/presentation using a visible window\n");
    printf("Timeline options:\n");
    printf("  --time SECONDS          start at a global or scene-local time (default: 0)\n");
    printf("  --scene SCENE           select a scene and interpret --time relative to it\n");
    printf("  --resolution WIDTHxHEIGHT (default: 320x200)\n");
}

static bool parse_options(int argc, char **argv, ProgramOptions *options)
{
    *options = (ProgramOptions) {
        .scene_id = SCENE_STARSCROLL,
        .frames = 600,
        .warmup_frames = 60,
        .size_index = 1,
    };
    bool profile_only_option = false;
    bool timeline_option = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            options->show_help = true;
        } else if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc) {
            options->profile_enabled = true;
            if (!scene_parse(argv[++i], &options->scene_id)) {
                fprintf(stderr, "Unknown scene: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            options->screenshot_path = argv[++i];
        } else if (strcmp(argv[i], "--time") == 0 && i + 1 < argc) {
            timeline_option = true;
            if (!parse_time(argv[++i], &options->start_time)) {
                fprintf(stderr, "Invalid start time: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            timeline_option = true;
            options->scene_selected = true;
            if (!scene_parse(argv[++i], &options->scene_id)) {
                fprintf(stderr, "Unknown scene: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            profile_only_option = true;
            if (!parse_count(argv[++i], 1, &options->frames)) {
                fprintf(stderr, "Invalid frame count: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            profile_only_option = true;
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
            profile_only_option = true;
            options->visible = true;
        } else {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            return false;
        }
    }
    if (options->profile_enabled && options->screenshot_path != NULL) {
        fprintf(stderr, "Choose either --profile or --screenshot\n");
        return false;
    }
    if (profile_only_option && !options->profile_enabled) {
        fprintf(stderr, "--frames, --warmup, and --visible require --profile SCENE\n");
        return false;
    }
    if (timeline_option && options->profile_enabled) {
        fprintf(stderr, "--time and --scene cannot be used with --profile SCENE\n");
        return false;
    }
    return true;
}

static float requested_timeline_time(ProgramOptions options)
{
    if (!options.scene_selected)
        return options.start_time;

    float local_time = fmodf(options.start_time, scene_duration(options.scene_id));
    return scene_start_time(options.scene_id) + local_time;
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

static int run_screenshot(ProgramOptions options)
{
    VirtualSize size = VIRTUAL_SIZES[options.size_index];
    size_t pixel_count = (size_t)size.width * (size_t)size.height;
    Color *pixels = malloc(pixel_count * sizeof(*pixels));
    if (pixels == NULL) {
        fprintf(stderr, "Unable to allocate the screenshot framebuffer\n");
        return 1;
    }

    float timeline_time = requested_timeline_time(options);
    SceneFrame scene = scene_at(timeline_time);
    float beat = timeline_time * 125.0f / 60.0f;
    scene_draw(pixels, size, scene, timeline_time, beat);

    Image image = {
        .data = pixels,
        .width = size.width,
        .height = size.height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    SetTraceLogLevel(LOG_WARNING);
    bool exported = ExportImage(image, options.screenshot_path);
    free(pixels);
    if (!exported) {
        fprintf(stderr, "Unable to export screenshot: %s\n", options.screenshot_path);
        return 1;
    }

    printf("screenshot=%s\n", options.screenshot_path);
    printf("scene=%s\n", scene_name(scene.id));
    printf("scene_time=%.3f\n", scene.time);
    printf("timeline_time=%.3f\n", timeline_time);
    printf("resolution=%dx%d\n", size.width, size.height);
    return 0;
}

static int run_profile(ProgramOptions options)
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
        scene_draw(pixels, size, scene, timeline_time, beat);
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

    ProfileStats frame = profile_stats_linear(frame_samples, options.frames);
    ProfileStats generation = profile_stats_linear(generation_samples, options.frames);
    ProfileStats upload = profile_stats_linear(upload_samples, options.frames);
    ProfileStats present = profile_stats_linear(present_samples, options.frames);
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
    printf("noise_table_bytes=%zu\n", scenes_noise_table_bytes());
    printf("framebuffer_bytes=%zu\n", pixel_count * sizeof(Color));
    printf("final_framebuffer_checksum=%016llx\n", (unsigned long long)checksum);
    printf("generation_scope=cpu_scene_generation\n");
    printf("upload_scope=update_texture_or_zero_when_graphics_disabled\n");
    printf("present_scope=begin_drawing_through_end_drawing_or_zero_when_graphics_disabled\n");
    printf("frame_scope=generation_and_optional_upload_and_present\n");
    profile_stats_print("generation", generation);
    profile_stats_print("upload", upload);
    profile_stats_print("present", present);
    profile_stats_print("frame", frame);
    printf("proteus_profile_end=1\n");

    free(present_samples);
    free(upload_samples);
    free(generation_samples);
    free(frame_samples);
    return 0;
}

int main(int argc, char **argv)
{
    ProgramOptions options;
    if (!parse_options(argc, argv, &options)) {
        print_usage(argv[0]);
        return 2;
    }
    if (options.show_help) {
        print_usage(argv[0]);
        return 0;
    }

    scenes_initialize();
    atexit(scenes_shutdown);
    if (options.profile_enabled)
        return run_profile(options);
    if (options.screenshot_path != NULL)
        return run_screenshot(options);

    int size_index = options.size_index;
    VirtualSize size = VIRTUAL_SIZES[size_index];
    float timeline_start = requested_timeline_time(options);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1280, 800, "Proteus");
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    InitAudioDevice();

    VirtualFramebuffer framebuffer;
    if (!virtual_framebuffer_create(&framebuffer, size)) {
        fprintf(stderr, "Unable to allocate the virtual framebuffer\n");
        CloseAudioDevice();
        CloseWindow();
        return 1;
    }

    Music music = { 0 };
    bool music_ready = false;
    bool spectrum_attached = false;
    if (FileExists("assets/music/starter.xm")) {
        music = LoadMusicStream("assets/music/starter.xm");
        music_ready = IsMusicValid(music);
        if (music_ready) {
            music.looping = true;
            float music_start = fmodf(timeline_start, scene_timeline_duration());
            if (!xm_music_start_at(music, music_start))
                TraceLog(LOG_WARNING, "Unable to seek XM music to %.3f seconds", music_start);
            spectrum_attached = audio_spectrum_attach(music.stream);
            if (!spectrum_attached)
                TraceLog(LOG_WARNING, "Unable to attach the audio spectrum analyzer");
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
    AudioSpectrum spectrum = { 0 };
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
            VirtualFramebuffer resized;
            if (!virtual_framebuffer_create(&resized, VIRTUAL_SIZES[requested]))
                break;
            virtual_framebuffer_destroy(&framebuffer);
            framebuffer = resized;
            size_index = requested;
            size = framebuffer.size;
        }

        if (music_ready)
            UpdateMusicStream(music);
        if (spectrum_attached)
            audio_spectrum_update(&spectrum, GetFrameTime());
        float time = timeline_start + (float)(GetTime() - animation_start_time);
        float beat = time * 125.0f / 60.0f;
        SceneFrame scene = scene_at(time);
        double scene_started_at = GetTime();
        scene_draw(framebuffer.pixels, size, scene, time, beat);
        UpdateTexture(framebuffer.texture, framebuffer.pixels);
        previous_scene_ms = (float)((GetTime() - scene_started_at) * 1000.0);

        int screen_width = GetScreenWidth();
        int screen_height = GetScreenHeight();
        Rectangle destination = integer_scaled_destination(screen_width, screen_height, size);
        Rectangle source = { 0, 0, (float)size.width, (float)size.height };
        if (current_frame_started_at >= next_snapshot_time) {
            snapshot = profiler_snapshot(
                &profiler, scene, time, size, destination, music_ready, music, spectrum);
            next_snapshot_time = current_frame_started_at + 0.25;
        }

        double present_started_at = GetTime();
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(framebuffer.texture, source, destination, (Vector2) { 0, 0 }, 0, WHITE);
        if (profiler_visible)
            profiler_draw(&profiler, &snapshot, size);
        DrawText(TextFormat("%dx%d  |  1/2/3 resolution  F3 profiler  F11 fullscreen  Esc quit",
                     size.width, size.height),
            12, screen_height - 28, 16, RAYWHITE);
        EndDrawing();
        previous_present_ms = (float)((GetTime() - present_started_at) * 1000.0);
        completed_frame = true;
    }

    if (spectrum_attached)
        audio_spectrum_detach(music.stream);
    if (music_ready)
        UnloadMusicStream(music);
    virtual_framebuffer_destroy(&framebuffer);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
