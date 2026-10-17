#include "boat.h"
#include "boat_map.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Boat check failed: %s\n", #condition);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int particles(const Boat *boat, bool spray)
{
    int count = 0;
    for (int i = 0; i < BOAT_WATER_PARTICLES; ++i)
        if (boat->water[i].lifetime > 0 && boat->water[i].spray == spray)
            ++count;
    return count;
}

static float kinetic_energy(const Boat *boat)
{
    // Normalized mass 1 and yaw inertia 2.2; total energy includes rotation.
    return 0.5f * (boat->velocity.x * boat->velocity.x + boat->velocity.y * boat->velocity.y)
        + 1.1f * boat->yaw_velocity * boat->yaw_velocity;
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
        boat_step(&forward_turn, 0, 0);
        boat_step(&reverse_turn, 0, 0);
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

int main(void)
{
    // The GPU consumes the same held engine angle and mirrored world coordinates as CPU.
    Boat player = { .position = { 4, 9 }, .heading = .7f, .engine_angle = .2f, .time = 3 };
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
    CHECK(mapped.eriksberg && boat_map_water(mapped.position, 2.5f));
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
            CHECK(boat_map_water(mapped.position, 2.5f));
            if (mapped.velocity.x * side < -0.01f)
                bumped = true;
        }
        CHECK(bumped);
    }
    // The channel remains navigable through the crane towards the river.
    boat_reset_location(&mapped, true);
    mapped.throttle = 1;
    for (int tick = 0; tick < 1200; ++tick) {
        boat_step(&mapped, 0, 0);
        CHECK(boat_map_water(mapped.position, 2.5f));
    }
    CHECK(mapped.position.y < -50);
    boat_reset_location(&mapped, false);
    CHECK(!mapped.eriksberg && mapped.position.y == 10);
    boat_map_destroy();
    CHECK(validate_coasting() == 0);
    CHECK(validate_signed_propulsion() == 0);
    CHECK(validate_signed_throttle() == 0);
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
    // Wheel adjustments are bounded, persistent, reversible, and cleared by reset.
    for (int i = 0; i < 60; ++i)
        boat_step(&boat, 1, 0);
    CHECK(fabsf(boat.wheel - 0.5f) < 0.001f);
    float held_wheel = boat.wheel;
    float held_engine = boat.engine_angle;
    for (int i = 0; i < 120; ++i)
        boat_step(&boat, 0, 0);
    CHECK(boat.wheel == held_wheel && boat.engine_angle == held_engine);
    for (int i = 0; i < 600; ++i)
        boat_step(&boat, 1, 0);
    CHECK(boat.wheel == 1);
    for (int i = 0; i < 600; ++i)
        boat_step(&boat, -1, 0);
    CHECK(boat.wheel == -1);
    for (int i = 0; i < 120; ++i)
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
    CHECK(boat.velocity.x < 0 && boat.velocity.y < 0);
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
    CHECK(boat.wheel > 0 && boat.engine_angle > 0 && fabsf(boat.yaw_velocity) < 0.001f);
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
    CHECK(particles(&boat, true) == 0);
    boat.throttle = 0;
    while (boat.engine_response > 0)
        boat_step(&boat, 0, 0);
    float speed = hypotf(boat.velocity.x, boat.velocity.y);
    for (int i = 0; i < 120; ++i)
        boat_step(&boat, 0, 0);
    CHECK(hypotf(boat.velocity.x, boat.velocity.y) < speed * 0.7f);
    CHECK(hypotf(boat.velocity.x, boat.velocity.y) > speed * 0.4f);

    Boat left, right;
    boat_reset(&left);
    boat_reset(&right);
    left.throttle = right.throttle = 1;
    left.wheel = -0.5f;
    right.wheel = 0.5f;
    for (int i = 0; i < 180; ++i) {
        boat_step(&left, 0, 0);
        boat_step(&right, 0, 0);
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

    boat = (Boat) { .position = { BOAT_LIMIT_X, BOAT_LIMIT_Z }, .velocity = { 8, 8 } };
    boat_step(&boat, 0, 0);
    CHECK(boat.position.x <= BOAT_LIMIT_X && boat.position.y <= BOAT_LIMIT_Z);
    CHECK(boat.velocity.x < 0 && boat.velocity.y < 0);
    for (int i = 0; i < 120 * 120; ++i) {
        boat_step(&boat, sinf(i * 0.003f), 1);
        CHECK(isfinite(boat.heading) && isfinite(boat.velocity.x) && isfinite(boat.velocity.y));
        CHECK(isfinite(boat.yaw_velocity) && fabsf(boat.yaw_velocity) < 4);
        CHECK(isfinite(boat.camera_heading));
        CHECK(fabsf(boat.position.x) <= BOAT_LIMIT_X);
        CHECK(fabsf(boat.position.y) <= BOAT_LIMIT_Z);
        CHECK(hypotf(boat.velocity.x, boat.velocity.y) < 21);
    }
    boat_reset(&boat);
    CHECK(boat.position.x == 0 && boat.position.y == 10);
    CHECK(boat.heading == 0 && boat.velocity.x == 0 && boat.velocity.y == 0);
    CHECK(boat.wheel == 0 && boat.engine_angle == 0 && boat.yaw_velocity == 0);
    CHECK(boat.throttle == 0 && boat.engine_response == 0);
    CHECK(particles(&boat, false) == 0 && particles(&boat, true) == 0);
    puts("boat_handling=ok");
    return 0;
}
