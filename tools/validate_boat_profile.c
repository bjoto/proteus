#include "boat.h"
#include "boat_profile.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int check_records(void)
{
    BoatProfile profile = { 0 };
    CHECK(boat_profile_stats(&profile, boat_metric_frame).mean == 0);
    CHECK(boat_metric_stats(NULL, 0).p95 == 0);
    float known[] = { 9, 1, 7, 3, 5 };
    BoatMetricStats stats = boat_metric_stats(known, 5);
    CHECK(stats.latest == 5 && stats.mean == 5 && stats.p50 == 5 && stats.p95 == 7 && stats.p99 == 7
        && stats.maximum == 9);
    CHECK(known[0] == 9); // Reporting must not reorder caller storage.
    BoatFrameMetrics running = { .frame_ms = 16,
        .simulation_ms = 8,
        .physics_ms = 6,
        .physics_max_ms = 3,
        .course_replay_ms = 1,
        .audio_ms = 2,
        .snapshot_render_ms = 4,
        .present_ms = 2,
        .ticks = 2 };
    boat_profile_add(&profile, running);
    CHECK(boat_profile_stats(&profile, boat_metric_ticks).latest == 2);
    CHECK(boat_profile_stats(&profile, boat_metric_simulation).latest == 8);
    CHECK(boat_profile_stats(&profile, boat_metric_physics).latest == 6);
    CHECK(boat_profile_stats(&profile, boat_metric_course_replay).latest == 1);
    CHECK(boat_profile_stats(&profile, boat_metric_frame).latest == 16);
    CHECK(boat_profile_discarded_seconds(.16, true) > .0599f);
    CHECK(boat_profile_discarded_seconds(.16, false) == 0);
    CHECK(boat_profile_discarded_seconds(.08, true) == 0);
    BoatFrameMetrics paused = { .frame_ms = 16, .paused = true };
    boat_profile_add(&profile, paused);
    CHECK(profile.frames[1].paused && profile.frames[1].discarded_active_ms == 0);
    BoatProfileSnapshot snapshot = boat_profile_snapshot(&profile);
    CHECK(snapshot.rows[0].latest == 0);
    CHECK(snapshot.rows[8].mean == 1);
    return 0;
}

static int check_drive(void)
{
    Boat plain, measured;
    boat_reset(&plain);
    boat_reset(&measured);
    BoatProfile profile = { 0 };
    for (int tick = 0; tick < 360; ++tick) {
        float steering = tick > 80 && tick < 250 ? .4f : 0;
        float throttle = tick < 180 ? 1 : 0;
        boat_step(&plain, steering, throttle);
        BoatFrameMetrics frame = { 0 };
        double started = boat_profile_time(false);
        boat_step(&measured, steering, throttle);
        frame.physics_ms = (float)((boat_profile_time(false) - started) * 1000.0);
        frame.physics_max_ms = frame.physics_ms;
        frame.simulation_ms = frame.physics_ms;
        frame.ticks = 1;
        boat_profile_add(&profile, frame);
    }
    CHECK(profile.count == BOAT_PROFILE_HISTORY);
    CHECK(plain.position.x == measured.position.x && plain.position.y == measured.position.y);
    CHECK(plain.velocity.x == measured.velocity.x && plain.velocity.y == measured.velocity.y);
    CHECK(plain.heading == measured.heading && plain.time == measured.time);
    CHECK(plain.water_settings.player_wake_count == measured.water_settings.player_wake_count);
    Color *left = calloc(320 * 200, sizeof(Color));
    Color *right = calloc(320 * 200, sizeof(Color));
    CHECK(left && right);
    Renderer3D renderer_left = { 0 }, renderer_right = { 0 };
    CHECK(boat_draw(&renderer_left, left, 320, 200, &plain, BOAT_CAMERA_DEFAULT));
    CHECK(boat_draw(&renderer_right, right, 320, 200, &measured, BOAT_CAMERA_DEFAULT));
    CHECK(memcmp(left, right, 320 * 200 * sizeof(Color)) == 0);
    free(left);
    free(right);
    return 0;
}

int main(void)
{
    if (check_records() || check_drive())
        return 1;
    puts("boating profile records and deterministic drive passed");
    return 0;
}
