#include "xm_audio.h"

#include <stddef.h>
#include <stdint.h>

// raylib 6.0 exposes Music.ctxData but does not expose the bundled jar_xm API.
// These declarations match the pinned decoder and let us advance its state before playback.
typedef struct jar_xm_context_s jar_xm_context_t;
void jar_xm_generate_samples(jar_xm_context_t *context, float *output, size_t frame_count);
void jar_xm_reset(jar_xm_context_t *context);

bool xm_music_start_at(Music music, float position)
{
    if (music.ctxData == NULL || music.stream.sampleRate == 0 || position < 0.0f)
        return false;

    jar_xm_context_t *context = music.ctxData;
    jar_xm_reset(context);

    uint64_t frames_remaining = (uint64_t)((double)position * music.stream.sampleRate);
    enum {
        DISCARD_FRAME_COUNT = 4096,
    };
    float discard[DISCARD_FRAME_COUNT * 2];
    while (frames_remaining > 0) {
        size_t frame_count = frames_remaining < DISCARD_FRAME_COUNT ? (size_t)frames_remaining
                                                                    : DISCARD_FRAME_COUNT;
        jar_xm_generate_samples(context, discard, frame_count);
        frames_remaining -= frame_count;
    }
    return true;
}
