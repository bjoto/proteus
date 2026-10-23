#include "boat.h"
#include "boat_map.h"
#include "raymath.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Boat check failed at line %d: %s\n", __LINE__, #condition);           \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int validate_flight_outcomes(void)
{
    // Both axes can invert in flight without magical righting or immediate failure.
    for (int axis = 0; axis < 2; ++axis) {
        Boat boat;
        boat_reset(&boat);
        boat.height = 20;
        if (axis == 0)
            boat.pitch = PI;
        else
            boat.roll = 80 * DEG2RAD;
        float angle = axis == 0 ? boat.pitch : boat.roll;
        for (int tick = 0; tick < 60; ++tick)
            boat_step(&boat, 0, 0);
        CHECK(boat.motion_state == BOAT_AIRBORNE);
        CHECK(fabsf((axis == 0 ? boat.pitch : boat.roll) - angle) < .00001f);
        CHECK(boat.inverted_contact_time == 0);
    }
    Boat flying;
    boat_reset(&flying);
    flying.height = 20;
    flying.velocity = (Vector2) { 3, -7 };
    flying.throttle = flying.engine_response = flying.wheel = 1;
    for (int tick = 0; tick < 60; ++tick)
        boat_step(&flying, 1, 0);
    CHECK(flying.velocity.x == 3 && flying.velocity.y == -7);
    CHECK(flying.heading > 0 && flying.heading < .05f);
    CHECK(flying.roll == 0 && flying.pitch == 0);
    CHECK(fabsf(flying.height - (20 - .5f * 9.81f * .5f * (.5f + BOAT_STEP))) < .001f);
    // Inversion is an orientation, not an Euler-angle range. Test odd/even full turns.
    for (int axis = 0; axis < 2; ++axis) {
        for (int turns = -3; turns <= 3; ++turns) {
            Boat boat;
            boat_reset(&boat);
            float angle = turns * PI;
            if (axis == 0)
                boat.pitch = angle;
            else
                boat.roll = angle;
            for (int tick = 0; tick < 120; ++tick)
                boat_step(&boat, 0, 1);
            CHECK((boat.motion_state == BOAT_CAPSIZED) == (abs(turns) % 2 == 1));
            if (boat.motion_state == BOAT_CAPSIZED) {
                CHECK(boat.throttle == 0 && boat.engine_response == 0);
                boat_reset(&boat);
                CHECK(boat.motion_state == BOAT_WATERBORNE);
                CHECK(boat.inverted_contact_time == 0 && boat.landings == 0);
            }
        }
    }
    Boat boat;
    boat_reset(&boat);
    boat.height = 2;
    bool landing = false;
    for (int tick = 0; tick < 600; ++tick) {
        boat_step(&boat, 0, 0);
        landing |= boat.motion_state == BOAT_LANDING;
    }
    CHECK(landing && boat.landings == 1 && boat.motion_state == BOAT_WATERBORNE);
    CHECK(fabsf(boat.height) < .001f);
    return 0;
}

static int validate_dynamic_pair(void)
{
    Boat a, b;
    boat_reset(&a);
    boat_reset(&b);
    a.position = (Vector2) { -.7f, 0 };
    b.position = (Vector2) { .7f, 0 };
    a.velocity.x = 6;
    Vector2 initial_a = a.position, initial_b = b.position;
    b.height = 10;
    CHECK(!boat_collide_pair(&a, 1, &b, 1.6f));
    CHECK(a.velocity.x == 6 && b.velocity.x == 0);
    b.height = 0;
    CHECK(boat_collide_pair(&a, 1, &b, 1.6f));
    CHECK(b.velocity.x > 0 && a.velocity.x < 6);
    CHECK(fabsf(a.velocity.x + 1.6f * b.velocity.x - 6) < .0001f);
    CHECK(.5f * a.velocity.x * a.velocity.x + .8f * b.velocity.x * b.velocity.x < 18);
    CHECK(Vector2Distance(a.position, initial_a) > Vector2Distance(b.position, initial_b));
    CHECK(!boat_collide_pair(&a, 1, &b, 1.6f));
    // Offset impact transfers angular momentum as well as linear momentum.
    a.yaw_velocity = b.yaw_velocity = 0;
    a.position = (Vector2) { -.7f, 1 };
    b.position = (Vector2) { .7f, 0 };
    a.velocity = (Vector2) { 6, 0 };
    b.velocity = (Vector2) { 0 };
    CHECK(boat_collide_pair(&a, 1, &b, 1.6f));
    CHECK(fabsf(a.yaw_velocity) > .1f && fabsf(b.yaw_velocity) > .1f);
    return 0;
}

static int particles(const Boat *boat, bool spray)
{
    int count = 0;
    for (int i = 0; i < BOAT_WATER_PARTICLES; ++i)
        if (boat->water[i].lifetime > 0 && boat->water[i].spray == spray)
            ++count;
    return count;
}

// Stern variation must not change the accepted bow spray or any handling state.
static int validate_wake_independence(void)
{
    Boat a, b;
    boat_reset(&a);
    boat_reset(&b);
    b.wake_emission = 931;
    for (int tick = 0; tick < 720; ++tick) {
        a.throttle = b.throttle = tick < 480 ? 1 : -1;
        boat_step(&a, tick > 360 ? .4f : 0, 0);
        boat_step(&b, tick > 360 ? .4f : 0, 0);
        CHECK(a.position.x == b.position.x && a.position.y == b.position.y);
        CHECK(a.heading == b.heading && a.emission == b.emission
            && a.turn_roll_load == b.turn_roll_load);
        CHECK(particles(&a, true) == particles(&b, true));
        for (int i = 0; i < BOAT_WATER_PARTICLES; ++i) {
            const BoatWaterParticle *p = &a.water[i];
            if (!p->spray || p->lifetime <= 0)
                continue;
            bool found = false;
            for (int j = 0; j < BOAT_WATER_PARTICLES; ++j) {
                const BoatWaterParticle *q = &b.water[j];
                if (q->spray && q->lifetime > 0 && p->age == q->age
                    && p->position.x == q->position.x && p->position.y == q->position.y
                    && p->height == q->height && p->size == q->size && p->lift == q->lift) {
                    found = true;
                    break;
                }
            }
            CHECK(found);
        }
    }
    // Even a saturated pool must not let a foam birth overwrite live airborne spray.
    boat_reset(&a);
    a.throttle = a.engine_response = 1;
    a.propeller_time = .075f;
    for (int i = 0; i < BOAT_WATER_PARTICLES; ++i)
        a.water[i] = (BoatWaterParticle) { .height = 100, .lifetime = 100, .spray = true };
    boat_step(&a, 0, 0);
    CHECK(particles(&a, true) == BOAT_WATER_PARTICLES);
    CHECK(particles(&a, false) == 0);
    return 0;
}

static float kinetic_energy(const Boat *boat)
{
    // Normalized mass 1 and yaw inertia 2.2; total energy includes rotation.
    return 0.5f * (boat->velocity.x * boat->velocity.x + boat->velocity.y * boat->velocity.y)
        + 1.1f * boat->yaw_velocity * boat->yaw_velocity;
}

static int validate_countersteering(void)
{
    for (int direction = -1; direction <= 1; direction += 2) {
        for (int coast = 0; coast <= 1; ++coast) {
            Boat boat = { .velocity = { 0, -12 },
                .throttle = coast ? 0 : 1,
                .engine_response = coast ? 0 : 1,
                .planing = 1 };
            // Establish a powered or unpowered turn, then ask to catch it.
            for (int tick = 0; tick < 120; ++tick)
                boat_step(&boat, direction, 0);
            CHECK(fabsf(boat.wheel - direction) < .00001f && boat.yaw_velocity * direction > 0);
            float heading = boat.heading;
            float overshoot = 0;
            int reversed = 0;
            for (int tick = 1; tick <= 120; ++tick) {
                float energy = kinetic_energy(&boat);
                boat_step(&boat, -direction, 0);
                if (coast)
                    CHECK(kinetic_energy(&boat) <= energy + .00001f);
                if (tick == 42)
                    CHECK(boat.wheel * direction < 0 && boat.engine_angle * direction <= 0);
                if (!reversed && boat.yaw_velocity * direction <= 0) {
                    reversed = tick;
                    overshoot = direction * remainderf(boat.heading - heading, 2 * PI);
                }
            }
            // Baseline needed ~1.0s powered / 1.13s coasting and overshot by 40 / 16 degrees.
            CHECK(reversed > 0 && reversed <= 84);
            CHECK(overshoot > 0 && overshoot < (coast ? 10 : 24) * DEG2RAD);
        }
    }
    // Tiny corrections that cross centre use the ordinary rate for the rest of that tick.
    Boat small = { .wheel = .001f };
    boat_step(&small, -1, 0);
    CHECK(small.wheel < 0 && small.wheel >= -BOAT_STEP);
    Boat partial = { .wheel = 1 }, clamped = partial, full = partial;
    for (int tick = 0; tick < 40; ++tick) {
        boat_step(&partial, -.5f, 0);
        boat_step(&clamped, -8, 0);
        boat_step(&full, -1, 0);
    }
    CHECK(fabsf(partial.wheel - .5f) < .0001f);
    CHECK(fabsf(full.wheel) < .0001f);
    CHECK(memcmp(&clamped, &full, sizeof(Boat)) == 0);
    return 0;
}

