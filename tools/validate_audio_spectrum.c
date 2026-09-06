#include "audio_spectrum.h"

#include <math.h>
#include <stdio.h>

static bool validate_band(AudioSpectrumBand expected_band, float frequency)
{
    const float tau = 6.28318530718f;
    const unsigned int sample_rate = 48000;
    float samples[AUDIO_SPECTRUM_WINDOW_SIZE];
    for (int i = 0; i < AUDIO_SPECTRUM_WINDOW_SIZE; ++i)
        samples[i] = 0.8f * sinf(tau * frequency * (float)i / (float)sample_rate);

    AudioSpectrum spectrum
        = audio_spectrum_analyze_mono(samples, AUDIO_SPECTRUM_WINDOW_SIZE, sample_rate);
    float expected = spectrum.bands[expected_band];
    if (expected < 0.05f) {
        fprintf(
            stderr, "band %d did not detect %.2f Hz: %.4f\n", expected_band, frequency, expected);
        return false;
    }
    for (int band = 0; band < AUDIO_SPECTRUM_BAND_COUNT; ++band) {
        if (band != (int)expected_band && spectrum.bands[band] >= expected * 0.5f) {
            fprintf(stderr, "band %d incorrectly matched %.2f Hz: %.4f versus %.4f\n", band,
                frequency, spectrum.bands[band], expected);
            return false;
        }
    }
    return true;
}

int main(void)
{
    const float frequencies[AUDIO_SPECTRUM_BAND_COUNT] = {
        46.875f,
        140.625f,
        421.875f,
        984.375f,
        3000.0f,
        7031.25f,
    };
    for (int band = 0; band < AUDIO_SPECTRUM_BAND_COUNT; ++band) {
        if (!validate_band((AudioSpectrumBand)band, frequencies[band]))
            return 1;
    }

    float silence[AUDIO_SPECTRUM_WINDOW_SIZE] = { 0 };
    AudioSpectrum spectrum
        = audio_spectrum_analyze_mono(silence, AUDIO_SPECTRUM_WINDOW_SIZE, 48000);
    if (spectrum.overall != 0.0f) {
        fprintf(stderr, "silence produced a non-zero overall level: %.4f\n", spectrum.overall);
        return 1;
    }

    printf("audio spectrum: 2048 samples, 6 bands validated\n");
    return 0;
}
