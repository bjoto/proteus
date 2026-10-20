#ifndef PROTEUS_BOAT_REPLAY_H
#define PROTEUS_BOAT_REPLAY_H

#include "boat.h"

#define BOAT_REPLAY_MAX_SECONDS 600
#define BOAT_PHYSICS_REVISION 4
#define BOAT_REPLAY_MAX_SAMPLES (BOAT_REPLAY_MAX_SECONDS * 120 + 2)

typedef struct {
    double at;
    Vector2 position, velocity;
    float heading, height, pitch, roll, engine_angle;
    float wheel, throttle, engine_response, camera_heading, world_time;
} BoatReplaySample;

// Zero-initialize; owns samples. A recording contains only START through FINISH.
typedef struct {
    BoatReplaySample *samples;
    size_t count, capacity;
    double splits[BOAT_COURSE_GATES];
    bool waves, complete, failed;
} BoatReplay;

BoatReplaySample boat_replay_pose(const Boat *boat);
// Called after each physics tick, with the pose immediately before that tick.
void boat_replay_record(BoatReplay *run, BoatReplaySample before, const Boat *boat);
bool boat_replay_sample(const BoatReplay *run, double at, Boat *boat);
bool boat_replay_copy(BoatReplay *destination, const BoatReplay *source);
void boat_replay_destroy(BoatReplay *run);
// Versioned portable text; load preserves destination on failure, save atomically replaces path.
bool boat_replay_load(BoatReplay *run, const char *path);
bool boat_replay_save(const BoatReplay *run, const char *path);
bool boat_replay_data_directory(char *path, size_t size);

// Race-session state survives boat resets; opponent is pinned until the next attempt.
typedef struct {
    BoatReplay recording, last, best, opponent, reference;
    const BoatReplay *playing;
    double replay_time;
    int replay_speed_index; // 0..4: quarter, half, normal, double, quadruple speed.
    bool paused, ghost_visible, finished;
    char best_path[2048];
    const char *export_path;
    const char *notice;
} BoatReplayRuns;
// Transport controls change playback only; recorded samples and personal bests are untouched.
bool boat_replay_play(BoatReplayRuns *runs, const BoatReplay *run);
void boat_replay_seek(BoatReplayRuns *runs, double at);
float boat_replay_speed(const BoatReplayRuns *runs);
void boat_replay_speed_step(BoatReplayRuns *runs, int direction);
// Frame delta is capped at 0.1 seconds, as in live simulation. Finish pauses playback.
void boat_replay_advance(BoatReplayRuns *runs, double seconds, bool focused);
void boat_replay_runs_destroy(BoatReplayRuns *runs);
void boat_replay_runs_reset(BoatReplayRuns *runs);
void boat_replay_runs_step(BoatReplayRuns *runs, BoatReplaySample before, const Boat *boat);

#endif