static int validate_speed_view(void)
{
    Boat boat;
    boat_reset(&boat);
    WaterfrontSceneFrame frame;
    float previous = 55 * DEG2RAD;
    for (int step = 0; step <= 240; ++step) {
        boat.velocity.y = -(float)step * .1f;
        boat_scene_frame(&boat, BOAT_CAMERA_DEFAULT, &frame);
        float fov = frame.camera.vertical_fov;
        CHECK(fov >= previous && fov <= 62 * DEG2RAD + .000001f);
        CHECK(fov - previous < .08f * DEG2RAD);
        if (step <= 40)
            CHECK(fov == 55 * DEG2RAD);
        if (step >= 180)
            CHECK(fabsf(fov - 62 * DEG2RAD) < .000001f);
        CHECK(frame.camera.up.x == 0 && frame.camera.up.y == 1 && frame.camera.up.z == 0);
        CHECK(frame.camera.position.y == BOAT_CAMERA_DEFAULT.height);
        previous = fov;
    }
    BoatCamera fixed = BOAT_CAMERA_DEFAULT;
    fixed.fixed_fov = true;
    boat_scene_frame(&boat, fixed, &frame);
    CHECK(frame.camera.vertical_fov == 55 * DEG2RAD);
    // Reverse, sideways drift and rest do not open the lens; it uses ground travel, not flow.
    boat.velocity = (Vector2) { 0, 20 };
    boat_scene_frame(&boat, BOAT_CAMERA_DEFAULT, &frame);
    CHECK(frame.camera.vertical_fov == 55 * DEG2RAD);
    boat.velocity = (Vector2) { 20, 0 };
    boat_scene_frame(&boat, BOAT_CAMERA_DEFAULT, &frame);
    CHECK(frame.camera.vertical_fov == 55 * DEG2RAD);
    boat_reset(&boat);
    boat.water_settings.current = (Vector2) { 0, -20 };
    boat_scene_frame(&boat, BOAT_CAMERA_DEFAULT, &frame);
    CHECK(frame.camera.vertical_fov == 55 * DEG2RAD);
    return 0;
}

static int validate_coasting(void)
{
    Boat fast = { .velocity = { 0, -10 }, .wheel = 0.6f, .engine_angle = 0.33f };
    Boat slow = { .velocity = { 0, -2 }, .wheel = 0.6f, .engine_angle = 0.33f };
    Boat left = { .velocity = { 0, -10 }, .wheel = -0.6f, .engine_angle = -0.33f };
    Boat straight = { .velocity = { 0, -10 } };
    float energy = kinetic_energy(&fast);
    boat_step(&fast, 0, 0);
    boat_step(&slow, 0, 0);
    boat_step(&left, 0, 0);
    boat_step(&straight, 0, 0);
    CHECK(fast.throttle == 0 && fast.yaw_velocity > 0);
    CHECK(fast.yaw_velocity > slow.yaw_velocity * 10);
    CHECK(fabsf(left.yaw_velocity + fast.yaw_velocity) < 0.0001f);
    CHECK(straight.yaw_velocity == 0 && straight.velocity.x == 0);
    CHECK(kinetic_energy(&fast) < energy);
    // A sustained unpowered turn changes heading while always losing total kinetic energy.
    for (int i = 0; i < 120; ++i) {
        energy = kinetic_energy(&fast);
        boat_step(&fast, 0, 0);
        CHECK(kinetic_energy(&fast) <= energy + 0.00001f);
    }
    CHECK(fast.heading > 0.1f);
    CHECK(fabsf(fast.position.x) < BOAT_LIMIT_X && fabsf(fast.position.y) < BOAT_LIMIT_Z);

    // Broad drift, backwards motion, and spin must not turn passive resistance into propulsion.
    for (int direction = -1; direction <= 1; direction += 2) {
        for (int wheel = -1; wheel <= 1; ++wheel) {
            Boat drift = { .velocity = { 7, direction * 8 },
                .yaw_velocity = direction * 2,
                .wheel = wheel,
                .engine_angle = wheel * 0.55f };
            for (int i = 0; i < 600; ++i) {
                energy = kinetic_energy(&drift);
                boat_step(&drift, 0, 0);
                CHECK(kinetic_energy(&drift) <= energy + 0.00001f);
            }
        }
    }
    // Straight-line drag grows more than linearly with speed.
    fast = (Boat) { .velocity = { 0, -10 } };
    slow = (Boat) { .velocity = { 0, -2 } };
    boat_step(&fast, 0, 0);
    boat_step(&slow, 0, 0);
    CHECK((10 + fast.velocity.y) / 10 > (2 + slow.velocity.y) / 2);
    return 0;
}

static int validate_signed_propulsion(void)
{
    Boat forward, reverse, forward_turn, reverse_turn;
    boat_reset(&forward);
    boat_reset(&reverse);
    forward.throttle = 1;
    reverse.throttle = -1;
    for (int i = 0; i < 240; ++i) {
        boat_step(&forward, 0, 0);
        boat_step(&reverse, 0, 0);
    }
    CHECK(forward.engine_response > .99f && reverse.engine_response < -.99f);
    CHECK(forward.velocity.y < -1 && reverse.velocity.y > 1);

    boat_reset(&forward_turn);
    boat_reset(&reverse_turn);
    forward_turn.throttle = 1;
    reverse_turn.throttle = -1;
    forward_turn.wheel = reverse_turn.wheel = .6f;
    for (int i = 0; i < 240; ++i) {
        boat_step(&forward_turn, 1, 0);
        boat_step(&reverse_turn, 1, 0);
    }
    CHECK(forward_turn.yaw_velocity > 0 && reverse_turn.yaw_velocity < 0);

    Boat saturated = { .throttle = 4 };
    boat_step(&saturated, 0, 0);
    CHECK(saturated.throttle == 1 && saturated.engine_response > 0);
    saturated.throttle = -4;
    boat_step(&saturated, 0, 0);
    CHECK(saturated.throttle == -1 && saturated.engine_response <= 0);

    Boat deterministic_a = { .throttle = -.8f };
    Boat deterministic_b = deterministic_a;
    for (int i = 0; i < 720; ++i) {
        boat_step(&deterministic_a, 0, 0);
        boat_step(&deterministic_b, 0, 0);
    }
    CHECK(memcmp(&deterministic_a, &deterministic_b, sizeof(Boat)) == 0);

    Boat sustained = { .throttle = -1 };
    for (int i = 0; i < 120 * 120; ++i) {
        boat_step(&sustained, 0, 0);
        CHECK(isfinite(sustained.engine_response));
        CHECK(isfinite(sustained.velocity.x) && isfinite(sustained.velocity.y));
        CHECK(sustained.engine_response >= -1 && sustained.engine_response <= 1);
        CHECK(hypotf(sustained.velocity.x, sustained.velocity.y) < 21);
    }
    return 0;
}

static int validate_signed_throttle(void)
{
    Boat boat;
    boat_reset(&boat);
    for (int i = 0; i < 120; ++i)
        boat_step(&boat, 0, 1);
    CHECK(boat.throttle > .49f);
    for (int i = 0; i < 240; ++i)
        boat_step(&boat, 0, -1);
    CHECK(boat.throttle < -.49f && boat.engine_response < 0);
    for (int i = 0; i < 240; ++i)
        boat_step(&boat, 0, -1);
    CHECK(boat.throttle == -1);
    for (int i = 0; i < 1200; ++i)
        boat_step(&boat, 0, 1);
    CHECK(boat.throttle == 1);
    boat_reset(&boat);
    CHECK(fabsf(boat.throttle) < .001f);
    return 0;
}

