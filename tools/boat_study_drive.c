#include "boat_study_drive.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
void object_study_reset(ObjectStudy *study, Boat *boat, bool crossing, unsigned life, float height)
{
    *study = (ObjectStudy) { .crossing = crossing };
    boat_wake_objects_init(&study->world, life, height);
    boat->water_settings.objects = &study->world;
    boat_reset(&study->other);
    study->other.position = (Vector2) { -65, -12 };
    study->other.heading = PI / 2;
    study->other.water_settings.objects = &study->world;
}

static void object_track(ObjectStudy *study, const Boat *boat, unsigned id)
{
    BoatWakeStern s = boat_wake_stern(boat);
    if (!boat_wake_objects_track(&study->world, id, (Vector2) { (float)s.x, (float)s.z },
            (float)hypot(s.vx, s.vz), (float)s.wet)) {
        fprintf(stderr, "Wake object emission contract failed.\n");
        exit(1);
    }
}

void timed_boat_step(Boat *boat, float steering, float throttle, BoatFrameMetrics *metrics,
    BoatWakePlayer *wake, ObjectStudy *objects)
{
    double started = boat_profile_time(IsWindowReady());
    if (objects)
        boat_wake_objects_advance(&objects->world);
    BoatWakeStern before = wake ? boat_wake_stern(boat) : (BoatWakeStern) { 0 };
    boat_step(boat, steering, throttle);
    if (objects) {
        object_track(objects, boat, 0);
        if (objects->crossing) {
            const Vector2 route[] = { { 60, -12 }, { 0, -55 }, { -60, -12 }, { 0, 45 } };
            Boat *other = &objects->other;
            Vector2 delta = Vector2Subtract(route[objects->waypoint], other->position);
            if (Vector2Length(delta) < 16)
                objects->waypoint = (objects->waypoint + 1) % 4;
            float target = atan2f(delta.x, -delta.y);
            float error = remainderf(target - other->heading, 2 * PI);
            float wheel = Clamp(2 * error - other->yaw_velocity, -1, 1);
            boat_step(other, Clamp((wheel - other->wheel) / BOAT_STEP, -1, 1),
                Clamp((.75f - other->throttle) / (.5f * BOAT_STEP), -1, 1));
            boat_collide_pair(boat, 1, other, 1.6f);
            if (other->motion_state == BOAT_CAPSIZED) {
                objects->recovery_time += BOAT_STEP;
                Vector2 spawn = { -65, -12 };
                if (objects->recovery_time >= 2 && Vector2Distance(boat->position, spawn) > 10) {
                    float time = other->time;
                    boat_reset(other);
                    other->position = spawn;
                    other->heading = PI / 2;
                    other->time = time;
                    other->water_settings.objects = &objects->world;
                    objects->world.emitters[1]
                        = (BoatWakeObjectEmitter) { .next = objects->world.emitters[1].next };
                    objects->waypoint = 0;
                    objects->recovery_time = 0;
                }
            }
            object_track(objects, other, 1);
        }
    }
    if (wake && !boat_wake_player_step(wake, before, boat_wake_stern(boat))) {
        fprintf(stderr, "Wake field update failed.\n");
        exit(1);
    }
    float elapsed = (float)((boat_profile_time(IsWindowReady()) - started) * 1000.0);
    if (metrics) {
        metrics->physics_ms += elapsed;
        metrics->physics_max_ms = fmaxf(metrics->physics_max_ms, elapsed);
        ++metrics->ticks;
    }
}

void demo_step(
    Boat *boat, int tick, BoatFrameMetrics *metrics, BoatWakePlayer *wake, ObjectStudy *objects)
{
    if (wake || objects || boat->water_settings.player_wake_enabled) {
        float throttle = tick < 1440 ? 1 : 0;
        float wheel = tick >= 420 && tick < 1440 ? 1 : 0;
        timed_boat_step(boat, Clamp((wheel - boat->wheel) / BOAT_STEP, -1, 1),
            Clamp((throttle - boat->throttle) / (.5f * BOAT_STEP), -1, 1), metrics, wake, objects);
        return;
    }
    if (boat->water_settings.wake_amplitude != 0
        || Vector2LengthSqr(boat->water_settings.current) > 0) {
        // Let the source become visible, approach its spreading wake, then recover in neutral.
        float target = tick < 720 ? 0 : tick < 1200 ? .65f : 0;
        timed_boat_step(boat, 0, Clamp((target - boat->throttle) / (.5f * BOAT_STEP), -1, 1),
            metrics, wake, objects);
        return;
    }
    // Ease through neutral into reverse with the same held steering.
    float target_wheel = tick >= 360 ? 0.65f : 0;
    float wheel_change = fmaxf(-1, fminf(1, (target_wheel - boat->wheel) / BOAT_STEP));
    float throttle_change = tick < 240 ? 1 : tick < 480 ? 0 : tick < 960 ? -1 : 0;
    timed_boat_step(boat, wheel_change, throttle_change, metrics, wake, objects);
}
