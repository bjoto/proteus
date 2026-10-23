#ifndef PROTEUS_BOAT_REPLAY_H
#define PROTEUS_BOAT_REPLAY_H

#include "boat_presentation.h"

#define BOAT_REPLAY_MAX_SECONDS 600
// Recorded world time is absolute session time, so a course may start long after launch.
#define BOAT_REPLAY_MAX_WORLD_SECONDS (100.0 * 365 * 86400)
#define BOAT_PHYSICS_REVISION 6
#define BOAT_REPLAY_MAX_SAMPLES (BOAT_REPLAY_MAX_SECONDS * BOAT_TICK_HZ + 2)

typedef struct {
    double at;
    Vector2 position, velocity;
    float heading, height, pitch, roll, engine_angle;
    float wheel, throttle, engine_response, camera_heading;
    double world_time;
} BoatReplaySample;

// Zero-initialize; owns samples. A recording contains only START through FINISH.
typedef struct {
    BoatReplaySample *samples;
    size_t count, capacity;
    double splits[BOAT_COURSE_GATES];
    bool waves, complete, failed;
} BoatReplay;

// Physical pose plus the presentation's camera follow, recorded as a view value.
BoatReplaySample boat_replay_pose(const Boat *boat, const BoatPresentation *presentation);
// Called after each physics tick, with the pose immediately before that tick.
void boat_replay_record(BoatReplay *run, BoatReplaySample before, const Boat *boat,
    const BoatPresentation *presentation, const BoatWorld *world);
// Water a recording was made in. View a sample in its own world, never the live one.
BoatWater boat_replay_water(const BoatReplay *run);
// A view boat at AT. PRESENTATION, when given, is reset to the recorded camera follow;
// its cloth comes from boat_replay_animate_flag().
bool boat_replay_sample(
    const BoatReplay *run, double at, Boat *boat, BoatPresentation *presentation);
// Caller-owned visual history, separate for playback and ghost. Zero-initialize.
// Continuous playback advances at fixed ticks; jumps reconstruct at most one second.
// Seek cloth is an approximation, not additional recorded physics or a file-format change.
typedef struct {
    const BoatReplay *run;
    const BoatReplaySample *samples;
    double start_time;
    BoatFlag flag;
    int tick;
} BoatReplayFlag;
void boat_replay_animate_flag(
    const BoatReplay *run, const Boat *boat, BoatReplayFlag *cache, BoatFlag *flag);
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
void boat_replay_runs_step(BoatReplayRuns *runs, BoatReplaySample before, const Boat *boat,
    const BoatPresentation *presentation, const BoatWorld *world);

#endif