static int validate_planing(void)
{
    Boat boat;
    boat_reset(&boat);
    boat.velocity.y = -7;
    boat.throttle = 1;
    boat.engine_response = 1;
    float previous_speed = 7;
    float previous_acceleration = 0;
    for (int i = 0; i < 300; ++i) {
        boat_step(&boat, 0, 0);
        float speed = hypotf(boat.velocity.x, boat.velocity.y);
        float acceleration = speed - previous_speed;
        // The rate-limited blend cannot produce a gear-shift-like velocity jump.
        if (i > 0)
            CHECK(fabsf(acceleration - previous_acceleration) < .003f);
        CHECK(boat.planing >= 0 && boat.planing <= 1);
        previous_speed = speed;
        previous_acceleration = acceleration;
    }
    CHECK(boat.planing > .9f);

    // With the engine at neutral, planing drains away and cannot create kinetic energy.
    boat.throttle = 0;
    while (boat.engine_response > 0)
        boat_step(&boat, 0, 0);
    for (int i = 0; i < 1200; ++i) {
        float energy = kinetic_energy(&boat);
        boat_step(&boat, 0, 0);
        CHECK(kinetic_energy(&boat) <= energy + .00001f);
    }
    CHECK(boat.planing < .01f);

    boat_reset(&boat);
    boat.velocity.y = 9;
    boat.throttle = -1;
    for (int i = 0; i < 480; ++i) {
        boat_step(&boat, 0, 0);
        CHECK(boat.planing == 0);
    }
    return 0;
}

static int validate_planing_attitude(void)
{
    Boat boat;
    boat_reset(&boat);
    boat.throttle = 1;
    float peak = 0;
    for (int tick = 0; tick < 360; ++tick) {
        float previous = boat.pitch;
        boat_step(&boat, 0, 0);
        CHECK(isfinite(boat.pitch));
        CHECK(fabsf(boat.pitch - previous) < .008f);
        peak = fmaxf(peak, boat.pitch);
    }
    CHECK(peak > 5 * DEG2RAD);
    CHECK(boat.pitch > 2 * DEG2RAD && boat.pitch < 4 * DEG2RAD);
    CHECK(particles(&boat, true) > 0);

    // Validate rigid attachments and local bow rise across headings, including Euler singularities.
    WaterfrontSceneFrame frame;
    for (int angle = 0; angle < 16; ++angle) {
        boat.heading = angle * PI / 8;
        boat.engine_angle = .4f;
        for (int attitude = 0; attitude < 2; ++attitude) {
            boat.pitch = attitude * .15f;
            boat_scene_frame(&boat, BOAT_CAMERA_DEFAULT, &frame);
            Matrix hull = MatrixRotateXYZ(frame.player.hull.rotation);
            Matrix expected
                = MatrixMultiply(MatrixRotateX(boat.pitch), MatrixRotateY(boat.heading));
            Vector3 bow = Vector3Transform((Vector3) { 0, 0, -1 }, hull);
            CHECK(fabsf(bow.y - sinf(boat.pitch)) < .0001f);
            Vector3 cabin = Vector3Add(
                frame.player.hull.position, Vector3Transform((Vector3) { 0, 1.1f, 0 }, expected));
            CHECK(Vector3Distance(cabin, frame.player.cabin.position) < .0001f);
            Vector3 stern = Vector3Add(
                frame.player.hull.position, Vector3Transform((Vector3) { 0, .4f, 1.6f }, expected));
            CHECK(Vector3Distance(stern, frame.player.engine.position) < .0001f);
            Matrix engine = MatrixRotateXYZ(frame.player.engine.rotation);
            Matrix expected_engine = MatrixMultiply(MatrixRotateY(-boat.engine_angle), expected);
            for (int axis = 0; axis < 3; ++axis) {
                Vector3 basis = { axis == 0, axis == 1, axis == 2 };
                CHECK(Vector3Distance(
                          Vector3Transform(basis, engine), Vector3Transform(basis, expected_engine))
                    < .0001f);
            }
            CHECK(frame.camera.up.x == 0 && frame.camera.up.y == 1 && frame.camera.up.z == 0);
            CHECK(frame.camera.position.y == BOAT_CAMERA_DEFAULT.height);
        }
    }
    // Presentation state must never feed back into horizontal handling or the camera.
    Boat level = boat;
    level.pitch = 0;
    boat_step(&boat, 0, 0);
    boat_step(&level, 0, 0);
    CHECK(Vector2Distance(boat.velocity, level.velocity) == 0);
    CHECK(boat.heading == level.heading && boat.camera_heading == level.camera_heading);
    boat.throttle = 0;
    for (int tick = 0; tick < 2400; ++tick) {
        float previous = boat.pitch;
        boat_step(&boat, 0, 0);
        CHECK(fabsf(boat.pitch - previous) < .008f);
    }
    CHECK(boat.pitch < .0001f && boat.planing == 0);
    boat_reset(&boat);
    CHECK(boat.pitch == 0);
    return 0;
}

static bool footprint_in_water(const Boat *boat)
{
    const Vector2 samples[]
        = { { 0, 2 }, { -.75f, 2 }, { .75f, 2 }, { 0, -1.6f }, { -.75f, -1.6f }, { .75f, -1.6f } };
    for (size_t i = 0; i < sizeof(samples) / sizeof(*samples); ++i) {
        Vector2 p = { boat->position.x
                + (sinf(boat->heading) * samples[i].y + cosf(boat->heading) * samples[i].x),
            boat->position.y
                + (-cosf(boat->heading) * samples[i].y + sinf(boat->heading) * samples[i].x) };
        if (boat->eriksberg
                ? !boat_map_water(p, .35f)
                : fabsf(p.x) > BOAT_BASIN_HALF_X - .35f || fabsf(p.y) > BOAT_BASIN_HALF_Z - .35f)
            return false;
    }
    return true;
}

// Contact trajectories diverge with float rounding; exercise nearby headings on every platform.
static int validate_basin_stress(float heading)
{
    Boat boat = (Boat) { .position = { 116, 76 }, .velocity = { 8, 8 }, .heading = heading };
    bool corner_contact = false;
    for (int i = 0; i < 120; ++i) {
        boat_step(&boat, 0, 0);
        CHECK(boat.position.x < BOAT_BASIN_HALF_X && boat.position.y < BOAT_BASIN_HALF_Z);
        if (boat.velocity.x < 0 || boat.velocity.y < 0) {
            corner_contact = true;
            break;
        }
    }
    CHECK(corner_contact);
    for (int i = 0; i < 120 * 120; ++i) {
        boat_step(&boat, sinf(i * 0.003f), 1);
        CHECK(isfinite(boat.heading) && isfinite(boat.velocity.x) && isfinite(boat.velocity.y));
        CHECK(isfinite(boat.yaw_velocity));
        // Wall impulses can exchange translation for rotation. Bound their combined energy
        // using the old speed/yaw budget rather than imposing a free-turn yaw limit on impacts.
        float energy_limit = .5f * 23 * 23 + 1.1f * 4 * 4;
        if (!(kinetic_energy(&boat) < energy_limit))
            fprintf(stderr, "Basin stress: heading=%g tick=%d yaw=%g speed=%g energy=%g\n", heading,
                i, boat.yaw_velocity, hypotf(boat.velocity.x, boat.velocity.y),
                kinetic_energy(&boat));
        CHECK(kinetic_energy(&boat) < energy_limit);
        CHECK(isfinite(boat.camera_heading));
        CHECK(fabsf(boat.position.x) < BOAT_BASIN_HALF_X);
        CHECK(fabsf(boat.position.y) < BOAT_BASIN_HALF_Z);
        CHECK(hypotf(boat.velocity.x, boat.velocity.y) < 23);
    }
    return 0;
}

static int validate_hull_contacts(void)
{
    // A starboard-bow glancing impact should rebound, scrub speed, and turn the hull away.
    Boat glancing = { .position = { 118.5f, 10 }, .velocity = { 7, -3 } };
    float energy = kinetic_energy(&glancing);
    bool contacted = false;
    for (int tick = 0; tick < 180; ++tick) {
        boat_step(&glancing, 0, 0);
        CHECK(fabsf(glancing.position.x) < BOAT_BASIN_HALF_X);
        CHECK(fabsf(glancing.position.y) < BOAT_BASIN_HALF_Z);
        if (glancing.velocity.x < 0) {
            contacted = true;
            break;
        }
    }
    CHECK(contacted);
    CHECK(glancing.heading < 0);
    CHECK(kinetic_energy(&glancing) < energy);

    // A faster beam impact legitimately exceeds the old 4 rad/s stress-test limit.
    // It must exchange translation for rotation without creating kinetic energy.
    Boat fast = { .position = { 118.5f, 10 }, .velocity = { 19, -3 } };
    float peak_yaw = 0;
    for (int tick = 0; tick < 180; ++tick) {
        float before = kinetic_energy(&fast);
        boat_step(&fast, 0, 0);
        CHECK(footprint_in_water(&fast));
        CHECK(kinetic_energy(&fast) <= before + .00001f);
        peak_yaw = fmaxf(peak_yaw, fabsf(fast.yaw_velocity));
    }
    CHECK(peak_yaw > 4);

    // A shallow scrape retains useful forward motion while the contact redirects the hull.
    Boat sliding = { .position = { 118.89f, 10 }, .velocity = { .3f, -6 } };
    float sliding_energy = kinetic_energy(&sliding);
    for (int tick = 0; tick < 60; ++tick) {
        boat_step(&sliding, 0, 0);
        CHECK(footprint_in_water(&sliding));
    }
    CHECK(sliding.position.y < 8.5f);
    CHECK(kinetic_energy(&sliding) > sliding_energy * .45f);
    CHECK(kinetic_energy(&sliding) < sliding_energy);

    // Rotation into a wall used to leave an overlapping hull and block every later move.
    Boat rotating = { .position = { 118.89f, 10 }, .yaw_velocity = 1 };
    for (int tick = 0; tick < 120; ++tick) {
        boat_step(&rotating, 0, 0);
        CHECK(footprint_in_water(&rotating));
    }

    // Bow, stern, and both beams stay contained across representative headings.
    for (int heading = 0; heading < 8; ++heading) {
        Boat hull = { .position = { 0, 0 },
            .heading = heading * PI / 4,
            .velocity = { 13 * sinf(heading * PI / 4), -13 * cosf(heading * PI / 4) } };
        for (int tick = 0; tick < 3600; ++tick) {
            boat_step(&hull, 0, 0);
            CHECK(fabsf(hull.position.x) < BOAT_BASIN_HALF_X);
            CHECK(fabsf(hull.position.y) < BOAT_BASIN_HALF_Z);
            CHECK(isfinite(hull.heading) && isfinite(hull.yaw_velocity));
        }
    }
    return 0;
}

