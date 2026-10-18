#include "boat.h"
#include "raymath.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test);                             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int current_checks(void)
{
    BoatWater channel = boat_water_test(false, true, false);
    CHECK(Vector2Length(boat_water_current(channel, (Vector2) { 0 })) > .8f);
    CHECK(Vector2Length(boat_water_current(channel, (Vector2) { 33, 0 })) == 0);
    CHECK(Vector2Length(boat_water_current(channel, (Vector2) { 31.999f, 0 })) < 1e-6f);
    for (float x = -33; x < 33; x += .1f) {
        Vector2 a = boat_water_current(channel, (Vector2) { x, 0 });
        Vector2 b = boat_water_current(channel, (Vector2) { x + .001f, 0 });
        CHECK(Vector2Distance(a, b) < .0001f);
    }
    // Galilean equivalence in a uniform flow, including steering and the local stern station.
    Boat still, moving;
    boat_reset(&still);
    boat_reset(&moving);
    Vector2 flow = { .7f, -.3f };
    moving.water_settings.current = flow;
    moving.velocity = flow;
    still.throttle = moving.throttle = .45f;
    for (int tick = 0; tick < 240; tick++) {
        boat_step(&still, .2f, 0);
        boat_step(&moving, .2f, 0);
        CHECK(Vector2Distance(still.velocity, Vector2Subtract(moving.velocity, flow)) < .00003f);
        CHECK(fabsf(still.heading - moving.heading) < .00001f);
        CHECK(fabsf(still.height - moving.height) < .00001f);
        CHECK(fabsf(still.planing - moving.planing) < .00001f);
    }
    Vector2 offset = Vector2Scale(flow, 240 * BOAT_STEP);
    CHECK(Vector2Distance(moving.position, Vector2Add(still.position, offset)) < .0001f);
    Boat drift;
    boat_reset(&drift);
    drift.water_settings = channel;
    for (int tick = 0; tick < 1200; tick++)
        boat_step(&drift, 0, 0);
    CHECK(drift.position.x > 3 && drift.velocity.x > .4f && drift.planing == 0);
    // Authored flow remains contained by the existing basin shore contacts.
    drift.position = (Vector2) { 117, 0 };
    drift.water_settings.current_width = 0;
    for (int tick = 0; tick < 3600; tick++) {
        boat_step(&drift, 0, 0);
        CHECK(drift.position.x < BOAT_BASIN_HALF_X && fabsf(drift.position.y) < BOAT_BASIN_HALF_Z);
        CHECK(isfinite(drift.height));
    }
    return 0;
}

