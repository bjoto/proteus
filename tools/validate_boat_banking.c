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
    // Explicit physical/control state, never padding or a struct-byte hash.
    // Presentation particles do not feed physics.
#define SAME(member) (a->member == b->member)
    return SAME(position.x) && SAME(position.y) && SAME(velocity.x) && SAME(velocity.y)
        && SAME(time) && SAME(heading) && SAME(yaw_velocity) && SAME(planing) && SAME(height)
        && SAME(vertical_velocity) && SAME(pitch) && SAME(pitch_velocity) && SAME(roll)
        && SAME(roll_velocity) && SAME(turn_roll_load) && SAME(throttle) && SAME(engine_response)
        && SAME(wheel) && SAME(engine_angle) && SAME(wave_spray) && SAME(camera_heading);
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
        "%s,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",
        name, tick, tick * (double)BOAT_STEP, u, before->wheel, before->engine_angle,
        before->planing, before->yaw_velocity, before->roll, before->roll_velocity, before->height,
        before->pitch, energy(before), acceleration, radius, residual,
        Vector2DotProduct(before->velocity, right(before->heading)), before->turn_roll_load);
}

// Independent finite-difference force measurement: production never reads this diagnostic.
static int check_filter(const Boat *before, const Boat *after, float measured_load)
{
    float u = Vector2DotProduct(before->velocity, forward(before->heading));
    float blend = Clamp((u - 1) / 5, 0, 1);
    blend = blend * blend * (3 - 2 * blend);
    float gain = .12f + (.85f - .12f) * after->planing * blend;
    float expected = before->turn_roll_load
        + (gain * measured_load - before->turn_roll_load) * (1 - expf(-BOAT_STEP / .12f));
    CHECK(fabsf(after->turn_roll_load - expected) < .00002f);
    return 0;
}

static int validate_core(void)
{
    Boat boat;
    boat_reset(&boat);
    for (int tick = 0; tick < 240; ++tick) {
        boat_step(&boat, 1, 0);
        CHECK(boat.turn_roll_load == 0 && fabsf(boat.roll) < .000001f
            && fabsf(boat.roll_velocity) < .000001f);
    }
    // A no-force residual decays exactly; the filter is still active in neutral.
    boat.turn_roll_load = 2;
    float previous = boat.turn_roll_load;
    boat_step(&boat, 0, 0);
    CHECK(fabsf(boat.turn_roll_load - previous * expf(-BOAT_STEP / .12f)) < .000001f);
    CHECK(boat.roll > 0 && boat.roll_velocity > 0);
    boat_reset(&boat);
    CHECK(boat.turn_roll_load == 0 && boat.roll == 0 && boat.roll_velocity == 0);
    boat.turn_roll_load = 3;
    boat_reset_location(&boat, true);
    CHECK(boat.turn_roll_load == 0);
    boat.turn_roll_load = 3;
    boat_reset_course(&boat);
    CHECK(boat.turn_roll_load == 0);
    // Saturated demand is a bounded torque, integrated through angular velocity.
    for (int sign = -1; sign <= 1; sign += 2) {
        boat_reset(&boat);
        boat.turn_roll_load = sign * 10000;
        boat_step(&boat, 0, 0);
        float moment = .45f * boat.roll_velocity / BOAT_STEP;
        CHECK(fabsf(moment - sign * 27.04f * 12 * DEG2RAD) < .00001f);
        CHECK(fabsf(boat.roll - boat.roll_velocity * BOAT_STEP) < .000001f);
    }
    // Reverse keeps the low gain, even with stale planing state.
    boat_reset(&boat);
    boat.velocity = (Vector2) { .8f, 3 };
    boat.yaw_velocity = .4f;
    boat.planing = 1;
    Boat before = boat;
    boat_step(&boat, 0, 0);
    CHECK(check_filter(&before, &boat, (boat.velocity.x - before.velocity.x) / BOAT_STEP) == 0);
    CHECK(fabsf(boat.turn_roll_load) < .1f);
    float reverse_peak = 0;
    for (int tick = 0; tick < 960; ++tick) {
        // External tow at fixed reverse water-relative motion; vertical state evolves.
        boat.position = (Vector2) { 0 };
        boat.velocity = Vector2Add(
            Vector2Scale(forward(boat.heading), -3), Vector2Scale(right(boat.heading), .8f));
        boat.yaw_velocity = .4f;
        boat_step(&boat, 0, 0);
        reverse_peak = fmaxf(reverse_peak, fabsf(boat.roll));
        CHECK(reverse_peak < 3 * DEG2RAD);
    }
    printf(
        "reverse_external_work u=-3 yaw=.4 slip=.8 peak_roll_deg=%.6f\n", reverse_peak * RAD2DEG);
    // Guard recovery does not clamp angle/rate or discard the disturbed state.
    for (int sign = -1; sign <= 1; sign += 2) {
        boat_reset(&boat);
        boat.roll = sign * 32 * DEG2RAD;
        boat_step(&boat, 0, 0);
        CHECK(sign * boat.roll > 30 * DEG2RAD && sign * boat.roll_velocity < 0);
        for (int tick = 0; tick < 360; ++tick)
            boat_step(&boat, 0, 0);
        CHECK(fabsf(boat.roll) < .5f * DEG2RAD && fabsf(boat.roll_velocity) < .5f * DEG2RAD);
    }
    puts("core=ok rest_lock,neutral_filter,reset,moment_limit,reverse_gain,guard_recovery");
    return 0;
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
            CHECK(check_filter(&before, &boat, load) == 0);
            if (scenario == 4)
                CHECK(energy(&boat) <= energy(&before) + .00001f);
            row(report, names[scenario], tick, &before, load, 0, 0);
            CHECK(isfinite(load) && isfinite(boat.roll) && isfinite(energy(&boat)));
            if (scenario == 0 || scenario == 5 || scenario == 6)
                CHECK(fabsf(boat.roll) < .000001f);
            peak_roll = fmaxf(peak_roll, fabsf(boat.roll));
            peak_rate = fmaxf(peak_rate, fabsf(boat.roll_velocity));
            peak_load = fmaxf(peak_load, fabsf(load));
            sum_roll += boat.roll;
        }
        if (scenario == 1 || scenario == 2)
            CHECK((scenario == 1 ? -1 : 1) * boat.roll > .01f);
        if (scenario == 4)
            CHECK(peak_roll > DEG2RAD); // Neutral coasting still produces a bank.
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
                CHECK(check_filter(&before, &boat, load) == 0);
                float residual = load - target;
                CHECK(fabsf(residual) < .0003f);
                peak_residual = fmaxf(peak_residual, fabsf(residual));
                peak_roll = fmaxf(peak_roll, fabsf(boat.roll));
                if (tick >= 720)
                    mean_roll += boat.roll / 240;
                row(report, name, tick, &before, load, sign * radius, residual);
            }
            CHECK(sign * mean_roll > .001f);
            CHECK(peak_roll < 25 * DEG2RAD);
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
            "load_residual,side_slip,turn_roll_load\n");
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [--report <path>]\n", argv[0]);
        return 1;
    }
    int result = validate_core() || validate_sides() || validate_restoring(report)
        || validate_free(report) || validate_tow(report);
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
        puts("boat_banking=ok (T2 load-driven physical roll)");
    return result;
}
