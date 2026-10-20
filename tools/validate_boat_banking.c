#include "boat.h"
#include "raymath.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Banking check failed at line %d: %s\n", __LINE__, #condition);        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static Vector2 forward(float heading) { return (Vector2) { sinf(heading), -cosf(heading) }; }
static Vector2 right(float heading) { return (Vector2) { cosf(heading), sinf(heading) }; }

static float energy(const Boat *boat)
{
    // Normalized horizontal mass=1, yaw inertia=2.2; excludes imposed tow work.
    return .5f * Vector2LengthSqr(boat->velocity) + 1.1f * boat->yaw_velocity * boat->yaw_velocity;
}

static bool open_water(const Boat *boat)
{
    // Conservative circumscribed hull radius (including shore clearance). Check before
    // stepping as well: a contact solver pulling a boat back cannot disguise a wall hit.
    return fabsf(boat->position.x) + 3 < BOAT_BASIN_HALF_X
        && fabsf(boat->position.y) + 3 < BOAT_BASIN_HALF_Z;
}

static bool same_state(const Boat *a, const Boat *b)
{
    // Explicit physical/control state, never padding or a struct-byte hash. Extend when
    // live state gains turn_roll_load in T2. Presentation particles do not feed physics.
#define SAME(member) (a->member == b->member)
    return SAME(position.x) && SAME(position.y) && SAME(velocity.x) && SAME(velocity.y)
        && SAME(time) && SAME(heading) && SAME(yaw_velocity) && SAME(planing) && SAME(height)
        && SAME(vertical_velocity) && SAME(pitch) && SAME(pitch_velocity) && SAME(roll)
        && SAME(roll_velocity) && SAME(throttle) && SAME(engine_response) && SAME(wheel)
        && SAME(engine_angle) && SAME(wave_spray) && SAME(camera_heading);
#undef SAME
}

// All rows describe the pre-tick state. Acceleration is measured over the following tick,
// projected on its PRE-tick right vector. This is a force probe only in calm, contact-free
// water: no waves, impacts or post-tick heading projection may contaminate this estimate.
static void row(FILE *report, const char *name, int tick, const Boat *before, float acceleration,
    float radius, float residual)
{
    if (!report)
        return;
    float u = Vector2DotProduct(Vector2Subtract(before->velocity,
                                    boat_water_current(before->water_settings, before->position)),
        forward(before->heading));
    fprintf(report,
        "%s,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n", name,
        tick, tick * (double)BOAT_STEP, u, before->wheel, before->engine_angle, before->planing,
        before->yaw_velocity, before->roll, before->roll_velocity, before->height, before->pitch,
        energy(before), acceleration, radius, residual,
        Vector2DotProduct(before->velocity, right(before->heading)));
}

static int validate_sides(void)
{
    const float headings[] = { 0, PI / 2, -PI / 2, PI - .001f, -PI + .001f };
    for (size_t i = 0; i < sizeof(headings) / sizeof(*headings); ++i) {
        for (int sign = -1; sign <= 1; sign += 2) {
            Boat boat;
            boat_reset(&boat);
            boat.heading = headings[i];
            boat.roll = sign * .1f;
            WaterfrontPlayerState pose = { 0 };
            boat_player_pose(&boat, &pose);
            Matrix rotation = MatrixRotateXYZ(pose.hull.rotation);
            // Simulation starboard is renderer local negative X at every heading.
            Vector3 starboard = Vector3Transform((Vector3) { -.65f, 0, 0 }, rotation);
            Vector3 port = Vector3Transform((Vector3) { .65f, 0, 0 }, rotation);
            Vector2 rendered_right = { -cosf(boat.heading), sinf(boat.heading) };
            CHECK(Vector2DotProduct((Vector2) { starboard.x, starboard.z }, rendered_right) > .64f);
            CHECK(sign * (port.y - starboard.y) > .12f);
            CHECK(fabsf(starboard.y + .65f * sinf(boat.roll)) < .000001f);
        }
    }
    puts("side_height_signs=ok headings=0,+/-pi/2,near+/-pi positive_roll_lowers_starboard");
    return 0;
}

