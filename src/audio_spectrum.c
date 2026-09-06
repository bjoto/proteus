#include "audio_spectrum.h"

#include <math.h>
#include <stdatomic.h>
#include <string.h>

enum {
    CAPTURE_BUFFER_SIZE = 16384,
};

static const float BAND_MIN_FREQUENCIES[AUDIO_SPECTRUM_BAND_COUNT] = {
    30.0f,
    90.0f,
    220.0f,
    700.0f,
    2000.0f,
    5000.0f,
};

static const float BAND_MAX_FREQUENCIES[AUDIO_SPECTRUM_BAND_COUNT] = {
    90.0f,
    220.0f,
    700.0f,
    2000.0f,
    5000.0f,
    10000.0f,
};

static float capture_buffer[CAPTURE_BUFFER_SIZE];
static _Atomic unsigned int capture_read;
static _Atomic unsigned int capture_write;
static float analysis_window[AUDIO_SPECTRUM_WINDOW_SIZE];
static size_t analysis_sample_count;
static AudioSpectrum previous_instantaneous;
static bool processor_attached;
static unsigned int capture_sample_rate;

static float clamp01(float value) { return fmaxf(0.0f, fminf(value, 1.0f)); }

static void fft(float *real, float *imaginary)
{
    for (unsigned int i = 1, reversed = 0; i < AUDIO_SPECTRUM_WINDOW_SIZE; ++i) {
        unsigned int bit = AUDIO_SPECTRUM_WINDOW_SIZE >> 1;
        while (reversed & bit) {
            reversed ^= bit;
            bit >>= 1;
        }
        reversed ^= bit;
        if (i < reversed) {
            float real_value = real[i];
            real[i] = real[reversed];
            real[reversed] = real_value;
            float imaginary_value = imaginary[i];
            imaginary[i] = imaginary[reversed];
            imaginary[reversed] = imaginary_value;
        }
    }

    const float tau = 6.28318530718f;
    for (unsigned int length = 2; length <= AUDIO_SPECTRUM_WINDOW_SIZE; length <<= 1) {
        float angle = -tau / (float)length;
        float step_real = cosf(angle);
        float step_imaginary = sinf(angle);
        for (unsigned int start = 0; start < AUDIO_SPECTRUM_WINDOW_SIZE; start += length) {
            float twiddle_real = 1.0f;
            float twiddle_imaginary = 0.0f;
            for (unsigned int offset = 0; offset < length / 2; ++offset) {
                unsigned int even = start + offset;
                unsigned int odd = even + length / 2;
                float odd_real = real[odd] * twiddle_real - imaginary[odd] * twiddle_imaginary;
                float odd_imaginary = real[odd] * twiddle_imaginary + imaginary[odd] * twiddle_real;
                real[odd] = real[even] - odd_real;
                imaginary[odd] = imaginary[even] - odd_imaginary;
                real[even] += odd_real;
                imaginary[even] += odd_imaginary;

                float next_real = twiddle_real * step_real - twiddle_imaginary * step_imaginary;
                twiddle_imaginary = twiddle_real * step_imaginary + twiddle_imaginary * step_real;
                twiddle_real = next_real;
            }
        }
    }
}

static float normalize_level(float level)
{
    const float scale = 16.0f;
    return clamp01(log1pf(level * scale) / log1pf(scale));
}

AudioSpectrum audio_spectrum_analyze_mono(
    const float *samples, size_t sample_count, unsigned int sample_rate)
{
    AudioSpectrum spectrum = { 0 };
    if (samples == NULL || sample_count == 0 || sample_rate == 0)
        return spectrum;

    float real[AUDIO_SPECTRUM_WINDOW_SIZE] = { 0 };
    float imaginary[AUDIO_SPECTRUM_WINDOW_SIZE] = { 0 };
    size_t copy_count
        = sample_count < AUDIO_SPECTRUM_WINDOW_SIZE ? sample_count : AUDIO_SPECTRUM_WINDOW_SIZE;
    size_t source_offset = sample_count - copy_count;
    size_t destination_offset = AUDIO_SPECTRUM_WINDOW_SIZE - copy_count;
    float square_sum = 0.0f;
    const float tau = 6.28318530718f;
    for (size_t i = 0; i < copy_count; ++i) {
        float sample = samples[source_offset + i];
        float position = (float)(destination_offset + i) / (float)(AUDIO_SPECTRUM_WINDOW_SIZE - 1);
        float window = 0.5f * (1.0f - cosf(tau * position));
        real[destination_offset + i] = sample * window;
        square_sum += sample * sample;
    }
    fft(real, imaginary);

    float band_square_sums[AUDIO_SPECTRUM_BAND_COUNT] = { 0 };
    int band_bin_counts[AUDIO_SPECTRUM_BAND_COUNT] = { 0 };
    for (int bin = 1; bin <= AUDIO_SPECTRUM_WINDOW_SIZE / 2; ++bin) {
        float frequency = (float)bin * (float)sample_rate / AUDIO_SPECTRUM_WINDOW_SIZE;
        float magnitude = 4.0f * sqrtf(real[bin] * real[bin] + imaginary[bin] * imaginary[bin])
            / AUDIO_SPECTRUM_WINDOW_SIZE;
        for (int band = 0; band < AUDIO_SPECTRUM_BAND_COUNT; ++band) {
            if (frequency >= BAND_MIN_FREQUENCIES[band] && frequency < BAND_MAX_FREQUENCIES[band]) {
                band_square_sums[band] += magnitude * magnitude;
                ++band_bin_counts[band];
                break;
            }
        }
    }

    for (int band = 0; band < AUDIO_SPECTRUM_BAND_COUNT; ++band) {
        if (band_bin_counts[band] > 0) {
            float level = sqrtf(band_square_sums[band] / (float)band_bin_counts[band]);
            spectrum.bands[band] = normalize_level(level);
        }
    }
    spectrum.overall = normalize_level(sqrtf(square_sum / (float)copy_count));
    return spectrum;
}

