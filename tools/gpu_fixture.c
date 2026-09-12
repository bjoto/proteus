#include "external/glad.h"
#include "raylib.h"
#include "renderer_gpu.h"
#include "rlgl.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PROTEUS_ASSET_DIR
#define PROTEUS_ASSET_DIR "assets"
#endif

typedef struct {
    int width;
    int height;
} FixtureSize;

typedef struct {
    FixtureSize size;
    const char *screenshot_path;
    int frames;
    bool visible;
    bool check;
    bool invalid_shader;
    bool city;
    float time;
} FixtureOptions;

static const FixtureSize SUPPORTED_SIZES[] = {
    { 320, 180 },
    { 320, 200 },
    { 320, 240 },
};

static void print_usage(const char *program)
{
    printf("Usage: %s [--check] [--visible] [--frames N] [--screenshot FILE]\n", program);
    printf("       %s [--city --time SECONDS] [--resolution WIDTHxHEIGHT]\n", program);
    printf("Options:\n");
    printf("  --check                 verify depth, orientation, target resizing, and capture\n");
    printf("  --visible               present the virtual target in a window\n");
    printf("  --frames N              visible frames to present (default: 1)\n");
    printf("  --resolution WIDTHxHEIGHT (default: 320x200)\n");
    printf("  --screenshot FILE       export the virtual target as a PNG\n");
    printf("  --invalid-shader        verify a shader error fails without a fallback\n");
    printf("  --city                  render the static waterfront GPU scene\n");
    printf("  --time SECONDS          scene-local city time (default: 0)\n");
}

static bool parse_count(const char *text, int *value)
{
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (text[0] == '\0' || *end != '\0' || parsed < 1 || parsed > INT_MAX)
        return false;
    *value = (int)parsed;
    return true;
}

static bool parse_time(const char *text, float *value)
{
    char *end = NULL;
    float parsed = strtof(text, &end);
    if (text[0] == '\0' || *end != '\0' || parsed < 0.0f)
        return false;
    *value = parsed;
    return true;
}

static bool parse_size(const char *text, FixtureSize *size)
{
    for (size_t i = 0; i < sizeof(SUPPORTED_SIZES) / sizeof(SUPPORTED_SIZES[0]); ++i) {
        char candidate[32];
        snprintf(candidate, sizeof(candidate), "%dx%d", SUPPORTED_SIZES[i].width,
            SUPPORTED_SIZES[i].height);
        if (strcmp(candidate, text) == 0) {
            *size = SUPPORTED_SIZES[i];
            return true;
        }
    }
    return false;
}

