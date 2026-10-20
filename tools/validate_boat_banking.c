#include "boat.h"
#include "boat_map.h"
#include "raymath.h"
#include "waterfront_harbor.h"
#include "waterfront_traffic.h"

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
// Spatial-wave rows use NAN for unmeasured acceleration/residual, never a fabricated zero.
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

static float probe_planing(float speed, float slip, float yaw, float planing)
{
    Boat boat;
    boat_reset(&boat);
    prescribe(&boat, speed, slip, yaw);
    boat.planing = planing;
    Vector2 velocity = boat.velocity;
    boat_step(&boat, 0, 0);
    return (boat.velocity.x - velocity.x) / BOAT_STEP;
}

static float probe(float speed, float slip, float yaw)
{
    return probe_planing(speed, slip, yaw, 1);
}

static int validate_tow(FILE *report)
{
    const float speeds[] = { 4, 8, 12, 8, 8, 8 };
    const float radii[] = { 30, 30, 30, 20, 40, 80 };
    float means[6][2] = { 0 };
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
            means[trial][(sign + 1) / 2] = mean_roll;
            CHECK(sign * mean_roll > .001f);
            CHECK(peak_roll < 25 * DEG2RAD);
            printf("tow_external_work u=%.0f nominal_radius=%g slip=%.6f load=%.6f "
                   "peak_residual=%.8f mean_roll_deg=%.6f peak_roll_deg=%.6f\n",
                speed, sign * radius, slip, target, peak_residual, mean_roll * RAD2DEG,
                peak_roll * RAD2DEG);
        }
    }
    for (int trial = 0; trial < 6; ++trial)
        CHECK(fabsf(means[trial][0] + means[trial][1]) < .00001f);
    CHECK(means[0][1] < means[1][1] && means[1][1] < means[2][1]);
    CHECK(means[3][1] > means[4][1] && means[4][1] > means[5][1]);
    return 0;
}

// Independent potential of unilateral supports, gravity and the passive guard.
// Only valid for unforced, non-planing flat water; imposed tow work is excluded.
static float floating_energy(const Boat *boat)
{
    float value = .5f * boat->vertical_velocity * boat->vertical_velocity
        + boat->pitch_velocity * boat->pitch_velocity
        + .225f * boat->roll_velocity * boat->roll_velocity + 9.81f * boat->height;
    float draft = 9.81f / 64;
    for (int end = -1; end <= 1; ++end)
        for (int side = -1; side <= 1; side += 2) {
            float height = boat->height + side * .65f * sinf(boat->roll) * cosf(boat->pitch)
                + end * 1.4f * sinf(boat->pitch);
            float immersion = fmaxf(0, draft - height);
            value += (64.0f / 12) * (immersion * immersion - draft * draft);
        }
    float excess = fmaxf(0, fabsf(boat->roll) - 25 * DEG2RAD);
    return value + 90 * excess * excess;
}

static int validate_release(void)
{
    const float angles[] = { -32, -10, 10, 32 };
    for (int trial = 0; trial < 4; ++trial) {
        Boat boat;
        boat_reset(&boat);
        boat.roll = angles[trial] * DEG2RAD;
        float previous = floating_energy(&boat), peak_gain = 0;
        int last_unsettled = 0;
        for (int tick = 0; tick < 1200; ++tick) {
            boat_step(&boat, 0, 0);
            float next = floating_energy(&boat);
            peak_gain = fmaxf(peak_gain, next - previous);
            CHECK(next <= previous + .000002f);
            CHECK(boat.turn_roll_load == 0);
            previous = next;
            if (fabsf(boat.roll) >= .5f * DEG2RAD || fabsf(boat.roll_velocity) >= .5f * DEG2RAD)
                last_unsettled = tick + 1;
        }
        CHECK(last_unsettled < 360 && fabsf(previous) < .000002f);
        printf("release initial_deg=%g settle_seconds=%.6f peak_energy_gain=%.9g\n", angles[trial],
            (last_unsettled + 1) * BOAT_STEP, peak_gain);
    }
    return 0;
}