static int validate_course(void)
{
    Boat boat;
    boat_reset_course(&boat);
    CHECK(boat.course.enabled && boat.course.next_gate == 0);
    CHECK(boat.position.x == 0 && boat.position.y > boat_course_gates[0].centre.y);
    for (int i = 0; i < 120; ++i)
        boat_step(&boat, 0, 0);
    CHECK(boat.course.elapsed == 0);
    BoatCourse *course = &boat.course;
    // Wrong direction, outside the posts, and later gates cannot start the clock.
    boat_course_step(course, (Vector2) { 0, 34 }, (Vector2) { 0, 36 }, 1);
    boat_course_step(course, (Vector2) { 20, 36 }, (Vector2) { 20, 34 }, 1);
    boat_course_step(course, (Vector2) { 22, 8 }, (Vector2) { 22, 6 }, 1);
    CHECK(course->next_gate == 0 && course->elapsed == 0);
    // Interpolate both the crossing time and lateral position, even for a large step.
    boat_course_step(course, (Vector2) { -20, 36 }, (Vector2) { 20, 34 }, 1);
    CHECK(course->next_gate == 1 && fabs(course->elapsed - .5) < .00001);
    boat_course_step(course, (Vector2) { 0, -52 }, (Vector2) { 0, -54 }, 1);
    CHECK(course->next_gate == 1 && fabs(course->elapsed - 1.5) < .00001);
    boat_course_step(course, (Vector2) { 0, 8 }, (Vector2) { 0, 6 }, 1);
    CHECK(course->next_gate == 1); // Short-cut misses the first bend.
    boat_course_step(course, (Vector2) { 22, 8 }, (Vector2) { 22, 6 }, 1);
    CHECK(course->next_gate == 2);
    boat_course_step(course, (Vector2) { -22, -24 }, (Vector2) { -22, -22 }, 1);
    CHECK(course->next_gate == 2); // Reverse crossing does not advance.
    boat_course_step(course, (Vector2) { -22, -22 }, (Vector2) { -22, -24 }, 1);
    CHECK(course->next_gate == 3);
    double before_finish = course->elapsed;
    boat_course_step(course, (Vector2) { 0, -52 }, (Vector2) { 0, -56 }, 1);
    CHECK(course->next_gate == BOAT_COURSE_GATES);
    CHECK(fabs(course->elapsed - before_finish - .25) < .00001);
    double finished = course->elapsed;
    boat_course_step(course, (Vector2) { 0, 50 }, (Vector2) { 0, -60 }, 10);
    CHECK(course->elapsed == finished);
    boat_reset_course(&boat);
    CHECK(course->elapsed == 0 && course->next_gate == 0);
    // Integration: powered movement starts the timer on the fixed simulation ticks.
    boat.throttle = 1;
    for (int i = 0; i < 360; ++i)
        boat_step(&boat, 0, 0);
    CHECK(course->next_gate == 1 && course->elapsed > 0);
    boat_reset_location(&boat, false);
    CHECK(!course->enabled && course->elapsed == 0);
    return 0;
}

// Potential of the same unilateral springs and gravity, relative to calm-water equilibrium.
static float floating_energy(const Boat *boat)
{
    float energy = .5f * boat->vertical_velocity * boat->vertical_velocity
        + boat->pitch_velocity * boat->pitch_velocity
        + .225f * boat->roll_velocity * boat->roll_velocity + 9.81f * boat->height;
    float draft = 9.81f / 64;
    for (int end = -1; end <= 1; ++end) {
        for (int side = -1; side <= 1; side += 2) {
            float height = boat->height + side * .65f * sinf(boat->roll) * cosf(boat->pitch)
                + end * 1.4f * sinf(boat->pitch);
            float immersion = fmaxf(0, draft - height);
            energy += .5f * (64.0f / 6) * (immersion * immersion - draft * draft);
        }
    }
    return energy;
}

