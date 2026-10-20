#include "boat_replay.h"
#include "raymath.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Replay check failed at line %d: %s\n", __LINE__, #condition);         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

// Prescribed route exercises the real swept gate timer and recorder without input/window timing.
static int flag_playback(void)
{
    BoatReplaySample samples[41] = { 0 };
    for (int i = 0; i < 41; ++i) {
        float t = i * .1f;
        samples[i] = (BoatReplaySample) { .at = t,
            .world_time = t + 7,
            .position = { t, -5 * t },
            .velocity = { 1, -5 },
            .heading = .2f * t,
            .roll = .15f * sinf(t * 2),
            .pitch = .03f * sinf(t) };
    }
    BoatReplay run = { .samples = samples, .count = 41 };
    BoatReplayFlag fine = { 0 }, coarse = { 0 };
    Boat a, b;
    for (int tick = 0; tick < 240; ++tick) {
        CHECK(boat_replay_sample(&run, tick * (double)BOAT_STEP, &a));
        boat_replay_animate_flag(&run, &a, &fine);
        if (tick % 2 == 0) {
            CHECK(boat_replay_sample(&run, tick * (double)BOAT_STEP, &b));
            boat_replay_animate_flag(&run, &b, &coarse);
            CHECK(memcmp(&fine.flag, &coarse.flag, sizeof(BoatFlag)) == 0);
        }
    }
    BoatReplayFlag seek = { 0 }, fresh = { 0 };
    CHECK(boat_replay_sample(&run, 3.175, &a));
    boat_replay_animate_flag(&run, &a, &seek);
    BoatFlagSurface paused = a.flag.surface;
    boat_replay_animate_flag(&run, &a, &seek);
    CHECK(memcmp(&paused, &a.flag.surface, sizeof(paused)) == 0);
    CHECK(boat_replay_sample(&run, .2, &a));
    boat_replay_animate_flag(&run, &a, &seek);
    CHECK(boat_replay_sample(&run, 3.175, &a));
    CHECK(boat_replay_sample(&run, 3.175, &b));
    boat_replay_animate_flag(&run, &a, &seek);
    boat_replay_animate_flag(&run, &b, &fresh);
    CHECK(memcmp(&a.flag, &b.flag, sizeof(BoatFlag)) == 0);
    WaterfrontPlayerState player = { 0 };
    boat_player_pose(&a, &player);
    for (int row = 0; row < BOAT_FLAG_ROWS; ++row)
        CHECK(Vector3Distance(
                  player.flag.points[row * BOAT_FLAG_COLS], boat_flag_attachment(player.hull, row))
            < .00001f);
    return 0;
}

static int route(BoatReplay *recording, BoatReplayRuns *runs, int ticks, bool waves)
{
    const Vector2 points[] = { { 0, 37 }, { 22, 7 }, { -22, -23 }, { 0, -55 } };
    Boat boat;
    boat_reset_course(&boat);
    boat.position = points[0];
    boat.water_settings = waves ? boat_water_gentle() : (BoatWater) { 0 };
    for (int leg = 0; leg < 3; ++leg) {
        for (int tick = 1; tick <= ticks; ++tick) {
            BoatReplaySample before = boat_replay_pose(&boat);
            float t = (float)tick / ticks;
            boat.position = (Vector2) { points[leg].x + (points[leg + 1].x - points[leg].x) * t,
                points[leg].y + (points[leg + 1].y - points[leg].y) * t };
            boat.velocity = (Vector2) { (boat.position.x - before.position.x) / BOAT_STEP,
                (boat.position.y - before.position.y) / BOAT_STEP };
            boat.heading = atan2f(boat.velocity.x, -boat.velocity.y);
            boat.camera_heading = boat.heading;
            boat.time += BOAT_STEP;
            boat_course_step(&boat.course, before.position, boat.position, BOAT_STEP);
            if (recording)
                boat_replay_record(recording, before, &boat);
            if (runs)
                boat_replay_runs_step(runs, before, &boat);
            if (boat.course.next_gate == BOAT_COURSE_GATES)
                return 0;
        }
    }
    CHECK(false);
}

static int malformed(BoatReplay *destination, const char *path, const char *contents)
{
    FILE *file = fopen(path, "w");
    CHECK(file);
    CHECK(fputs(contents, file) >= 0);
    CHECK(fclose(file) == 0);
    BoatReplaySample *samples = destination->samples;
    size_t count = destination->count;
    CHECK(!boat_replay_load(destination, path));
    CHECK(destination->samples == samples && destination->count == count);
    return 0;
}

