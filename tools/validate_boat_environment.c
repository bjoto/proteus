#include "boat.h"
#include "boat_map.h"
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
    BoatWater shifted = wake;
    shifted.wake_offset = (Vector2) { -140, -220 };
    for (float t = 4; t <= 16; t += 2) {
        Vector2 p = { -10, 4 };
        BoatWaterSample a = boat_water_query(wake, p, t);
        BoatWaterSample b = boat_water_query(shifted, Vector2Add(p, shifted.wake_offset), t);
        CHECK(fabsf(a.height - b.height) < .00001f);
        CHECK(Vector3Distance(a.normal, b.normal) < .00001f);
        CHECK(Vector3Distance(a.velocity, b.velocity) < .00001f);
    }
    for (int river = 0; river < 2; ++river) {
        waterfront_scene_select_river(river != 0);
        for (int tick = 0; tick <= 120; ++tick) {
            CHECK(boat_water_wake_source(shifted, tick * .1f, &source));
            CHECK(boat_map_water(source, 3));
        }
        boat_map_destroy();
    }
    waterfront_scene_select_river(false);
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

static int player_wake_checks(void)
{
    BoatWater water = { .player_wake_enabled = true };
    boat_water_emit_wake(&water, (Vector2) { -8, 0 }, (Vector2) { 0, 0 }, 2, .4f, 20);
    boat_water_emit_wake(&water, (Vector2) { 0, 0 }, (Vector2) { 8, 0 }, 2.4f, .4f, 20);
    boat_water_emit_wake(&water, (Vector2) { 8, 0 }, (Vector2) { 8, 8 }, 2.8f, .4f, 20);
    float peak = 0;
    // Derivatives must include the normalization when curved/intersecting patches overlap.
    for (float t = 3; t < 24; t += .71f)
        for (float x = -12; x < 12; x += 2.3f)
            for (float z = -16; z < 16; z += 2.7f) {
                Vector2 p = { x, z };
                BoatWaterSample q = boat_water_query(water, p, t);
                peak = fmaxf(peak, fabsf(q.height));
                CHECK(fabsf(q.height) <= .65f && q.normal.y > .4f);
                float dt = .001f;
                float dy = (boat_water_query(water, p, t + dt).height
                               - boat_water_query(water, p, t - dt).height)
                    / (2 * dt);
                float dx = (boat_water_query(water, (Vector2) { x + dt, z }, t).height
                               - boat_water_query(water, (Vector2) { x - dt, z }, t).height)
                    / (2 * dt);
                float dz = (boat_water_query(water, (Vector2) { x, z + dt }, t).height
                               - boat_water_query(water, (Vector2) { x, z - dt }, t).height)
                    / (2 * dt);
                CHECK(fabsf(dy - q.velocity.y) < .002f);
                CHECK(fabsf(dx + q.normal.x / q.normal.y) < .002f);
                CHECK(fabsf(dz + q.normal.z / q.normal.y) < .002f);
            }
    CHECK(peak > .25f);
    // Track the same positive crest in world space: it advances 1.5 m in half a second,
    // while the original point falls to a zero crossing. This catches a frozen phase.
    BoatWater travelling = { .player_wake_enabled = true };
    boat_water_emit_wake(&travelling, (Vector2) { -8, 0 }, (Vector2) { 0, 0 }, .2f, .4f, 20);
    float radius0 = .9f + 3 * 4 - 4.5f;
    float radius1 = radius0 + 3 * .5f;
    Vector2 crest0 = { -4, sqrtf(radius0 * radius0 - .25f) };
    Vector2 crest1 = { -4, sqrtf(radius1 * radius1 - .25f) };
    CHECK(boat_water_query(travelling, crest0, 4).height > .1f);
    CHECK(boat_water_query(travelling, crest1, 4.5f).height > .1f);
    CHECK(fabsf(boat_water_query(travelling, crest0, 4.5f).height) < .001f);
    CHECK(Vector2Distance(crest0, crest1) > 1.49f);
    // Densely overlapping history is bounded; copies own their history, reset clears it.
    BoatWater copy = water;
    for (int i = 0; i < 200; ++i)
        boat_water_emit_wake(&copy, (Vector2) { -8, 0 }, (Vector2) { 0, 0 }, 2, .4f, 20);
    CHECK(water.player_wake_count == 3 && copy.player_wake_count == BOAT_WAKE_SEGMENTS);
    for (float z = 0; z < 20; z += .1f)
        CHECK(fabsf(boat_water_query(copy, (Vector2) { -4, z }, 8).height) <= .65f);
    CHECK(boat_water_query(copy, (Vector2) { -4, 6 }, 25).height == 0);
    Boat drifting;
    boat_reset(&drifting);
    drifting.water_settings = (BoatWater) { .player_wake_enabled = true, .current = { 1, 0 } };
    drifting.velocity = (Vector2) { 1, 0 };
    for (int tick = 0; tick < 480; ++tick)
        boat_step(&drifting, 0, 0);
    CHECK(drifting.water_settings.player_wake_count == 0);
    // Drive a loop back through the emitted wake, then coast and settle after it passes.
    Boat a, b;
    boat_reset(&a);
    boat_reset(&b);
    a.water_settings.player_wake_enabled = b.water_settings.player_wake_enabled = true;
    float encounter = 0, max_height = 0;
    for (int tick = 0; tick < 6000; ++tick) {
        float throttle = tick < 1440 ? 1 : 0;
        float wheel = tick >= 420 && tick < 1440 ? 1 : 0;
        boat_step(&a, Clamp((wheel - a.wheel) / BOAT_STEP, -1, 1),
            Clamp((throttle - a.throttle) / (.5f * BOAT_STEP), -1, 1));
        boat_step(&b, Clamp((wheel - b.wheel) / BOAT_STEP, -1, 1),
            Clamp((throttle - b.throttle) / (.5f * BOAT_STEP), -1, 1));
        CHECK(a.height == b.height && a.position.x == b.position.x && a.roll == b.roll);
        CHECK(fabsf(a.roll) < .7f && fabsf(a.pitch) < .7f && fabsf(a.height) < 1.5f);
        CHECK(Vector2Length(a.velocity) < 23);
        CHECK(fabsf(a.position.x) < BOAT_BASIN_HALF_X && fabsf(a.position.y) < BOAT_BASIN_HALF_Z);
        max_height = fmaxf(max_height, fabsf(a.height));
        if (tick > 720 && tick < 1440)
            encounter = fmaxf(
                encounter, fabsf(boat_water_query(a.water_settings, a.position, a.time).height));
    }
    CHECK(encounter > .05f && max_height > .1f);
    CHECK(fabsf(a.height) < .003f && fabsf(a.pitch) < .003f && fabsf(a.roll) < .003f);
    CHECK(Vector2Length(a.velocity) < .01f);
    boat_reset(&a);
    CHECK(a.water_settings.player_wake_count == 0 && a.physical_wake_time == 0);
    printf("player_wake=ok encounter=%g max_hull_height=%g\n", encounter, max_height);
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
        "finalColor = vec4((crossingWake(probePosition, time) + playerWake(probePosition, time)) * "
        "0.5 + 0.5, 1.0); }");
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
    wake.player_wake_enabled = true;
    // Exercise a full uniform array: the local packets occupy its last three slots.
    for (int i = 0; i < BOAT_WAKE_SEGMENTS - 3; ++i)
        boat_water_emit_wake(&wake, (Vector2) { 500, 500 }, (Vector2) { 508, 500 }, 4, .4f, 20);
    boat_water_emit_wake(&wake, (Vector2) { -20, -8 }, (Vector2) { -12, -8 }, 4, .4f, 20);
    boat_water_emit_wake(&wake, (Vector2) { -12, -8 }, (Vector2) { -4, -4 }, 4.4f, .4f, 20);
    boat_water_emit_wake(&wake, (Vector2) { -4, -4 }, (Vector2) { 4, -4 }, 4.8f, .4f, 20);
    float origins[BOAT_WAKE_SEGMENTS * 4], parameters[BOAT_WAKE_SEGMENTS * 4];
    int count = boat_water_wake_uniforms(wake, 8, origins, parameters);
    const Shader shaders[] = { slope, height };
    for (int si = 0; si < 2; ++si) {
        Shader shader = shaders[si];
        SetShaderValue(
            shader, GetShaderLocation(shader, "playerWakeCount"), &count, SHADER_UNIFORM_INT);
        SetShaderValueV(shader, GetShaderLocation(shader, "playerWakeOrigins[0]"), origins,
            SHADER_UNIFORM_VEC4, count);
        SetShaderValueV(shader, GetShaderLocation(shader, "playerWakeParameters[0]"), parameters,
            SHADER_UNIFORM_VEC4, count);
    }
    wake.wake_offset = (Vector2) { 3, -2 };
    float offset[] = { -wake.wake_offset.x, wake.wake_offset.y };
    SetShaderValue(slope, GetShaderLocation(slope, "wakeOffset"), offset, SHADER_UNIFORM_VEC2);
    SetShaderValue(height, GetShaderLocation(height, "wakeOffset"), offset, SHADER_UNIFORM_VEC2);
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
    CHECK(player_wake_checks() == 0);
    puts("boat_environment=ok");
    return 0;
}