static int validate_buoyancy(void)
{
    Boat boat;
    boat_reset(&boat);
    for (int tick = 0; tick < 1200; ++tick)
        boat_step(&boat, 0, 0);
    CHECK(fabsf(boat.height) < .000001f && fabsf(boat.vertical_velocity) < .000001f);
    CHECK(fabsf(boat.pitch) < .000001f && fabsf(boat.roll) < .000001f);
    // Drop, push under water, and disturb both angular modes with and without initial velocity.
    for (int trial = 0; trial < 4; ++trial) {
        boat_reset(&boat);
        boat.height = trial == 0 ? 1.2f : trial == 1 ? -.35f : .1f;
        boat.pitch = trial % 2 ? -.16f : .12f;
        boat.roll = trial % 2 ? .14f : -.1f;
        boat.vertical_velocity = trial == 2 ? -1 : 0;
        boat.pitch_velocity = trial == 3 ? .4f : 0;
        boat.roll_velocity = trial == 3 ? -.3f : 0;
        float initial_energy = floating_energy(&boat);
        for (int tick = 0; tick < 1200; ++tick) {
            boat_step(&boat, 0, 0);
            CHECK(isfinite(boat.height) && isfinite(boat.pitch) && isfinite(boat.roll));
            CHECK(floating_energy(&boat) < initial_energy * 1.01f + .00001f);
        }
        CHECK(fabsf(boat.height) < .0001f && fabsf(boat.pitch) < .0001f);
        CHECK(fabsf(boat.roll) < .0001f && floating_energy(&boat) < .00001f);
    }
    // A static sloping wave must lift the correct bow and side (simulation X is mirrored).
    boat_reset(&boat);
    boat.position = (Vector2) { 0 };
    boat.water_settings = boat_water_gentle();
    boat.water_settings.angular_speed = 0;
    boat.water_settings.crossing[0] = boat.water_settings.crossing[1] = (BoatWave) { 0 };
    for (int tick = 0; tick < 720; ++tick)
        boat_step(&boat, 0, 0);
    CHECK(boat.pitch > .01f && boat.roll < -.01f);
    // The full rigid pose must carry every attachment through heave and roll as well as pitch.
    for (int angle = 0; angle < 16; ++angle) {
        boat.heading = angle * PI / 8;
        boat.height = .25f;
        boat.pitch = .12f;
        boat.roll = -.09f;
        boat.engine_angle = .3f;
        WaterfrontSceneFrame frame;
        boat_scene_frame(&boat, BOAT_CAMERA_DEFAULT, &frame);
        Matrix expected
            = MatrixMultiply(MatrixMultiply(MatrixRotateZ(boat.roll), MatrixRotateX(boat.pitch)),
                MatrixRotateY(boat.heading));
        Matrix actual = MatrixRotateXYZ(frame.player.hull.rotation);
        for (int axis = 0; axis < 3; ++axis) {
            Vector3 basis = { axis == 0, axis == 1, axis == 2 };
            CHECK(
                Vector3Distance(Vector3Transform(basis, actual), Vector3Transform(basis, expected))
                < .0001f);
        }
        Vector3 cabin = Vector3Add(
            frame.player.hull.position, Vector3Transform((Vector3) { 0, 1.1f, 0 }, expected));
        Vector3 engine = Vector3Add(
            frame.player.hull.position, Vector3Transform((Vector3) { 0, .4f, 1.6f }, expected));
        CHECK(Vector3Distance(cabin, frame.player.cabin.position) < .0001f);
        CHECK(Vector3Distance(engine, frame.player.engine.position) < .0001f);
        CHECK(frame.player.hull.position.y == boat.height);
        CHECK(frame.camera.position.y == BOAT_CAMERA_DEFAULT.height);
        CHECK(frame.camera.up.x == 0 && frame.camera.up.y == 1 && frame.camera.up.z == 0);
    }
    // Repeatable rest, head-on, beam and following sampling at low and cruising speeds.
    for (int encounter = 0; encounter < 4; ++encounter) {
        for (int speed = 0; speed <= 10; speed += 5) {
            boat_reset(&boat);
            boat.position = (Vector2) { 0 };
            boat.water_settings = boat_water_gentle();
            boat.heading = atan2f(.6f, .8f) + encounter * PI / 2;
            Boat repeat = boat;
            float motion = 0;
            for (int tick = 0; tick < 960; ++tick) {
                boat.velocity = repeat.velocity
                    = (Vector2) { speed * sinf(boat.heading), -speed * cosf(boat.heading) };
                boat_step(&boat, 0, 0);
                boat_step(&repeat, 0, 0);
                CHECK(fabsf(boat.height) < .4f && fabsf(boat.pitch) < .35f
                    && fabsf(boat.roll) < .35f);
                motion = fmaxf(motion, fabsf(boat.height));
                for (int i = 0; i < BOAT_WATER_PARTICLES; ++i) {
                    BoatWaterParticle p = boat.water[i];
                    if (p.lifetime <= 0)
                        continue;
                    float surface
                        = boat_water_query(boat.water_settings, p.position, boat.time).height;
                    if (!p.spray)
                        CHECK(fabsf(p.height - surface - .015f) < .00001f);
                    else
                        CHECK(p.height >= surface);
                }
            }
            CHECK(motion > .02f);
            CHECK(memcmp(&boat, &repeat, sizeof(Boat)) == 0);
        }
    }
    // Longer powered/reversing run, then remove the forcing and recover in calm water.
    boat_reset(&boat);
    boat.water_settings = boat_water_gentle();
    float combined_peak_roll = 0;
    int peak_tick = 0;
    Boat peak_state = { 0 };
    for (int tick = 0; tick < 36000; ++tick) {
        boat_step(&boat, sinf(tick * .005f), sinf(tick * .001f));
        CHECK(isfinite(boat.height) && fabsf(boat.height) < .5f);
        CHECK(isfinite(boat.pitch) && fabsf(boat.pitch) < .4f);
        CHECK(isfinite(boat.roll));
        // T3 tuned the passive guard against this coupled stress drive; no angle clamp.
        CHECK(fabsf(boat.roll) < 30 * DEG2RAD);
        if (fabsf(boat.roll) > combined_peak_roll) {
            combined_peak_roll = fabsf(boat.roll);
            peak_tick = tick;
            peak_state = boat;
        }
    }
    printf("combined_300s_peak_roll_deg=%.9g time=%.6f load=%.9g rate=%.9g speed=%.9g\n",
        combined_peak_roll * RAD2DEG, (peak_tick + 1) * BOAT_STEP, peak_state.turn_roll_load,
        peak_state.roll_velocity, Vector2Length(peak_state.velocity));
    boat.water_settings = (BoatWater) { 0 };
    boat.throttle = boat.engine_response = 0;
    for (int tick = 0; tick < 2400; ++tick)
        boat_step(&boat, 0, 0);
    CHECK(fabsf(boat.height) < .0001f && fabsf(boat.pitch) < .0001f);
    CHECK(fabsf(boat.roll) < .0001f);
    boat_reset(&boat);
    CHECK(boat.height == 0 && boat.vertical_velocity == 0);
    CHECK(boat.pitch == 0 && boat.pitch_velocity == 0 && boat.roll == 0 && boat.roll_velocity == 0
        && boat.turn_roll_load == 0);
    return 0;
}

static int validate_wave_resistance(void)
{
    // Test resistance in isolation: its work is nonpositive relative to local moving water.
    for (int i = 0; i < 2000; ++i) {
        BoatWaterSample water = boat_water_query(
            boat_water_gentle(), (Vector2) { i * .13f, sinf(i * .07f) * 20 }, i * .021f);
        water.velocity.x = sinf(i * .11f) * .4f;
        water.velocity.z = cosf(i * .05f) * .3f;
        Vector3 velocity = { sinf(i * .07f) * 15, cosf(i * .17f) * 3, cosf(i * .03f) * 12 };
        Vector3 relative = Vector3Subtract(velocity, water.velocity);
        float support = .2f + (i % 13) * .4f;
        Vector3 force = boat_water_resistance(water, velocity, 3, support);
        CHECK(Vector3DotProduct(force, relative) <= .00001f);
        CHECK(support + force.y >= -.00001f);
        CHECK(Vector3Length(Vector3CrossProduct(force, water.normal)) < .00001f);
        Vector3 next = Vector3Add(relative, Vector3Scale(force, BOAT_STEP));
        CHECK(Vector3LengthSqr(next) <= Vector3LengthSqr(relative) + .0001f);
        CHECK(Vector3LengthSqr(boat_water_resistance(water, velocity, 3, 0)) == 0);
        CHECK(Vector3LengthSqr(boat_water_resistance(water, water.velocity, 3, support)) == 0);
    }
    // Moving water can supply ground-frame energy, without supplying relative-frame energy.
    BoatWaterSample rising = { .normal = { 0, 1, 0 }, .velocity = { 0, .5f, 0 } };
    Vector3 force = boat_water_resistance(rising, (Vector3) { 0 }, 3, 2);
    CHECK(force.y > 0);
    CHECK(Vector3DotProduct(force, Vector3Negate(rising.velocity)) < 0);
    rising.velocity.y = 0;
    CHECK(boat_water_resistance(rising, (Vector3) { 0, 50, 0 }, 3, 2).y == -2);
    return 0;
}

typedef struct {
    float angular_motion, vertical_acceleration, spray_load;
    Boat final;
} Encounter;

static Encounter wave_encounter(float heading, float speed)
{
    Encounter result = { 0 };
    Boat *boat = &result.final;
    boat_reset(boat);
    boat->position = (Vector2) { 0 };
    boat->water_settings = boat_water_gentle();
    for (int tick = 0; tick < 960; ++tick) {
        // Controlled straight tow isolates heading and speed; it supplies horizontal work.
        boat->heading = heading;
        boat->yaw_velocity = 0;
        boat->velocity = (Vector2) { speed * sinf(heading), -speed * cosf(heading) };
        float previous = boat->vertical_velocity;
        boat_step(boat, 0, 0);
        if (tick >= 240) {
            float acceleration = (boat->vertical_velocity - previous) / BOAT_STEP;
            result.angular_motion += boat->pitch_velocity * boat->pitch_velocity
                + boat->roll_velocity * boat->roll_velocity;
            result.vertical_acceleration += acceleration * acceleration;
            result.spray_load += boat->wave_spray;
        }
    }
    result.angular_motion = sqrtf(result.angular_motion / 720);
    result.vertical_acceleration = sqrtf(result.vertical_acceleration / 720);
    result.spray_load /= 720;
    return result;
}

static int validate_gentle_encounters(void)
{
    float following = atan2f(.6f, .8f);
    Encounter fast[3];
    for (int angle = 0; angle < 3; ++angle) {
        float heading = following + (angle == 0 ? PI : angle == 1 ? PI / 2 : 0);
        Encounter slow = wave_encounter(heading, 2);
        fast[angle] = wave_encounter(heading, 12);
        Encounter repeat = wave_encounter(heading, 12);
        CHECK(memcmp(&fast[angle].final, &repeat.final, sizeof(Boat)) == 0);
        CHECK(slow.angular_motion < fast[angle].angular_motion * .8f);
        CHECK(slow.vertical_acceleration < fast[angle].vertical_acceleration);
        CHECK(slow.spray_load < fast[angle].spray_load);
        CHECK(fast[angle].angular_motion < .4f && fast[angle].vertical_acceleration < 5);
        CHECK(fast[angle].spray_load < .4f);
        WaterfrontSceneFrame frame;
        boat_scene_frame(&fast[angle].final, BOAT_CAMERA_DEFAULT, &frame);
        CHECK(frame.camera.up.y == 1 && frame.camera.position.y == BOAT_CAMERA_DEFAULT.height);
        CHECK(frame.player.particle_count > 0);
        int foam_count = 0;
        for (int i = 0; i < frame.player.particle_count; ++i) {
            WaterfrontPlayerParticle p = frame.player.particles[i];
            Vector2 position = { -p.transform.position.x, p.transform.position.z };
            BoatWaterSample water = boat_water_query(
                fast[angle].final.water_settings, position, fast[angle].final.time);
            if (p.spray)
                CHECK(p.transform.position.y >= water.height);
            else {
                Vector3 up = Vector3Transform(
                    (Vector3) { 0, 1, 0 }, MatrixRotateXYZ(p.transform.rotation));
                Vector3 normal = { -water.normal.x, water.normal.y, water.normal.z };
                CHECK(Vector3Distance(up, normal) < .0001f);
                CHECK(fabsf(p.transform.position.y - water.height - .015f) < .00001f);
                ++foam_count;
            }
        }
        CHECK(foam_count > 0);
    }
    CHECK(fast[0].vertical_acceleration > fast[2].vertical_acceleration * 2);
    CHECK(fast[1].angular_motion > fast[2].angular_motion);
    // Wave forces cannot bypass the existing hull/shore containment.
    for (int angle = 0; angle < 4; ++angle) {
        Boat boat;
        boat_reset_location(&boat, true);
        boat.water_settings = boat_water_gentle();
        boat.heading = angle * PI / 2;
        boat.throttle = 1;
        for (int tick = 0; tick < 1200; ++tick) {
            boat_step(&boat, 0, 0);
            CHECK(boat_map_water(boat.position, .3f));
            CHECK(isfinite(boat.heading) && fabsf(boat.yaw_velocity) < 4);
        }
    }
    // Unpowered waves may transfer energy, but must not create a runaway neutral drift or turn.
    Boat neutral;
    boat_reset(&neutral);
    neutral.water_settings = boat_water_gentle();
    for (int tick = 0; tick < 14400; ++tick) {
        boat_step(&neutral, 0, 0);
        CHECK(Vector2Length(neutral.velocity) < .5f);
        CHECK(fabsf(neutral.yaw_velocity) < .2f);
        CHECK(
            fabsf(neutral.height) < .4f && fabsf(neutral.pitch) < .3f && fabsf(neutral.roll) < .3f);
    }
    Boat reset = fast[0].final;
    boat_reset(&reset);
    CHECK(reset.wave_spray == 0);
    return 0;
}