static int validate_blend_flow(void)
{
    float means[3] = { 0 };
    for (int trial = 0; trial < 3; ++trial) {
        Boat still, flow;
        boat_reset(&still);
        flow = still;
        flow.water_settings.current = (Vector2) { 1.7f, -.8f };
        float lo = -4, hi = 4, target = 64.0f / 30;
        CHECK(probe_planing(8, lo, 8.0f / 30, trial * .5f) > target);
        CHECK(probe_planing(8, hi, 8.0f / 30, trial * .5f) < target);
        for (int iteration = 0; iteration < 32; ++iteration) {
            float mid = (lo + hi) / 2;
            if (probe_planing(8, mid, 8.0f / 30, trial * .5f) > target)
                lo = mid;
            else
                hi = mid;
        }
        float slip = (lo + hi) / 2;
        for (int tick = 0; tick < 960; ++tick) {
            prescribe(&still, 8, slip, 8.0f / 30);
            prescribe(&flow, 8, slip, 8.0f / 30);
            still.planing = flow.planing = trial * .5f;
            flow.velocity = Vector2Add(flow.velocity, flow.water_settings.current);
            // Flat, spatially uniform flow: recentering is legitimate external tow work.
            Boat before = still;
            boat_step(&still, 0, 0);
            boat_step(&flow, 0, 0);
            float load = Vector2DotProduct(Vector2Subtract(still.velocity, before.velocity),
                             right(before.heading))
                / BOAT_STEP;
            CHECK(fabsf(load - target) < .0003f);
            CHECK(check_filter(&before, &still, load) == 0);
            CHECK(fabsf(still.roll - flow.roll) < .00001f);
            CHECK(fabsf(still.roll_velocity - flow.roll_velocity) < .00001f);
            CHECK(fabsf(still.turn_roll_load - flow.turn_roll_load) < .00002f);
            if (tick >= 720)
                means[trial] += still.roll / 240;
        }
        printf("blend_external_work planing=%g load=2.133333 mean_roll_deg=%.6f "
               "uniform_flow=matched\n",
            trial * .5f, means[trial] * RAD2DEG);
    }
    CHECK(means[0] > 0 && means[0] < means[1] && means[1] < means[2]);
    // Continuity of the blend: neighboring imposed planing values cannot jump the demand.
    Boat a, b;
    boat_reset(&a);
    prescribe(&a, 8, -.55f, 8.0f / 30);
    a.planing = .5f;
    b = a;
    b.planing += .0001f;
    boat_step(&a, 0, 0);
    boat_step(&b, 0, 0);
    CHECK(fabsf(a.turn_roll_load - b.turn_roll_load) < .0001f);
    const float speeds[] = { 0, 4, 12, 18 };
    for (int si = 0; si < 4; ++si) {
        boat_reset(&a);
        for (int tick = 0; tick < 960; ++tick) {
            prescribe(&a, speeds[si], 0, 0);
            boat_step(&a, 0, 0);
            CHECK(fabsf(a.turn_roll_load) < .000001f && fabsf(a.roll) < .000001f);
        }
    }
    // Docking: prescribed 1 unit/s with a substantial .4 rad/s yaw and .2 slip.
    boat_reset(&a);
    float docking_peak = 0;
    for (int tick = 0; tick < 960; ++tick) {
        prescribe(&a, 1, .2f, .4f);
        boat_step(&a, 0, 0);
        docking_peak = fmaxf(docking_peak, fabsf(a.roll));
    }
    CHECK(docking_peak < 2 * DEG2RAD);
    printf("docking_external_work u=1 yaw=.4 slip=.2 peak_roll_deg=%.6f\n", docking_peak * RAD2DEG);
    return 0;
}