static bool parse_options(int argc, char **argv, FixtureOptions *options)
{
    *options = (FixtureOptions) {
        .size = { 320, 200 },
        .frames = 1,
    };

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--check") == 0) {
            options->check = true;
        } else if (strcmp(argv[i], "--visible") == 0) {
            options->visible = true;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            if (!parse_count(argv[++i], &options->frames)) {
                fprintf(stderr, "Invalid frame count: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--resolution") == 0 && i + 1 < argc) {
            if (!parse_size(argv[++i], &options->size)) {
                fprintf(stderr, "Unsupported resolution: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            options->screenshot_path = argv[++i];
        } else if (strcmp(argv[i], "--invalid-shader") == 0) {
            options->invalid_shader = true;
        } else if (strcmp(argv[i], "--city") == 0) {
            options->city = true;
        } else if (strcmp(argv[i], "--time") == 0 && i + 1 < argc) {
            if (!parse_time(argv[++i], &options->time)) {
                fprintf(stderr, "Invalid scene time: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            return false;
        }
    }

    if (options->invalid_shader
        && (options->check || options->visible || options->screenshot_path || options->city)) {
        fprintf(stderr, "--invalid-shader cannot be combined with rendering options\n");
        return false;
    }
    return true;
}

static void draw_city_sky(RenderTexture2D target)
{
    BeginTextureMode(target);
    ClearBackground(BLACK);
    for (int y = 0; y < target.texture.height; ++y) {
        int band = y * 7 / target.texture.height;
        DrawRectangle(0, y, target.texture.width, 1,
            (Color) { (unsigned char)(20 + band * 5), (unsigned char)(29 + band * 5),
                (unsigned char)(52 + band * 6), 255 });
    }
    EndTextureMode();
}

static bool draw_city(RenderTexture2D target, WaterfrontGpuScene *scene, FixtureOptions options)
{
    char shader_path[1024];
    snprintf(shader_path, sizeof(shader_path), "%s/shaders/waterfront_city.fs", PROTEUS_ASSET_DIR);
    if (!waterfront_gpu_scene_init(scene, shader_path)) {
        fprintf(stderr, "GPU fixture: waterfront city resources failed: %s\n", shader_path);
        return false;
    }
    WaterfrontSceneFrame frame;
    waterfront_scene_evaluate(options.time, true, &frame);
    draw_city_sky(target);
    BeginTextureMode(target);
    waterfront_gpu_scene_draw(scene, frame, target.texture.width, target.texture.height);
    EndTextureMode();
    return true;
}

static bool color_matches(Color actual, Color expected)
{
    return actual.r == expected.r && actual.g == expected.g && actual.b == expected.b
        && actual.a == expected.a;
}

static void print_context_details(FixtureSize size)
{
    const GLubyte *vendor = glGetString(GL_VENDOR);
    const GLubyte *renderer = glGetString(GL_RENDERER);
    const GLubyte *version = glGetString(GL_VERSION);
    const GLubyte *glsl = glGetString(GL_SHADING_LANGUAGE_VERSION);
    printf("opengl_vendor=%s\n", vendor != NULL ? (const char *)vendor : "unavailable");
    printf("opengl_renderer=%s\n", renderer != NULL ? (const char *)renderer : "unavailable");
    printf("opengl_version=%s\n", version != NULL ? (const char *)version : "unavailable");
    printf("glsl_version=%s\n", glsl != NULL ? (const char *)glsl : "unavailable");
    printf("render_target=%dx%d\n", size.width, size.height);
}

static bool replace_target(RenderTexture2D *target, FixtureSize size)
{
    RenderTexture2D replacement = LoadRenderTexture(size.width, size.height);
    if (!IsRenderTextureValid(replacement)) {
        fprintf(stderr, "GPU fixture: incomplete %dx%d framebuffer\n", size.width, size.height);
        if (replacement.id != 0)
            UnloadRenderTexture(replacement);
        return false;
    }

    SetTextureFilter(replacement.texture, TEXTURE_FILTER_POINT);
    if (target->id != 0)
        UnloadRenderTexture(*target);
    *target = replacement;
    return true;
}

static void draw_fixture(RenderTexture2D target, Shader shader)
{
    Camera3D camera = {
        .position = { 0.0f, 0.0f, 8.0f },
        .target = { 0.0f, 0.0f, 0.0f },
        .up = { 0.0f, 1.0f, 0.0f },
        .fovy = 2.5f,
        .projection = CAMERA_ORTHOGRAPHIC,
    };

    BeginTextureMode(target);
    ClearBackground((Color) { 20, 28, 45, 255 });
    BeginMode3D(camera);
    BeginShaderMode(shader);

    // Draw the nearer blue cube first. The later red cube intersects it but is farther away.
    DrawCubeV((Vector3) { 0.20f, 0.0f, 0.35f }, (Vector3) { 1.5f, 1.5f, 1.5f }, BLUE);
    DrawCubeV((Vector3) { -0.20f, 0.0f, 0.0f }, (Vector3) { 1.5f, 1.5f, 1.5f }, RED);

    EndShaderMode();
    EndMode3D();

    // Four unequal corner colors make vertical flips and horizontal mirroring unambiguous.
    DrawRectangle(0, 0, 8, 8, RED);
    DrawRectangle(target.texture.width - 8, 0, 8, 8, GREEN);
    DrawRectangle(0, target.texture.height - 8, 8, 8, BLUE);
    DrawRectangle(target.texture.width - 8, target.texture.height - 8, 8, 8, YELLOW);
    EndTextureMode();
}

static bool normalize_and_check_image(Image *image, FixtureSize size, bool check)
{
    if (image->data == NULL) {
        fprintf(stderr, "GPU fixture: render-target readback failed\n");
        return false;
    }
    if (image->width != size.width || image->height != size.height) {
        fprintf(stderr, "GPU fixture: readback dimensions %dx%d, expected %dx%d\n", image->width,
            image->height, size.width, size.height);
        return false;
    }

    ImageFlipVertical(image);
    if (!check)
        return true;

    Color top_left = GetImageColor(*image, 2, 2);
    Color top_right = GetImageColor(*image, size.width - 3, 2);
    Color bottom_left = GetImageColor(*image, 2, size.height - 3);
    Color bottom_right = GetImageColor(*image, size.width - 3, size.height - 3);
    Color center = GetImageColor(*image, size.width / 2, size.height / 2);
    if (!color_matches(top_left, RED) || !color_matches(top_right, GREEN)
        || !color_matches(bottom_left, BLUE) || !color_matches(bottom_right, YELLOW)) {
        fprintf(stderr, "GPU fixture: orientation marker check failed\n");
        return false;
    }
    if (!color_matches(center, BLUE)) {
        fprintf(stderr, "GPU fixture: depth probe is %u,%u,%u,%u, expected blue\n", center.r,
            center.g, center.b, center.a);
        return false;
    }
    return true;
}

static bool verify_shader_rejection(void)
{
    static const char INVALID_FRAGMENT_SHADER[]
        = "#version 330\n"
          "out vec4 finalColor;\n"
          "void main() { finalColor = vec4(this is invalid); }\n";
    Shader shader = LoadShaderFromMemory(NULL, INVALID_FRAGMENT_SHADER);
    bool rejected = shader.id == 0 || shader.id == rlGetShaderIdDefault();
    if (shader.id != 0 && shader.id != rlGetShaderIdDefault())
        UnloadShader(shader);
    if (!rejected)
        fprintf(stderr, "GPU fixture: invalid shader unexpectedly linked\n");
    return rejected;
}

static void present_target(RenderTexture2D target, FixtureOptions options)
{
    Rectangle source = { 0, 0, (float)target.texture.width, (float)-target.texture.height };
    Rectangle destination = { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
    for (int frame = 0; frame < options.frames && !WindowShouldClose(); ++frame) {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(target.texture, source, destination, (Vector2) { 0 }, 0.0f, WHITE);
        EndDrawing();
    }
}

int main(int argc, char **argv)
{
    FixtureOptions options;
    if (!parse_options(argc, argv, &options)) {
        print_usage(argv[0]);
        return 2;
    }

    SetConfigFlags(options.visible ? 0 : FLAG_WINDOW_HIDDEN);
    InitWindow(options.size.width, options.size.height, "Proteus GPU fixture");
    if (!IsWindowReady()) {
        fprintf(stderr, "GPU fixture: unable to create a raylib window/context\n");
        return 1;
    }
    SetTraceLogLevel(LOG_WARNING);
    print_context_details(options.size);

    int result = 1;
    if (options.invalid_shader) {
        result = verify_shader_rejection() ? 1 : 2;
        CloseWindow();
        return result;
    }

    Shader shader = { 0 };
    if (!options.city) {
        char shader_path[1024];
        snprintf(shader_path, sizeof(shader_path), "%s/shaders/gpu_fixture.fs", PROTEUS_ASSET_DIR);
        shader = LoadShader(NULL, shader_path);
        if (!IsShaderValid(shader) || shader.id == rlGetShaderIdDefault()) {
            fprintf(stderr, "GPU fixture: custom fragment shader failed: %s\n", shader_path);
            if (shader.id != 0 && shader.id != rlGetShaderIdDefault())
                UnloadShader(shader);
            CloseWindow();
            return 1;
        }
    }

    WaterfrontGpuScene city = { 0 };
    RenderTexture2D target = { 0 };
    FixtureSize intermediate = { options.size.width / 2, options.size.height / 2 };
    if (!replace_target(&target, intermediate) || !replace_target(&target, options.size))
        goto cleanup;

    if (options.city) {
        if (!draw_city(target, &city, options))
            goto cleanup;
    } else {
        draw_fixture(target, shader);
    }
    Image image = LoadImageFromTexture(target.texture);
    if (!normalize_and_check_image(&image, options.size, options.check && !options.city)) {
        UnloadImage(image);
        goto cleanup;
    }
    if (options.screenshot_path != NULL && !ExportImage(image, options.screenshot_path)) {
        fprintf(stderr, "GPU fixture: unable to export screenshot: %s\n", options.screenshot_path);
        UnloadImage(image);
        goto cleanup;
    }
    UnloadImage(image);

    if (options.visible)
        present_target(target, options);
    printf("gpu_fixture=ok\n");
    if (options.screenshot_path != NULL)
        printf("screenshot=%s\n", options.screenshot_path);
    result = 0;

cleanup:
    waterfront_gpu_scene_destroy(&city);
    if (target.id != 0)
        UnloadRenderTexture(target);
    if (shader.id != 0)
        UnloadShader(shader);
    CloseWindow();
    return result;
}
