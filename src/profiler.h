#ifndef PROTEUS_PROFILER_H
#define PROTEUS_PROFILER_H

#include "audio_spectrum.h"
#include "scenes.h"

#include <stdint.h>

enum {
    PROFILER_HISTORY_COUNT = 240,
};

typedef struct {
    float frame_ms[PROFILER_HISTORY_COUNT];
    float scene_ms[PROFILER_HISTORY_COUNT];
    float present_ms[PROFILER_HISTORY_COUNT];
    int next;
    int count;
} Profiler;

typedef struct {
    float latest;
    float minimum;
    float average;
    float percentile_95;
    float maximum;
} ProfileStats;

typedef struct {
    ProfileStats frame;
    ProfileStats scene_cost;
    ProfileStats present;
    AudioSpectrum spectrum;
    SceneFrame scene;
    float timeline_time;
    float observed_fps;
    float music_played;
    float music_length;
    int raylib_fps;
    int monitor_refresh_rate;
    int sample_count;
    int window_width;
    int window_height;
    int render_width;
    int render_height;
    int scale;
    int framebuffer_kib;
    bool music_playing;
} ProfilerSnapshot;

void profiler_add(Profiler *profiler, float frame_ms, float scene_ms, float present_ms);
ProfilerSnapshot profiler_snapshot(const Profiler *profiler, SceneFrame scene, float timeline_time,
    VirtualSize size, Rectangle destination, bool music_ready, Music music, AudioSpectrum spectrum);
void profiler_draw(const Profiler *profiler, const ProfilerSnapshot *snapshot, VirtualSize size);
ProfileStats profile_stats_linear(float *samples, int count);
void profile_stats_print(const char *name, ProfileStats stats);
double profile_time(void);
uint64_t framebuffer_checksum(const Color *pixels, size_t pixel_count);

#endif