static int validate_transitions(FILE *report)
{
    Boat a, b;
    boat_reset(&a);
    a.position = (Vector2) { 0, 40 };
    a.velocity.y = -12;
    a.throttle = 1;
    b = a;
    bool crossed = false;
    int crossing_tick = -1;
    float peak = 0;
    for (int tick = 0; tick < 960; ++tick) {
        float wheel = tick < 480 ? 1 : -1;
        Boat before = a;
        CHECK(open_water(&a) && open_water(&b));
        boat_step(&a, wheel, 0);
        boat_step(&b, -wheel, 0);
        CHECK(open_water(&a) && open_water(&b));
        CHECK(fabsf(a.roll + b.roll) < .00002f);
        CHECK(fabsf(a.roll_velocity + b.roll_velocity) < .00005f);
        CHECK(fabsf(a.height - b.height) < .00001f && fabsf(a.pitch - b.pitch) < .00001f);
        CHECK(fabsf(Vector2Length(a.velocity) - Vector2Length(b.velocity)) < .00005f);
        CHECK(fabsf(a.roll - before.roll - a.roll_velocity * BOAT_STEP) < .000001f);
        if (tick == 480)
            CHECK(a.roll > DEG2RAD); // Existing bank survives opposite wheel command.
        if (tick >= 480 && !crossed && a.roll < 0) {
            crossed = true;
            crossing_tick = tick;
        }
        peak = fmaxf(peak, fabsf(a.roll));
        float load
            = Vector2DotProduct(Vector2Subtract(a.velocity, before.velocity), right(before.heading))
            / BOAT_STEP;
        row(report, "mirror_transition", tick, &before, load, 0, 0);
    }
    CHECK(crossed && crossing_tick > 481 && a.roll < -DEG2RAD && a.yaw_velocity < 0);
    printf("countersteer roll_zero_delay_seconds=%.6f peak_roll_deg=%.6f mirror=ok\n",
        (crossing_tick - 480 + 1) * BOAT_STEP, peak * RAD2DEG);
    // Release from a driven turn, with residual load preserved; this is not a passive
    // floating-energy test. Horizontal kinetic energy must still decrease in calm water.
    a.throttle = 0;
    float release_peak = 0;
    for (int tick = 0; tick < 2400; ++tick) {
        Boat before = a;
        CHECK(open_water(&a));
        boat_step(&a, 0, 0);
        CHECK(open_water(&a));
        CHECK(energy(&a) <= energy(&before) + .00002f);
        float load
            = Vector2DotProduct(Vector2Subtract(a.velocity, before.velocity), right(before.heading))
            / BOAT_STEP;
        CHECK(check_filter(&before, &a, load) == 0);
        release_peak = fmaxf(release_peak, fabsf(a.roll));
    }
    CHECK(fabsf(a.roll) < .5f * DEG2RAD && fabsf(a.roll_velocity) < .5f * DEG2RAD);
    CHECK(fabsf(a.turn_roll_load) < .0001f);
    printf("driven_release peak_roll_deg=%.6f settled_after_seconds=20\n", release_peak * RAD2DEG);
    return 0;
}

static bool finite_state(const Boat *boat)
{
    return isfinite(boat->height) && isfinite(boat->vertical_velocity) && isfinite(boat->pitch)
        && isfinite(boat->pitch_velocity) && isfinite(boat->roll) && isfinite(boat->roll_velocity)
        && isfinite(boat->turn_roll_load) && isfinite(boat->heading) && isfinite(boat->yaw_velocity)
        && isfinite(boat->position.x) && isfinite(boat->position.y) && isfinite(boat->velocity.x)
        && isfinite(boat->velocity.y);
}

