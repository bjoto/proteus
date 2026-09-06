#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "xm_audio.h"

#define JAR_XM_IMPLEMENTATION
#include "jar_xm.h"

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s module.xm\n", argv[0]);
        return 2;
    }
    jar_xm_context_t *xm = NULL;
    int result = jar_xm_create_context_from_file(&xm, 48000, argv[1]);
    if (result != 0) {
        fprintf(stderr, "invalid XM (jar_xm error %d)\n", result);
        return 1;
    }
    printf("%s: %u channels, %u patterns, %u instruments\n", jar_xm_get_module_name(xm),
        jar_xm_get_number_of_channels(xm), jar_xm_get_number_of_patterns(xm),
        jar_xm_get_number_of_instruments(xm));
    const float seek_time = 48.58f;
    Music music = {
        .stream.sampleRate = 48000,
        .ctxData = xm,
    };
    if (!xm_music_start_at(music, seek_time)) {
        fprintf(stderr, "unable to seek XM decoder\n");
        jar_xm_free_context(xm);
        return 1;
    }
    uint8_t pattern_index = 0;
    uint64_t generated_frames = 0;
    jar_xm_get_position(xm, &pattern_index, NULL, NULL, &generated_frames);
    uint64_t expected_frames = (uint64_t)((double)seek_time * music.stream.sampleRate);
    if (pattern_index != 6 || generated_frames != expected_frames) {
        fprintf(stderr,
            "XM seek mismatch: pattern %u, frames %llu (expected pattern 6, frames %llu)\n",
            pattern_index, (unsigned long long)generated_frames,
            (unsigned long long)expected_frames);
        jar_xm_free_context(xm);
        return 1;
    }
    printf("seek %.2f seconds: pattern %u, frame %llu\n", seek_time, pattern_index,
        (unsigned long long)generated_frames);

    jar_xm_reset(xm);
    float frames[48000 * 2];
    jar_xm_generate_samples(xm, frames, 48000);
    jar_xm_free_context(xm);
    return 0;
}