static int validate_calm_reference(void)
{
    // Straight sample retains the accepted C2 baseline. Turning/coasting samples are
    // measured H3 trajectories. Revision 5 updates only the final coast after dry-stern
    // skeg disengagement; the powered straight/turn references remain unchanged.
    const float expected[3][10] = { { 0, 3.60687423f, 0, -8.85409927f, 0, 0, .00596522959f,
                                        .0537321381f, 0, .276249886f },
        { 23.757431f, -49.1584587f, 15.6087561f, 1.88263535f, 1.85332334f, 1.05519712f,
            .0500000007f, .053120777f, .211947575f, 1 },
        { 39.4533844f, -33.0364151f, 9.76989174f, -13.2869892f, -2.50756764f, 0, 0, 0, 0, 0 } };
    Boat boat;
    boat_reset(&boat);
    int sample = 0;
    for (int tick = 0; tick < 1800; ++tick) {
        float wheel = tick >= 360 && tick < 900 ? .35f : 0;
        float thrust = tick < 240 ? 1 : tick < 900 ? 0 : tick < 1380 ? -1 : 0;
        boat_step(&boat, wheel, thrust);
        CHECK(boat.wave_spray == 0);
        if (tick == 239 || tick == 719 || tick == 1799) {
            const float actual[] = { boat.position.x, boat.position.y, boat.velocity.x,
                boat.velocity.y, boat.heading, boat.yaw_velocity, boat.height, boat.pitch,
                boat.roll, boat.planing };
            for (int i = 0; i < 10; ++i)
                CHECK(fabsf(actual[i] - expected[sample][i]) < (i < 4 ? .002f : .00001f));
            if (sample == 1) {
                printf("banked_calm_reference height=%.9g pitch=%.9g roll=%.9g load=%.9g\n",
                    boat.height, boat.pitch, boat.roll, boat.turn_roll_load);
                // Measured H3 attitude, backed by independent force/filter/tow/entry checks.
                CHECK(fabsf(boat.height - .0500000007f) < .00001f);
                CHECK(fabsf(boat.pitch - .053120777f) < .00001f);
                CHECK(fabsf(boat.roll - .211947575f) < .00001f);
                CHECK(boat.roll > 5 * DEG2RAD && boat.roll < 25 * DEG2RAD);
                CHECK(fabsf(boat.height) < .2f && fabsf(boat.pitch) < .15f);
                CHECK(boat.turn_roll_load > 0);
            }
            ++sample;
        }
    }
    return 0;
}

static int shared_water(void)
{
    BoatWater water = boat_water_gentle();
    water.crossing[0] = water.crossing[1] = (BoatWave) { 0 };
    BoatWaterSample q = boat_water_query((BoatWater) { 0 }, (Vector2) { 83, -51 }, 120);
    CHECK(q.height == 0 && q.normal.x == 0 && q.normal.y == 1 && q.normal.z == 0);
    CHECK(Vector3LengthSqr(q.velocity) == 0);
    q = boat_water_query(water, (Vector2) { 0 }, 0);
    CHECK(q.height == 0 && q.normal.x < 0 && q.normal.z > 0);
    CHECK(fabsf(q.velocity.y + water.amplitude * water.angular_speed) < .00001f);
    Vector2 crest = Vector2Scale(water.direction, water.wavelength / 4);
    q = boat_water_query(water, crest, 0);
    CHECK(fabsf(q.height - water.amplitude) < .00001f);
    CHECK(fabsf(q.normal.y - 1) < .00001f && fabsf(q.velocity.y) < .00001f);
    BoatWaterSample moved = boat_water_query(water,
        Vector2Scale(water.direction,
            water.wavelength / 4 + water.angular_speed / (2 * PI / water.wavelength)),
        1);
    CHECK(fabsf(moved.height - q.height) < .00001f);
    water = boat_water_gentle();
    for (int i = 0; i < 10000; ++i) {
        float time = i * .13f;
        Vector2 point = { sinf(time) * 250, cosf(time) * 300 };
        q = boat_water_query(water, point, time);
        CHECK(isfinite(q.height)
            && fabsf(q.height) <= water.amplitude + water.crossing[0].amplitude
                    + water.crossing[1].amplitude + .00001f);
        CHECK(fabsf(Vector3Length(q.normal) - 1) < .00001f);
        float dt = .002f;
        if (time < 20) {
            float derivative = (boat_water_query(water, point, time + dt).height
                                   - boat_water_query(water, point, time - dt).height)
                / (2 * dt);
            CHECK(fabsf(derivative - q.velocity.y) < .001f);
            float ds = .02f;
            Vector2 dx = { point.x + ds, point.y }, dz = { point.x, point.y + ds };
            float sx = (boat_water_query(water, dx, time).height - q.height) / ds;
            float sz = (boat_water_query(water, dz, time).height - q.height) / ds;
            CHECK(fabsf(sx + q.normal.x / q.normal.y) < .001f);
            CHECK(fabsf(sz + q.normal.z / q.normal.y) < .001f);
        }
    }
    for (int location = 0; location < 2; ++location) {
        const Mesh3D *mesh = boat_water_surface(water, .7f, location);
        CHECK(mesh && mesh->triangle_count > 100);
        for (size_t i = 0; i < mesh->triangle_count; ++i) {
            Vector3 centre = { 0 };
            float height = 0;
            for (int j = 0; j < 3; ++j) {
                Vector3 v = mesh->vertices[mesh->triangles[i].vertices[j]];
                q = boat_water_query(water, (Vector2) { -v.x, v.z }, .7f);
                CHECK(fabsf(v.y - q.height) < .000001f);
                centre = Vector3Add(centre, Vector3Scale(v, 1.0f / 3));
                height += v.y / 3;
            }
            q = boat_water_query(water, (Vector2) { -centre.x, centre.z }, .7f);
            CHECK(fabsf(height - q.height) < .035f); // Bounded interpolation error.
        }
    }
    Boat flat, waves;
    boat_reset(&flat);
    waves = flat;
    waves.water_settings = water;
    for (int i = 0; i < 2400; ++i) {
        boat_step(&flat, sinf(i * .01f), i < 300 ? 1 : 0);
        boat_step(&waves, sinf(i * .01f), i < 300 ? 1 : 0);
    }
    CHECK(Vector2Distance(flat.position, waves.position) > .01f);
    CHECK(Vector2Distance(flat.velocity, waves.velocity) > .001f);
    CHECK(fabsf(flat.pitch - waves.pitch) + fabsf(flat.height - waves.height) > .001f);
    boat_reset(&waves);
    CHECK(waves.water_settings.amplitude == 0 && waves.time == 0);
    boat_water_destroy();
    return 0;
}