static int validate_wave_turns(FILE *report)
{
    const char *names[] = { "beam", "head", "following", "scripted_wake", "own_wake", "calm_long" };
    for (int trial = 0; trial < 6; ++trial) {
        Boat boat;
        boat_reset(&boat);
        boat.position = (Vector2) { 0, 0 };
        boat.velocity.y = -8;
        boat.throttle = .55f;
        if (trial < 3) {
            boat.water_settings = boat_water_gentle();
            boat.heading = atan2f(.6f, .8f) + (trial == 0 ? PI / 2 : trial == 1 ? 0 : PI);
            boat.velocity = Vector2Scale(forward(boat.heading), 8);
        } else if (trial == 3) {
            boat.water_settings = boat_water_test(false, false, true);
            boat.position.y = 1.6f;
        } else if (trial == 4)
            boat.water_settings.player_wake_enabled = true;
        Boat repeat = boat;
        float peak = 0, peak_rate = 0, peak_load = 0, sum = 0, late_peak = 0, early_peak = 0;
        float peak_height = 0, peak_pitch = 0;
        for (int tick = 0; tick < 14400; ++tick) {
            // Free constant-wheel circles; no imposed positions in spatial wave fields.
            float wheel = .55f;
            Boat before = boat;
            boat_step(&boat, wheel, 0);
            boat_step(&repeat, wheel, 0);
            CHECK(same_state(&boat, &repeat) && finite_state(&boat));
            CHECK(open_water(&boat));
            CHECK(fabsf(boat.roll) < 30 * DEG2RAD);
            CHECK(fabsf(boat.height) < 1 && fabsf(boat.pitch) < .65f);
            peak_height = fmaxf(peak_height, fabsf(boat.height));
            peak_pitch = fmaxf(peak_pitch, fabsf(boat.pitch));
            peak = fmaxf(peak, fabsf(boat.roll));
            peak_rate = fmaxf(peak_rate, fabsf(boat.roll_velocity));
            peak_load = fmaxf(peak_load, fabsf(boat.turn_roll_load));
            sum += boat.roll;
            if (tick >= 7200)
                late_peak = fmaxf(late_peak, fabsf(boat.roll));
            else
                early_peak = fmaxf(early_peak, fabsf(boat.roll));
            row(report, names[trial], tick, &before, NAN, 0, NAN);
        }
        CHECK(late_peak <= early_peak + DEG2RAD);
        printf("standard_turn=%s seconds=120 mean_roll_deg=%.6f peak_roll_deg=%.6f "
               "late_peak_deg=%.6f peak_rate_deg_s=%.6f peak_filtered_load=%.6f peak_height=%.6f "
               "peak_pitch_deg=%.6f\n",
            names[trial], sum / 14400 * RAD2DEG, peak * RAD2DEG, late_peak * RAD2DEG,
            peak_rate * RAD2DEG, peak_load, peak_height, peak_pitch * RAD2DEG);
        // Remove external forcing, preserve momentum/filter and recover without a reset.
        boat.water_settings = (BoatWater) { 0 };
        boat.throttle = 0;
        for (int tick = 0; tick < 14400; ++tick) {
            boat_step(&boat, 0, 0);
            CHECK(finite_state(&boat) && fabsf(boat.roll) < 30 * DEG2RAD);
        }
        CHECK(fabsf(boat.roll) < .0001f && fabsf(boat.roll_velocity) < .0001f);
        Vector2 rest = boat.position;
        for (int tick = 0; tick < 14400; ++tick)
            boat_step(&boat, 0, 0);
        printf("rest_drift=%g speed=%g\n", Vector2Distance(rest, boat.position),
            Vector2Length(boat.velocity));
        CHECK(Vector2Distance(rest, boat.position) < .001f && fabsf(boat.roll) < .000001f);
    }
    return 0;
}

static bool footprint_clear(const Boat *boat)
{
    const Vector2 samples[]
        = { { 0, 2 }, { -.75f, 2 }, { .75f, 2 }, { 0, -1.6f }, { -.75f, -1.6f }, { .75f, -1.6f } };
    for (size_t i = 0; i < sizeof(samples) / sizeof(*samples); ++i) {
        Vector2 p = Vector2Add(boat->position,
            Vector2Add(Vector2Scale(right(boat->heading), samples[i].x),
                Vector2Scale(forward(boat->heading), samples[i].y)));
        if (boat->eriksberg) {
            if (!boat_map_water(p, .35f) || waterfront_harbor_contact(p, .35f, NULL))
                return false;
        } else if (fabsf(p.x) > BOAT_BASIN_HALF_X - .35f || fabsf(p.y) > BOAT_BASIN_HALF_Z - .35f)
            return false;
    }
    WaterfrontTrafficContact contact;
    return !boat->eriksberg
        || !waterfront_traffic_contact(
            boat->position, boat->heading, 2, 1.6f, .75f, boat->time, &contact);
}