static int validate_restoring(FILE *report)
{
    Boat boat;
    boat_reset(&boat);
    boat.roll = .0001f;
    boat_step(&boat, 0, 0);
    float stiffness = -(.45f * boat.roll_velocity / BOAT_STEP) / .0001f;
    CHECK(fabsf(stiffness - 64 * .65f * .65f) < .002f);
    boat_reset(&boat);
    boat.roll = 10 * DEG2RAD;
    int last_unsettled = 0;
    for (int tick = 0; tick < 360; ++tick) {
        Boat before = boat;
        boat_step(&boat, 0, 0);
        row(report, "free_decay", tick, &before, 0, 0, 0);
        CHECK(isfinite(boat.roll) && isfinite(boat.roll_velocity));
        if (fabsf(boat.roll) >= .5f * DEG2RAD || fabsf(boat.roll_velocity) >= .5f * DEG2RAD)
            last_unsettled = tick + 1;
    }
    CHECK(fabsf(boat.roll) < .5f * DEG2RAD && fabsf(boat.roll_velocity) < .5f * DEG2RAD);
    printf(
        "restoring_stiffness=%.6f recovery_10deg_seconds=%.6f threshold_deg=0.5 rate_deg_s=0.5\n",
        stiffness, (last_unsettled + 1) * BOAT_STEP);
    return 0;
}

static int validate_free(FILE *report)
{
    const char *names[]
        = { "straight", "left", "right", "countersteer", "coast", "reverse", "stop" };
    for (int scenario = 0; scenario < 7; ++scenario) {
        Boat boat;
        boat_reset(&boat);
        boat.position.y = scenario == 5 ? -40 : 40;
        if (scenario == 4 || scenario == 6)
            boat.velocity.y = -12;
        Boat repeat = boat;
        float peak_roll = 0, peak_rate = 0, peak_load = 0, sum_roll = 0;
        int ticks = scenario == 0 ? 720 : scenario == 5 ? 600 : 960;
        for (int tick = 0; tick < ticks; ++tick) {
            float wheel = scenario == 1 ? -1
                : scenario == 2         ? 1
                : scenario == 3         ? (tick < 480 ? 1 : -1)
                : scenario == 4         ? .5f
                                        : 0;
            float throttle = scenario == 5 ? -1
                : scenario == 4            ? 0
                : scenario == 6            ? (tick < 240 ? -1 : 0)
                                           : 1;
            boat.throttle = repeat.throttle = throttle;
            CHECK(open_water(&boat));
            Boat before = boat;
            boat_step(&boat, wheel, 0);
            boat_step(&repeat, wheel, 0);
            CHECK(same_state(&boat, &repeat));
            CHECK(open_water(&boat));
            float load = Vector2DotProduct(
                             Vector2Subtract(boat.velocity, before.velocity), right(before.heading))
                / BOAT_STEP;
            row(report, names[scenario], tick, &before, load, 0, 0);
            CHECK(isfinite(load) && isfinite(boat.roll) && isfinite(energy(&boat)));
            if (scenario == 0 || scenario == 5 || scenario == 6)
                CHECK(fabsf(boat.roll) < .000001f);
            peak_roll = fmaxf(peak_roll, fabsf(boat.roll));
            peak_rate = fmaxf(peak_rate, fabsf(boat.roll_velocity));
            peak_load = fmaxf(peak_load, fabsf(load));
            sum_roll += boat.roll;
        }
        printf(
            "free=%s mean_roll_deg=%.6f peak_roll_deg=%.6f peak_rate_deg_s=%.6f peak_load=%.6f\n",
            names[scenario], sum_roll / ticks * RAD2DEG, peak_roll * RAD2DEG, peak_rate * RAD2DEG,
            peak_load);
    }
    return 0;
}

static void prescribe(Boat *boat, float speed, float slip, float yaw)
{
    // External tow work: only horizontal position/velocity/yaw and planing are imposed.
    // Recenter on spatially uniform calm water; heave, pitch and roll keep evolving.
    boat->position = (Vector2) { 0 };
    boat->velocity = Vector2Add(
        Vector2Scale(forward(boat->heading), speed), Vector2Scale(right(boat->heading), slip));
    boat->yaw_velocity = yaw;
    boat->planing = 1;
}

