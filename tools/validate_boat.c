#include "boat.h"

#include <math.h>
#include <stdio.h>

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

int main(void)
{
    Boat boat;
    boat_reset(&boat);
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
    // Cutting power removes engine torque, but angular momentum decays rather than snapping off.
    float yaw = boat.yaw_velocity;
    boat.throttle = 0;
    boat_step(&boat, 0, 0);
    CHECK(boat.yaw_velocity > 0 && boat.yaw_velocity < yaw);
    for (int i = 0; i < 600; ++i)
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
    for (int i = 0; i < 600; ++i)
        boat_step(&boat, 0, -1);
    CHECK(boat.throttle == 0);
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
    float speed = hypotf(boat.velocity.x, boat.velocity.y);
    boat.throttle = 0;
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
        CHECK(fabsf(boat.position.x) <= BOAT_LIMIT_X);
        CHECK(fabsf(boat.position.y) <= BOAT_LIMIT_Z);
        CHECK(hypotf(boat.velocity.x, boat.velocity.y) < 21);
    }
    boat_reset(&boat);
    CHECK(boat.position.x == 0 && boat.position.y == 10);
    CHECK(boat.heading == 0 && boat.velocity.x == 0 && boat.velocity.y == 0);
    CHECK(boat.wheel == 0 && boat.engine_angle == 0 && boat.yaw_velocity == 0);
    CHECK(particles(&boat, false) == 0 && particles(&boat, true) == 0);
    puts("boat_handling=ok");
    return 0;
}