static int ghost_rendering(void)
{
    enum { WIDTH = 320, HEIGHT = 200 };
    Color *plain = malloc(WIDTH * HEIGHT * sizeof(*plain));
    Color *pixels = malloc(WIDTH * HEIGHT * sizeof(*pixels));
    CHECK(plain && pixels);
    Renderer3D renderer = { 0 };
    Boat boat, ghost;
    boat_reset_course(&boat);
    ghost = boat;
    CHECK(boat_draw(&renderer, plain, WIDTH, HEIGHT, &boat, BOAT_CAMERA_DEFAULT));
    CHECK(boat_draw_with_ghost(
        &renderer, pixels, WIDTH, HEIGHT, &boat, BOAT_CAMERA_DEFAULT, &ghost, .5f));
    // Coincident ghost surfaces cannot overwrite the player's hull.
    CHECK(memcmp(plain, pixels, WIDTH * HEIGHT * sizeof(*plain)) == 0);
    ghost.position.y -= 10;
    CHECK(boat_draw_with_ghost(
        &renderer, pixels, WIDTH, HEIGHT, &boat, BOAT_CAMERA_DEFAULT, &ghost, .5f));
    int cyan = 0;
    for (int i = 0; i < WIDTH * HEIGHT; ++i)
        cyan += pixels[i].r == 90 && pixels[i].g == 240 && pixels[i].b == 255;
    CHECK(cyan > 10);
    CHECK(boat_draw_with_ghost(
        &renderer, pixels, WIDTH, HEIGHT, &boat, BOAT_CAMERA_DEFAULT, &ghost, 0));
    CHECK(memcmp(plain, pixels, WIDTH * HEIGHT * sizeof(*plain)) == 0);
    CHECK(renderer.ghost_opacity == 0);
    renderer3d_destroy(&renderer);
    free(plain);
    free(pixels);
    return 0;
}

static int playback_controls(const BoatReplay *run)
{
    BoatReplayRuns runs = { 0 };
    boat_replay_runs_reset(&runs);
    CHECK(boat_replay_speed(&runs) == 1);
    boat_replay_seek(&runs, 5);
    boat_replay_advance(&runs, .1, true);
    boat_replay_speed_step(&runs, 1);
    CHECK(runs.replay_time == 0 && boat_replay_speed(&runs) == 1);
    CHECK(!boat_replay_play(&runs, NULL));
    BoatReplay empty = { 0 };
    CHECK(!boat_replay_play(&runs, &empty));
    CHECK(boat_replay_play(&runs, run));
    CHECK(runs.playing == run && runs.replay_time == 0 && !runs.paused);
    double finish = run->splits[BOAT_COURSE_GATES - 1];
    unsigned char first[sizeof(BoatReplaySample)], last[sizeof(BoatReplaySample)];
    memcpy(first, &run->samples[0], sizeof(first));
    memcpy(last, &run->samples[run->count - 1], sizeof(last));
    // Every speed advances in course seconds, with no dependence on render frame count.
    const float speeds[] = { .25f, .5f, 1, 2, 4 };
    boat_replay_speed_step(&runs, -1);
    boat_replay_speed_step(&runs, -1);
    boat_replay_speed_step(&runs, -1);
    for (int i = 0; i < 5; ++i) {
        CHECK(boat_replay_speed(&runs) == speeds[i]);
        boat_replay_seek(&runs, 0);
        for (int frame = 0; frame < 60; ++frame)
            boat_replay_advance(&runs, 1.0 / 60, true);
        CHECK(fabs(runs.replay_time - speeds[i]) < .000001);
        double sixty = runs.replay_time;
        boat_replay_seek(&runs, 0);
        for (int frame = 0; frame < 120; ++frame)
            boat_replay_advance(&runs, 1.0 / 120, true);
        CHECK(fabs(runs.replay_time - sixty) < .000001);
        boat_replay_seek(&runs, 0);
        for (int frame = 0; frame < 30; ++frame)
            boat_replay_advance(&runs, 1.0 / 30, true);
        CHECK(fabs(runs.replay_time - sixty) < .000001);
        boat_replay_speed_step(&runs, 1);
    }
    CHECK(boat_replay_speed(&runs) == 4); // Clamp at both ends of the speed range.
    double before = runs.replay_time;
    boat_replay_advance(&runs, .1, false);
    CHECK(runs.replay_time == before && !runs.paused);
    runs.paused = true;
    boat_replay_advance(&runs, .1, true);
    CHECK(runs.replay_time == before);
    // Seeking works while paused and updates pose, heading, world time and gate progress.
    double gate = run->splits[1];
    boat_replay_seek(&runs, gate + .01);
    Boat view;
    CHECK(runs.paused && boat_replay_sample(run, runs.replay_time, &view));
    CHECK(view.course.next_gate == 2);
    boat_replay_seek(&runs, gate - .01);
    CHECK(boat_replay_sample(run, runs.replay_time, &view) && view.course.next_gate == 1);
    CHECK(view.time > 0 && view.course.elapsed == runs.replay_time);
    boat_replay_seek(&runs, finish + 10);
    CHECK(runs.replay_time == finish && runs.paused);
    CHECK(boat_replay_sample(run, runs.replay_time, &view));
    CHECK(view.position.y == -53 && view.course.next_gate == BOAT_COURSE_GATES);
    boat_replay_seek(&runs, -10);
    CHECK(runs.replay_time == 0 && runs.paused);
    boat_replay_seek(&runs, NAN);
    CHECK(runs.replay_time == 0);
    runs.paused = false;
    boat_replay_advance(&runs, INFINITY, true);
    boat_replay_advance(&runs, NAN, true);
    boat_replay_advance(&runs, -1, true);
    CHECK(runs.replay_time == 0);
    boat_replay_advance(&runs, 10, true);
    CHECK(fabs(runs.replay_time - .4) < .000001); // Long frames match the live time cap.
    boat_replay_seek(&runs, finish - .01);
    boat_replay_advance(&runs, .1, true);
    CHECK(runs.replay_time == finish && runs.paused);
    CHECK(boat_replay_play(&runs, run));
    CHECK(runs.replay_time == 0 && !runs.paused && boat_replay_speed(&runs) == 1);
    CHECK(memcmp(&run->samples[0], first, sizeof(first)) == 0);
    CHECK(memcmp(&run->samples[run->count - 1], last, sizeof(last)) == 0);
    boat_replay_runs_reset(&runs);
    CHECK(!runs.playing && runs.replay_time == 0 && boat_replay_speed(&runs) == 1);
    boat_replay_runs_destroy(&runs);
    return 0;
}