static bool patch_contains(const Mesh3D *mesh, Vector2 p)
{
    for (size_t i = 0; i < mesh->triangle_count; ++i) {
        Vector3 a = mesh->vertices[mesh->triangles[i].vertices[0]];
        Vector3 b = mesh->vertices[mesh->triangles[i].vertices[1]];
        Vector3 c = mesh->vertices[mesh->triangles[i].vertices[2]];
        if (p.x < fminf(a.x, fminf(b.x, c.x)) || p.x > fmaxf(a.x, fmaxf(b.x, c.x))
            || p.y < fminf(a.z, fminf(b.z, c.z)) || p.y > fmaxf(a.z, fmaxf(b.z, c.z)))
            continue;
        float ab = (b.x - a.x) * (p.y - a.z) - (b.z - a.z) * (p.x - a.x);
        float bc = (c.x - b.x) * (p.y - b.z) - (c.z - b.z) * (p.x - b.x);
        float ca = (a.x - c.x) * (p.y - c.z) - (a.z - c.z) * (p.x - c.x);
        if ((ab >= -1e-5f && bc >= -1e-5f && ca >= -1e-5f)
            || (ab <= 1e-5f && bc <= 1e-5f && ca <= 1e-5f))
            return true;
    }
    return false;
}

static uint64_t patch_overlap(const Mesh3D *mesh)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < mesh->triangle_count; ++i) {
        bool inside = true;
        for (int j = 0; j < 3; ++j) {
            Vector3 p = mesh->vertices[mesh->triangles[i].vertices[j]];
            inside &= fabsf(p.x) < 40 && p.z > -280 && p.z < -220;
        }
        if (!inside)
            continue;
        for (int j = 0; j < 3; ++j) {
            Vector3 p = mesh->vertices[mesh->triangles[i].vertices[j]];
            hash = (hash ^ (uint32_t)lrintf(p.x * 1000)) * UINT64_C(1099511628211);
            hash = (hash ^ (uint32_t)lrintf(p.z * 1000)) * UINT64_C(1099511628211);
        }
    }
    return hash;
}

static int validate_water_patch(void)
{
    BoatWater water = boat_water_gentle();
    const Vector2 centers[]
        = { { 12, 60 }, { 12, -125 }, { 170, -250 }, { -80, -455 }, { -60, -175 }, { 940, 100 } };
    for (int detail = 0; detail < 2; ++detail)
        for (int location = 0; location < 2; ++location)
            for (size_t k = 0; k < sizeof(centers) / sizeof(*centers); ++k) {
                if (!location && k > 1)
                    break;
                Vector2 center = location ? centers[k] : (Vector2) { k ? 100 : 0, 0 };
                const Mesh3D *mesh = detail ? boat_water_patch_arcade(center, location)
                                            : boat_water_patch(center, location);
                CHECK(mesh && mesh->triangle_count > 0
                    && mesh->triangle_count < (detail ? 80000 : 50000));
                for (size_t i = 0; i < mesh->triangle_count; ++i) {
                    Vector3 centroid = { 0 };
                    float height = 0;
                    for (int j = 0; j < 3; ++j) {
                        Vector3 p = mesh->vertices[mesh->triangles[i].vertices[j]];
                        Vector3 q = mesh->vertices[mesh->triangles[i].vertices[(j + 1) % 3]];
                        CHECK(p.y == 0 && isfinite(p.x) && isfinite(p.z));
                        CHECK(Vector3Distance(p, q) <= 2.122f);
                        Vector2 snapped
                            = { roundf(center.x / 12) * 12, roundf(center.y / 12) * 12 };
                        if (detail && fabsf(p.x - snapped.x) < 23 && fabsf(q.x - snapped.x) < 23
                            && fabsf(p.z - snapped.y) < 23 && fabsf(q.z - snapped.y) < 23)
                            CHECK(Vector3Distance(p, q) <= 1.061f);
                        centroid = Vector3Add(centroid, Vector3Scale(p, 1.0f / 3));
                        height += boat_water_query(water, (Vector2) { -p.x, p.z }, .7f).height / 3;
                    }
                    float exact
                        = boat_water_query(water, (Vector2) { -centroid.x, centroid.z }, .7f)
                              .height;
                    CHECK(fabsf(height - exact) < .035f);
                }
                // Sample the entire visible disk against the original water, including land,
                // narrow dock edges, the southern bank, and the artificial map crop.
                for (int z = -78; z <= 78; z += 6)
                    for (int x = -78; x <= 78; x += 6) {
                        if (x * x + z * z > 78 * 78)
                            continue;
                        Vector2 p = { center.x + x + .17f, center.y + z + .23f };
                        bool expected = location ? boat_map_water((Vector2) { -p.x, p.y }, 0)
                                                 : fabsf(p.x) <= 120 && fabsf(p.y) <= 80;
                        CHECK(patch_contains(mesh, p) == expected);
                    }
            }
    const Mesh3D *mesh = boat_water_patch((Vector2) { 5.99f, -250 }, true);
    CHECK(mesh);
    uint64_t before = patch_overlap(mesh);
    mesh = boat_water_patch((Vector2) { 6.01f, -250 }, true);
    CHECK(mesh && before == patch_overlap(mesh));
    // Recentring changes only cells beyond the rendered disk, not overlapping world geometry.
    boat_water_destroy();
    return 0;
}