static int wake_checks(void)
{
    BoatWater wake = boat_water_test(false, false, true);
    float peak = 0;
    for (float t = 1; t < 25; t += .37f)
        for (float x = -28; x < 28; x += 3.3f)
            for (float z = -36; z < 36; z += 2.7f) {
                Vector2 p = { x, z };
                BoatWaterSample q = boat_water_query(wake, p, t);
                CHECK(fabsf(q.height) <= wake.wake_amplitude && q.normal.y > .5f);
                peak = fmaxf(peak, fabsf(q.height));
                float dt = .001f;
                float dy = (boat_water_query(wake, p, t + dt).height
                               - boat_water_query(wake, p, t - dt).height)
                    / (2 * dt);
                float dx = (boat_water_query(wake, (Vector2) { x + dt, z }, t).height
                               - boat_water_query(wake, (Vector2) { x - dt, z }, t).height)
                    / (2 * dt);
                float dz = (boat_water_query(wake, (Vector2) { x, z + dt }, t).height
                               - boat_water_query(wake, (Vector2) { x, z - dt }, t).height)
                    / (2 * dt);
                CHECK(fabsf(dy - q.velocity.y) < .002f);
                CHECK(fabsf(dx + q.normal.x / q.normal.y) < .002f);
                CHECK(fabsf(dz + q.normal.z / q.normal.y) < .002f);
            }
    CHECK(peak > .2f);
    BoatWater combined = boat_water_test(true, true, true);
    for (float x = -20; x < 20; x += 3) {
        Vector2 p = { x, 2 }, flow = boat_water_current(combined, p);
        BoatWaterSample q = boat_water_query(combined, p, 10);
        float dt = .001f;
        float derivative
            = (boat_water_query(combined, Vector2Add(p, Vector2Scale(flow, dt)), 10 + dt).height
                  - boat_water_query(combined, Vector2Subtract(p, Vector2Scale(flow, dt)), 10 - dt)
                      .height)
            / (2 * dt);
        CHECK(fabsf(derivative - q.velocity.y) < .002f);
    }
    Vector2 source;
    CHECK(boat_water_wake_source(wake, 6, &source) && source.x == 0 && source.y == -8);
    CHECK(!boat_water_wake_source(wake, 13, &source));
    for (float x = -40; x < 40; x += 4) {
        BoatWaterSample q = boat_water_query(wake, (Vector2) { x, 0 }, 25);
        CHECK(q.height == 0 && q.velocity.y == 0 && q.normal.y == 1);
    }
    // Repeated crossings at different speeds and angles must settle without stored energy.
    const float speeds[] = { 0, 3, 9 };
    float encounter = 0;
    for (int si = 0; si < 3; si++)
        for (int ai = 0; ai < 3; ai++) {
            Boat a, b;
            boat_reset(&a);
            a.position = (Vector2) { 0, 1.6f };
            a.heading = ai * PI / 4;
            a.camera_heading = a.heading;
            a.time = 10;
            a.velocity = (Vector2) { sinf(a.heading) * speeds[si], -cosf(a.heading) * speeds[si] };
            a.water_settings = wake;
            b = a;
            for (int tick = 0; tick < 3600; tick++) {
                boat_step(&a, 0, 0);
                boat_step(&b, 0, 0);
                CHECK(a.height == b.height && a.position.x == b.position.x && a.pitch == b.pitch);
                CHECK(isfinite(a.height) && fabsf(a.height) < 1);
                CHECK(fabsf(a.roll) < .65f && fabsf(a.pitch) < .65f);
                CHECK(Vector2Length(a.velocity) < 12);
                if (si == 0)
                    encounter = fmaxf(encounter, fabsf(a.height));
            }
            CHECK(fabsf(a.height) < .002f && fabsf(a.pitch) < .002f && fabsf(a.roll) < .002f);
            CHECK(fabsf(a.vertical_velocity) < .002f && Vector2Length(a.velocity) < .08f);
        }
    CHECK(encounter > .01f);
    return 0;
}

