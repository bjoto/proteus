// Deterministic banking presentation audit, sharing free-drive inputs with the dynamics validator.
#include "boat_banking_fixture.h"
#include "boat_gpu.h"
#include "raymath.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Banking review line %d: %s\n", __LINE__, #c);                         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

// Independent elementary rotations: roll in hull space, pitch, then world heading.
static Vector3 rotate(Vector3 p, const Boat *b)
{
    float c = cosf(b->roll), s = sinf(b->roll);
    p = (Vector3) { c * p.x - s * p.y, s * p.x + c * p.y, p.z };
    c = cosf(b->pitch);
    s = sinf(b->pitch);
    p = (Vector3) { p.x, c * p.y - s * p.z, s * p.y + c * p.z };
    c = cosf(b->heading);
    s = sinf(b->heading);
    return (Vector3) { c * p.x + s * p.z, p.y, -s * p.x + c * p.z };
}

static Vector3 point(Transform3D t, Vector3 p)
{
    return Vector3Add(
        t.position, Vector3Transform(Vector3Multiply(p, t.scale), MatrixRotateXYZ(t.rotation)));
}

static int pose_check(const Boat *b)
{
    WaterfrontPlayerState p = { 0 };
    boat_player_pose(b, &p);
    Vector3 base = { -b->position.x, b->height, b->position.y };
    const Vector3 probes[] = { { 0, 0, 0 }, { 1, .3f, -.7f }, { -.8f, 1, 1 } };
    for (int i = 0; i < 3; ++i) {
        Vector3 q = probes[i];
        CHECK(Vector3Distance(point(p.hull, q), Vector3Add(base, rotate(Vector3Scale(q, 1.4f), b)))
            < .00003f);
        Vector3 cabin
            = Vector3Add(Vector3Multiply(q, (Vector3) { 1, .7f, 1 }), (Vector3) { 0, 1.1f, 0 });
        CHECK(Vector3Distance(point(p.cabin, q), Vector3Add(base, rotate(cabin, b))) < .00003f);
        Vector3 engine = Vector3Transform(
            Vector3Multiply(q, (Vector3) { .55f, .65f, .9f }), MatrixRotateY(-b->engine_angle));
        engine = Vector3Add(engine, (Vector3) { 0, .4f, 1.6f });
        CHECK(Vector3Distance(point(p.engine, q), Vector3Add(base, rotate(engine, b))) < .00003f);
    }
    // Each new airborne particle is born at the rolled rail (or lifted to the waterline).
    // A symmetric numerical derivative checks inherited vertical attachment velocity.
    Vector2 right = { cosf(b->heading), sinf(b->heading) };
    Vector2 flow = boat_water_current(b->water_settings, b->position);
    float slip = Vector2DotProduct(Vector2Subtract(b->velocity, flow), right);
    for (int i = 0; i < BOAT_WATER_PARTICLES; ++i) {
        const BoatWaterParticle *w = &b->water[i];
        if (!w->spray || w->lifetime <= 0 || w->age != 0)
            continue;
        bool matched = false;
        for (int side = -1; side <= 1; side += 2) {
            Vector3 local = { -side * 1.05f, .25f, -.4f };
            Vector3 origin = Vector3Add(base, rotate(local, b));
            if (Vector2Distance(w->position, (Vector2) { -origin.x, origin.z }) > .00003f)
                continue;
            float surface = boat_water_query(b->water_settings, w->position, b->time).height;
            CHECK(fabsf(w->height - fmaxf(surface + .03f, origin.y)) < .00003f);
            Boat plus = *b, minus = *b;
            const float epsilon = .001f;
            plus.roll += epsilon * b->roll_velocity;
            minus.roll -= epsilon * b->roll_velocity;
            plus.pitch += epsilon * b->pitch_velocity;
            minus.pitch -= epsilon * b->pitch_velocity;
            float vertical = b->vertical_velocity
                + (rotate(local, &plus).y - rotate(local, &minus).y) / (2 * epsilon);
            float variation = (float)((b->emission * 37u) % 101u) / 100;
            float strength = fabsf(slip) > .9f ? 1 : fmaxf(b->planing, b->wave_spray);
            float expected = (2 + variation * 2 + fminf(fabsf(slip) * .25f, 1.5f)) * strength
                + vertical + (boat_water_displaced(b->water_settings) ? b->wave_spray : 0);
            CHECK(fabsf(w->lift - expected) < .0002f);
            matched = true;
        }
        CHECK(matched);
    }
    WaterfrontSceneFrame frame;
    boat_scene_frame(b, BOAT_CAMERA_DEFAULT, &frame);
    CHECK(Vector3Distance(frame.camera.up, (Vector3) { 0, 1, 0 }) < .000001f);
    CHECK(Vector3Distance(frame.player.engine.position, p.engine.position) < .000001f);
    return 0;
}