// Freely simulated banked S-course: the controller supplies inputs only.
static int banked_course(const char *directory)
{
    Boat b;
    boat_reset_course(&b);
    BoatReplay live = { 0 }, loaded = { 0 };
    float peak = 0;
    for (int tick = 0; tick < 120 * 120 && !live.complete; ++tick) {
        int gate = b.course.next_gate;
        CHECK(gate < BOAT_COURSE_GATES);
        Vector2 target = boat_course_gates[gate].centre;
        target.y -= 2;
        Vector2 delta = Vector2Subtract(target, b.position);
        float error = remainderf(atan2f(delta.x, -delta.y) - b.heading, 2 * PI);
        // H3's firmer hull needs less anticipatory yaw braking and full steering
        // authority through the offset gates. This is a test driver, not live input.
        float wheel = Clamp(3.0f * error - .7f * b.yaw_velocity, -1, 1);
        float throttle = fabsf(error) > .4f ? .25f : .45f;
        BoatReplaySample before = boat_replay_pose(&b);
        boat_step(&b, Clamp((wheel - b.wheel) / BOAT_STEP, -1, 1),
            Clamp((throttle - b.throttle) / (.5f * BOAT_STEP), -1, 1));
        boat_replay_record(&live, before, &b);
        if (live.count && !live.complete) {
            Boat sampled;
            CHECK(boat_replay_sample(&live, b.course.elapsed, &sampled));
            CHECK(fabsf(sampled.roll - b.roll) < .000002f
                && fabsf(sampled.pitch - b.pitch) < .000002f);
            WaterfrontPlayerState actual = { 0 }, recorded = { 0 };
            boat_player_pose(&b, &actual);
            boat_player_pose(&sampled, &recorded);
            CHECK(Vector3Distance(actual.engine.position, recorded.engine.position) < .00002f);
        }
        peak = fmaxf(peak, fabsf(b.roll));
        CHECK(fabsf(b.position.x) + 3 < BOAT_BASIN_HALF_X
            && fabsf(b.position.y) + 3 < BOAT_BASIN_HALF_Z);
    }
    printf("Banked course complete=%d gates=%d peak=%.6f position=%.3f,%.3f\n", live.complete,
        b.course.next_gate, peak * RAD2DEG, b.position.x, b.position.y);
    CHECK(live.complete && peak > 5 * DEG2RAD);
    char path[2048];
    CHECK(snprintf(path, sizeof(path), "%s/banked.run", directory) < (int)sizeof(path));
    CHECK(boat_replay_save(&live, path) && boat_replay_load(&loaded, path));
    CHECK(loaded.count == live.count);
    BoatReplayRuns transport = { 0 };
    CHECK(boat_replay_play(&transport, &loaded));
    transport.paused = true;
    // Exact samples and midpoints, then backward seeking through the same banked poses.
    for (int direction = 0; direction < 2; ++direction)
        for (size_t j = 0; j + 1 < live.count; ++j) {
            size_t i = direction ? live.count - 2 - j : j;
            for (int half = 0; half <= 1; ++half) {
                double at = live.samples[i].at;
                if (half)
                    at = (at + live.samples[i + 1].at) * .5;
                Boat original, replay, ghost;
                CHECK(boat_replay_sample(&live, at, &original));
                boat_replay_seek(&transport, at);
                boat_replay_advance(&transport, .1, true);
                CHECK(transport.paused && transport.replay_time == at);
                CHECK(boat_replay_sample(transport.playing, transport.replay_time, &replay));
                CHECK(boat_replay_sample(&loaded, at, &ghost));
                float expected = half ? (live.samples[i].roll + live.samples[i + 1].roll) * .5f
                                      : live.samples[i].roll;
                CHECK(fabsf(replay.roll - expected) < .000002f);
                CHECK(fabsf(replay.pitch - original.pitch) < .000002f);
                CHECK(fabsf(replay.roll - original.roll) < .000002f);
                CHECK(replay.turn_roll_load == 0 && replay.roll_velocity == 0);
                WaterfrontPlayerState a = { 0 }, c = { 0 }, g = { 0 };
                boat_player_pose(&original, &a);
                boat_player_pose(&replay, &c);
                boat_player_pose(&ghost, &g);
                CHECK(Vector3Distance(a.engine.position, c.engine.position) < .00002f);
                CHECK(Vector3Distance(c.cabin.position, g.cabin.position) < .000001f);
                CHECK(Vector3Distance(c.hull.rotation, g.hull.rotation) < .000001f);
            }
        }
    // Physical stress angles fit the existing file bounds; arbitrary and nonfinite angles fail.
    loaded.samples[1].roll = 29.7f * DEG2RAD;
    CHECK(boat_replay_save(&loaded, path));
    loaded.samples[1].roll = PI;
    CHECK(!boat_replay_save(&loaded, path));
    loaded.samples[1].roll = NAN;
    CHECK(!boat_replay_save(&loaded, path));
    CHECK(boat_replay_save(&live, path));
    boat_replay_runs_destroy(&transport);
    boat_replay_destroy(&live);
    boat_replay_destroy(&loaded);
    return 0;
}