static int validate_long_transitions(void)
{
    Boat boat;
    boat_reset(&boat);
    boat.water_settings = boat_water_gentle();
    Boat repeat = boat;
    float peak = 0, peak_rate = 0;
    for (int tick = 0; tick < 14400; ++tick) {
        float wheel = sinf(tick * .005f), throttle_change = sinf(tick * .001f);
        boat_step(&boat, wheel, throttle_change);
        boat_step(&repeat, wheel, throttle_change);
        CHECK(same_state(&boat, &repeat) && finite_state(&boat));
        CHECK(footprint_clear(&boat));
        CHECK(fabsf(boat.roll) < 30 * DEG2RAD);
        peak = fmaxf(peak, fabsf(boat.roll));
        peak_rate = fmaxf(peak_rate, fabsf(boat.roll_velocity));
    }
    printf("long_transitions seconds=120 peak_roll_deg=%.6f peak_rate_deg_s=%.6f repeat=exact\n",
        peak * RAD2DEG, peak_rate * RAD2DEG);
    return 0;
}

static int validate_contacts(void)
{
    for (int trial = 0; trial < 8; ++trial) {
        Boat boat;
        boat_reset_location(&boat, trial >= 2);
        boat.water_settings = (BoatWater) { 0 };
        if (trial < 2) {
            boat.position = (Vector2) { 116, trial == 0 ? 0 : 76 };
            boat.velocity = (Vector2) { 8, trial == 0 ? 0 : 8 };
            boat.heading = .2f;
        } else if (trial == 2 || trial == 3) {
            boat.position = trial == 2 ? (Vector2) { -34, 35 } : (Vector2) { 0, 182 };
            boat.heading = trial == 2 ? -PI / 2 : PI / 2;
            boat.velocity = Vector2Scale(forward(boat.heading), 8);
        } else {
            int vessel = trial - 4;
            WaterfrontVesselMotion ship = waterfront_traffic_motion(vessel, 40);
            BoatModelSpec spec = waterfront_traffic_spec(vessel);
            boat.time = 40;
            boat.position = Vector2Add((Vector2) { -ship.position.x, ship.position.z },
                Vector2Scale(right(ship.heading), spec.beam * .5f + 3));
            boat.heading = ship.heading - PI / 2;
            boat.velocity = Vector2Scale(forward(boat.heading), 8);
        }
        int impacts = 0;
        float peak = 0, max_load_error = 0;
        for (int tick = 0; tick < 1200; ++tick) {
            // Same PRE-contact hydrodynamics at a safe basin origin. Flat water has no
            // spatial forcing; no impact delta may enter the filter or physical roll.
            Boat probe_boat = boat;
            probe_boat.eriksberg = false;
            probe_boat.position = (Vector2) { 0 };
            boat_step(&probe_boat, 0, 0);
            boat_step(&boat, 0, 0);
            float error = fabsf(boat.turn_roll_load - probe_boat.turn_roll_load);
            max_load_error = fmaxf(max_load_error, error);
            CHECK(error < .000001f);
            CHECK(fabsf(boat.roll - probe_boat.roll) < .000001f
                && fabsf(boat.roll_velocity - probe_boat.roll_velocity) < .000001f);
            if (Vector2Distance(boat.velocity, probe_boat.velocity) > .001f)
                ++impacts;
            CHECK(footprint_clear(&boat) && finite_state(&boat));
            peak = fmaxf(peak, fabsf(boat.roll));
            CHECK(peak < 30 * DEG2RAD);
        }
        CHECK(impacts > 0);
        CHECK(fabsf(boat.roll) < .5f * DEG2RAD);
        printf("contact=%d impact_ticks=%d peak_roll_deg=%.6f max_filter_error=%.9g\n", trial,
            impacts, peak * RAD2DEG, max_load_error);
    }
    boat_map_destroy();
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
        || validate_free(report) || validate_tow(report) || validate_release()
        || validate_blend_flow() || validate_transitions(report) || validate_wave_turns(report)
        || validate_long_transitions() || validate_contacts();
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
        puts("boat_banking=ok (T3 dynamics/environment regression)");
    return result;
}
