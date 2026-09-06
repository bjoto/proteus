#ifndef PROTEUS_AUDIO_SPECTRUM_H
#define PROTEUS_AUDIO_SPECTRUM_H

#include "raylib.h"

#include <stdbool.h>
#include <stddef.h>

enum {
    AUDIO_SPECTRUM_WINDOW_SIZE = 2048,
    AUDIO_SPECTRUM_BAND_COUNT = 6,
};

typedef enum {
    // 30-90, 90-220, 220-700, 700-2000, 2000-5000, and 5000-10000 Hz.
    AUDIO_SPECTRUM_SUB_BASS,
    AUDIO_SPECTRUM_BASS,
    AUDIO_SPECTRUM_LOW_MID,
    AUDIO_SPECTRUM_MID,
    AUDIO_SPECTRUM_PRESENCE,
    AUDIO_SPECTRUM_HIGH,
} AudioSpectrumBand;

typedef struct {
    float bands[AUDIO_SPECTRUM_BAND_COUNT];
    float overall;
    float onset;
} AudioSpectrum;

AudioSpectrum audio_spectrum_analyze_mono(
    const float *samples, size_t sample_count, unsigned int sample_rate);
bool audio_spectrum_attach(AudioStream stream);
void audio_spectrum_detach(AudioStream stream);
void audio_spectrum_update(AudioSpectrum *spectrum, float delta_time);

#endif
