#include "boat_replay.h"

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

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    CHECK(ghost_rendering() == 0);
    CHECK(MakeDirectory(argv[1]) == 0);
    char fixture[2048], best_path[2048], invalid[2048], wave_path[2048], saved_best[2048];
    CHECK(snprintf(fixture, sizeof(fixture), "%s/replay-fixture.run", argv[1]) > 0);
    CHECK(snprintf(best_path, sizeof(best_path), "%s/best.run", argv[1]) > 0);
    CHECK(snprintf(invalid, sizeof(invalid), "%s/invalid.run", argv[1]) > 0);
    CHECK(snprintf(wave_path, sizeof(wave_path), "%s/waves.run", argv[1]) > 0);
    CHECK(snprintf(saved_best, sizeof(saved_best), "%s/course-calm.run", argv[1]) > 0);
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
    CHECK(malformed(&loaded, invalid, "PROTEUS_RUN 999 1 1 0.0083333337679505348 0 2\n") == 0);
    CHECK(malformed(&loaded, invalid, "PROTEUS_RUN 1 1 1 0.0083333337679505348 0 99999999\n") == 0);
    CHECK(malformed(&loaded, invalid, "PROTEUS_RUN 1 1 1 0.0083333337679505348 0 2\n0 1 2 nan\n")
        == 0);
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