int main(int argc, char **argv)
{
    CHECK(flag_playback() == 0);
    CHECK(argc == 2);
    CHECK(ghost_rendering() == 0);
    CHECK(MakeDirectory(argv[1]) == 0);
    char fixture[2048], best_path[2048], invalid[2048], wave_path[2048], saved_best[2048];
    CHECK(snprintf(fixture, sizeof(fixture), "%s/replay-fixture.run", argv[1]) > 0);
    CHECK(snprintf(best_path, sizeof(best_path), "%s/best.run", argv[1]) > 0);
    CHECK(snprintf(invalid, sizeof(invalid), "%s/invalid.run", argv[1]) > 0);
    CHECK(snprintf(wave_path, sizeof(wave_path), "%s/waves.run", argv[1]) > 0);
    CHECK(snprintf(saved_best, sizeof(saved_best), "%s/course-calm-physics%d.run", argv[1],
              BOAT_PHYSICS_REVISION)
        > 0);
    CHECK(banked_course(argv[1]) == 0);
    BoatReplay run = { 0 }, loaded = { 0 }, waves = { 0 };
    CHECK(route(&run, NULL, 480, false) == 0);
    CHECK(run.complete && !run.failed && run.count > 1000);
    CHECK(playback_controls(&run) == 0);
    CHECK(run.samples[0].at == 0);
    CHECK(fabsf(run.samples[0].position.y - 35) < .00001f);
    CHECK(fabsf(run.samples[run.count - 1].position.y + 53) < .00001f);
    CHECK(run.splits[0] == 0 && run.splits[3] == run.samples[run.count - 1].at);
    for (int i = 1; i < BOAT_COURSE_GATES; ++i)
        CHECK(run.splits[i] > run.splits[i - 1]);
    CHECK(boat_replay_save(&run, fixture));
    CHECK(boat_replay_save(&run, saved_best));
    CHECK(boat_replay_load(&loaded, fixture));
    CHECK(loaded.complete && !loaded.waves && loaded.count == run.count);
    CHECK(memcmp(loaded.splits, run.splits, sizeof(run.splits)) == 0);
    for (size_t i = 0; i < run.count; ++i) {
        CHECK(loaded.samples[i].at == run.samples[i].at);
        CHECK(loaded.samples[i].position.x == run.samples[i].position.x);
        CHECK(loaded.samples[i].position.y == run.samples[i].position.y);
        CHECK(loaded.samples[i].heading == run.samples[i].heading);
        CHECK(loaded.samples[i].world_time == run.samples[i].world_time);
    }
    Boat playback;
    CHECK(boat_replay_sample(&loaded, -5, &playback));
    CHECK(playback.position.y == 35 && playback.course.next_gate == 1);
    CHECK(boat_replay_sample(&loaded, 1000, &playback));
    CHECK(playback.position.y == -53 && playback.course.next_gate == BOAT_COURSE_GATES);
    CHECK(!boat_replay_sample(&loaded, NAN, &playback));
    size_t mid = run.count / 2;
    double at = (run.samples[mid].at + run.samples[mid + 1].at) / 2;
    CHECK(boat_replay_sample(&run, at, &playback));
    CHECK(fabsf(playback.position.x
              - (run.samples[mid].position.x + run.samples[mid + 1].position.x) / 2)
        < .00001f);
    CHECK(playback.course.next_gate == 2);
    // Angular interpolation must cross pi rather than spin through zero.
    loaded.samples[mid].heading = 3.13f;
    loaded.samples[mid + 1].heading = -3.13f;
    loaded.samples[mid].camera_heading = 3.13f;
    loaded.samples[mid + 1].camera_heading = -3.13f;
    CHECK(boat_replay_sample(&loaded, at, &playback));
    CHECK(fabsf(fabsf(playback.heading) - PI) < .0001f);
    CHECK(fabsf(fabsf(playback.camera_heading) - PI) < .0001f);
    // Reject truncated, unsupported, oversized and non-finite recordings without losing a best.
    CHECK(malformed(&loaded, invalid, "") == 0);
    CHECK(malformed(&loaded, invalid, "PROTEUS_RUN 999 1 2 0.0083333337679505348 0 2\n") == 0);
    CHECK(malformed(&loaded, invalid, "PROTEUS_RUN 1 1 2 0.0083333337679505348 0 99999999\n") == 0);
    CHECK(malformed(&loaded, invalid, "PROTEUS_RUN 1 1 2 0.0083333337679505348 0 2\n0 1 2 nan\n")
        == 0);
    // An otherwise valid previous-handling run cannot become an opponent or personal best.
    CHECK(boat_replay_save(&run, invalid));
    FILE *old = fopen(invalid, "r+");
    CHECK(old && fseek(old, 16, SEEK_SET) == 0 && fputc('0' + BOAT_PHYSICS_REVISION - 1, old) != EOF
        && fclose(old) == 0);
    BoatReplaySample *retained_samples = loaded.samples;
    CHECK(!boat_replay_load(&loaded, invalid) && loaded.samples == retained_samples);
    old = fopen(invalid, "r");
    int old_version, old_course, old_physics;
    CHECK(old && fscanf(old, "PROTEUS_RUN %d %d %d", &old_version, &old_course, &old_physics) == 3);
    CHECK(old_physics == BOAT_PHYSICS_REVISION - 1
        && fclose(old) == 0); // Rejection leaves the old file intact.

    CHECK(boat_replay_save(&run, invalid));
    FILE *trailer = fopen(invalid, "a");
    CHECK(trailer && fputs("garbage", trailer) >= 0 && fclose(trailer) == 0);
    CHECK(!boat_replay_load(&loaded, invalid));
    CHECK(boat_replay_copy(&loaded, &run));
    loaded.samples[mid].at = loaded.samples[mid - 1].at;
    CHECK(!boat_replay_save(&loaded, fixture)); // Existing valid file is untouched.
    CHECK(boat_replay_load(&loaded, fixture));
    CHECK(loaded.samples[mid].at == run.samples[mid].at);
    CHECK(route(&waves, NULL, 480, true) == 0);
    CHECK(boat_replay_save(&waves, wave_path));
    CHECK(boat_replay_load(&loaded, wave_path));
    CHECK(loaded.waves && boat_replay_sample(&loaded, 1, &playback));
    CHECK(playback.water_settings.amplitude == boat_water_gentle().amplitude);
    // An exact end-of-tick start yields one time-zero sample, not duplicate timestamps.
    Boat boat;
    boat_reset_course(&boat);
    boat.position = (Vector2) { 0, 36 };
    BoatReplaySample before = boat_replay_pose(&boat);
    boat.position.y = 35;
    boat_course_step(&boat.course, before.position, boat.position, BOAT_STEP);
    BoatReplay boundary = { 0 };
    boat_replay_record(&boundary, before, &boat);
    CHECK(boundary.count == 1 && boundary.samples[0].at == 0);
    before = boat_replay_pose(&boat);
    boat.position.y = 34;
    boat_course_step(&boat.course, before.position, boat.position, BOAT_STEP);
    boat_replay_record(&boundary, before, &boat);
    CHECK(boundary.count == 2 && boundary.samples[1].at == BOAT_STEP);
    // A stalled/overlong attempt stops recording and leaves existing completed runs alone.
    boat.course.elapsed = BOAT_REPLAY_MAX_SECONDS + 1;
    boat_replay_record(&boundary, before, &boat);
    CHECK(boundary.failed && !boundary.complete);
    boat_replay_destroy(&boundary);
    // Session lifecycle: completion, pinned ghost, aborted retry, slower/tied/faster attempts.
    BoatReplayRuns session = { .ghost_visible = true };
    snprintf(session.best_path, sizeof(session.best_path), "%s", best_path);
    CHECK(route(NULL, &session, 480, false) == 0);
    CHECK(session.last.complete && session.best.complete && !session.opponent.complete);
    double first = session.best.splits[3];
    BoatReplaySample *first_samples = session.last.samples;
    CHECK(route(NULL, &session, 300, false) == 0);
    CHECK(session.last.samples == first_samples && session.best.splits[3] == first);
    boat_replay_runs_reset(&session);
    CHECK(session.opponent.complete && session.opponent.splits[3] == first);
    CHECK(route(NULL, &session, 600, false) == 0);
    CHECK(session.last.splits[3] > first && session.best.splits[3] == first);
    CHECK(session.opponent.splits[3] == first);
    boat_replay_runs_reset(&session);
    CHECK(route(NULL, &session, 480, false) == 0);
    CHECK(session.best.splits[3] == first);
    BoatReplaySample *old_best = session.best.samples;
    boat_replay_runs_reset(&session);
    CHECK(route(NULL, &session, 480, false) == 0);
    CHECK(session.best.samples == old_best); // Ties do not replace the best.
    boat_replay_runs_reset(&session);
    CHECK(route(NULL, &session, 360, false) == 0);
    CHECK(session.best.splits[3] < first && session.opponent.splits[3] == first);
    CHECK(boat_replay_load(&loaded, best_path));
    CHECK(loaded.splits[3] == session.best.splits[3]);
    boat_replay_runs_reset(&session);
    CHECK(session.opponent.splits[3] == session.best.splits[3]);
    CHECK(!session.recording.count && session.last.complete);
    boat_reset_course(&boat);
    boat.position = (Vector2) { 0, 36 };
    before = boat_replay_pose(&boat);
    boat.position.y = 34;
    boat_course_step(&boat.course, before.position, boat.position, BOAT_STEP);
    boat_replay_runs_step(&session, before, &boat);
    CHECK(session.recording.count > 0);
    double retained = session.best.splits[3];
    boat_replay_runs_reset(&session);
    CHECK(!session.recording.count && session.best.splits[3] == retained);
    CHECK(session.last.complete);
    // Save failure keeps a playable best in memory.
    snprintf(session.best_path, sizeof(session.best_path), "%s/missing/best.run", fixture);
    CHECK(route(NULL, &session, 300, false) == 0);
    CHECK(session.best.complete && session.last.complete);
    CHECK(strstr(session.notice, "memory"));
    boat_replay_runs_destroy(&session);
    boat_replay_destroy(&run);
    boat_replay_destroy(&loaded);
    boat_replay_destroy(&waves);
    remove(best_path);
    remove(invalid);
    puts("Boat recording, playback and personal-best checks passed");
    return 0;
}