static void capture_audio(void *buffer_data, unsigned int frame_count)
{
    const float *samples = buffer_data;
    unsigned int write = atomic_load_explicit(&capture_write, memory_order_relaxed);
    unsigned int read = atomic_load_explicit(&capture_read, memory_order_acquire);
    for (unsigned int frame = 0; frame < frame_count; ++frame) {
        unsigned int next = (write + 1) % CAPTURE_BUFFER_SIZE;
        if (next == read)
            break;
        capture_buffer[write] = (samples[frame * 2] + samples[frame * 2 + 1]) * 0.5f;
        write = next;
    }
    atomic_store_explicit(&capture_write, write, memory_order_release);
}

static void reset_capture(void)
{
    atomic_store_explicit(&capture_read, 0, memory_order_relaxed);
    atomic_store_explicit(&capture_write, 0, memory_order_relaxed);
    memset(analysis_window, 0, sizeof(analysis_window));
    analysis_sample_count = 0;
    previous_instantaneous = (AudioSpectrum) { 0 };
}

bool audio_spectrum_attach(AudioStream stream)
{
    if (processor_attached || !IsAudioStreamValid(stream))
        return false;
    reset_capture();
    capture_sample_rate = stream.sampleRate;
    AttachAudioStreamProcessor(stream, capture_audio);
    processor_attached = true;
    return true;
}

void audio_spectrum_detach(AudioStream stream)
{
    if (!processor_attached)
        return;
    DetachAudioStreamProcessor(stream, capture_audio);
    processor_attached = false;
    capture_sample_rate = 0;
    reset_capture();
}

static float smooth_level(
    float current, float target, float delta_time, float attack, float release)
{
    float time_constant = target > current ? attack : release;
    float amount = 1.0f - expf(-delta_time / time_constant);
    return current + (target - current) * clamp01(amount);
}

void audio_spectrum_update(AudioSpectrum *spectrum, float delta_time)
{
    if (spectrum == NULL)
        return;

    float captured[CAPTURE_BUFFER_SIZE];
    size_t captured_count = 0;
    unsigned int read = atomic_load_explicit(&capture_read, memory_order_relaxed);
    unsigned int write = atomic_load_explicit(&capture_write, memory_order_acquire);
    while (read != write) {
        captured[captured_count++] = capture_buffer[read];
        read = (read + 1) % CAPTURE_BUFFER_SIZE;
    }
    atomic_store_explicit(&capture_read, read, memory_order_release);
    if (captured_count == 0)
        return;

    if (captured_count >= AUDIO_SPECTRUM_WINDOW_SIZE) {
        memcpy(analysis_window, captured + captured_count - AUDIO_SPECTRUM_WINDOW_SIZE,
            sizeof(analysis_window));
        analysis_sample_count = AUDIO_SPECTRUM_WINDOW_SIZE;
    } else {
        size_t retained_count = analysis_sample_count;
        if (retained_count + captured_count > AUDIO_SPECTRUM_WINDOW_SIZE)
            retained_count = AUDIO_SPECTRUM_WINDOW_SIZE - captured_count;
        memmove(analysis_window, analysis_window + analysis_sample_count - retained_count,
            retained_count * sizeof(*analysis_window));
        memcpy(analysis_window + retained_count, captured, captured_count * sizeof(*captured));
        analysis_sample_count = retained_count + captured_count;
    }
    if (analysis_sample_count < AUDIO_SPECTRUM_WINDOW_SIZE)
        return;

    AudioSpectrum instantaneous = audio_spectrum_analyze_mono(
        analysis_window, AUDIO_SPECTRUM_WINDOW_SIZE, capture_sample_rate);
    float flux = 0.0f;
    for (int band = 0; band < AUDIO_SPECTRUM_BAND_COUNT; ++band) {
        flux += fmaxf(0.0f, instantaneous.bands[band] - previous_instantaneous.bands[band]);
    }
    instantaneous.onset = clamp01(flux * 1.5f);

    for (int band = 0; band < AUDIO_SPECTRUM_BAND_COUNT; ++band) {
        spectrum->bands[band] = smooth_level(
            spectrum->bands[band], instantaneous.bands[band], delta_time, 0.035f, 0.22f);
    }
    spectrum->overall
        = smooth_level(spectrum->overall, instantaneous.overall, delta_time, 0.035f, 0.22f);
    spectrum->onset = smooth_level(spectrum->onset, instantaneous.onset, delta_time, 0.015f, 0.12f);
    previous_instantaneous = instantaneous;
}
