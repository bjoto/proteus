#ifndef PROTEUS_XM_AUDIO_H
#define PROTEUS_XM_AUDIO_H

#include "raylib.h"

#include <stdbool.h>

// Advance a freshly loaded, stopped XM stream to reconstruct its playback state.
bool xm_music_start_at(Music music, float position);

#endif