int main(void)
{
    CHECK(validate_flight_outcomes() == 0);
    CHECK(validate_dynamic_pair() == 0);
    CHECK(validate_water_patch() == 0);
    CHECK(shared_water() == 0);
    CHECK(validate_buoyancy() == 0);
    CHECK(validate_wave_resistance() == 0);
    CHECK(validate_gentle_encounters() == 0);
    CHECK(validate_calm_reference() == 0);
    // The GPU consumes the same held engine angle and mirrored world coordinates as CPU.
    Boat player = { .position = { 4, 9 }, .heading = .7f, .engine_angle = .2f };
    boat_set_tick(&player, 360);
    player.water[0]
        = (BoatWaterParticle) { .position = { 2, 8 }, .height = .1f, .lifetime = 2, .size = .4f };
    player.water[1] = (BoatWaterParticle) {
        .position = { 3, 8 }, .height = 1, .lifetime = 1, .size = .2f, .spray = true
    };
    WaterfrontSceneFrame frame;
    boat_scene_frame(&player, BOAT_CAMERA_DEFAULT, &frame);
    CHECK(frame.player_active && frame.time == 3);
    CHECK(frame.player.hull.position.x == -4 && frame.player.hull.position.z == 9);
    CHECK(fabsf(frame.player.engine.rotation.y - .5f) < .0001f);
    CHECK(frame.player.particle_count == 2);
    CHECK(!frame.player.particles[0].spray && frame.player.particles[1].spray);
    CHECK(frame.player.particles[0].transform.position.x == -2);
    CHECK(frame.player.particles[1].transform.position.y == 1);
    boat_reset(&player);
    boat_scene_frame(&player, BOAT_CAMERA_DEFAULT, &frame);
    CHECK(frame.time == 0 && frame.player.particle_count == 0);
    waterfront_scene_evaluate(0, true, &frame);
    CHECK(!frame.player_active);
    Boat mapped;
    boat_reset_location(&mapped, true);
    CHECK(mapped.eriksberg && boat_map_water(mapped.position, .3f));
    CHECK(!boat_map_water((Vector2) { 10000, 0 }, 0));
    CHECK(!boat_map_water((Vector2) { -100, 60 }, 0));
    // Drive into both dock quays. Hull clearance must remain in water and impacts rebound.
    for (int side = -1; side <= 1; side += 2) {
        boat_reset_location(&mapped, true);
        mapped.heading = mapped.camera_heading = side * 1.5707963f;
        mapped.throttle = 1;
        bool bumped = false;
        for (int tick = 0; tick < 1200; ++tick) {
            boat_step(&mapped, 0, 0);
            CHECK(footprint_in_water(&mapped));
            if (mapped.velocity.x * side < -0.01f)
                bumped = true;
        }
        CHECK(bumped);
    }
    // The turn can leave the boat moving forward when reverse thrust is selected. Measure
    // actual backward travel: braking and returning near the initial position is not a jam.
    // Small heading perturbations cover contact trajectories sensitive to float rounding.
    const float heading_offsets[] = { 0, -.00001f, .00001f };
    for (size_t i = 0; i < sizeof(heading_offsets) / sizeof(*heading_offsets); ++i)
        for (int side = -1; side <= 1; side += 2) {
            boat_reset_location(&mapped, true);
            mapped.heading = mapped.camera_heading = side * PI / 2 + heading_offsets[i];
            mapped.throttle = 1;
            for (int tick = 0; tick < 900; ++tick) {
                boat_step(&mapped, tick > 500 ? side : 0, 0);
                CHECK(footprint_in_water(&mapped));
            }
            float reverse_travel = 0;
            mapped.throttle = -1;
            for (int tick = 0; tick < 360; ++tick) {
                Vector2 previous = mapped.position;
                Vector2 forward = { sinf(mapped.heading), -cosf(mapped.heading) };
                boat_step(&mapped, 0, 0);
                CHECK(footprint_in_water(&mapped));
                reverse_travel += fmaxf(
                    0, -Vector2DotProduct(Vector2Subtract(mapped.position, previous), forward));
            }
            if (reverse_travel <= 3)
                fprintf(stderr, "Mapped reverse: side=%d heading_offset=%g backward_travel=%g\n",
                    side, heading_offsets[i], reverse_travel);
            CHECK(reverse_travel > 3);
        }
    // The channel remains navigable through the crane towards the river.
    boat_reset_location(&mapped, true);
    mapped.throttle = 1;
    for (int tick = 0; tick < 1200; ++tick) {
        boat_step(&mapped, 0, 0);
        CHECK(boat_map_water(mapped.position, .3f));
    }
    CHECK(mapped.position.y < -50);
    boat_reset_location(&mapped, false);
    CHECK(!mapped.eriksberg && mapped.position.y == 10);
    boat_map_destroy();
    CHECK(validate_course() == 0);
    CHECK(validate_coasting() == 0);
    CHECK(validate_countersteering() == 0);
    CHECK(validate_speed_view() == 0);
    CHECK(validate_wake_independence() == 0);
    CHECK(validate_signed_propulsion() == 0);
    CHECK(validate_signed_throttle() == 0);
    CHECK(validate_planing() == 0);
    CHECK(validate_planing_attitude() == 0);
    CHECK(validate_hull_contacts() == 0);
    Boat boat;
    boat_reset(&boat);
    // Follow the short arc across the +/-pi boundary, without a full camera orbit.
    boat.heading = -3.13f;
    boat.camera_heading = 3.13f;
    boat_step(&boat, 0, 0);
    float camera_turn = remainderf(boat.camera_heading - 3.13f, 6.2831853f);
    CHECK(camera_turn > 0 && camera_turn < 0.01f);
    boat_reset(&boat);
    CHECK(boat.camera_heading == 0);
    // Wheel adjustments are bounded and reversible; release springs back symmetrically.
    for (int i = 0; i < 60; ++i)
        boat_step(&boat, 1, 0);
    CHECK(fabsf(boat.wheel - 0.5f) < 0.001f);
    Boat mirrored = boat;
    mirrored.wheel = -boat.wheel;
    mirrored.engine_angle = -boat.engine_angle;
    for (int i = 0; i < 360; ++i) {
        float previous = boat.wheel;
        boat_step(&boat, 0, 0);
        boat_step(&mirrored, 0, 0);
        CHECK(boat.wheel >= 0 && boat.wheel <= previous);
        CHECK(fabsf(boat.wheel + mirrored.wheel) < 0.000001f);
        CHECK(fabsf(boat.engine_angle + mirrored.engine_angle) < 0.000001f);
        if (i == 119)
            CHECK(boat.wheel > 0.01f && boat.wheel < 0.05f);
    }
    CHECK(boat.wheel == 0 && boat.engine_angle == 0);
    CHECK(mirrored.wheel == 0 && mirrored.engine_angle == 0);
    boat_step(&boat, 0, 0);
    CHECK(boat.wheel == 0 && boat.engine_angle == 0);
    for (int i = 0; i < 600; ++i)
        boat_step(&boat, 1, 0);
    CHECK(boat.wheel == 1);
    for (int i = 0; i < 600; ++i)
        boat_step(&boat, -1, 0);
    CHECK(boat.wheel == -1);
    for (int i = 0; i < 40; ++i)
        boat_step(&boat, 1, 0);
    CHECK(fabsf(boat.wheel) < 0.001f);
    boat_reset(&boat);
    CHECK(boat.wheel == 0 && boat.engine_angle == 0);
    // Turning the engine at idle cannot rotate a stationary boat.
    for (int i = 0; i < 120; ++i)
        boat_step(&boat, 1, 0);
    CHECK(boat.heading == 0 && boat.yaw_velocity == 0);
    CHECK(boat.position.x == 0 && boat.position.y == 10);
    CHECK(boat.engine_angle > 0 && boat.engine_angle < 0.6f);
    CHECK(particles(&boat, false) == 0);
    // The first powered impulse pushes the stern left while yawing the bow right.
    boat.throttle = 1;
    boat_step(&boat, 1, 0);
    CHECK(boat.velocity.x < 0 || boat.velocity.y < 0);
    CHECK(boat.heading > 0 && boat.yaw_velocity > 0);
    for (int i = 0; i < 10; ++i)
        boat_step(&boat, 1, 0);
    CHECK(particles(&boat, false) > 0);
    CHECK(boat.water[0].velocity.x > 0 && boat.water[0].velocity.y > 0);
    // Cutting the command lets the small engine-response state spool down first.
    float response = boat.engine_response;
    boat.throttle = 0;
    boat_step(&boat, 0, 0);
    CHECK(boat.engine_response > 0 && boat.engine_response < response);
    while (boat.engine_response > 0)
        boat_step(&boat, 0, 0);
    // Once neutral, angular momentum and passive steering remain while energy decays.
    float energy = kinetic_energy(&boat);
    boat_step(&boat, 0, 0);
    CHECK(boat.yaw_velocity > 0 && kinetic_energy(&boat) < energy);
    for (int i = 0; i < 2400; ++i)
        boat_step(&boat, 0, 0);
    CHECK(boat.wheel == 0 && boat.engine_angle == 0 && fabsf(boat.yaw_velocity) < 0.001f);
    boat_reset(&boat);
    for (int i = 0; i < 120; ++i)
        boat_step(&boat, 0, 1);
    CHECK(fabsf(boat.throttle - 0.5f) < 0.001f);
    float held = boat.throttle;
    for (int i = 0; i < 60; ++i)
        boat_step(&boat, 0, 0);
    CHECK(boat.throttle == held);
    for (int i = 0; i < 600; ++i)
        boat_step(&boat, 0, 1);
    CHECK(boat.throttle == 1);
    for (int i = 0; i < 240; ++i)
        boat_step(&boat, 0, -1);
    CHECK(fabsf(boat.throttle) < .001f);
    boat.throttle = 0.75f;
    boat_reset(&boat);
    CHECK(boat.throttle == 0);
    boat.throttle = 1;
    for (int i = 0; i < 120; ++i)
        boat_step(&boat, 0, 0);
    CHECK(boat.position.y < 10 && boat.velocity.y < -5);
    CHECK(fabsf(boat.position.x) < 0.001f);
    CHECK(particles(&boat, false) > 0);
    CHECK(particles(&boat, true) > 0); // Forward planing now throws symmetric spray.
    boat.throttle = 0;
    while (boat.engine_response > 0)
        boat_step(&boat, 0, 0);
    float speed = hypotf(boat.velocity.x, boat.velocity.y);
    for (int i = 0; i < 120; ++i)
        boat_step(&boat, 0, 0);
    // A boat still on plane coasts farther than the displacement-only model, but slows.
    CHECK(hypotf(boat.velocity.x, boat.velocity.y) < speed * 0.85f);
    CHECK(hypotf(boat.velocity.x, boat.velocity.y) > speed * 0.4f);

    Boat left, right;
    boat_reset(&left);
    boat_reset(&right);
    left.throttle = right.throttle = 1;
    left.wheel = -0.5f;
    right.wheel = 0.5f;
    for (int i = 0; i < 180; ++i) {
        boat_step(&left, -1, 0);
        boat_step(&right, 1, 0);
    }
    CHECK(right.position.x > 1 && left.position.x < -1);
    CHECK(fabsf(left.position.x + right.position.x) < 0.001f);
    CHECK(fabsf(left.position.y - right.position.y) < 0.001f);
    CHECK(particles(&right, true) > 0);
    CHECK(particles(&left, true) == particles(&right, true));
    // Stopping leaves existing water effects to fade; steering at idle produces no new wash.
    right.velocity = (Vector2) { 0 };
    right.throttle = 0;
    for (int i = 0; i < 600; ++i)
        boat_step(&right, 1, 0);
    CHECK(particles(&right, false) == 0 && particles(&right, true) == 0);
    // A powered turn retains sideways momentum.
    CHECK(
        fabsf(left.velocity.x * cosf(left.heading) + left.velocity.y * sinf(left.heading)) > 0.1f);

    CHECK(validate_basin_stress(-.00001f) == 0);
    CHECK(validate_basin_stress(0) == 0);
    CHECK(validate_basin_stress(.00001f) == 0);
    boat_reset(&boat);
    CHECK(boat.position.x == 0 && boat.position.y == 10);
    CHECK(boat.heading == 0 && boat.velocity.x == 0 && boat.velocity.y == 0);
    CHECK(boat.wheel == 0 && boat.engine_angle == 0 && boat.yaw_velocity == 0);
    CHECK(boat.throttle == 0 && boat.engine_response == 0);
    CHECK(boat.planing == 0);
    CHECK(particles(&boat, false) == 0 && particles(&boat, true) == 0);
    puts("boat_handling=ok");
    return 0;
}