static int capture(BoatGpu *gpu, Renderer3D *cpu, const Boat *b, bool use_gpu, BoatCamera camera,
    const char *path, bool ghost)
{
    Color pixels[320 * 200];
    Boat opponent = *b;
    opponent.position.x += 7 * sinf(b->heading) + 3 * cosf(b->heading);
    opponent.position.y += -7 * cosf(b->heading) + 3 * sinf(b->heading);
    Image image;
    if (use_gpu) {
        boat_gpu_draw_with_ghost(gpu, b, camera, ghost ? &opponent : NULL, .6f);
        image = LoadImageFromTexture(gpu->target.texture);
        CHECK(image.data);
        ImageFlipVertical(&image);
        ImageFlipHorizontal(&image);
    } else {
        CHECK(
            boat_draw_with_ghost(cpu, pixels, 320, 200, b, camera, ghost ? &opponent : NULL, .6f));
        image = (Image) { pixels, 320, 200, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    }
    bool ok = ExportImage(image, path);
    if (use_gpu)
        UnloadImage(image);
    CHECK(ok);
    return 0;
}

int main(int argc, char **argv)
{
    bool gpu_enabled = false;
    const char *output = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--gpu"))
            gpu_enabled = true;
        else if (!strcmp(argv[i], "--output") && i + 1 < argc)
            output = argv[++i];
        else
            return 2;
    }
    if (gpu_enabled && !output)
        return 2;
    SetTraceLogLevel(LOG_WARNING);
    if (output)
        CHECK(DirectoryExists(output) || MakeDirectory(output) == 0);
    BoatGpu gpu = { 0 };
    Renderer3D cpu = { 0 };
    if (gpu_enabled) {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(320, 200, "Banking presentation review");
        CHECK(IsWindowReady() && boat_gpu_init(&gpu, true));
    }
    const char *names[] = { "straight", "left", "right", "reversal", "coast", "wave-peak" };
    for (int fixture = 0; fixture < 6; ++fixture) {
        int scenario = fixture == 5 ? 2 : fixture;
        Boat boat, selected;
        banking_fixture_reset(&boat, scenario);
        if (fixture == 5)
            boat.water_settings = boat_water_gentle();
        selected = boat;
        float peak = 0;
        int selected_tick = 0;
        int ticks = banking_fixture_ticks(scenario);
        for (int tick = 0; tick < ticks; ++tick) {
            CHECK(fabsf(boat.position.x) + 3 < BOAT_BASIN_HALF_X
                && fabsf(boat.position.y) + 3 < BOAT_BASIN_HALF_Z);
            boat_step(&boat, banking_fixture_input(&boat, scenario, tick), 0);
            CHECK(pose_check(&boat) == 0);
            CHECK(isfinite(boat.roll) && fabsf(boat.roll) < 30 * DEG2RAD);
            bool select = fixture >= 4 ? fabsf(boat.roll) > peak
                : fixture == 3         ? tick == 599
                                       : tick == ticks - 1;
            if (select) {
                selected = boat;
                selected_tick = tick + 1;
                peak = fabsf(boat.roll);
            }
        }
        if (fixture == 0)
            CHECK(fabsf(selected.roll) < .000001f);
        if (fixture == 1 || fixture == 2)
            CHECK((fixture == 1 ? -1 : 1) * selected.roll > DEG2RAD);
        if (fixture >= 4)
            CHECK(peak > DEG2RAD);
        printf("%s tick=%d roll_deg=%.6f rate_deg_s=%.6f\n", names[fixture], selected_tick,
            selected.roll * RAD2DEG, selected.roll_velocity * RAD2DEG);
        if (output)
            for (int view = 0; view < 3; ++view) {
                BoatCamera camera
                    = view == 0 ? BOAT_CAMERA_DEFAULT : (BoatCamera) { .distance = 8, .height = 4 };
                Boat shown = selected;
                if (view == 2)
                    shown.camera_heading = shown.heading + .65f;
                char path[2048];
                CHECK(snprintf(path, sizeof(path), "%s/%s-%s.png", output, names[fixture],
                          view == 0       ? "default"
                              : view == 1 ? "rear"
                                          : "side")
                    < (int)sizeof(path));
                CHECK(capture(&gpu, &cpu, &shown, gpu_enabled, camera, path, false) == 0);
                if (fixture == 2 && view == 1) {
                    CHECK(snprintf(path, sizeof(path), "%s/banked-ghost.png", output)
                        < (int)sizeof(path));
                    CHECK(capture(&gpu, &cpu, &shown, gpu_enabled, camera, path, true) == 0);
                }
            }
    }
    // Probe wrap/gimbal neighborhoods explicitly, in addition to the driven headings.
    const float headings[] = { 0, PI / 2, -PI / 2, PI - .0001f, -PI + .0001f };
    for (int i = 0; i < 5; ++i)
        for (int sign = -1; sign <= 1; sign += 2) {
            Boat b;
            boat_reset(&b);
            b.heading = headings[i];
            b.roll = sign * 29 * DEG2RAD;
            b.pitch = .2f;
            b.engine_angle = .4f;
            CHECK(pose_check(&b) == 0);
        }
    renderer3d_destroy(&cpu);
    if (gpu_enabled) {
        boat_gpu_destroy(&gpu);
        CloseWindow();
    }
    boat_water_destroy();
    return 0;
}