// Compile the production fragment wake function into a readback probe, then test the
// actual production vertex displacement independently. No copied wave formula in tests.
static int gpu_checks(void)
{
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(32, 32, "Environment parity");
    CHECK(IsWindowReady());
    char *fragment = LoadFileText("assets/shaders/waterfront_water.fs");
    char *vertex = LoadFileText("assets/shaders/waterfront_water.vs");
    CHECK(fragment && vertex);
    char *main = strstr(fragment, "void main()");
    CHECK(main);
    size_t prefix = (size_t)(main - fragment);
    char *probe = malloc(prefix + 512);
    CHECK(probe);
    memcpy(probe, fragment, prefix);
    strcpy(probe + prefix,
        "uniform vec2 probePosition;\nvoid main() { "
        "finalColor = vec4(crossingWake(probePosition, time) * 0.5 + 0.5, 1.0); }");
    Shader slope = LoadShaderFromMemory(NULL, probe);
    const char *height_fragment = "#version 330\nin vec3 fragWorldPosition;\n"
                                  "out vec4 finalColor; void main() { "
                                  "finalColor=vec4(fragWorldPosition.y * 0.5 + 0.5,0,0,1); }";
    Shader height = LoadShaderFromMemory(vertex, height_fragment);
    CHECK(IsShaderValid(slope) && IsShaderValid(height));
    free(probe);
    UnloadFileText(fragment);
    UnloadFileText(vertex);
    RenderTexture2D target = LoadRenderTexture(8, 8);
    CHECK(IsRenderTextureValid(target));
    Mesh plane = GenMeshPlane(.01f, .01f, 1, 1);
    float *rest = malloc((size_t)plane.vertexCount * 3 * sizeof(float));
    CHECK(rest);
    memcpy(rest, plane.vertices, (size_t)plane.vertexCount * 3 * sizeof(float));
    Material material = LoadMaterialDefault();
    material.shader = height;
    BoatWater wake = boat_water_test(false, false, true);
    float amplitude = wake.wake_amplitude;
    int enabled = 1, patch = 0;
    SetShaderValue(
        slope, GetShaderLocation(slope, "wakeAmplitude"), &amplitude, SHADER_UNIFORM_FLOAT);
    SetShaderValue(
        height, GetShaderLocation(height, "wakeAmplitude"), &amplitude, SHADER_UNIFORM_FLOAT);
    SetShaderValue(
        height, GetShaderLocation(height, "physicalWaves"), &enabled, SHADER_UNIFORM_INT);
    SetShaderValue(height, GetShaderLocation(height, "waterPatchPass"), &patch, SHADER_UNIFORM_INT);
    int checked = 0;
    for (float time = 8; time <= 16; time += 4) {
        SetShaderValue(slope, GetShaderLocation(slope, "time"), &time, SHADER_UNIFORM_FLOAT);
        SetShaderValue(height, GetShaderLocation(height, "time"), &time, SHADER_UNIFORM_FLOAT);
        for (int x = -16; x <= 16; x += 8)
            for (int z = -24; z <= 16; z += 4) {
                BoatWaterSample q = boat_water_query(wake, (Vector2) { (float)x, (float)z }, time);
                float world[] = { -(float)x, (float)z };
                SetShaderValue(
                    slope, GetShaderLocation(slope, "probePosition"), world, SHADER_UNIFORM_VEC2);
                BeginTextureMode(target);
                ClearBackground(BLACK);
                BeginShaderMode(slope);
                DrawRectangle(0, 0, 8, 8, WHITE);
                EndShaderMode();
                EndTextureMode();
                Image image = LoadImageFromTexture(target.texture);
                CHECK(image.data);
                Color pixel = GetImageColor(image, 4, 4);
                UnloadImage(image);
                float values[] = { q.height, q.normal.x / q.normal.y, -q.normal.z / q.normal.y };
                unsigned char actual[] = { pixel.r, pixel.g, pixel.b };
                for (int i = 0; i < 3; ++i)
                    CHECK(abs((int)actual[i] - (int)roundf((values[i] * .5f + .5f) * 255)) <= 2);
                Camera3D camera = { .position = { world[0], 10, world[1] },
                    .target = { world[0], 0, world[1] },
                    .up = { 0, 0, -1 },
                    .fovy = .01f,
                    .projection = CAMERA_ORTHOGRAPHIC };
                BeginTextureMode(target);
                ClearBackground(BLACK);
                BeginMode3D(camera);
                for (int v = 0; v < plane.vertexCount; ++v) {
                    plane.vertices[v * 3] = rest[v * 3] + world[0];
                    plane.vertices[v * 3 + 2] = rest[v * 3 + 2] + world[1];
                }
                UpdateMeshBuffer(
                    plane, 0, plane.vertices, plane.vertexCount * 3 * (int)sizeof(float), 0);
                DrawMesh(plane, material, MatrixIdentity());
                EndMode3D();
                EndTextureMode();
                image = LoadImageFromTexture(target.texture);
                CHECK(image.data);
                pixel = GetImageColor(image, 4, 4);
                UnloadImage(image);
                if (abs((int)pixel.r - (int)roundf((q.height * .5f + .5f) * 255)) > 2)
                    fprintf(stderr, "vertex mismatch x=%d z=%d t=%g h=%g pixel=%u\n", x, z, time,
                        q.height, pixel.r);
                CHECK(abs((int)pixel.r - (int)roundf((q.height * .5f + .5f) * 255)) <= 2);
                ++checked;
            }
    }
    // Material borrows the shader; release it separately exactly once.
    material.shader = (Shader) { 0 };
    free(rest);
    UnloadMaterial(material);
    UnloadMesh(plane);
    UnloadShader(slope);
    UnloadShader(height);
    UnloadRenderTexture(target);
    CloseWindow();
    printf("boat_environment_gpu=ok samples=%d\n", checked);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--gpu") == 0)
        return gpu_checks();
    CHECK(argc == 1);
    CHECK(current_checks() == 0);
    CHECK(wake_checks() == 0);
    puts("boat_environment=ok");
    return 0;
}