static float probe(float speed, float slip, float yaw)
{
    Boat boat;
    boat_reset(&boat);
    prescribe(&boat, speed, slip, yaw);
    Vector2 velocity = boat.velocity;
    boat_step(&boat, 0, 0);
    return (boat.velocity.x - velocity.x) / BOAT_STEP;
}

static int validate_tow(FILE *report)
{
    const float speeds[] = { 4, 8, 12, 8, 8, 8 };
    const float radii[] = { 30, 30, 30, 20, 40, 80 };
    for (int trial = 0; trial < 6; ++trial) {
        for (int sign = -1; sign <= 1; sign += 2) {
            float speed = speeds[trial], radius = radii[trial];
            float yaw = sign * speed / radius, target = speed * yaw;
            float lo = -16, hi = 16;
            float low_load = probe(speed, lo, yaw), high_load = probe(speed, hi, yaw);
            if (!(low_load > target && high_load < target)) {
                fprintf(stderr,
                    "No tow slip root: u=%g radius=%g target=%g bracket=[%g,%g] loads=[%g,%g]\n",
                    speed, sign * radius, target, lo, hi, low_load, high_load);
                return 1;
            }
            for (int iteration = 0; iteration < 32; ++iteration) {
                float mid = (lo + hi) / 2;
                if (probe(speed, mid, yaw) > target)
                    lo = mid;
                else
                    hi = mid;
            }
            float slip = (lo + hi) / 2;
            CHECK(fabsf(probe(speed, slip, yaw) - target) < .0002f);
            Boat boat;
            boat_reset(&boat);
            float peak_residual = 0, mean_roll = 0, peak_roll = 0;
            char name[64];
            snprintf(name, sizeof(name), "tow_u%.0f_r%.0f_sign%d", speed, radius, sign);
            for (int tick = 0; tick < 960; ++tick) {
                prescribe(&boat, speed, slip, yaw);
                CHECK(open_water(&boat));
                Boat before = boat;
                boat_step(&boat, 0, 0);
                CHECK(open_water(&boat));
                float load = Vector2DotProduct(Vector2Subtract(boat.velocity, before.velocity),
                                 right(before.heading))
                    / BOAT_STEP;
                float residual = load - target;
                CHECK(fabsf(residual) < .0003f);
                peak_residual = fmaxf(peak_residual, fabsf(residual));
                peak_roll = fmaxf(peak_roll, fabsf(boat.roll));
                if (tick >= 720)
                    mean_roll += boat.roll / 240;
                row(report, name, tick, &before, load, sign * radius, residual);
            }
            printf("tow_external_work u=%.0f nominal_radius=%g slip=%.6f load=%.6f "
                   "peak_residual=%.8f mean_roll_deg=%.6f peak_roll_deg=%.6f\n",
                speed, sign * radius, slip, target, peak_residual, mean_roll * RAD2DEG,
                peak_roll * RAD2DEG);
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    FILE *report = NULL;
    if (argc == 3 && strcmp(argv[1], "--report") == 0) {
        report = fopen(argv[2], "w");
        if (!report) {
            perror(argv[2]);
            return 1;
        }
        fprintf(report,
            "fixture,tick,time,u,wheel,engine_angle,planing,yaw_rate,roll,roll_rate,"
            "height,pitch,horizontal_energy,lateral_acceleration,nominal_radius,"
            "load_residual,side_slip\n");
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [--report <path>]\n", argv[0]);
        return 1;
    }
    int result = validate_sides() || validate_restoring(report) || validate_free(report)
        || validate_tow(report);
    if (report) {
        bool failed = ferror(report) != 0;
        if (fclose(report) != 0)
            failed = true;
        if (failed) {
            fprintf(stderr, "Failed to write banking report\n");
            result = 1;
        }
    }
    if (!result)
        puts("boat_banking_baseline=ok (T1; no load-driven roll model yet)");
    return result;
}
